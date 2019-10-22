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
 * librpma/base.h -- base definitions of librpma entry points (EXPERIMENTAL)
 *
 * This library provides low-level support for remote access to persistent
 * memory utilizing RDMA-capable RNICs.
 *
 * See librpma(7) for details.
 */

#ifndef LIBRPMA_BASE_H
#define LIBRPMA_BASE_H 1

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RPMA_E_OK			0
#define RPMA_E_EXTERNAL		1
#define RPMA_E_NOSUPP		2

/* config setup */

struct rpma_config;

int rpma_config_new(struct rpma_config **cfg);

int rpma_config_set_addr(struct rpma_config *cfg, const char *addr);

int rpma_config_set_service(struct rpma_config *cfg, const char *service);

int rpma_config_delete(struct rpma_config **cfg);

/* context setup */

struct rpma_ctx;

int rpma_ctx_new(struct rpma_config *cfg, struct rpma_ctx **ctx);

int rpma_ctx_delete(struct rpma_ctx **ctx);

/* connection setup */

struct rpma_conn;

int rpma_listen(struct rpma_ctx *ctx);

int rpma_conn_new(struct rpma_ctx *ctx, struct rpma_conn **conn);

int rpma_conn_wait_for_shutdown(struct rpma_conn *conn);

int rpma_conn_accept(struct rpma_conn *conn);

int rpma_conn_connect(struct rpma_conn *conn);

int rpma_conn_delete(struct rpma_conn **conn);

/* error handling */

const char *rpma_errormsg(void);

#ifdef __cplusplus
}
#endif
#endif	/* librpma/base.h */
