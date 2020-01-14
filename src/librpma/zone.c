/*
 * Copyright 2019, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * zone.c -- entry points for librpma zone
 */

#include <netinet/in.h>
#include <stdint.h>
#include <rdma/fabric.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_eq.h>
#include <rdma/fi_endpoint.h>

#include <librpma.h>

#include "ravl.h"
#include "zone.h"
#include "config.h"
#include "rpma_utils.h"
#include "alloc.h"
#include "valgrind_internal.h"
#include "connection.h"

#define PROVIDER_STR "sockets" /* XXX */
#define RX_TX_SIZE 256 /* XXX */
#define RPMA_FIVERSION FI_VERSION(1, 4)
#define DEFAULT_TIMEOUT 1000

#define EQ_TIMEOUT 1
#define EQ_ERR 2

static int
hints_new(struct fi_info **hints_ptr)
{
	struct fi_info *hints = fi_allocinfo();
	if (!hints) {
		ERR("!fi_allocinfo");
		return RPMA_E_ERRNO;
	}

	/* connection-oriented endpoint */
	hints->ep_attr->type = FI_EP_MSG;

	/*
	 * Basic memory registration mode indicates that MR attributes
	 * (rkey, lkey) are selected by provider.
	 */
	hints->domain_attr->mr_mode = FI_MR_BASIC;

	/*
	 * FI_THREAD_SAFE indicates MT applications can access any
	 * resources through interface without any restrictions
	 */
	hints->domain_attr->threading = FI_THREAD_SAFE;

	/*
	 * FI_MSG - SEND and RECV
	 * FI_RMA - WRITE and READ
	 */
	hints->caps = FI_MSG | FI_RMA;

	/* must register locally accessed buffers */
	hints->mode = FI_CONTEXT | FI_LOCAL_MR | FI_RX_CQ_DATA;

	/* READ-after-WRITE and SEND-after-WRITE message ordering required */
	hints->tx_attr->msg_order = FI_ORDER_RAW | FI_ORDER_SAW;

	hints->addr_format = FI_SOCKADDR;

	hints->fabric_attr->prov_name = strdup(PROVIDER_STR);
	if (!hints->fabric_attr->prov_name) {
		ERR("!strdup");
		goto err_strdup;
	}

	hints->tx_attr->size = RX_TX_SIZE;
	hints->rx_attr->size = RX_TX_SIZE;

	*hints_ptr = hints;

	return 0;

err_strdup:
	Free(hints);
	return RPMA_E_ERRNO;
}

static void
hints_delete(struct fi_info **hints)
{
	struct fi_info *ptr = *hints;

	Free(ptr->fabric_attr->prov_name);
	ptr->fabric_attr->prov_name = NULL;

	rpma_utils_freeinfo(hints);
}

static int
info_new(struct rpma_config *cfg, struct fi_info **info)
{
	struct fi_info *hints = NULL;
	int ret = hints_new(&hints);
	if (ret)
		return ret;

	uint64_t flags = 0;
	if (cfg->flags & RPMA_CONFIG_IS_SERVER)
		flags |= FI_SOURCE;

	ret = fi_getinfo(RPMA_FIVERSION, cfg->addr, cfg->service, flags,
			hints, info);
	if (ret) {
		ERR_FI(ret, "fi_getinfo");
		goto err_getinfo;
	}

err_getinfo:
	hints_delete(&hints);
	return ret;
}

static void
info_delete(struct fi_info **info_ptr)
{
	rpma_utils_freeinfo(info_ptr);
}

static int
eq_new(struct fid_fabric *fabric, struct fid_eq **eq_ptr)
{
	struct fi_eq_attr eq_attr = {
		.size = 0, /* use default value */
		.flags = 0,
		.wait_obj = FI_WAIT_UNSPEC,
		.signaling_vector = 0,
		.wait_set = NULL,
	};

	int ret = fi_eq_open(fabric, &eq_attr, eq_ptr, NULL);
	if (ret) {
		ERR_FI(ret, "fi_eq_open");
		return ret;
	}

	return 0;
}

static int
zone_init(struct rpma_config *cfg, struct rpma_zone *zone)
{
	int ret = info_new(cfg, &zone->info);
	if (ret)
		return ret;

	ret = fi_fabric(zone->info->fabric_attr, &zone->fabric, NULL);
	if (ret) {
		ERR_FI(ret, "fi_fabric");
		goto err_fabric;
	}

	ret = fi_domain(zone->fabric, zone->info, &zone->domain, NULL);
	if (ret) {
		ERR_FI(ret, "fi_domain");
		goto err_domain;
	}

	ret = eq_new(zone->fabric, &zone->eq);
	if (ret)
		goto err_eq;

	return 0;

err_eq:
	rpma_utils_res_close(&zone->domain->fid, "domain");
err_domain:
	rpma_utils_res_close(&zone->fabric->fid, "fabric");
err_fabric:
	info_delete(&zone->info);
	return ret;
}

