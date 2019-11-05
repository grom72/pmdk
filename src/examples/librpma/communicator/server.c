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

#include "pstructs.h"
#include "msgs.h"

/* server-side assumptions */
#define CLIENTS_CAPACITY (10)
#define MSG_LOG_MIN_CAPACITY (1000)

/* derive minimal pool size from the assumptions */
#define CLIENT_VECTOR_SIZE(capacity) \
	(sizeof(struct client_row) * (capacity))
#define MSG_LOG_SIZE(capacity) (sizeof(struct msg_row) * (capacity))
#define POOL_MIN_SIZE \
	(CLIENT_VECTOR_SIZE(CLIENTS_CAPACITY) + MSG_LOG_SIZE(MSG_LOG_MIN_CAPACITY))

/* server-side persistent root object */
struct root_obj {
	struct client_row cv[CLIENTS_CAPACITY]; /* the clients vector */
	struct msg_log ml;
};

/* server-side client context */
struct client_ctx {
	uint64_t client_id;
	const struct server_ctx *server;

	/* persistent client-row and its id */
	struct client_row *cr;
	struct rpma_lmr_id cr_id;

	/* RPMA send and recv messages */
	struct rpma_msg *send_msg;
	struct rpma_msg *recv_msg;

	/* client's connection and its thread */
	struct rpma_conn *conn;
	pthread_t thread;
};

/* server context */
struct server_ctx {
	struct rpma_ctx *rctx;

	/* persistent data and its derivatives */
	struct root_obj *root;
	size_t root_size;
	size_t ml_capacity; /* the message log capacity */

	/* client's contextes */
	uint64_t nclients; /* current # of clients */
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

/*
 * on_notify -- on notify callback
 */
static int
on_notify(void *addr, size_t length, void *arg)
{
	struct client_ctx *client = arg;
	struct client_row *cr = addr;
	struct msg_log *ml = &client->server->root->ml;
	ASSERTeq(cr->status, CLIENT_MSG_READY);
	mlog_append(ml, client->client_id, cr->msg_size, cr->msg);
	// sem_dec(ml_sem, 0);
	cr->status = CLIENT_MSG_DONE;
	pmem_persist(&cr->status, sizeof(cr->status));
	return NTFY_ACK; // SEND is done by the library
}

/*
 * on_recv -- on receive callback
 */
static int
on_recv(struct rpma_msg *msg, size_t length, void *arg)
{
	/* obtain message content */
	struct msg_base_t *base;
	rpma_msg_get_ptr(msg, &base);

	struct msg_hello_ack_t *hello_ack;

	/* process message */
	switch (base->type) {
	case MSG_TYPE_HELLO_ACK:
		hello_ack = base;
		if (hello_ack->status)
			return 0;
		break;
	}
	// if RECV(bye bye message)
		// print the bye bye message
		// set exiting
		// cq break
	// if RECV(ML_UPDATE_ACK)
		// sem_dec(distribution_sem, 1);
}

/*
 * conn_thread -- single client connection entry point
 */
static void *
conn_thread(void *arg)
{
	struct client_ctx *client = arg;
	struct rpma_ctx *rctx = client->server->rctx;
	struct msg_hello_t *send;

	/* allocate & post the hello message ack recv */
	rpma_msg_new(rctx, sizeof(struct msg_hello_ack_t), RPMA_MSG_RECV,
			&client->recv_msg);
	rpma_conn_recv_post(client->conn, client->recv_msg);

	/* allocate the hello message */
	rpma_msg_new(rctx, sizeof(struct msg_hello_t), RPMA_MSG_SEND,
			&client->send_msg);
	rpma_msg_get_ptr(client->send_msg, &send);

	/* send the hello message */
	send->base.type = MSG_TYPE_HELLO;
	memcpy(send->cr_id, client->cr_id, sizeof(struct rpma_lmr_id));
	rpma_conn_send(client->conn, client->send_msg);

	/* register connection callbacks */
	rpma_conn_register_on_recv(client->conn, on_recv);
	rpma_conn_register_on_notify(client->conn, on_notify);
	rpma_conn_loop(client->conn, client);
	// - handles also emulated Atomic Writes

	rpma_msg_delete(&client->send_msg);
	rpma_msg_delete(&client->recv_msg);

	return NULL;
}

#define RPMA_TIMEOUT (60) /* 1m */

/*
 * on_timeout -- timeout callback
 */
static void
on_timeout(struct rpma_ctx *ctx)
{
	rpma_loop_break(ctx);
	return;
}

/*
 * on_conn_event -- connection event callback
 */
static int
on_conn_event(struct rpma_ctx *rctx, uint64_t event,
		struct rpma_conn *conn, void *arg)
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

