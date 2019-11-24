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
 * distributor.c -- msg distributor for librpma-based communicator server
 */

#include <pthread.h>
#include <semaphore.h>

#include "msgs.h"
#include "server.h"

struct distributor_t {
	sem_t notify;
	sem_t acks;
	pthread_t thread;
};

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
distributor_wait_acks(struct distributor_t *dist, int nacks, struct server_ctx *ctx)
{
	int ret;

	/* wait for the acks from the clients */
	while (nacks > 0 && !ctx->exiting) {
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
	struct distributor_t *dist = &svr->distributor;
	int ret;

	while (!svr->exiting) {
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
		rpma_connection_group_enqueue(svr->grp, proto_mlog_update, &args);

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
distributor_fini(struct distributor_t *dist)
{
	pthread_join(dist->thread, NULL);
	sem_destroy(&dist->acks);
	sem_destroy(&dist->notify);

	free(dist);
}

