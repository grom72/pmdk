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

#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <librpma/rma.h>

#include "workers.h"
#include "pstructs.h"
#include "protocol.h"

struct distributor_t {
	sem_t notify;
	sem_t acks;
	pthread_t thread;
};

struct writer_t {
	uint64_t running;

	pthread_t thread;
	sem_t done;

	struct write_msg_args_t write_msg_args;
	struct rpma_sequence *write_msg_seq;

	struct rpma_connection **conn_ptr;
	struct client_row *cr;
	struct rpma_memory_local **src_ptr;
	struct rpma_memory_remote **dst_ptr;
} writer;

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
workers_init(struct rpma_zone *zone, struct worker_ctx **ws_ptr,
		uint64_t nworkers)
{
	struct worker_ctx *ws = malloc(nworkers * sizeof(*ws));

	for (int i = 0; i < nworkers; ++i) {
		struct worker_ctx *w = &ws[i];

		rpma_dispatcher_new(zone, &w->disp);
		pthread_create(&w->thread, NULL, worker_thread_func, w);
	}

	*ws_ptr = ws;
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
struct worker_ctx *
worker_next(struct worker_ctx *ws, uint64_t mask)
{
	static uint64_t worker_id = 0;

	return &ws[__sync_fetch_and_add(&worker_id, 1) & mask];
}

/*
 * distributor_notify --  notify the distributor that new messages are ready
 */
void
distributor_notify(struct distributor_t *dist)
{
	sem_post(&dist->notify);
}

/*
 * distributor_trywait -- (internal) wait for new messages
 */
static int
distributor_trywait(struct distributor_t *dist)
{
	return sem_trywait(&dist->notify);
}

/*
 * distributor_wait_acks -- (internal) wait for specified # of ACKs
 */
static int
distributor_wait_acks(struct distributor_t *dist, int nacks,
		struct server_ctx *svr)
{
	int ret;

	/* wait for the acks from the clients */
	while (nacks > 0 && !svr->running) {
		ret = sem_trywait(&dist->acks);
		if (ret)
			continue;
		--nacks;
	}

	return nacks == 0;
}

/*
 * distributor_ack -- send ACK to the distributor
 */
void
distributor_ack(struct distributor_t *dist)
{
	sem_post(&dist->acks);
}

#define DISTRIBUTOR_SLEEP (1)

/*
 * distributor_thread_func -- (internal) the message log distributor
 */
static void *
distributor_thread_func(void *arg)
{
	struct server_ctx *svr = (struct server_ctx *)arg;
	struct distributor_t *dist = svr->distributor;
	int ret;

	while (!svr->running) {
		/* wait for new messages */
		ret = distributor_trywait(dist);
		if (ret) {
			sleep(DISTRIBUTOR_SLEEP);
			continue;
		}
		/* no new messages */
		if (!ml_ready(&svr->root->ml))
			continue;

		/* get current write pointer and # of acks to collect */
		struct mlog_update_args_t args;
		args.wptr = ml_get_wptr(&svr->root->ml);

		/* send updates to the clients */
		rpma_connection_group_enqueue(svr->grp, proto_mlog_update,
				&args);

		distributor_wait_acks(dist, svr->nclients, svr);

		/* set read pointer */
		ml_set_rptr(&svr->root->ml, args.wptr);
	}

	return NULL;
}

/*
 * distributor_init -- spawn the ML distributor thread
 */
void
distributor_init(struct server_ctx *svr)
{
	struct distributor_t *dist = malloc(sizeof(*dist));
	svr->distributor = dist;

	sem_init(&dist->notify, 0, 0);
	sem_init(&dist->acks, 0, 0);
	pthread_create(&dist->thread, NULL, distributor_thread_func, svr);
}

/*
 * distributor_fini -- clean up the ML distributor
 */
void
distributor_fini(struct server_ctx *svr)
{
	struct distributor_t *dist = svr->distributor;

	pthread_join(dist->thread, NULL);
	sem_destroy(&dist->acks);
	sem_destroy(&dist->notify);

	free(dist);
}

/*
 * writer_publish_msg -- write the message to the server
 */
static void
writer_publish_msg(struct writer_t *wrt)
{
	struct write_msg_args_t *args = &wrt->write_msg_args;
	args->cr = wrt->cr;
	args->dst = *wrt->dst_ptr;
	args->src = *wrt->src_ptr;
	args->done = &wrt->done;

	rpma_connection_enqueue_sequence(*wrt->conn_ptr, wrt->write_msg_seq);

	sem_wait(args->done);
}

/*
 * writer_thread_func -- client writer entry point
 */
static void *
writer_thread_func(void *arg)
{
	struct writer_t *wrt = arg;
	ssize_t ret;

	while (wrt->running) { // XXX atomic
		printf("< ");
		ret = read(STDIN_FILENO, wrt->cr->msg.msg, MSG_SIZE_MAX);

		if (ret == 0)
			continue;
		if (ret < 0) {
			rpma_connection_enqueue(*wrt->conn_ptr, proto_bye_bye,
					NULL);
			wrt->running = 0; /* XXX atomic */
			break;
		}

		/* new message */
		p_client_pending(wrt->cr);
		writer_publish_msg(wrt);
	}

	return NULL;
}

/*
 * writer_init -- initialize writer thread
 */
void
writer_init(struct writer_t **wrt_ptr, struct rpma_connection **conn_ptr,
		struct client_local_t *cloc, struct hello_result_t *hres)
{
	struct writer_t *wrt = malloc(sizeof(*wrt));

	sem_init(&wrt->done, 0, 0);
	wrt->conn_ptr = conn_ptr;
	wrt->cr = cloc->cr_ptr;
	wrt->src_ptr = &cloc->cr;
	wrt->dst_ptr = &hres->cr;

	proto_writer_init(&wrt->write_msg_seq, &wrt->write_msg_args);

	pthread_create(&wrt->thread, NULL, writer_thread_func, wrt);

	*wrt_ptr = wrt;
}

/*
 * writer_fini -- cleanup writer thread
 */
void
writer_fini(struct writer_t *wrt)
{
	pthread_join(wrt->thread, NULL);
	sem_destroy(&wrt->done);

	proto_writer_fini(&wrt->write_msg_seq);

	free(wrt);
}
