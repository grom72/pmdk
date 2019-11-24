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

#include <librpma.h>

#include "pstructs.h"
#include "protocol.h"

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
	struct mlog_update_args_t args = uarg;
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
 * proto_mlog_update_read -- schedule RDMA.read of the updated part of the mlog
 */
int
proto_mlog_update_read(struct rpma_connection *conn, void *uarg)
{
	struct client_ctx *clnt;
	rpma_connection_get_custom_data(conn, (void **)&clnt);

	struct mlog_update_args_t args = uarg;

	struct msg_log *ml = &clnt->root->ml;

	/* calculate remote read parameters */
	uintptr_t wptr = ml_get_wptr(ml);
	size_t offset = ml_offset(ml, wptr);
	size_t length = ml_offset(ml, args.wptr) - offset;

	/* read mlog data */
	rpma_connection_read(conn, clnt->ml_local, offset, clnt->ml_remote,
			offset, length);

	return RPMA_E_OK;
}

/*
 * proto_mlog_update_ack -- send MSG_TYPE_MLOG_UPDATE ACK
 */
int
proto_mlog_update_ack(struct rpma_connection *conn, void *uarg)
{
	struct client_ctx *clnt;
	rpma_connection_get_custom_data(conn, (void **)&clnt);

	struct mlog_update_args_t args = uarg;

	struct msg_log *ml = &clnt->root->ml;

	/* progress the mlog write pointer */
	ml_set_wptr(ml, args.wptr);

	/* prepare the mlog update ack */
	struct msg_t *ack;
	rpma_msg_get_ptr(conn, (void **)&ack);

	ack->base = MSG_TYPE_ACK;
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
	struct proto_write_msg_args_t *args = uarg;

	/* write the user name*/
	size_t offset = offsetof(struct client_row, user);
	size_t length = sizeof(char) * (USER_NAME_MAX);
	rpma_connection_write(conn, args->dst, offset, args->src, offset, length);

	/* write the message */
	offset = offsetof(struct client_row, msg);
	length = sizeof(char) * (args->cr->msg_size);
	rpma_connection_write(conn, args->dst, offset, args->src, offset, length);

	/* write the message length */
	offset = offsetof(struct client_row, msg_size);
	length = sizeof(args->cr->msg_size);
	rpma_connection_write(conn, args->dst, offset, args->src, offset, length);

	rpma_connection_commit(conn);

	return RPMA_E_OK;
}

/*
 * proto_write_msg_status -- RDMA.ATOMIC_WRITE message status
 */
int
proto_write_msg_status(struct rpma_connection *conn, void *uarg)
{
	struct proto_write_msg_args_t *args = uarg;

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
	struct proto_write_msg_args_t *args = uarg;

	sem_post(args->done);

	return RPMA_E_OK;
}
