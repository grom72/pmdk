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
 * fi_test.c -- basic libfabric test
 */

#include "unittest.h"

#include <rdma/fabric.h>

#define TEST_PROVIDER "sockets"
#define TEST_FIVERSION FI_VERSION(1, 4)

/*
 * test_fi_getinfo -- XXX
 */
static void
test_fi_getinfo(const char *node, const char *service)
{
	/* build hints */
	struct fi_info *hints = fi_allocinfo();
	UT_ASSERTne(hints, NULL);

	hints->ep_attr->type = FI_EP_MSG;
	hints->domain_attr->mr_mode = FI_MR_BASIC;
	hints->domain_attr->threading = FI_THREAD_SAFE;
	hints->caps = FI_MSG | FI_RMA;
	hints->mode = FI_CONTEXT | FI_LOCAL_MR | FI_RX_CQ_DATA;
	hints->tx_attr->msg_order = FI_ORDER_RAW | FI_ORDER_SAW;
	hints->addr_format = FI_SOCKADDR;
	hints->fabric_attr->prov_name = strdup(TEST_PROVIDER);
	UT_ASSERTne(hints->fabric_attr->prov_name, NULL);

	hints->tx_attr->size = 1;
	hints->rx_attr->size = 1;

	/* get fabric interface information */
	struct fi_info *info;
	int ret = fi_getinfo(TEST_FIVERSION, node, service, 0, hints, &info);
	UT_ASSERTeq(ret, 0);

	/* cleanup */
	fi_freeinfo(hints);
	fi_freeinfo(info);
}

int
main(int argc, char *argv[])
{
	if (argc < 3) {
		printf("usage: %s <node> <service>", argv[0]);
		return 1;
	}

	const char *node = argv[1];
	const char *service = argv[2];

	test_fi_getinfo(node, service);

	return 0;
}
