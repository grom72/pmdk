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
 * librpma.h -- definitions of librpma entry points (EXPERIMENTAL)
 *
 * This library provides low-level support for remote access to persistent
 * memory utilizing RDMA-capable RNICs.
 *
 * See librpma(7) for details.
 */

#ifndef LIBRPMA_H
#define LIBRPMA_H 1

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

int rpma_config_delete(struct rpma_config *cfg);

/* domain setup */

struct rpma_domain;

int rpma_domain_new(struct rpma_config *cfg, struct rpma_domain **dom);

int rpma_domain_delete(struct rpma_domain *dom);

/* memory region setup */

#define RPMA_MR_SEND	(1 << 0)
#define RPMA_MR_RECV	(1 << 1)
#define RPMA_MR_READ	(1 << 2)
#define RPMA_MR_WRITE	(1 << 3)
#define RPMA_MR_REMOTE_READ		(1 << 4)
#define RPMA_MR_REMOTE_WRITE	(1 << 5)

struct rpma_mr;

int rpma_mr_new(struct rpma_domain *dom, void *ptr, size_t length, int usage,
		struct rpma_mr **mr);

int rpma_mr_get_ptr(struct rpma_mr *mr, void **ptr);

int rpma_mr_get_size(struct rpma_mr *mr, size_t *size);

int rpma_mr_delete(struct rpma_mr *mr);

/* remote memory region setup */

struct rpma_rmr;

/* XXX probably details have to be kept internally but the size has to be known */
struct rpma_mr_packed {
	uint64_t raddr;
	uint64_t rkey;
	size_t size;
};

int rpma_mr_pack(struct rpma_mr *mr, struct rpma_mr_packed *packed, size_t *size);

int rpma_rmr_unpack_new(struct rpma_mr_packed *packed, size_t size, struct rpma_rmr **rmr);

int rpma_rmr_get_size(struct rpma_rmr *rmr, size_t *size);

int rpma_rmr_delete(struct rpma_rmr *rmr);

/* connection setup */

struct rpma_conn;

int rpma_listen(struct rpma_domain *dom);

int rpma_conn_new(struct rpma_domain *dom, struct rpma_conn **conn);

int rpma_conn_cq(struct rpma_conn *conn);

int rpma_conn_wait_for_shutdown(struct rpma_conn *conn);

int rpma_conn_accept(struct rpma_conn *conn);

int rpma_conn_connect(struct rpma_conn *conn);

int rpma_conn_delete(struct rpma_conn *conn);

/* messaging */

int rpma_conn_send(struct rpma_conn *conn, struct rpma_mr *send);

int rpma_conn_recv(struct rpma_conn *conn, struct rpma_mr *recv);

typedef int (*rpma_onrecv_fn)(struct rpma_conn *conn, struct rpma_mr *recv, void *context);

int rpma_conn_set_onrecv(struct rpma_conn*conn, rpma_onrecv_fn onrecv, void *context);

/* direct memory access */

int rpma_conn_read(struct rpma_conn *conn, struct rpma_mr *dst, size_t dst_off,
		struct rpma_rmr *src, size_t src_off, size_t length);

int rpma_conn_write(struct rpma_conn *conn, struct rpma_rmr *dst, size_t dst_off,
		struct rpma_mr *src, size_t src_off, size_t length);

int rpma_conn_commit(struct rpma_conn *conn);

/* versioning and error handling */

const char *rpma_check_version(unsigned major_required,
		unsigned minor_required);

const char *rpma_errormsg(void);

#ifdef __cplusplus
}
#endif
#endif	/* librpma.h */
