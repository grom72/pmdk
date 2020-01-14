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
 * distributor.h -- librpma-based communicator server
 */

#ifndef COMM_WORKERS_H
#define COMM_WORKERS_H 1

#include <pthread.h>

#include "protocol.h"
#include "server.h"

/* worker context */
struct worker_ctx {
	uint64_t running;

	struct rpma_dispatcher *disp;
	pthread_t thread;
};

void workers_init(struct rpma_zone *zone, struct worker_ctx **ws_ptr,
		uint64_t nworkers);

void workers_fini(struct worker_ctx *ws, uint64_t nworkers);

struct worker_ctx *worker_next(struct worker_ctx *ws, uint64_t mask);

struct distributor_t;

void distributor_notify(struct distributor_t *dist);

void distributor_ack(struct distributor_t *dist);

void distributor_init(struct server_ctx *svr);

void distributor_fini(struct server_ctx *svr);

struct writer_t;

void writer_init(struct writer_t **wrt_ptr, struct rpma_connection **conn_ptr,
		struct client_local_t *cloc, struct hello_result_t *hres);

void writer_fini(struct writer_t *wrt);

#endif /* workers.h */
