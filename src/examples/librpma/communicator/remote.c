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
 * remote.c -- common communication parts for librpma-based communicator
 */

#include <stdlib.h>

#define SEND_Q_LENGTH 10
#define RECV_Q_LENGTH 10

/*
 * remote_init -- prepare RPMA zone
 */
void
remote_init(struct rpma_zone **zone_ptr, const char *addr, const char *service, size_t msg_size)
{
	/* prepare RPMA configuration */
	struct rpma_config *cfg;
	rpma_config_new(&cfg);
	rpma_config_set_addr(cfg, addr);
	rpma_config_set_service(cfg, service);
	rpma_config_set_msg_size(cfg, msg_size);
	rpma_config_set_send_queue_length(cfg, SEND_Q_LENGTH);
	rpma_config_set_recv_queue_length(cfg, RECV_Q_LENGTH);
	rpma_config_set_queue_alloc_funcs(cfg, malloc, free);

	/* allocate RPMA zone */
	struct rpma_zone *zone;
	rpma_zone_new(cfg, &zone);

	/* delete RPMA configuration */
	rpma_config_delete(&cfg);

	*zone_ptr = zone;
}

/*
 * remote_fini -- delete RPMA zone
 */
void
remote_fini(struct rpma_zone *zone)
{
	rpma_zone_delete(&zone);
}
