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
 * librpma/memory.h -- base definitions of librpma memory management (EXPERIMENTAL)
 *
 * This library provides low-level support for remote access to persistent
 * memory utilizing RDMA-capable RNICs.
 *
 * See librpma(7) for details.
 */

#ifndef LIBRPMA_MEMORY_H
#define LIBRPMA_MEMORY_H 1

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* local memory region */

struct rpma_memory_local;

typedef struct rpma_memory_local Rpma_memory;

#define RPMA_MR_READ_SRC	(1 << 0)
#define RPMA_MR_READ_DST	(1 << 1)
#define RPMA_MR_WRITE_SRC	(1 << 2)
#define RPMA_MR_WRITE_DST	(1 << 3)

int rpma_memory_local_new(struct rpma_zone *zone, void *ptr, size_t size, int usage,
		Rpma_memory **mem);

int rpma_memory_local_get_ptr(Rpma_memory *mem, void **ptr);

int rpma_memory_local_get_size(Rpma_memory *mem, size_t *size);

struct rpma_memory_id {
	uint64_t data[4];
};

int rpma_memory_local_get_id(Rpma_memory *mem, struct rpma_memory_id *id);

int rpma_memory_local_delete(Rpma_memory **mem);

/* remote memory region */

struct rpma_memory_remote;

typedef struct rpma_memory_remote Rpma_rmemory;

int rpma_memory_remote_new(struct rpma_zone *zone, struct rpma_memory_id *id, Rpma_rmemory **rmem);

int rpma_memory_remote_get_size(Rpma_rmemory *rmem, size_t *size);

int rpma_memory_remote_delete(Rpma_rmemory **rmem);

#ifdef __cplusplus
}
#endif
#endif	/* librpma/memory.h */
