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
 * connection.c -- entry points for librpma connection
 */

#include <rdma/fi_cm.h>

#include <librpma.h>

#include "alloc.h"
#include "connection.h"
#include "dispatcher.h"
#include "memory.h"
#include "rpma_utils.h"
#include "zone.h"

#define CQ_SIZE 10 /* XXX */

int
rpma_connection_new(struct rpma_zone *zone, struct rpma_connection **conn)
{
	struct rpma_connection *ptr = Malloc(sizeof(struct rpma_connection));
	if (!ptr)
		return RPMA_E_ERRNO;

	ptr->zone = zone;

	ptr->disp = NULL;

	ptr->on_connection_recv_func = NULL;
	ptr->on_transmission_notify_func = NULL;

	ptr->custom_data = NULL;

	int ret = rpma_connection_rma_init(ptr);
	if (ret)
		goto err_rma_init;

	ret = rpma_connection_msg_init(ptr);
	if (ret)
		goto err_msg_init;

	*conn = ptr;

	return 0;

err_msg_init:
	(void)rpma_connection_rma_fini(ptr);
err_rma_init:
	Free(ptr);
	return ret;
}

static int
ep_init(struct rpma_connection *conn, struct fi_info *info)
{
	struct rpma_zone *zone = conn->zone;

	int ret = fi_endpoint(zone->domain, info, &conn->ep, NULL);
	if (ret) {
		ERR_FI(ret, "fi_endpoint");
		return ret;
	}

	/* bind event queue to the endpoint */
	ret = fi_ep_bind(conn->ep, &zone->eq->fid, 0);
	if (ret) {
		ERR_FI(ret, "fi_ep_bind(eq)");
		goto err_bind_eq;
	}

	struct fi_cq_attr cq_attr = {
		.size = CQ_SIZE,
		.flags = 0,
		.format = FI_CQ_FORMAT_MSG, /* need context and flags */
		.wait_obj = FI_WAIT_UNSPEC,
		.signaling_vector = 0,
		.wait_cond = FI_CQ_COND_NONE,
		.wait_set = NULL,
	};

	void *context = conn;

	ret = fi_cq_open(zone->domain, &cq_attr, &conn->cq, context);
	if (ret) {
		ERR_FI(ret, "fi_cq_open");
		goto err_cq_open;
	}

	/*
	 * Bind completion queue to the endpoint.
	 * Use a single completion queue for outbound and inbound work
	 * requests. Use selective completion implies adding FI_COMPLETE
	 * flag to each WR which needs a completion.
	 */
	ret = fi_ep_bind(conn->ep, &conn->cq->fid,
			FI_RECV | FI_TRANSMIT | FI_SELECTIVE_COMPLETION);
	if (ret) {
		ERR_FI(ret, "fi_ep_bind(cq)");
		goto err_bind_cq;
	}

	/* enable the endpoint */
	ret = fi_enable(conn->ep);
	if (ret) {
		ERR_FI(ret, "fi_enable");
		goto err_enable;
	}

	return 0;

err_enable:
err_bind_cq:
err_cq_open:
err_bind_eq:
	rpma_utils_res_close(&conn->ep->fid, "ep");
	return ret;
}

static int
ep_fini(struct rpma_connection *conn)
{
	if (conn->ep)
		rpma_utils_res_close(&conn->ep->fid, "ep");

	if (conn->cq)
		rpma_utils_res_close(&conn->cq->fid, "cq");

	return 0;
}

static int
recv_post_all(struct rpma_connection *conn)
{
	int ret;
	void *ptr = conn->recv_buff->ptr;

	for (uint64_t i = 0; i < conn->zone->recv_queue_length; ++i) {
		ret = rpma_connection_recv_post(conn, ptr);
		if (ret)
			return ret;
		ptr = (void *)((uintptr_t)ptr + conn->zone->msg_size);
	}

	return 0;
}

int
rpma_connection_accept(struct rpma_connection *conn)
{
	int ret = ep_init(conn, conn->zone->conn_req_info);
	if (ret)
		return ret;

	ret = recv_post_all(conn);
	if (ret)
		goto err_recv_post_all;

	/* XXX use param buffer? */
	ret = fi_accept(conn->ep, NULL, 0);
	if (ret) {
		ERR_FI(ret, "fi_enable");
		goto err_accept;
	}

	ret = rpma_zone_wait_connected(conn->zone, conn);
	if (ret)
		goto err_connected;

	return 0;

err_connected:
err_accept:
err_recv_post_all:
	ep_fini(conn);
	return ret;
}

int
rpma_connection_reject(struct rpma_zone *zone)
{
	/* XXX use param buffer? */
	fi_reject(zone->pep, zone->conn_req_info->handle, NULL, 0);

	return 0;
}

int
rpma_connection_establish(struct rpma_connection *conn)
{
	int ret = ep_init(conn, conn->zone->info);
	if (ret)
		return ret;

	ret = recv_post_all(conn);
	if (ret)
		return ret;

	/* XXX use param buffer? */
	ret = fi_connect(conn->ep, conn->zone->info->dest_addr, NULL, 0);
	if (ret) {
		ERR_FI(ret, "fi_connect");
		goto err_connect;
	}

	ret = rpma_zone_wait_connected(conn->zone, conn);
	if (ret)
		goto err_connected;

	return 0;

err_connected:
err_connect:
	ep_fini(conn);
	return ret;
}

