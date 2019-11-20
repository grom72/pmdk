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
 * server.c -- librpma-based communicator server
 */

#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <libpmem.h>
#include <librpma/base.h>
#include <librpma/memory.h>
#include <librpma/msg.h>
#include <librpma/transmission.h>

#include "pstructs.h"
#include "mlog.h"
#include "msgs.h"

/* server-side assumptions */
#define CLIENTS_MAX (10)
#define MSG_LOG_MIN_CAPACITY (1000)

/* server-side persistent root object */
struct root_obj {
	struct client_row cv[CLIENTS_MAX]; /* the clients vector */
	struct msg_log ml;
};

/* derive minimal pool size from the assumptions */
#define POOL_MIN_SIZE \
	(sizeof(struct client_row) * CLIENTS_MAX + MSG_LOG_SIZE(MSG_LOG_MIN_CAPACITY))

/* server-side client context */
struct client_ctx {
	uint64_t client_id;
	struct server_ctx *server;

	/* persistent client-row and its id */
	struct client_row *cr;
	struct rpma_memory_local *cr_mr; /* memory region */
	struct rpma_memory_id cr_id; /* MR identifier */

	/* RPMA send and recv messages */
	struct rpma_msg *send_msg;
	struct rpma_msg *recv_msg;

	/* client's connection and its thread */
	struct rpma_connection *conn;
	pthread_t thread;
};

/* server context */
struct server_ctx {
	struct rpma_zone *zone;
	uint64_t exiting;

	/* persistent data and its derivatives */
	struct root_obj *root;
	size_t root_size;
	size_t ml_capacity; /* the message log capacity */

	/* client's contextes */
	uint64_t nclients; /* current # of clients */
	struct client_ctx clients[CLIENTS_MAX];

	/* ML distributor resources */
	struct distributor_t {
		sem_t notify;
		sem_t acks;
		pthread_t thread;
	} distributor;
};

/*
 * distributor_notify --  notify the distributor that new messages are ready
 */
static void
distributor_notify(struct distributor_t *dist)
{
	sem_post(&dist->notify);
}

/*
 * distributor_trywait -- wait for new messages
 */
static int
distributor_trywait(struct distributor_t *dist)
{
	return sem_trywait(&dist->notify);
}

/*
 * distributor_wait_acks -- wait for specified # of ACKs
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
static void
distributor_ack(struct distributor_t *dist)
{
	sem_post(&dist->acks);
}

/*
 * distributor_send -- send the ML update to the client
 */
static void
distributor_send(struct client_ctx *client, uintptr_t wptr)
{
	struct msg_t *msg;
	rpma_msg_get_ptr(client->send_msg, (void **)&msg);

	/* prepare for the message ACK */
	rpma_connection_recv_post(client->conn, client->recv_msg);

	/* prepare the message */
	msg->base.type = MSG_TYPE_MLOG_UPDATE;
	msg->update.wptr = wptr;

	/* send the message */
	rpma_connection_send(client->conn, client->send_msg);
}

#define DISTRIBUTOR_SLEEP (1)

/*
 * distributor_thread_func -- the message log distributor
 */
static void *
distributor_thread_func(void *arg)
{
	struct server_ctx *ctx = (struct server_ctx *)arg;
	struct distributor_t *dist = &ctx->distributor;
	int ret;

	while (!ctx->exiting) {
		/* wait for new messages */
		ret = distributor_trywait(dist);
		if (ret) {
			sleep(DISTRIBUTOR_SLEEP);
			continue;
		}
		/* no new messages */
		if (!ml_ready(&ctx->root->ml))
			continue;

		/* get current write pointer and # of acks to collect */
		size_t wptr = ml_get_wptr(&ctx->root->ml);

		/* send updates to the clients */
		for (int i = 0; i < CLIENTS_MAX; ++i) {
			if (!ctx->clients[i].conn)
				continue;

			distributor_send(&ctx->clients[i], wptr);
		}

		distributor_wait_acks(dist, ctx->nclients, ctx);

		/* set read pointer */
		ml_set_rptr(&ctx->root->ml, wptr);
	}

	return NULL;
}

/*
 * on_transmission_notify -- on transmission notify callback
 */
static int
on_transmission_notify(struct rpma_connection *conn, void *addr, size_t length, void *arg)
{
	/* verify the client's message is ready */
	struct client_row *cr = addr;
	assert(cr->status == CLIENT_MSG_READY);

	/* obtain custom connection data - the client */
	struct client_ctx *client;
	rpma_connection_get_custom_data(conn, (void **)&client);

	/* append the client's message to ML */
	struct msg_log *ml = &client->server->root->ml;
	mlog_append(ml, client->client_id, cr->msg_size, cr->msg);
	distributor_notify(&client->server->distributor);

	/* set the message is already processed */
	cr->status = CLIENT_MSG_DONE;
	pmem_persist(&cr->status, sizeof(cr->status));

	return RPMA_E_OK;
}

