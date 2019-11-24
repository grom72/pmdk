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

#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <libpmem.h>
#include <librpma/base.h>
#include <librpma/memory.h>
#include <librpma/msg.h>

#include "pstructs.h"
#include "protocol.h"
#include "remote.h"

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
	uint64_t hello_done;

	/* persistent data and its derivatives */
	struct root_obj *root;
	struct rpma_memory_local *ml_local; /* client side mlog */
	struct rpma_memory_remote *ml_remote; /* server side mlog */

	/* transient data */
	struct client_row cr;
	struct rpma_memory_local *cr_local; /* client side cr */
	struct rpma_memory_remote *cr_remote; /* server side cr */

	struct writer_t {
		pthread_t thread;
		sem_t done;
	} writer;

	struct rpma_sequence *mlog_update_seq;
	struct mlog_update_args_t mlog_update_args;
	struct rpma_sequence *write_msg_seq;
	struct proto_write_msg_args_t write_msg_args;
};

/*
 * on_connection_recv_process_ack -- process an ACK message
 */
static int
on_connection_recv_process_ack(struct client_ctx *ctx, union msg_t *msg)
{
	if (msg->ack.status != 0)
		return msg->ack.status;

	switch (msg->ack.original_msg_type) {
	case MSG_TYPE_BYE_BYE:
		writer_fini(ctx);
		return RPMA_E_OK;
	default:
		return RPMA_E_INVALID_MSG;
	}
}

/*
 * process_hello -- process MSG_TYPE_HELLO
 */
static int
process_hello(struct client_ctx *clnt, struct msg_t *msg)
{
	struct rpma_zone *zone = clnt->zone;

	/* decode and allocate remote memory regions descriptor */
	rpma_memory_remote_new(zone, &msg->hello.cr_id, &clnt->cr_remote);
	rpma_memory_remote_new(zone, &msg->hello.ml_id, &clnt->ml_remote);
	clnt->hello_done = 1;

	/* initialize writer */
	writer_init(clnt);

	rpma_connection_enqueue(clnt->conn, proto_hello_ack, NULL);

	return RPMA_E_OK;
}

/*
 * process_hello -- process MSG_TYPE_MLOG_UPDATE
 */
static int
process_mlog_update(struct client_ctx *clnt, struct msg_t *msg)
{
	clnt->mlog_update_args.wptr = msg->update.wptr;
	rpma_connection_enqueue_sequence(clnt->conn, clnt->mlog_update_seq);

	/* display the mlog */
	ml_read(ml); /* XXX move to the writer ? */

	return 0;
}

/*
 * on_connection_recv -- on connection receive callback
 */
static int
on_connection_recv(struct rpma_connection *conn, struct rpma_msg *rmsg, size_t length, void *uarg)
{
	struct client_ctx *clnt = uarg;

	/* obtain a message content */
	union msg_t *msg;
	rpma_msg_get_ptr(rmsg, &msg);

	/* process the message */
	switch (msg->base.type) {
	case MSG_TYPE_ACK:
		return on_connection_recv_process_ack(clnt, msg);
	case MSG_TYPE_HELLO:
		return process_hello(clnt, msg);
	case MSG_TYPE_MLOG_UPDATE:
		return process_mlog_update(clnt, msg);
	default:
		return RPMA_E_INVALID_MSG;
	}
}

/*
 * on_connection_timeout -- connection timeout callback
 */