int
rpma_connection_disconnect(struct rpma_connection *conn)
{
	/* XXX any prior messaging? */
	fi_shutdown(conn->ep, 0);

	return 0;
}

int
rpma_connection_delete(struct rpma_connection **conn)
{
	struct rpma_connection *ptr = *conn;

	ASSERTne(ptr->disp, NULL);

	ep_fini(ptr);

	int ret = rpma_connection_rma_fini(ptr);
	if (ret)
		goto err_rma_fini;

	Free(ptr);
	*conn = NULL;

	return 0;

err_rma_fini:
	return ret;
}

int
rpma_connection_set_custom_data(struct rpma_connection *conn, void *data)
{
	conn->custom_data = data;

	return 0;
}

int
rpma_connection_get_custom_data(struct rpma_connection *conn, void **data)
{
	*data = conn->custom_data;

	return 0;
}

int
rpma_connection_get_zone(struct rpma_connection *conn, struct rpma_zone **zone)
{
	*zone = conn->zone;

	return 0;
}

int
rpma_connection_attach(struct rpma_connection *conn,
		struct rpma_dispatcher *disp)
{
	int ret = rpma_dispatcher_attach_connection(disp, conn);
	if (ret)
		return ret;

	conn->disp = disp;

	return 0;
}

int
rpma_connection_detach(struct rpma_connection *conn)
{
	int ret = rpma_dispatcher_detach_connection(conn->disp, conn);
	if (ret)
		return ret;

	conn->disp = NULL;

	return 0;
}

int
rpma_connection_enqueue(struct rpma_connection *conn, rpma_queue_func func,
		void *arg)
{
	ASSERTne(conn->disp, NULL); /* XXX ? */

	return rpma_dispatcher_enqueue_func(conn->disp, conn, func, arg);
}

int
rpma_connection_register_on_notify(struct rpma_connection *conn,
	rpma_on_transmission_notify_func func)
{
	conn->on_transmission_notify_func = func;

	return 0;
}

int
rpma_connection_register_on_recv(struct rpma_connection *conn,
		rpma_on_connection_recv_func func)
{
	conn->on_connection_recv_func = func;

	return 0;
}

int
rpma_connection_cq_entry_process(struct rpma_connection *conn,
		struct fi_cq_msg_entry *cq_entry, void *uarg)
{
	int ret = 0;
	if (cq_entry->flags & FI_MSG) {
		ASSERTeq(cq_entry->flags, FI_MSG | FI_RECV); /* XXX */

		/* XXX uarg is still necesarry here? */
		ret = conn->on_connection_recv_func(conn, cq_entry->op_context,
				conn->zone->msg_size, uarg);
	} else {
		ASSERT(0);
	}
	/*} else if (cq_entry->flags & FI_RMA) { */
		/* XXX conn->on_transmission_notify_func */
	/* } */

	return ret;
}

static int
cq_entry_process_or_enqueue(struct rpma_connection *conn,
		struct fi_cq_msg_entry *cq_entry)
{
	if (conn->disp)
		return rpma_dispatcher_enqueue_cq_entry(conn->disp, conn, cq_entry);

	return rpma_connection_cq_entry_process(conn, cq_entry, NULL);
}

#define CQ_DEFAULT_TIMEOUT 1000

static inline int
cq_read(struct rpma_connection *conn, struct fi_cq_msg_entry *cq_entry)
{
	int ret = (int)fi_cq_sread(conn->cq, &cq_entry, 1, NULL, CQ_DEFAULT_TIMEOUT);
	if (ret == -FI_EAGAIN)
		return 0;
	if (ret < 0) {
		ERR_FI(ret, "fi_cq_sread");
		return ret;
	}

	ASSERTeq(ret, 1); /* XXX ? */
	return ret;
}

int
rpma_connection_cq_wait(struct rpma_connection *conn, uint64_t flags,
		void *op_context)
{
	struct fi_cq_msg_entry cq_entry;
	int ret;
	int mismatch;

	/* XXX additional stop condition? */
	while (1) {
		ret = cq_read(conn, &cq_entry);
		if (ret == 0)
			continue;
		else if (ret < 0)
			return ret;

		mismatch = !(cq_entry.flags & flags);
		mismatch |= (cq_entry.op_context != op_context);

		if (mismatch) {
			ret = cq_entry_process_or_enqueue(conn, &cq_entry);
			if (ret) {
				/* XXX */
				ASSERT(0);
			}

			continue;
		}

		break;
	}

	return 0;
}

int
rpma_connection_cq_process(struct rpma_connection *conn, void *uarg)
{
	struct fi_cq_msg_entry cq_entry;
	int ret;

	/* XXX stop condition? */
	while (1) {
		ret = cq_read(conn, &cq_entry);
		if (ret == 0)
			continue;
		else if (ret < 0)
			return ret;

		ret = rpma_connection_cq_entry_process(conn, &cq_entry, uarg);
		if (ret)
			return ret;
	}

	return 0;
}