/*
 * on_transmission_recv_process_ack -- process an ACK message
 */
static int
on_transmission_recv_process_ack(struct msg_t *msg, struct client_ctx *client)
{
	if (msg->ack.status != 0)
		return msg->ack.status;

	switch (msg->ack.original_msg_type) {
	case MSG_TYPE_MLOG_UPDATE:
		distributor_ack(&client->server->distributor);
		return RPMA_E_OK;
	default:
		return RPMA_E_INVALID_MSG;
	}
}

/*
 * on_transmission_recv -- on transmission receive callback
 */
static int
on_transmission_recv(struct rpma_connection *conn, struct rpma_msg *rmsg, size_t length, void *arg)
{
	struct client_ctx *client;
	rpma_connection_get_custom_data(conn, (void **)&client);

	/* obtain a message content */
	struct msg_t *msg;
	rpma_msg_get_ptr(rmsg, (void **)&msg);

	/* process the message */
	switch (msg->base.type) {
	case MSG_TYPE_ACK:
		return on_transmission_recv_process_ack(msg, client);
	case MSG_TYPE_BYE_BYE:
		/* XXX print bye bye message */
		return rpma_transmission_loop_break(client->conn);
	default:
		return RPMA_E_INVALID_MSG;
	}
}

/*
 * client_hello_init -- send the hello message and prepare for ACK
 */
static void
client_hello_init(struct client_ctx *client)
{
	struct rpma_zone *zone = client->server->zone;
	struct msg_t *send;

	/* allocate & post the hello message ACK recv */
	rpma_msg_new(zone, RPMA_MSG_RECV, &client->recv_msg);
	rpma_connection_recv_post(client->conn, client->recv_msg);

	/* allocate the hello message */
	rpma_msg_new(zone, RPMA_MSG_SEND, &client->send_msg);
	rpma_msg_get_ptr(client->send_msg, (void **)&send);

	/* send the hello message */
	send->base.type = MSG_TYPE_HELLO;
	memcpy(&send->hello.cr_id, &client->cr_id, sizeof(struct rpma_memory_id));
	rpma_connection_send(client->conn, client->send_msg);
}

/*
 * client_hello_fini -- cleanup after the hello message exchange
 */
static void
client_hello_fini(struct client_ctx *client)
{
	rpma_msg_delete(&client->send_msg);
	rpma_msg_delete(&client->recv_msg);
}

/*
 * client_thread_func -- single client connection entry point
 */
static void *
client_thread_func(void *arg)
{
	struct client_ctx *client = arg;

	client_hello_init(client);

	/* register transmission callbacks */
	rpma_transmission_register_on_recv(client->conn, on_transmission_recv);
	rpma_transmission_register_on_notify(client->conn, on_transmission_notify);

	rpma_transmission_loop(client->conn, client);

	client_hello_fini(client);

	return NULL;
}

/*
 * get_empty_client_row -- find first empty client row
 */
static struct client_ctx *
get_empty_client_row(struct client_ctx clients[], uint64_t capacity)
{
	for (int i = 0; i < capacity; ++i) {
		if (clients[i].conn == NULL)
			return &clients[i];
	}

	return NULL;
}

#define RPMA_TIMEOUT (60) /* 1m */

/*
 * on_connection_timeout -- connection timeout callback
 */
static int
on_connection_timeout(struct rpma_zone *zone, void *uarg)
{
	struct server_ctx *ctx = uarg;
	ctx->exiting = true; /* XXX atomic */

	rpma_connection_loop_break(zone);
	return 0;
}

/*
 * on_connection_event -- connection event callback
 */
static int
on_connection_event(struct rpma_zone *zone, uint64_t event,
		struct rpma_connection *conn, void *uarg)
{
	struct server_ctx *ctx = uarg;
	struct client_ctx *client;

	switch (event) {
	case RPMA_CONNECTION_EVENT_INCOMING:
		/* not enough capacity */
		if (ctx->nclients == CLIENTS_MAX) {
			rpma_connection_reject(zone);
			return 0;
		}

		/* get empty client row */
		client = get_empty_client_row(ctx->clients, CLIENTS_MAX);
		++ctx->nclients;

		/* accept the incoming connection */
		rpma_connection_new(zone, &client->conn);
		rpma_connection_set_custom_data(client->conn, (void *)client);
		rpma_connection_set_msg_size(client->conn, sizeof(struct msg_t));
		rpma_connection_accept(client->conn);

		/* stop waiting for timeout */
		rpma_connection_unregister_on_timeout(zone);

		/* spawn the connection thread */
		pthread_create(&client->thread, NULL, client_thread_func,
				client);
		break;

	case RPMA_CONNECTION_EVENT_DISCONNECT:
		/* get client data from the connection */
		rpma_connection_get_custom_data(conn, (void **)&client);

		/* break its loop and wait for the thread join */
		rpma_transmission_loop_break(conn);
		pthread_join(client->thread, NULL);

		/* clean the RPMA connection resources */
		rpma_connection_delete(&client->conn);

		/* decrease # of clients */
		--ctx->nclients;

		/* optionally start waiting for timeout */
		if (ctx->nclients == 0)
			rpma_register_on_connection_timeout(zone, on_connection_timeout, RPMA_TIMEOUT);
		break;
	default:
		return RPMA_E_UNHANDLED_EVENT;
	}

