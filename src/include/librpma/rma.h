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
 * librpma/rma.h -- base definitions of librpma RMA entry points (EXPERIMENTAL)
 *
 * This library provides low-level support for remote access to persistent
 * memory utilizing RDMA-capable RNICs.
 *
 * See librpma(7) for details.
 */

#ifndef LIBRPMA_RMA_H
#define LIBRPMA_RMA_H 1

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <librpma/base.h>
#include <librpma/memory.h>
#include <librpma/rma_async.h>

inline int
rpma_connection_read(struct rpma_connection *conn,
		Rpma_memory *dst, size_t dst_off,
		Rpma_rmemory *src, size_t src_off, size_t length)
{
	int ret = rpma_connection_read_async(conn, dst, dst_off, src, src_off, length);
	if (ret)
		return ret;

	return rpma_connection_read_wait(conn, dst);
}

inline int
rpma_connection_write_and_commit(struct rpma_connection *conn,
		Rpma_rmemory *dst, size_t dst_off,
		Rpma_memory *src, size_t src_off, size_t length)
{
	int ret = rpma_connection_write_async(conn,
			dst, dst_off, src, src_off, length, 0);
	if (ret)
		return ret;

	ret = rpma_connection_commit_async(conn);
	if (ret)
		return ret;

	return rpma_connection_commit_wait(conn);
}

#ifdef __cplusplus
}
#endif
#endif	/* librpma/base.h */
