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
 * client.c -- librpma-based communicator client
 */

#include <libpmem.h>
#include <librpma/msg.h>

#include "pstructs.h"
#include "protocol.h"
#include "workers.h"

/* client-side assumptions */
#define MSG_LOG_MIN_CAPACITY (1000)

/* derive minimal pool size from the assumptions */
#define POOL_MIN_SIZE \
	(MSG_LOG_SIZE(MSG_LOG_MIN_CAPACITY))

/* client context */
struct client_ctx {
	struct rpma_zone *zone;
	struct rpma_connection *conn;

	struct worker_ctx *wrk;
	uint64_t running;

	/* persistent data */
	struct root_obj *root;

	/* transient data */
	struct client_row cr;

	struct writer_t *writer;

	struct mlog_update_args_t mlog_update_args;

	struct hello_result_t remote;
	struct client_local_t local;
};

/*
 * on_connection_recv_process_ack -- process an ACK message
 */
static int
on_connection_recv_process_ack(struct client_ctx *ctx, struct msg_t *msg)
{
	uint64_t original_msg_type = -1;
	int ret;
	if ((ret = proto_ack_process(msg, &original_msg_type)))
		return ret;

	switch (original_msg_type) {
	case MSG_TYPE_BYE_BYE:
		rpma_connection_disconnect(ctx->conn);
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
		return on_connection_recv_process_ack(clnt, msg);
	case MSG_TYPE_HELLO:
		proto_hello_process(clnt->conn, msg, &clnt->remote);
		break;
	case MSG_TYPE_MLOG_UPDATE:
		proto_mlog_update_process(msg, &clnt->mlog_update_args,
				clnt->conn, clnt->local.mlog_update_seq);
		break;
	default:
		return RPMA_E_INVALID_MSG;
	}

	return RPMA_E_OK;
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
	struct client_ctx *clnt = uarg;

	switch (event) {
	case RPMA_CONNECTION_EVENT_OUTGOING:
		/* establish the outgoing connection */
		rpma_connection_new(zone, &clnt->conn);
		rpma_connection_set_custom_data(clnt->conn, (void *)clnt);
		rpma_connection_establish(clnt->conn);
		rpma_connection_attach(clnt->conn, clnt->wrk->disp);

		/* stop waiting for timeout */
		rpma_zone_unregister_on_timeout(zone);

		/* register transmission callback */
		rpma_connection_register_on_recv(clnt->conn,
				on_connection_recv);
		break;

	case RPMA_CONNECTION_EVENT_DISCONNECT:
		/* clean the RPMA connection resources */
		rpma_connection_detach(clnt->conn);
		rpma_connection_delete(&clnt->conn);

		/* on disconnect break the connection loop */
		rpma_zone_wait_break(zone);
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
remote_main(struct client_ctx *clnt)
{
	struct rpma_zone *zone = clnt->zone;

	/* RPMA registers callbacks and start looping */
	rpma_zone_register_on_connection_event(zone, on_connection_event);
	rpma_zone_register_on_timeout(zone, on_connection_timeout,
			RPMA_TIMEOUT);

	/* no listenning zone will try to establish a connection */
	rpma_zone_wait_connections(zone, clnt);

	proto_hello_cleanup(&clnt->remote);
}

int
main(int argc, char *argv[])
{
	const char *path = argv[1];
	const char *addr = argv[2];
	const char *service = argv[3];

	struct client_ctx clnt = {0};

	pmem_init(&clnt.root, path, POOL_MIN_SIZE);
	proto_common_init(&clnt.zone, addr, service);

	clnt.local.cr_ptr = &clnt.cr;
	clnt.local.ml_ptr = &clnt.root->ml;
	proto_client_init(clnt.zone, &clnt.local, &clnt.mlog_update_args);

	workers_init(clnt.zone, &clnt.wrk, 1);
	writer_init(&clnt.writer, &clnt.conn, &clnt.local, &clnt.remote);

	remote_main(&clnt);

	writer_fini(clnt.writer);
	workers_fini(clnt.wrk, 1);

	proto_client_fini(&clnt.local);
	proto_common_fini(clnt.zone);
	pmem_fini(clnt.root);

	return 0;
}
