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
 * workers.c -- workers for librpma-based communicator server
 */

#include "workers.h"

/* worker context */
struct worker_ctx {
	uint64_t running;

	struct rpma_dispatcher *disp;
	pthread_t thread;
};

/*
 * worker_thread_func -- (internal) worker thread main function
 */
static void *
worker_thread_func(void *arg)
{
	struct worker_ctx *w = arg;

	while (w->running) { // XXX atomic
		// XXX timeout
		rpma_dispatch(w->disp);
	}

	return NULL;
}

/*
 * workers_init -- initialize worker threads
 */
void
workers_init(struct rpma_zone *zone, struct worker_ctx *w_ptr,
		uint64_t nworkers)
{
	struct worker_ctx *ws = malloc(nworkers * sizeof(*ws));

	for (int i = 0; i < nworkers; ++i) {
		struct worker_ctx *w = &ws[i];

		rpma_dispatcher_new(zone, &w->disp);
		pthread_create(&w->thread, NULL, worker_thread_func, w);
	}
}

/*
 * workers_fini -- finalize worker threads
 */
void
workers_fini(struct worker_ctx *ws, uint64_t nworkers)
{
	for (int i = 0; i < nworkers; ++i) {
		struct worker_ctx *w = &ws[i];
		w->running = 0; // XXX atomic
		pthread_join(w->thread, NULL);
	}

	free(ws);
}

/*
 * worker_next -- selects the next worker for the client connection
 * in a round robin fashion
 */
struct worker *
worker_next(struct worker_ctx *ws, uint64_t mask)
{
	static uint64_t worker_id = 0;

	return &ws[__sync_fetch_and_add(&worker_id, 1) & mask];
}
