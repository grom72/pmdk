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

#ifndef LIBRPMA_MR_H
#define LIBRPMA_MR_H 1

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* memory region setup */

struct rpma_mr; /* local memory region */
struct rpma_rmr; /* remote memory region */

#define RPMA_MR_READ_SRC	(1 << 0)
#define RPMA_MR_READ_DST	(1 << 1)
#define RPMA_MR_WRITE_SRC	(1 << 2)
#define RPMA_MR_WRITE_DST	(1 << 3)

int rpma_mr_new(struct rpma_ctx *ctx, void *ptr, size_t size, int usage,
		struct rpma_mr **mr);

int rpma_mr_get_ptr(struct rpma_mr *mr, void **ptr);

int rpma_mr_get_size(struct rpma_mr *mr, size_t *size);

int rpma_mr_delete(struct rpma_mr *mr);

int rpma_rmr_get_size(struct rpma_rmr *rmr, size_t *size);

int rpma_rmr_delete(struct rpma_rmr *rmr);

/* packing / unpacking memory regions */

struct rpma_mr_packed { /* memory region packed for transfer time */
	uint64_t raddr;
	uint64_t rkey;
	size_t size;
};

int rpma_mr_pack(struct rpma_mr *mr, struct rpma_mr_packed *packed, size_t *size);

int rpma_rmr_unpack_new(struct rpma_ctx *ctx, struct rpma_mr_packed *packed, size_t size, struct rpma_rmr **rmr);

#ifdef __cplusplus
}
#endif
#endif	/* librpma/base.h */
