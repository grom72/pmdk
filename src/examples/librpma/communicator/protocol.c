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
 * protocol.c -- communication protocol definitions
 */

#include <stdlib.h>
#include <string.h>

#include <librpma.h>

#include "pstructs.h"
#include "protocol.h"

#define SEND_Q_LENGTH 10
#define RECV_Q_LENGTH 10

/*
 * proto_hello -- send MSG_TYPE_HELLO
 */
int
proto_hello(struct rpma_connection *conn, void *uarg)
{
	struct hello_args_t *args = uarg;
	struct msg_t *msg;

	/* prepare the hello message */
	rpma_msg_get_ptr(conn, (void **)&msg);
	msg->base.type = MSG_TYPE_HELLO;
	memcpy(&msg->hello.cr_id, args->cr_id, sizeof(*args->cr_id));

	/* send the hello message */
	rpma_connection_send(conn, msg);

	return RPMA_E_OK;
}

/*
 * proto_hello_process -- process MSG_TYPE_HELLO
 */
int
proto_hello_process(struct rpma_connection *conn, struct msg_t *msg,
		struct hello_result_t *res)
{
	struct rpma_zone *zone;
	rpma_connection_get_zone(conn, &zone);

	/* decode and allocate remote memory regions descriptor */
	rpma_memory_remote_new(zone, &msg->hello.cr_id, &res->cr);
	rpma_memory_remote_new(zone, &msg->hello.ml_id, &res->ml);
	res->done = 1;

	rpma_connection_enqueue(conn, proto_hello_ack, NULL);

	return RPMA_E_OK;
}

/*
 * proto_hello_cleanup -- process MSG_TYPE_HELLO - cleanup
 */
int
proto_hello_cleanup(struct hello_result_t *res)
{
	if (!res->done)
		return RPMA_E_OK;

	rpma_memory_remote_delete(&res->cr);
	rpma_memory_remote_delete(&res->ml);
	return RPMA_E_OK;
}

/*
 * proto_hello_ack -- send MSG_TYPE_HELLO ACK
 */
int
proto_hello_ack(struct rpma_connection *conn, void *unused)
{
	struct msg_t *ack;

	/* prepare the hello message */
	rpma_msg_get_ptr(conn, (void **)&ack);
	ack->base.type = MSG_TYPE_ACK;
	ack->ack.original_msg_type = MSG_TYPE_HELLO;
	ack->ack.status = 0;

	/* send the hello message ACK */
	rpma_connection_send(conn, ack);

	return RPMA_E_OK;
}

/*
 * proto_mlog_update -- send MSG_TYPE_MLOG_UPDATE
 */
int
proto_mlog_update(struct rpma_connection *conn, void *uarg)
{
	struct mlog_update_args_t *args = uarg;
	struct msg_t *msg;

	/* prepare the message log update message */
	rpma_msg_get_ptr(conn, (void **)&msg);
	msg->base.type = MSG_TYPE_MLOG_UPDATE;
	msg->update.wptr = args->wptr;

	/* send the message */
	rpma_connection_send(conn, msg);

	return RPMA_E_OK;
}

/*
 * proto_mlog_update_process -- process MSG_TYPE_MLOG_UPDATE
 */
int
proto_mlog_update_process(struct msg_t *msg, struct mlog_update_args_t *args,
		struct rpma_connection *conn, struct rpma_sequence *seq)
{
	args->wptr = msg->update.wptr;
	rpma_connection_enqueue_sequence(conn, seq);

	/* display the mlog */
	// ml_read(ml); /* XXX move to the writer ? */

	return 0;
}

/*
 * proto_mlog_update_read -- schedule RDMA.read of the updated part of the mlog
 */
int
proto_mlog_update_read(struct rpma_connection *conn, void *uarg)
{
	struct mlog_update_args_t *args = uarg;
	struct msg_log *ml = args->cloc->ml_ptr;
	struct rpma_memory_local *dst = args->cloc->ml;
	struct rpma_memory_remote *src = args->hres->ml;

	/* calculate remote read parameters */
	uintptr_t wptr = ml_get_wptr(ml);
	size_t offset = ml_offset(ml, wptr);
	size_t length = ml_offset(ml, args->wptr) - offset;

	/* read mlog data */
	rpma_connection_read(conn, dst, offset, src, offset, length);

	return RPMA_E_OK;
}

/*
 * proto_mlog_update_ack -- send MSG_TYPE_MLOG_UPDATE ACK
 */
int
proto_mlog_update_ack(struct rpma_connection *conn, void *uarg)
{
	struct mlog_update_args_t *args = uarg;

	struct msg_log *ml = args->cloc->ml_ptr;

	/* progress the mlog write pointer */
	ml_set_wptr(ml, args->wptr);

	/* prepare the mlog update ack */
	struct msg_t *ack;
	rpma_msg_get_ptr(conn, (void **)&ack);

	ack->base.type = MSG_TYPE_ACK;
	ack->ack.original_msg_type = MSG_TYPE_HELLO;
	ack->ack.status = 0;

	rpma_connection_send(conn, ack);

	return RPMA_E_OK;
}

/*
 * proto_bye_bye -- send MSG_TYPE_BYE_BYE
 */
int
proto_bye_bye(struct rpma_connection *conn, void *unused)
{
	struct msg_t *msg;
	rpma_msg_get_ptr(conn, (void **)&msg);

	msg->base.type = MSG_TYPE_BYE_BYE;
	rpma_connection_send(conn, msg);

	return RPMA_E_OK;
}

