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
 * comm_server.c -- librpma-based communicator server
 */

#include <pthread.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <libpmem.h>
#include <librpma/base.h>
#include <librpma/mr.h>
#include <librpma/msg.h>

#include "comm_common.h"

#define CLIENTS_CAPACITY (10)
#define MSG_LOG_MIN_CAPACITY (1000)

#define POOL_MIN_SIZE \
	(CLIENT_VECTOR_SIZE(CLIENTS_CAPACITY) + MSG_LOG_SIZE(MSG_LOG_MIN_CAPACITY))

#define MSG_LOG_CAPACITY(size) \
	(((size) - CLIENT_VECTOR_SIZE(CLIENTS_CAPACITY)) / MSG_LOG_SIZE(1))

/* persistent */
struct root_obj {
	struct client_row cv[CLIENTS_CAPACITY]; /* the clients vector */
	struct msg_row ml[]; /* the message log */
};

#define MSG_TYPE_HELLO		1
#define MSG_TYPE_HELLO_ACK	2

struct msg_t {
	uint64_t type;
	union {
		struct {
			size_t id_size;
			uint8_t cv_id[];
		} hello;
		struct bye_bye_t {
			uint64_t status;
		} bye_bye;
	};
};

#define MSG_SIZE (sizeof(struct msg_t))

struct client_ctx {
	uint8_t *cv_ids;
	struct rpma_conn *conn;
	pthread_t thread;

	struct rpma_msg *send_msg;
};

struct server_ctx {
	struct root_obj *root;
	size_t root_size;
	size_t ml_capacity; /* the message log capacity */

	size_t id_size; /* size of a single lmr id */
	uint64_t nclients;
	struct client_ctx clients[CLIENTS_CAPACITY];

	pthread_t ml_monitor_thread;
};


static int
ml_monitor(void *argc) // the message log monitor
{
	while (!exiting) {
		// sem_wait(ml_sem, 0);
		if (ML.W != ML.R) {
			// sem_set_value(distrubtion_sem, C)
			for (int i = 0; i < C; ++i) {
				// SEND (ML_UPDATE (ML.W, ML.R))
			}
			// sem_wait(distribution_sem, 0);
			ML.R = ML.W;
		}
		// sem_set_value(ml_sem, 1);
	}
}

static int
cq_notify(/* XXX */) // the CQ notification callback
{
	// check CS for changes
	ASSERTeq(CS[x], MSG_READY);
	// append the MSG BUFF to the Message Log (ML)
	// sem_dec(ml_sem, 0);
	CS[x] = MSG_DONE;
	return NTFY_ACK; // SEND is done by the library
}

static int
cq_recv_callback()
{
	// if RECV(bye bye message)
		// print the bye bye message
		// set exiting
		// cq break
	// if RECV(ML_UPDATE_ACK)
		// sem_dec(distribution_sem, 1);
}

static void *
conn_thread(void *arg)
{
	struct client_ctx *client = arg;
	void *ptr;
	struct msg_t *send;

	rpma_msg_new(client->conn, MSG_SIZE, RPMA_MSG_SEND, &client->send_msg);
	rpma_msg_get_ptr(client->send_msg, &ptr);
	send = ptr;

	// send->id_size = client->
	// send the hello message
	// - client row for the client (CV[x])
	// recv the hello message ACK

	// register recv callback
	// register notify callback
	// CQ loop
	// - handles emulated Atomic Writes
	// - handles notify messages

	return NULL;
}

#define RPMA_CONN_TIMEOUT (60) /* 1m */

static void
on_timeout(struct rpma_ctx *ctx)
{
	rpma_conn_loop_break(ctx);
	return;
}

static uint64_t
get_empty_client_id(struct client_ctx *clients[])
{
	for (int i = 0; i < CLIENTS_CAPACITY; ++i) {
		if (clients[i]->conn == NULL)
			return i;
	}

	return UINT64_MAX;
}

static int
on_conn_event(struct rpma_ctx *rctx, uint64_t event, void *arg)
{
	struct server_ctx *ctx = arg;
	struct client_ctx *client;
	uint64_t client_id;
	int ret;

	switch (event) {
	case RPMA_CONN_EVENT_CONNECT:
		/* not enough capacity */
		if (ctx->nclients == CLIENTS_CAPACITY) {
			rpma_conn_reject(rctx);
			return 0;
		}
		/* accept the incoming connection */
		client_id = get_empty_conn_id(ctx->clients);
		client = &ctx->clients[client_id];
		rpma_conn_new(rctx, &client->conn);
		++ctx->nclients;
		rpma_conn_accept(client->conn);
		rpma_unregister_on_timeout(rctx);
		/* spawn the connection thread */
		pthread_create(&client->thread, NULL, conn_thread,
				&client);
		break;
	case RPMA_CONN_EVENT_DISCONNECT:
		// detect somehow the conn_id
		client = &ctx->clients[client_id];
		rpma_conn_rt_loop_break(client->conn);
		pthread_join(client->thread, &ret);
		--ctx->nclients;
		if (ctx->nclients == 0)
			rpma_register_on_timeout(rctx, on_timeout, RPMA_CONN_TIMEOUT);
		break;
	default:
		return -1; /* RPMA_E_UNHANDLED_EVENT */
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	const char *path = argv[1];
	const char *addr = argv[2];
	const char *service = argv[3];

	struct server_ctx ctx;

	/*
	 * map the server root object storing:
	 * - a clients vector - CV
	 * - a message log - ML
	 */
	ctx.root = pmem_map_file(path, POOL_MIN_SIZE, PMEM_FILE_CREATE, O_RDWR,
			&ctx.root_size, NULL);
	ctx.ml_capacity = MSG_LOG_CAPACITY(ctx.root_size);

	/* prepare RPMA connection */
	struct rpma_config *cfg;
	rpma_config_new(&cfg);
	rpma_config_set_addr(cfg, addr);
	rpma_config_set_service(cfg, service);

	struct rpma_ctx *rctx;
	rpma_ctx_new(cfg, &rctx);

	/* register CV rows for each client */
	struct rpma_lmr cv_lmrs[CLIENTS_CAPACITY];
	rpma_ctx_lmr_get_id_size(rctx, &ctx.id_size);
	for (int i = 0; i < CLIENTS_CAPACITY; ++i) {
		/* register lmr */
		rpma_lmr_new(rctx, &ctx.root->cv[i], CLIENT_VECTOR_SIZE(1),
				RPMA_MR_WRITE_DST, &cv_lmrs[i]);
		/* obtain lmr id */
		size_t id_size = ctx.id_size;
		ctx.cv_ids[i] = malloc(ctx.id_size);
		rpma_lmr_get_id(cv_lmrs[i], ctx.cv_ids[i], &id_size);
	}

	/* spawn ML monitor thread */
	pthread_create(&ctx.ml_monitor_thread, NULL, ml_monitor, &ctx);

	/* setup connection loop */
	ctx.nconns = 0;
	for (int i = 0; i < CLIENTS_CAPACITY; ++i)
		ctx.conns[i] = NULL;
	rpma_listen(rctx);
	rpma_register_on_conn_event(rctx, on_conn_event);
	rpma_register_on_timeout(rctx, on_timeout, RPMA_CONN_TIMEOUT);
	rpma_conn_loop(rctx);

	/* join ML monitor thread */
	int ret;
	pthread_join(ctx.ml_monitor_thread, &ret);

	return 0;
}