static int
on_connection_timeout(struct rpma_zone *zone, void *uarg)
{
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
	struct client_ctx *clnt = uarg;

	switch (event) {
	case RPMA_CONNECTION_EVENT_OUTGOING:
		/* establish the outgoing connection */
		rpma_connection_new(zone, &clnt->conn);
		rpma_connection_set_custom_data(clnt->conn, (void *)clnt);
		rpma_connection_establish(clnt->conn);
		rpma_connection_attach(clnt->conn, clnt->wrk->disp);

		/* stop waiting for timeout */
		rpma_connection_unregister_on_timeout(zone);

		/* register transmission callback */
		rpma_transmission_register_on_recv(clnt->conn, on_connection_recv);
		break;

	case RPMA_CONNECTION_EVENT_DISCONNECT:
		/* get client data from the connection */
		rpma_connection_get_custom_data(conn, (void **)&clnt);

		/* clean the RPMA connection resources */
		rpma_connection_detach(clnt->conn);
		rpma_connection_delete(&clnt->conn);

		/* on disconnect break the connection loop */
		rpma_connection_loop_break(zone);
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

	/* register local memory regions */
	rpma_memory_local_new(zone, &clnt->root->ml,
			MSG_LOG_SIZE(clnt->root->ml.capacity), RPMA_MR_READ_DST,
			&clnt->ml_local);
	rpma_memory_local_new(zone, &clnt->cr, sizeof(clnt->cr),
			RPMA_MR_WRITE_SRC, &clnt->cr_local);

	/* allocate mlog update sequence */
	struct rpma_sequence *seq;
	rpma_sequence_new(&seq);
	rpma_sequence_add_step(seq, proto_mlog_update_read, &clnt->mlog_update_args);
	rpma_sequence_add_step(seq, proto_mlog_update_ack, &clnt->mlog_update_args);
	clnt->mlog_update_seq = seq;

	/* allocate write msg sequence */
	rpma_sequence_new(&seq);
	rpma_sequence_add_step(seq, proto_write_msg_and_user, &clnt->write_msg_args);
	rpma_sequence_add_step(seq, proto_write_msg_status, &clnt->write_msg_args);
	rpma_sequence_add_step(seq, proto_write_msg_signal, &clnt->write_msg_args);
	clnt->write_msg_seq = seq;

	/* RPMA registers callbacks and start looping */
	rpma_register_on_connection_event(zone, on_connection_event);
	rpma_register_on_connection_timeout(zone, on_connection_timeout, RPMA_TIMEOUT);

	/* no listenning zone will try to establish a connection */
	rpma_connection_loop(zone, clnt);

	/* deallocate sequences */
	rpma_sequence_delete(clnt->write_msg_seq);
	rpma_sequence_delete(clnt->mlog_update_seq);

	/* deallocate remote memory regions */
	if (clnt->hello_done) {
		rpma_memory_remote_delete(&clnt->cr_remote);
		rpma_memory_remote_delete(&clnt->ml_remote);
	}

	/* deallocate local memory regions */
	rpma_memory_local_delete(&clnt->cr_local);
	rpma_memory_local_delete(&clnt->ml_local);
}

/*
 * writer_publish_msg -- write the message to the server
 */
static void
writer_publish_msg(struct client_ctx *clnt)
{
	struct proto_write_msg_args_t *args = &clnt->write_msg_args;
	args.cr = &clnt->cr;
	args.dst = clnt->cr_remote;
	args.src = clnt->cr_local;
	args.done = &clnt->writer.done;

	rpma_connection_enqueue_sequence(clnt->conn, clnt->write_msg_seq);

	sem_wait(args->done);
}

/*
 * writer_thread_func -- client writer entry point
 */
static void *
writer_thread_func(void *arg)
{
	struct client_ctx *clnt = arg;
	ssize_t ret;

	while(clnt->running) { // XXX atomic
		printf("< ");
		ret = read(STDIN_FILENO, clnt->cr.msg, MSG_SIZE_MAX);

		if (ret == 0)
			continue;
		if (ret < 0) {
			rpma_connection_enqueue(clnt->conn, proto_bye_bye, NULL);
			clnt->running = 0; /* XXX atomic */
			break;
		}

		/* new message */
		clnt->cr.status = CLIENT_MSG_PENDING;
		writer_publish_msg(clnt);
	}

	return NULL;
}

/*
 * writer_init -- initialize writer thread
 */
static void
writer_init(struct client_ctx *clnt)
{
	pthread_cond_init(&clnt->writer.cond);
	pthread_create(&clnt->writer.thread, NULL, writer_thread_func,
			clnt);
}

/*
 * writer_fini -- cleanup writer thread
 */
static void
writer_fini(struct client_ctx *clnt)
{
	pthread_join(clnt->writer.thread, NULL);
	pthread_cond_destroy(&clnt->writer.cond);
}

int
main(int argc, char *argv[])
{
	const char *path = argv[1];
	const char *addr = argv[2];
	const char *service = argv[3];

	struct client_ctx clnt = {0};

	pmem_init(&clnt->root, path, POOL_MIN_SIZE);
	remote_init(&clnt, addr, service);
	workers_init(clnt->zone, &clnt->wrk, 1);
	writer_init(clnt);

	remote_main(&clnt);

	writer_fini(clnt);
	workers_init(clnt->wrk, 1)
	remote_fini(&clnt);
	pmem_fini(clnt->root);

	return 0;
}