/*
 * proto_write_msg -- RDMA.write user name and message
 */
int
proto_write_msg_and_user(struct rpma_connection *conn, void *uarg)
{
	struct write_msg_args_t *args = uarg;

	size_t msg_row_offset = offsetof(struct client_row, msg);

	/* write the user name */
	size_t offset = offsetof(struct msg_row, user) + msg_row_offset;
	size_t length = sizeof(char) * (USER_NAME_MAX);
	rpma_connection_write(conn, args->dst, offset, args->src, offset,
			length);

	/* write the message */
	offset = offsetof(struct msg_row, msg) + msg_row_offset;
	length = sizeof(char) * (args->cr->msg.msg_size);
	rpma_connection_write(conn, args->dst, offset, args->src, offset,
			length);

	/* write the message length */
	offset = offsetof(struct msg_row, msg_size) + msg_row_offset;
	length = sizeof(args->cr->msg.msg_size);
	rpma_connection_write(conn, args->dst, offset, args->src, offset,
			length);

	rpma_connection_commit(conn);

	return RPMA_E_OK;
}

/*
 * proto_write_msg_status -- RDMA.ATOMIC_WRITE message status
 */
int
proto_write_msg_status(struct rpma_connection *conn, void *uarg)
{
	struct write_msg_args_t *args = uarg;

	/* write the client status */
	size_t offset = offsetof(struct client_row, status);
	size_t length = sizeof(args->cr->status);
	rpma_connection_atomic_write(conn, args->dst, offset, args->src, offset,
			length);
	rpma_connection_commit(conn);

	return RPMA_E_OK;
}

/*
 * proto_write_msg_signal -- signal the sequence has ended
 */
int
proto_write_msg_signal(struct rpma_connection *conn, void *uarg)
{
	struct write_msg_args_t *args = uarg;

	sem_post(args->done);

	return RPMA_E_OK;
}

/*
 * proto_ack_process -- process MSG_TYPE_ACK
 */
int
proto_ack_process(struct msg_t *msg, uint64_t *original_msg_type)
{
	if (msg->ack.status != 0)
		return msg->ack.status;

	*original_msg_type = msg->ack.original_msg_type;

	return RPMA_E_OK;
}

/*
 * proto_common_init -- prepare RPMA zone
 */
void
proto_common_init(struct rpma_zone **zone_ptr, const char *addr,
		const char *service)
{
	/* prepare RPMA configuration */
	struct rpma_config *cfg;
	rpma_config_new(&cfg);
	rpma_config_set_addr(cfg, addr);
	rpma_config_set_service(cfg, service);
	rpma_config_set_msg_size(cfg, sizeof(struct msg_t));
	rpma_config_set_send_queue_length(cfg, SEND_Q_LENGTH);
	rpma_config_set_recv_queue_length(cfg, RECV_Q_LENGTH);
	rpma_config_set_queue_alloc_funcs(cfg, malloc, free);

	/* allocate RPMA zone */
	struct rpma_zone *zone;
	rpma_zone_new(cfg, &zone);

	/* delete RPMA configuration */
	rpma_config_delete(&cfg);

	*zone_ptr = zone;
}

/*
 * proto_common_fini -- delete RPMA zone
 */
void
proto_common_fini(struct rpma_zone *zone)
{
	rpma_zone_delete(&zone);
}

/*
 * proto_client_init -- initialize client's RPMA resources
 */
void
proto_client_init(struct rpma_zone *zone, struct client_local_t *cloc,
		struct mlog_update_args_t *mlog_update_args)
{
	/* register local memory regions */
	rpma_memory_local_new(zone, cloc->ml_ptr,
			MSG_LOG_SIZE(cloc->ml_ptr->capacity),
			RPMA_MR_READ_DST, &cloc->ml);
	rpma_memory_local_new(zone, cloc->cr_ptr, sizeof(*cloc->cr_ptr),
			RPMA_MR_WRITE_SRC, &cloc->cr);

	/* allocate mlog update sequence */
	struct rpma_sequence *seq;
	rpma_sequence_new(&seq);
	rpma_sequence_add_step(seq, proto_mlog_update_read, mlog_update_args);
	rpma_sequence_add_step(seq, proto_mlog_update_ack, mlog_update_args);
	cloc->mlog_update_seq = seq;
}

/*
 * proto_client_fini -- delete client's RPMA resources
 */
void
proto_client_fini(struct client_local_t *cloc)
{
	/* deallocate sequence */
	rpma_sequence_delete(&cloc->mlog_update_seq);

	/* deallocate local memory regions */
	rpma_memory_local_delete(&cloc->cr);
	rpma_memory_local_delete(&cloc->ml);
}

/*
 * proto_writer_init -- allocate write msg sequence
 */
void
proto_writer_init(struct rpma_sequence **seq_ptr,
		struct write_msg_args_t *write_msg_args)
{
	struct rpma_sequence *seq;

	rpma_sequence_new(&seq);
	rpma_sequence_add_step(seq, proto_write_msg_and_user, write_msg_args);
	rpma_sequence_add_step(seq, proto_write_msg_status, write_msg_args);
	rpma_sequence_add_step(seq, proto_write_msg_signal, write_msg_args);
	*seq_ptr = seq;
}

/*
 * proto_writer_fini -- deallocate write msg sequence
 */
void
proto_writer_fini(struct rpma_sequence **seq_ptr)
{
	rpma_sequence_delete(seq_ptr);
}
