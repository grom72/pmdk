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
 * manpage.c -- example from librpma manpage
 */

#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

#include <librpma.h>

#include "manpage_common.h"

#define PATTERN ('c')

struct context {
	struct rpma_rmr *server_pool;
	sem_t sem;
};

static int
onrecv(struct rpma_conn *conn, struct rpma_mr *recv, void *context)
{
	void *ptr;
	size_t size;

	if (rpma_mr_get_ptr(recv, &ptr) != RPMA_E_OK)
		goto err;

	if (rpma_mr_get_size(recv, &size) != RPMA_E_OK)
		goto err;

	if (size < sizeof(struct rpma_mr_packed))
		goto err;

	struct context *ctx = (struct context *)context;
	if (rpma_rmr_unpack_new(ptr, size, &ctx->server_pool) != RPMA_E_OK)
		goto err;

	sem_post(&ctx->sem);

	return 0;

err:
	return -1;
}

int
main(int argc, char *argv[])
{
	const char *addr, *service;
	parse_args(argc, argv, &addr, &service);

	/* prepare a synchronization object */
	struct context ctx;
	if (sem_init(&ctx.sem, 0, 0) != 0)
		exit(1);

	/* setup the domain */
	struct rpma_config *cfg;
	struct rpma_domain *dom;
	if (rpma_config_new(&cfg) != RPMA_E_OK)
		goto err_config;

	rpma_config_set_addr(cfg, addr);
	rpma_config_set_service(cfg, service);

	if (rpma_domain_new(cfg, &dom) != RPMA_E_OK)
		goto err_domain;

	/* prepare the memory regions for RMA and messaging */
	struct rpma_mr *pool_mr;
	struct rpma_mr *msg_mr;

	size_t pool_size = roundup(POOL_SIZE, ALIGNMENT);
	size_t msg_size = roundup(sizeof(struct rpma_mr_packed), ALIGNMENT);

	void *pool = alloc_memory(pool_size + msg_size);
	struct rpma_mr_packed *msg = (struct rpma_mr_packed *)((char *)pool + pool_size);

	if (rpma_mr_new(dom, pool, pool_size, RPMA_MR_WRITE,
			&pool_mr) != RPMA_E_OK)
		goto err_pool_mr;

	if (rpma_mr_new(dom, msg, msg_size, RPMA_MR_RECV,
			&msg_mr) != RPMA_E_OK)
		goto err_msg_mr;

	/* create the connection */
	struct rpma_conn *conn;
	if (rpma_conn_new(dom, &conn) != RPMA_E_OK)
		goto err_conn_new;

	/* prepare for receiving the message from the server */
	if (rpma_conn_recv(conn, msg_mr) != RPMA_E_OK)
		goto err_recv;

	if (rpma_conn_set_onrecv(conn, onrecv, &ctx) != RPMA_E_OK)
		goto err_set_onrecv;

	/* connect to the server */
	if (rpma_conn_connect(conn) != RPMA_E_OK)
		goto err_connect;

	/* wait for the message from the server */
	if (sem_wait(&ctx.sem) != 0)
		goto err_sem_wait;

	/* perform write to the remote memory pool */
	memset(pool, PATTERN, pool_size);
	size_t size;
	if (rpma_rmr_get_size(ctx.server_pool, &size) != RPMA_E_OK)
		goto err_get_size;
	size = MIN(size, pool_size);
	if (rpma_conn_write(conn, ctx.server_pool, 0, pool_mr, 0, size) != RPMA_E_OK)
		goto err_write;

	if (rpma_conn_commit(conn) != RPMA_E_OK)
		goto err_commit;

	/* cleanup */
	rpma_rmr_delete(ctx.server_pool);
	rpma_conn_delete(conn);
	rpma_mr_delete(msg_mr);
	rpma_mr_delete(pool_mr);
	free(pool);
	rpma_domain_delete(dom);
	rpma_config_delete(cfg);
	sem_destroy(&ctx.sem);

	return 0;

err_commit:
err_write:
err_get_size:
err_sem_wait:
	rpma_rmr_delete(ctx.server_pool);
err_connect:
err_set_onrecv:
err_recv:
	rpma_conn_delete(conn);

err_conn_new:
	rpma_mr_delete(msg_mr);

err_msg_mr:
	rpma_mr_delete(pool_mr);

err_pool_mr:
	free(pool);
	rpma_domain_delete(dom);

err_domain:
	rpma_config_delete(cfg);

err_config:
	sem_destroy(&ctx.sem);
	return 1;
}
