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
 * config.c -- entry points for librpma config
 */

#include <string.h>

#include <librpma.h>

#include "config.h"
#include "rpma_utils.h"
#include "alloc.h"

#define RPMA_DEFAULT_MSG_SIZE 30
#define RPMA_DEFAULT_QUEUE_LENGTH 10

static void
config_init(struct rpma_config *cfg)
{
	cfg->addr = NULL;
	cfg->service = NULL;
	cfg->msg_size = RPMA_DEFAULT_MSG_SIZE;
	cfg->send_queue_length = RPMA_DEFAULT_QUEUE_LENGTH;
	cfg->recv_queue_length = RPMA_DEFAULT_QUEUE_LENGTH;
	cfg->malloc = _Malloc;
	cfg->free = Free;
}

int
rpma_config_new(struct rpma_config **cfg)
{
	struct rpma_config *ptr = Malloc(sizeof(struct rpma_config));
	if (!ptr)
		return RPMA_E_ERRNO;

	config_init(ptr);

	*cfg = ptr;

	return 0;
}

int
rpma_config_set_addr(struct rpma_config *cfg, const char *addr)
{
	cfg->addr = strdup(addr);

	if (!cfg->addr)
		return RPMA_E_ERRNO;

	return 0;
}

int
rpma_config_set_service(struct rpma_config *cfg, const char *service)
{
	cfg->service = strdup(service);

	if (!cfg->service)
		return RPMA_E_ERRNO;

	return 0;
}

int
rpma_config_set_msg_size(struct rpma_config *cfg, size_t msg_size)
{
	cfg->msg_size = msg_size;
	return 0;
}

int
rpma_config_set_send_queue_length(struct rpma_config *cfg, uint64_t queue_len)
{
	cfg->send_queue_length = queue_len;
	return 0;
}

int
rpma_config_set_recv_queue_length(struct rpma_config *cfg, uint64_t queue_len)
{
	cfg->recv_queue_length = queue_len;
	return 0;
}

int
rpma_config_set_queue_alloc_funcs(struct rpma_config *cfg,
		rpma_malloc_func malloc_func, rpma_free_func free_func)
{
	cfg->malloc = malloc_func;
	cfg->free = free_func;
	return 0;
}

int
rpma_config_set_flags(struct rpma_config *cfg, unsigned flags)
{
	/* XXX verify if all flags are valid */
	cfg->flags = flags;
	return 0;
}

int
rpma_config_delete(struct rpma_config **cfg)
{
	struct rpma_config *ptr = *cfg;

	if (ptr->addr)
		Free(ptr->addr);

	if (ptr->service)
		Free(ptr->service);

	Free(ptr);
	*cfg = NULL;

	return 0;
}

