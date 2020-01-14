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
#include <stdlib.h>

#include <libpmem.h>

#include "pstructs.h"
#include "protocol.h"
#include "workers.h"
#include "server.h"

/* server-side assumptions */
#define MAX_CLIENTS (100) /* # of reserved client rows at the begining */
#define CLIENT_ROWS_INC (10)
#define MSG_LOG_MIN_CAPACITY (1000)

#define N_WORKERS (4)
#define WORKERS_MASK (N_WORKERS - 1)

/* derive minimal pool size from the assumptions */
#define POOL_MIN_SIZE \
	(MSG_LOG_SIZE(MSG_LOG_MIN_CAPACITY) + \
			sizeof(struct client_row) * MAX_CLIENTS)

/* server-side client context */
struct client_ctx {
	struct server_ctx *svr;
	struct worker_ctx *wrk;

	/* persistent client-row and its id */
	struct client_row *cr;
	struct rpma_memory_local *cr_mr; /* memory region */
	struct rpma_memory_id cr_id; /* MR identifier */

	/* client's connection */
	struct rpma_connection *conn;

	struct hello_args_t hello_args;
};

/*
 * on_connection_notify -- on connection notify callback
 */
static int
on_connection_notify(struct rpma_connection *conn, void *ptr, size_t length,
		void *uarg)
{
	struct client_ctx *clnt = uarg;
	struct client_row *cr = ptr;

	/* verify the client's message is ready */
	assert(cr->status == CLIENT_MSG_PENDING);

	/* append the client's message to ML */
	struct msg_log *ml = &clnt->svr->root->ml;
	mlog_append(ml, cr);
	distributor_notify(clnt->svr->distributor);

	return RPMA_E_OK;
}

/*
 * on_connection_recv_process_ack -- process an ACK message
 */
static int
on_connection_recv_process_ack(struct msg_t *msg, struct client_ctx *clnt)
{
	uint64_t original_msg_type = 0;
	int ret;
	if ((ret = proto_ack_process(msg, &original_msg_type)))
		return ret;

	switch (original_msg_type) {
	case MSG_TYPE_MLOG_UPDATE:
		distributor_ack(clnt->svr->distributor);
		return RPMA_E_OK;
	default:
		return RPMA_E_INVALID_MSG;
	}
}

/*
 * on_connection_recv -- on connection receive callback
 */
static int
on_connection_recv(struct rpma_connection *conn, void *ptr, size_t length,
		void *uarg)
{
	struct client_ctx *clnt = uarg;
	struct msg_t *msg = ptr;

	/* process the message */
	switch (msg->base.type) {
	case MSG_TYPE_ACK:
		return on_connection_recv_process_ack(msg, clnt);
	case MSG_TYPE_BYE_BYE:
		/* XXX print bye bye message */
		return rpma_zone_wait_break(clnt->svr->zone);
	default:
		return RPMA_E_INVALID_MSG;
	}
}

/*
 * client_init -- initialize client context
 */
static struct client_ctx *
client_init(struct server_ctx *svr, struct worker_ctx *wrk,
		struct client_row *cr)
{
	struct client_ctx *clnt = malloc(sizeof(*clnt));

	clnt->svr = svr;
	clnt->wrk = wrk;
	clnt->cr = cr;
	clnt->conn = NULL;

	p_client_init(cr);

	/* RPMA part - client's row registration & id */
	rpma_memory_local_new(svr->zone, clnt->cr, sizeof(*cr),
			RPMA_MR_WRITE_DST, &clnt->cr_mr);
	rpma_memory_local_get_id(clnt->cr_mr, &clnt->cr_id);

	return clnt;
}

/*
 * client_fini -- cleanup client context
 */
static void
client_fini(struct client_ctx *clnt)
{
	/* RPMA - release memory registrations */
	rpma_memory_local_delete(&clnt->cr_mr);

	free(clnt);
}

/*
 * on_connection_timeout -- connection timeout callback
 */
static int
on_connection_timeout(struct rpma_zone *zone, void *uarg)
{
	rpma_zone_wait_break(zone);
	return 0;
}

/*
 * on_connection_event -- connection event callback
 */
static int
on_connection_event(struct rpma_zone *zone, uint64_t event,
		struct rpma_connection *conn, void *uarg)
{
	struct server_ctx *svr = uarg;
	struct client_ctx *clnt;
	struct client_row *cr;

	switch (event) {
	case RPMA_CONNECTION_EVENT_INCOMING:
		if (svr->nclients == MAX_CLIENTS)
			rpma_connection_reject(zone);

		++svr->nclients;

		/* allocate client context */
		cr = p_get_empty_client_row(svr->clients, svr->nclient_rows);
		clnt = client_init(svr, worker_next(svr->workers, WORKERS_MASK),
				cr);

		/* accept the incoming connection */
		rpma_connection_new(zone, &clnt->conn);
		rpma_connection_set_custom_data(clnt->conn, (void *)clnt);
		rpma_connection_accept(clnt->conn);
		rpma_connection_attach(clnt->conn, clnt->wrk->disp);
		rpma_connection_group_add(svr->grp, clnt->conn);

		/* stop waiting for timeout */
		rpma_zone_unregister_on_timeout(zone);

		/* register callback's */
		rpma_connection_register_on_recv(conn, on_connection_recv);
		rpma_connection_register_on_notify(conn, on_connection_notify);

		/* enqueue sending the hello message */
		clnt->hello_args.cr_id = &clnt->cr_id;
		rpma_connection_enqueue(clnt->conn, proto_hello,
				&clnt->hello_args);
		break;

	case RPMA_CONNECTION_EVENT_DISCONNECT:
		/* get client data from the connection */
		rpma_connection_get_custom_data(conn, (void **)&clnt);

		/* clean the RPMA connection resources */
		rpma_connection_group_remove(svr->grp, clnt->conn);
		rpma_connection_detach(clnt->conn);
		rpma_connection_delete(&clnt->conn);

		client_fini(clnt);

		/* decrease # of clients */
		--svr->nclients;

		/* optionally start waiting for timeout */
		if (svr->nclients == 0)
			rpma_zone_register_on_timeout(zone,
					on_connection_timeout, RPMA_TIMEOUT);
		break;
	default:
		return RPMA_E_UNHANDLED_EVENT;
	}

	return RPMA_E_OK;
}

/*
 * remote_main -- main entry-point to RPMA
 */
static void
remote_main(struct server_ctx *svr)
{
	struct rpma_zone *zone = svr->zone;

	rpma_connection_group_new(&svr->grp);

	rpma_listen(zone);

	/* RPMA registers callbacks and start looping */
	rpma_zone_register_on_connection_event(zone, on_connection_event);
	rpma_zone_register_on_timeout(zone,
			on_connection_timeout, RPMA_TIMEOUT);

	rpma_zone_wait_connections(zone, NULL);

	rpma_connection_group_delete(&svr->grp);
}

int
main(int argc, char *argv[])
{
	const char *path = argv[1];
	const char *addr = argv[2];
	const char *service = argv[3];

	struct server_ctx svr = {0};

	pmem_init(&svr.root, path, POOL_MIN_SIZE);
	proto_common_init(&svr.zone, addr, service);
	workers_init(svr.zone, &svr.workers, N_WORKERS);
	distributor_init(&svr);

	remote_main(&svr);

	distributor_fini(&svr);
	workers_fini(svr.workers, N_WORKERS);
	proto_common_fini(svr.zone);
	pmem_fini(svr.root);

	return 0;
}
