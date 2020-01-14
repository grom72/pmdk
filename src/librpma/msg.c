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
 * msg.c -- entry points for librpma MSG
 */

#include <errno.h>
#include <rdma/fi_endpoint.h>

#include "alloc.h"
#include "connection.h"
#include "memory.h"
#include "util.h"
#include "rpma_utils.h"

static int
msg_queue_init(struct rpma_connection *conn, size_t queue_length,
		uint64_t access, struct rpma_memory_local **buff)
{
	size_t buff_size = conn->zone->msg_size * queue_length;
	buff_size = ALIGN_UP(buff_size, Pagesize);

	void *ptr;
	errno = posix_memalign((void **)&ptr, Pagesize, buff_size);
	if (errno)
		return RPMA_E_ERRNO;

	int ret = rpma_memory_local_new_internal(conn->zone, ptr, buff_size,
			access, buff);
	if (ret)
		goto err_mem_local_new;

	return 0;

err_mem_local_new:
	Free(ptr);
	return ret;
}

/* XXX make it common with raw_buffer_init/_fini ? */
static int
msg_queue_fini(struct rpma_connection *conn, struct rpma_memory_local **buff)
{
	void *ptr;
	int ret = rpma_memory_local_get_ptr(*buff, &ptr);
	if (ret)
		return ret;

	ret = rpma_memory_local_delete(buff);
	if (ret)
		return ret;

	Free(ptr);

	return 0;
}

static void
msg_init(struct fi_msg *msg, void **desc, const struct iovec *msg_iov)
{
	msg->msg_iov = msg_iov;
	msg->desc = desc;
	msg->iov_count = 1;
	msg->addr = 0;
	msg->context = NULL; /* XXX */
	msg->data = 0;
}

int
rpma_connection_msg_init(struct rpma_connection *conn)
{
	int ret;

	conn->send_buff_id = 0;

	ret = msg_queue_init(conn, conn->zone->send_queue_length, FI_SEND,
			&conn->send_buff);
	if (ret)
		return ret;

	ret = msg_queue_init(conn, conn->zone->recv_queue_length, FI_RECV,
			&conn->recv_buff);
	if (ret)
		goto err_recv_queue_init;

	/* initialize msgs */
	msg_init(&conn->send.msg, &conn->send_buff->desc, &conn->send.iov);
	msg_init(&conn->recv.msg, &conn->recv_buff->desc, &conn->recv.iov);
	conn->send.flags = FI_COMPLETION;
	conn->recv.flags = FI_COMPLETION;

	return 0;

err_recv_queue_init:
	(void)msg_queue_fini(conn, &conn->send_buff);
	return ret;
}

int
rpma_connection_msg_fini(struct rpma_connection *conn)
{
	int ret;
	ret = msg_queue_fini(conn, &conn->recv_buff);
	if (ret)
		return ret;

	ret = msg_queue_fini(conn, &conn->send_buff);
	if (ret)
		return ret;

	return 0;
}

int
rpma_msg_get_ptr(struct rpma_connection *conn, void **ptr)
{
	void *buff;
	int ret = rpma_memory_local_get_ptr(conn->send_buff, &buff);
	if (ret)
		return ret;

	uint64_t buff_id = conn->send_buff_id;
	conn->send_buff_id = (buff_id + 1) % conn->zone->send_queue_length;

	buff = (void *)((uintptr_t)buff + buff_id * conn->zone->msg_size);

	*ptr = buff;

	return 0;
}

int
rpma_connection_send(struct rpma_connection *conn, void *ptr)
{
	struct rpma_msg *send = &conn->send;

	send->iov.iov_base = ptr;
	send->iov.iov_len = conn->zone->msg_size;
	send->msg.context = ptr;

	int ret = (int)fi_sendmsg(conn->ep, &send->msg, send->flags);
	if (ret) {
		ERR_FI(ret, "fi_sendmsg");
		return ret;
	}

	ret = rpma_connection_cq_wait(conn, FI_SEND, ptr);
	if (ret)
		return ret;

	return 0;
}

int
rpma_connection_recv_post(struct rpma_connection *conn, void *ptr)
{
	struct rpma_msg *recv = &conn->recv;

	recv->iov.iov_base = ptr;
	recv->iov.iov_len = conn->zone->msg_size;
	recv->msg.context = ptr;

	int ret = (int)fi_recvmsg(conn->ep, &recv->msg, recv->flags);
	if (ret) {
		ERR_FI(ret, "fi_recvmsg");
		return ret;
	}

	return 0;
}
