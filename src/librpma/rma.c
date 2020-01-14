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
 * rma.c -- entry points for librpma RMA
 */

#include <errno.h>
#include <rdma/fi_rma.h>

#include "alloc.h"
#include "connection.h"
#include "memory.h"
#include "rpma_utils.h"
#include "zone.h"

#define RAW_BUFF_SIZE 4096
#define RAW_SIZE 8

static int
raw_buffer_init(struct rpma_connection *conn)
{
	ASSERT(IS_PAGE_ALIGNED(RAW_BUFF_SIZE));

	void *ptr;
	errno = posix_memalign((void **)&ptr, Pagesize, RAW_BUFF_SIZE);
	if (errno)
		return RPMA_E_ERRNO;

	int ret = rpma_memory_local_new(conn->zone, ptr, RAW_BUFF_SIZE,
			RPMA_MR_READ_DST, &conn->raw_dst);
	if (ret)
		goto err_mem_local_new;

	return 0;

err_mem_local_new:
	Free(ptr);
	return ret;
}

static int
raw_buffer_fini(struct rpma_connection *conn)
{
	void *ptr;
	int ret = rpma_memory_local_get_ptr(conn->raw_dst, &ptr);
	if (ret)
		return ret;

	ret = rpma_memory_local_delete(&conn->raw_dst);
	if (ret)
		return ret;

	Free(ptr);

	return 0;
}

int
rpma_connection_rma_init(struct rpma_connection *conn)
{
	/* initialize RMA msg */
	struct fi_msg_rma *rma = &conn->rma.msg;
	conn->rma.flags = 0;
	rma->desc = &conn->rma.desc;
	rma->addr = 0;
	rma->context = NULL; /* XXX */
	rma->rma_iov = &conn->rma.rma_iov;
	rma->iov_count = 1;
	rma->msg_iov = &conn->rma.msg_iov;
	rma->iov_count = 1;

	return raw_buffer_init(conn);
}

int
rpma_connection_rma_fini(struct rpma_connection *conn)
{
	return raw_buffer_fini(conn);
}

int
rpma_connection_read(struct rpma_connection *conn,
		struct rpma_memory_local *dst, size_t dst_off,
		struct rpma_memory_remote *src, size_t src_off, size_t length)
{
	ASSERT(length < conn->zone->info->ep_attr->max_msg_size); /* XXX */

	/* XXX WQ flush */

	void *dst_addr = (void *)((uintptr_t)dst->ptr + dst_off);;

	struct fi_msg_rma *rma = &conn->rma.msg;
	rma->desc = dst->desc;
	rma->context = dst_addr; /* XXX */
	conn->rma.rma_iov.addr = src->raddr + src_off;
	conn->rma.rma_iov.len = length;
	conn->rma.msg_iov.iov_base = dst_addr;
	conn->rma.msg_iov.iov_len = length;

	uint64_t flags = conn->rma.flags;
	flags |= FI_COMPLETION;

	int ret = (int)fi_readmsg(conn->ep, rma, flags);
	if (ret) {
		ERR_FI(ret, "fi_readmsg");
		return ret;
	}

	ret = rpma_connection_cq_wait(conn, FI_READ, dst_addr);
	if (ret)
		return ret;

	return 0;
}

int
rpma_connection_write(struct rpma_connection *conn,
		struct rpma_memory_remote *dst, size_t dst_off,
		struct rpma_memory_local *src, size_t src_off, size_t length)
{
	ASSERT(length < conn->zone->info->ep_attr->max_msg_size); /* XXX */

	/* XXX WQ flush */

	struct rpma_rma *rma = &conn->rma;
	struct fi_msg_rma *msg = &rma->msg;
	rma->rma_iov.addr = dst->raddr + dst_off;
	rma->rma_iov.len = length;
	rma->msg_iov.iov_base = (void *)((uintptr_t)src->ptr + src_off);
	rma->msg_iov.iov_len = length;

	int ret = (int)fi_writemsg(conn->ep, msg, conn->rma.flags);
	if (ret) {
		ERR_FI(ret, "fi_writemsg");
		return (int)ret;
	}

	/* XXX */
	conn->raw_src = dst;

	return 0;
}

int
rpma_connection_atomic_write(struct rpma_connection *conn,
		struct rpma_memory_remote *dst, size_t dst_off,
		struct rpma_memory_local *src, size_t src_off, size_t length)
{
	/* XXX */
	return rpma_connection_write(conn, dst, dst_off, src, src_off, length);
}

int
rpma_connection_commit(struct rpma_connection *conn)
{
	return rpma_connection_read(conn, conn->raw_dst, 0, conn->raw_src, 0, RAW_SIZE);
}