static void
zone_fini(struct rpma_zone *zone)
{
	if (zone->pep)
		rpma_utils_res_close(&zone->pep->fid, "pep");

	if (zone->eq)
		rpma_utils_res_close(&zone->eq->fid, "eq");

	if (zone->domain)
			rpma_utils_res_close(&zone->domain->fid, "domain");

	if (zone->fabric)
			rpma_utils_res_close(&zone->fabric->fid, "fabric");

	info_delete(&zone->info);
}

struct ep_conn_pair
{
	fid_t ep_fid;
	struct rpma_connection *conn;
};

static int
ep_conn_pair_compare(const void *lhs, const void *rhs)
{
	const struct ep_conn_pair *l = lhs;
	const struct ep_conn_pair *r = rhs;

	int64_t diff = (int64_t)l->ep_fid - (int64_t)r->ep_fid;
	if (diff != 0)
		return diff > 0 ? 1 : -1;

	return 0;
}

static void
conn_store(struct ravl *store, struct rpma_connection *conn)
{
	struct ep_conn_pair *pair = Malloc(sizeof(*pair));
	pair->ep_fid = &conn->ep->fid;
	pair->conn = conn;

	ravl_insert(store, pair);
}

static struct rpma_connection *
conn_restore(struct ravl *store, fid_t ep_fid)
{
	struct ep_conn_pair to_find;
	to_find.ep_fid = ep_fid;
	to_find.conn = NULL;

	struct ravl_node *node = ravl_find(store, &to_find, RAVL_PREDICATE_EQUAL);
	if (!node)
		return NULL;

	struct ep_conn_pair *found = ravl_data(node);
	if (!found)
		return NULL;

	struct rpma_connection *ret = found->conn;
	Free(found);
	ravl_remove(store, node);

	return ret;
}

int
rpma_zone_new(struct rpma_config *cfg, struct rpma_zone **zone)
{
	struct rpma_zone *ptr = Malloc(sizeof(struct rpma_zone));
	if (!ptr)
		return RPMA_E_ERRNO;

	ptr->info = NULL;
	ptr->fabric = NULL;
	ptr->domain = NULL;
	ptr->eq = NULL;

	ptr->pep = NULL;
	ptr->conn_req_info = NULL;
	ptr->uarg = NULL;
	ptr->active_connections = 0;
	ptr->connections = ravl_new(ep_conn_pair_compare);

	ptr->waiting = 0;

	ptr->on_connection_event_func = NULL;
	ptr->on_timeout_func = NULL;
	ptr->timeout = DEFAULT_TIMEOUT;

	ptr->msg_size = cfg->msg_size;
	ptr->send_queue_length = cfg->send_queue_length;
	ptr->recv_queue_length = cfg->recv_queue_length;

	int ret = zone_init(cfg, ptr);

	if (ret)
		goto err_free;

	*zone = ptr;

	return ret;

err_free:
	Free(ptr);
	return ret;
}

static void
pep_dump(struct rpma_zone *zone)
{
	unsigned short port;

	if (zone->info->addr_format == FI_SOCKADDR_IN) {
		struct sockaddr_in addr_in;
		size_t addrlen = sizeof(addr_in);

		int ret = fi_getname(&zone->pep->fid, &addr_in, &addrlen);
		if (ret) {
			ERR_FI(ret, "fi_getname");
			return;
		}

		if (!addr_in.sin_port) {
			ERR("addr_in.sin_por == 0");
			return;
		}

		port = htons(addr_in.sin_port);

		fprintf(stderr, ": %u\n", port);
	} else {
		ASSERT(0);
	}
}

int
rpma_listen(struct rpma_zone *zone)
{
	int ret = fi_passive_ep(zone->fabric, zone->info, &zone->pep, NULL);
	if (ret) {
		ERR_FI(ret, "fi_passive_ep");
		return ret;
	}

	ret = fi_pep_bind(zone->pep, &zone->eq->fid, 0);
	if (ret) {
		ERR_FI(ret, "fi_pep_bind");
		goto err_pep_bind;
	}

	ret = fi_listen(zone->pep);
	if (ret) {
		ERR_FI(ret, "fi_listen");
		goto err_pep_listen;
	}

	pep_dump(zone);

	return 0;

err_pep_listen:
err_pep_bind:
	rpma_utils_res_close(&zone->pep->fid, "pep");
	zone->pep = NULL;
	return ret;
}

int
rpma_zone_delete(struct rpma_zone **zone)
{
	struct rpma_zone *ptr = *zone;

	zone_fini(ptr);

	Free(ptr);
	*zone = NULL;

	return 0;
}

