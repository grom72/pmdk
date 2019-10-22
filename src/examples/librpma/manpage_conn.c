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
 * manpage_conn.c -- example from librpma manpage (establishing a connection)
 */

#include <stdlib.h>
#include <sys/param.h>

#include <librpma/base.h>

#include "manpage_common.h"

/*
 * Client side:
 * 1. Prepares rpma_config pointing the server (address and service)
 * 2. Creates rpma_ctx using the rpma_config
 * 3. Creates a new rpma_conn inside the rpma_ctx
 * 4. Establishes the rpma_conn connection
 * 5. Waits for 5 seconds before shutting down the connection
 *
 * Server side:
 * 1. Prepares rpma_config pointing where the server will be listening for the
 * incoming connections (address and service)
 * 2. Creates rpma_ctx using the rpma_config
 * 3. Starts listening for the incoming connections
 * 4. Creates a new rpma_conn inside the rpma_ctx
 * 5. Accepts the rpma_conn
 * 6. Waits for the rpma_conn to shutdown
 */

int
main(int argc, char *argv[])
{
	const char *addr, *service;
	int is_server = UNDEFINED;
	parse_args(argc, argv, &addr, &service, &is_server);

	/* prepare rpma_config and create the RPMA context */
	struct rpma_config *cfg;
	struct rpma_ctx *ctx;
	if (rpma_config_new(&cfg) != RPMA_E_OK)
		exit(1);

	rpma_config_set_addr(cfg, addr);
	rpma_config_set_service(cfg, service);

	if (rpma_ctx_new(cfg, &ctx) != RPMA_E_OK)
		goto err_ctx;

	struct rpma_conn *conn;
	if (is_server) {
		/* start listening for the incoming connections */
		if (rpma_listen(ctx) != RPMA_E_OK)
			goto err_listen;
	}

	/* create the connection */
	if (rpma_conn_new(ctx, &conn) != RPMA_E_OK)
		goto err_conn;

	if (is_server) {
		/* accept the incoming connection */
		if (rpma_conn_accept(conn) != RPMA_E_OK)
			goto err_accept;
	} else {
		/* connect to the server */
		if (rpma_conn_connect(conn) != RPMA_E_OK)
			goto err_connect;
	}

	if (is_server) {
		/* wait for the connection shutdown */
		if (rpma_conn_wait_for_shutdown(conn) != RPMA_E_OK)
			goto err_wait;
	} else {
		printf("Wait for 5 seconds...\n");
		sleep(5);
	}

	rpma_conn_delete(conn);
	rpma_ctx_delete(ctx);
	rpma_config_delete(cfg);

	return 0;

err_wait:
err_accept:
err_connect:
	rpma_conn_delete(conn);

err_conn:
err_listen:
	rpma_ctx_delete(ctx);

err_ctx:
	rpma_config_delete(cfg);
	return 1;
}
