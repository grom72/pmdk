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
 * manpage_server.c -- example from librpma manpage
 */

#include <stdlib.h>
#include <sys/param.h>

#include <librpma.h>

#include "manpage_common.h"

int
main(int argc, char *argv[])
{
	const char *addr, *service;
	parse_args(argc, argv, &addr, &service);

	/* setup the domain */
	struct rpma_config *cfg;
	struct rpma_domain *dom;
	if (rpma_config_new(&cfg) != RPMA_E_OK)
		exit(1);

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

	if (rpma_mr_new(dom, pool, pool_size, RPMA_MR_REMOTE_WRITE,
			&pool_mr) != RPMA_E_OK)
		goto err_pool_mr;

	if (rpma_mr_new(dom, msg, msg_size, RPMA_MR_SEND,
			&msg_mr) != RPMA_E_OK)
		goto err_msg_mr;

	/* prepare the message describing the local memory pool */
	size_t real_msg_size = msg_size;
	if (rpma_mr_pack(pool_mr, msg, &real_msg_size) != RPMA_E_OK)
		goto err_rmr;

	/* create the connection */
	struct rpma_conn *conn;
	if (rpma_listen(dom) != RPMA_E_OK)
		goto err_listen;

	if (rpma_conn_new(dom, &conn) != RPMA_E_OK)
		goto err_conn;

	if (rpma_conn_accept(conn) != RPMA_E_OK)
		goto err_accept;

	/* send the message to the client */
	if (rpma_conn_send(conn, msg_mr) != RPMA_E_OK)
		goto err_send;

	/* wait for the connection shutdown */
	if (rpma_conn_wait_for_shutdown(conn) != RPMA_E_OK)
		goto err_wait;

	rpma_conn_delete(conn);
	rpma_mr_delete(msg_mr);
	rpma_mr_delete(pool_mr);
	free(pool);
	rpma_domain_delete(dom);
	rpma_config_delete(cfg);

	return 0;

err_wait:
err_send:
err_accept:
	rpma_conn_delete(conn);

err_conn:
err_listen:
err_rmr:
	rpma_mr_delete(msg_mr);

err_msg_mr:
	rpma_mr_delete(pool_mr);

err_pool_mr:
	free(pool);
	rpma_domain_delete(dom);

err_domain:
	rpma_config_delete(cfg);
}