		/* get empty client row */
		client_id = get_empty_client_row(ctx->clients, CLIENTS_CAPACITY);
		client = &ctx->clients[client_id];
		++ctx->nclients;

		/* accept the incoming connection */
		rpma_conn_new(rctx, &client->conn);
		rpma_conn_set_user_id(client->conn, client_id);
		rpma_conn_accept(client->conn);

		/* stop waiting for timeout */
		rpma_unregister_on_timeout(rctx);

		/* spawn the connection thread */
		pthread_create(&client->thread, NULL, conn_thread,
				client);
		break;

	case RPMA_CONN_EVENT_DISCONNECT:
		/* get client data from the connection */
		rpma_conn_get_user_id(conn, &client_id);
		client = &ctx->clients[client_id];

		/* break its loop and wait for thread join */
		rpma_conn_loop_break(conn);
		pthread_join(client->thread, &ret);

		/* clean the RPMA connection resources */
		rpma_conn_delete(&client->conn);

		/* decrease # of clients */
		--ctx->nclients;

		/* optionally start waiting for timeout */
		if (ctx->nclients == 0)
			rpma_register_on_timeout(rctx, on_timeout, RPMA_TIMEOUT);
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

	/* server context */
	struct server_ctx ctx;

	/* map the server root object */
	ctx.root = pmem_map_file(path, POOL_MIN_SIZE, PMEM_FILE_CREATE, O_RDWR,
			&ctx.root_size, NULL);
	size_t ml_size = ctx.root_size - (&ctx.root->ml - ctx.root);
	ml_init(&ctx.root->ml, ml_size);

	/* prepare RPMA configuration */
	struct rpma_config *cfg;
	rpma_config_new(&cfg);
	rpma_config_set_addr(cfg, addr);
	rpma_config_set_service(cfg, service);

	/* allocate RPMA context */
	rpma_ctx_new(cfg, &ctx->rctx);
	struct rpma_ctx *rctx = ctx->rctx;

	/* initialize client contexts */
	struct rpma_lmr cv_lmrs[CLIENTS_CAPACITY];
	ctx.nclients = 0;
	for (int i = 0; i < CLIENTS_CAPACITY; ++i) {
		/* local part */
		const struct client_ctx *client = &ctx.clients[i];
		client->client_id = i;
		client->server = ctx;
		client->cr = &ctx.root->cv[i];
		client->conn = NULL;
		/* RPMA part - client's row registration & id */
		rpma_lmr_new(rctx, client->cr, sizeof(struct client_row),
				RPMA_MR_WRITE_DST, &cv_lmrs[i]);
		rpma_lmr_get_id(cv_lmrs[i], &client->cr_id);
	}

	/* spawn ML monitor thread */
	pthread_create(&ctx.ml_monitor_thread, NULL, ml_monitor, &ctx);

	/* RPMA registers callbacks and start looping */
	rpma_listen(rctx);
	rpma_register_on_conn_event(rctx, on_conn_event);
	rpma_register_on_timeout(rctx, on_timeout, RPMA_TIMEOUT);
	rpma_conn_loop(rctx);

	/* join ML monitor thread */
	int ret;
	pthread_join(ctx.ml_monitor_thread, &ret);

	/* cleanup client contexts */
	for (int i = 0; i < CLIENTS_CAPACITY; ++i) {
		/* RPMA - release memory registrations */
		rpma_lmr_delete(&cv_lmrs[i]);
	}

	/* RPMA - cleanup server resources */
	rpma_ctx_delete(ctx->rctx);
	rpma_config_delete(&cfg);

	/* unmap the persistent part */
	pmem_unmap(ctx->root, ctx->root_size);

	return 0;
}