int
rpma_zone_register_on_connection_event(struct rpma_zone *zone,
		rpma_on_connection_event_func func)
{
	zone->on_connection_event_func = func;
	return 0;
}

int
rpma_zone_register_on_timeout(struct rpma_zone *zone,
		rpma_on_timeout_func func, int timeout)
{
	if (timeout < 0)
		return RPMA_E_NEGATIVE_TIMEOUT;

	zone->on_timeout_func = func;
	zone->timeout = timeout;
	return 0;
}

int
rpma_zone_unregister_on_timeout(struct rpma_zone *zone)
{
	zone->on_timeout_func = NULL;
	zone->timeout = DEFAULT_TIMEOUT;
	return 0;
}

static int
eq_read(struct fid_eq *eq, struct fi_eq_cm_entry *entry,
		uint32_t *event, int timeout)
{
	ssize_t sret;
	struct fi_eq_err_entry err;

	sret = fi_eq_sread(eq, event, entry, sizeof(*entry), timeout, 0);
	VALGRIND_DO_MAKE_MEM_DEFINED(&sret, sizeof(sret));

	if (sret == -FI_ETIMEDOUT || sret == -FI_EAGAIN)
		return EQ_TIMEOUT;

	if (sret < 0 || (size_t)sret != sizeof(*entry)) {
		sret = fi_eq_readerr(eq, &err, 0);
		if (sret < 0) {
			ERR_FI(sret, "fi_eq_readerr");
		} else if (sret > 0) {
			ASSERT(sret == sizeof(err));
			ERR("fi_eq_sread: %s",
					fi_eq_strerror(eq, err.prov_errno, NULL, NULL, 0));
		}

		return EQ_ERR;
	}

	return 0;
}

static int
zone_on_timeout(struct rpma_zone *zone, void *uarg)
{
	rpma_on_timeout_func func;
	util_atomic_load_explicit64(&zone->on_timeout_func, &func, memory_order_acquire);

	if (!func)
		return 0;

	return func(zone, uarg);
}

int
rpma_zone_wait_connections(struct rpma_zone *zone, void *uarg)
{
	zone->uarg = uarg;

	int ret;
	struct fi_eq_cm_entry entry;
	uint32_t event;
	struct rpma_connection *conn;

	uint64_t *waiting = &zone->waiting;
	rpma_utils_wait_start(waiting);

	if (!zone->pep) {
		ret = zone->on_connection_event_func(zone, RPMA_CONNECTION_EVENT_OUTGOING, NULL, uarg);
		if (ret)
			return ret;
	}

	while (rpma_utils_is_waiting(waiting)) {
		ret = eq_read(zone->eq, &entry, &event, zone->timeout);
		if (ret == EQ_TIMEOUT) {
			if (zone_on_timeout(zone, uarg))
				break;
			continue;
		} else if (ret == EQ_ERR) {
			ret = RPMA_E_EQ_READ;
			break;
		}

		switch (event) {
		case FI_CONNREQ:
			zone->conn_req_info = entry.info;
			ret = zone->on_connection_event_func(zone, RPMA_CONNECTION_EVENT_INCOMING, NULL, uarg);
			rpma_utils_freeinfo(&zone->conn_req_info);
			if (ret)
				return ret;
			++zone->active_connections;
			break;
		case FI_SHUTDOWN:
			conn = conn_restore(zone->connections, entry.fid);
			ret = zone->on_connection_event_func(zone, RPMA_CONNECTION_EVENT_DISCONNECT, conn, uarg);
			if (ret)
				return ret;
			--zone->active_connections;
			break;
		default:
			ERR("unexpected event received (%u)", event);
			ret = RPMA_E_EQ_EVENT;
			break;
		}
	}

	return 0;
}

int
rpma_zone_wait_connected(struct rpma_zone *zone, struct rpma_connection *conn)
{
	int ret = 0;
	struct fi_eq_cm_entry entry;
	uint32_t event;

	while (rpma_utils_is_waiting(&zone->waiting)) {
		ret = eq_read(zone->eq, &entry, &event, zone->timeout);
		if (ret == EQ_TIMEOUT) {
			if (zone_on_timeout(zone, zone->uarg))
				break;
			continue;
		} else if (ret == EQ_ERR) {
			ret = RPMA_E_EQ_READ;
			break;
		}

		if (event == FI_CONNECTED) {
			if (entry.fid != &conn->ep->fid) {
				ERR("unexpected fid received (%p)", entry.fid);
				ret = RPMA_E_EQ_EVENT_DATA;
			}
			conn_store(zone->connections, conn);
			break;
		} else {
			ERR("unexpected event received (%u)", event);
			ret = RPMA_E_EQ_EVENT;
			break;
		}
	}

	return ret;
}

int
rpma_zone_wait_break(struct rpma_zone *zone)
{
	rpma_utils_wait_break(&zone->waiting);
	return 0;
}