	return RPMA_E_OK;
}

/*
 * distributor_init -- spawn the ML distributor thread
 */
static void
distributor_init(struct server_ctx *ctx)
{
	sem_init(&ctx->distributor.notify, 0, 0);
	sem_init(&ctx->distributor.acks, 0, 0);
	pthread_create(&ctx->distributor.thread, NULL, distributor_thread_func, &ctx);
}

/*
 * distributor_fini -- clean up the ML distributor
 */
static void
distributor_fini(struct server_ctx *ctx)
{
	pthread_join(ctx->distributor.thread, NULL);
	sem_destroy(&ctx->distributor.acks);
	sem_destroy(&ctx->distributor.notify);
}

/*
 * clients_init -- initialize client contexts
 */
static void
clients_init(struct server_ctx *ctx)
{
	struct rpma_zone *zone = ctx->zone;

	ctx->nclients = 0;
	for (int i = 0; i < CLIENTS_MAX; ++i) {
		/* local part */
		struct client_ctx *client = &ctx->clients[i];
		client->client_id = i;
		client->server = ctx;
		client->cr = &ctx->root->cv[i];
		client->conn = NULL;
		/* RPMA part - client's row registration & id */
		rpma_memory_local_new(zone, client->cr, sizeof(struct client_row),
				RPMA_MR_WRITE_DST, &client->cr_mr);
		rpma_memory_local_get_id(client->cr_mr, &client->cr_id);
	}
}

/*
 * clients_fini -- cleanup client contexts
 */
static void
clients_fini(struct server_ctx *ctx)
{
	/* RPMA - release memory registrations */
	for (int i = 0; i < CLIENTS_MAX; ++i) {
		rpma_memory_local_delete(&ctx->clients[i].cr_mr);
	}
}

/*
 * remote_init -- prepare RPMA context
 */
static void
remote_init(struct server_ctx *ctx, const char *addr, const char *service)
{
	/* prepare RPMA configuration */
	struct rpma_config *cfg;
	rpma_config_new(&cfg);
	rpma_config_set_addr(cfg, addr);
	rpma_config_set_service(cfg, service);

	/* allocate RPMA context */
	rpma_zone_new(cfg, &ctx->zone);

	/* destroy RPMA configuration */
	rpma_config_delete(&cfg);
}

/*
 * remote_main -- main entry-point to RPMA
 */
static void
remote_main(struct server_ctx *ctx)
{
	struct rpma_zone *zone = ctx->zone;

	rpma_listen(zone);

	/* RPMA registers callbacks and start looping */
	rpma_register_on_connection_event(zone, on_connection_event);
	rpma_register_on_connection_timeout(zone, on_connection_timeout, RPMA_TIMEOUT);

	rpma_connection_loop(zone, NULL);
}

/*
 * remote_fini -- delete RPMA content
 */
static void
remote_fini(struct server_ctx *ctx)
{
	rpma_zone_delete(&ctx->zone);
}

/*
 * pmem_init -- map the server root object
 */
static void
pmem_init(struct server_ctx *ctx, const char *path)
{
	ctx->root = pmem_map_file(path, POOL_MIN_SIZE, PMEM_FILE_CREATE, O_RDWR,
			&ctx->root_size, NULL);
	const size_t ml_offset = offsetof(struct root_obj, ml);
	size_t ml_size = ctx->root_size - ml_offset;
	ml_init(&ctx->root->ml, ml_size);
}

/*
 * pmem_fini -- unmap the persistent part
 */
static void
pmem_fini(struct server_ctx *ctx)
{
	pmem_unmap(ctx->root, ctx->root_size);
}

int
main(int argc, char *argv[])
{
	const char *path = argv[1];
	const char *addr = argv[2];
	const char *service = argv[3];

	struct server_ctx ctx = {0};

	pmem_init(&ctx, path);
	remote_init(&ctx, addr, service);
	clients_init(&ctx);
	distributor_init(&ctx);

	remote_main(&ctx);

	distributor_fini(&ctx);
	clients_fini(&ctx);
	remote_fini(&ctx);
	pmem_fini(&ctx);

	return 0;
}
