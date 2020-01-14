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
 * dispatcher.h -- internal definitions for librpma dispatcher
 */
#ifndef RPMA_DISPATCHER_H
#define RPMA_DISPATCHER_H

#include "queue.h"
#include "os_thread.h"

struct rpma_dispatcher_cq_entry
{
	PMDK_TAILQ_ENTRY(rpma_dispatcher_cq_entry) next;

	struct rpma_connection *conn;
	struct fi_cq_msg_entry cq_entry;
};

struct rpma_dispatcher_func_entry
{
	PMDK_TAILQ_ENTRY(rpma_dispatcher_func_entry) next;

	struct rpma_connection *conn;
	rpma_queue_func func;
	void *arg;
};

struct rpma_dispatcher
{
	struct rpma_zone *zone;

	/*
	 * XXX if the user can guarantee no sequences for a dispatcher
	 * probably waitset will be better here
	 */
	struct fid_poll *pollset;

	uint64_t wait_breaking;

	PMDK_TAILQ_HEAD(head_cq, rpma_dispatcher_cq_entry) queue_cqe;

	os_mutex_t queue_func_mtx;
	PMDK_TAILQ_HEAD(head_fq, rpma_dispatcher_func_entry) queue_func;
};

int rpma_dispatcher_attach_connection(struct rpma_dispatcher *disp,
		struct rpma_connection *conn);

int rpma_dispatcher_detach_connection(struct rpma_dispatcher *disp,
		struct rpma_connection *conn);

int rpma_dispatcher_enqueue_cq_entry(struct rpma_dispatcher *disp,
		struct rpma_connection *conn, struct fi_cq_msg_entry *cq_entry);
int rpma_dispatcher_enqueue_func(struct rpma_dispatcher *disp,
		struct rpma_connection *conn, rpma_queue_func func, void *arg);

#endif /* dispatcher.h */
