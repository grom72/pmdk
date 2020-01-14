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
 * dispatcher.c -- entry points for librpma dispatcher
 */

#include <librpma/base.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_eq.h>

#include "alloc.h"
#include "connection.h"
#include "dispatcher.h"
#include "os_thread.h"
#include "rpma_utils.h"
#include "queue.h"
#include "zone.h"

static int
dispatcher_init(struct rpma_dispatcher *disp)
{
	int ret;

	struct fi_poll_attr attr;
	attr.flags = 0;

	ret = fi_poll_open(disp->zone->domain, &attr, &disp->pollset);
	if (ret) {
		ERR_FI(ret, "fi_poll_open");
		return ret;
	}

	PMDK_TAILQ_INIT(&disp->queue_cqe);
	PMDK_TAILQ_INIT(&disp->queue_func);

	os_mutex_init(&disp->queue_func_mtx);

	return 0;
}

static void
dispatcher_fini(struct rpma_dispatcher *disp)
{
	os_mutex_destroy(&disp->queue_func_mtx);

	/* XXX is it ok? */
	while (!PMDK_TAILQ_EMPTY(&disp->queue_cqe)) {
		struct rpma_dispatcher_cq_entry *e = PMDK_TAILQ_FIRST(&disp->queue_cqe);
		PMDK_TAILQ_REMOVE(&disp->queue_cqe, e, next);
		Free(e);
	}

	while (!PMDK_TAILQ_EMPTY(&disp->queue_func)) {
		struct rpma_dispatcher_func_entry *e = PMDK_TAILQ_FIRST(&disp->queue_func);
		PMDK_TAILQ_REMOVE(&disp->queue_func, e, next);
		Free(e);
	}

	rpma_utils_res_close(&disp->pollset->fid, "pollset");
}

int
rpma_dispatcher_new(struct rpma_zone *zone, struct rpma_dispatcher **disp)
{
	struct rpma_dispatcher *ptr = Malloc(sizeof(*ptr));
	if (!ptr)
		return RPMA_E_ERRNO;

	ptr->zone = zone;
	ptr->wait_breaking = 0;

	int ret = dispatcher_init(ptr);
	if (ret)
		goto err_init;

	*disp = ptr;

	return 0;

err_init:
	Free(ptr);
	return ret;
}

int
rpma_dispatcher_delete(struct rpma_dispatcher **ptr)
{
	struct rpma_dispatcher *disp = *ptr;
	dispatcher_fini(disp);

	Free(disp);
	*ptr = NULL;

	return 0;
}

int
rpma_dispatcher_attach_connection(struct rpma_dispatcher *disp,
		struct rpma_connection *conn)
{
	int ret;
	uint64_t flags = 0;
	ret = fi_poll_add(disp->pollset, &conn->cq->fid, flags);
	if (ret) {
		ERR_FI(ret, "fi_poll_add");
		return ret;
	}

	return 0;
}

int
rpma_dispatcher_detach_connection(struct rpma_dispatcher *disp,
		struct rpma_connection *conn)
{
	int ret;
	uint64_t flags = 0;
	ret = fi_poll_del(disp->pollset, &conn->cq->fid, flags);
	if (ret) {
		ERR_FI(ret, "fi_poll_del");
		return ret;
	}

	return 0;
}

int
rpma_dispatch(struct rpma_dispatcher *disp)
{
	int ret;
	void *context;
	int count = 1;
	struct rpma_connection *conn;

	struct rpma_dispatcher_cq_entry *cqe;
	struct rpma_dispatcher_func_entry *funce;

	uint64_t *wait_breaking = &disp->wait_breaking;
	rpma_utils_wait_start(wait_breaking);

	while (rpma_utils_is_waiting(wait_breaking)) {
		ret = fi_poll(disp->pollset, &context, count);

		if (ret == 0) {
			/* process cached CQ entries */
			while (!PMDK_TAILQ_EMPTY(&disp->queue_cqe)) {
				cqe = PMDK_TAILQ_FIRST(&disp->queue_cqe);
				PMDK_TAILQ_REMOVE(&disp->queue_cqe, cqe, next);

				ret = rpma_connection_cq_entry_process(
						cqe->conn, &cqe->cq_entry, NULL);
				ASSERTeq(ret, 0); /* XXX */
				Free(cqe);
			}

			while (!PMDK_TAILQ_EMPTY(&disp->queue_func)) {
				funce = PMDK_TAILQ_FIRST(&disp->queue_func);
				PMDK_TAILQ_REMOVE(&disp->queue_func, funce, next);

				ret = funce->func(funce->conn, funce->arg);
				ASSERTeq(ret, 0); /* XXX */
				Free(funce);
			}

			continue;
		}

		if (ret < 0) {
			ERR_FI(ret, "fi_poll");
			return ret;
		}

		ASSERTeq(ret, count);

		conn = context;

		ret = rpma_connection_cq_process(conn, NULL);
		if (ret)
			return ret;
	}

	return 0;
}

int
rpma_dispatcher_enqueue_cq_entry(struct rpma_dispatcher *disp,
		struct rpma_connection *conn, struct fi_cq_msg_entry *cq_entry)
{
	struct rpma_dispatcher_cq_entry *entry = Malloc(sizeof(*entry));
	if (!entry)
		return RPMA_E_ERRNO;

	entry->conn = conn;
	memcpy(&entry->cq_entry, cq_entry, sizeof(*cq_entry));

	PMDK_TAILQ_INSERT_TAIL(&disp->queue_cqe, entry, next);

	return 0;
}

int
rpma_dispatcher_enqueue_func(struct rpma_dispatcher *disp,
		struct rpma_connection *conn, rpma_queue_func func, void *arg)
{
	struct rpma_dispatcher_func_entry *entry = Malloc(sizeof(*entry));
	entry->conn = conn;
	entry->func = func;
	entry->arg = arg;

	os_mutex_lock(&disp->queue_func_mtx);
	PMDK_TAILQ_INSERT_TAIL(&disp->queue_func, entry, next);
	os_mutex_unlock(&disp->queue_func_mtx);

	return 0;
}
