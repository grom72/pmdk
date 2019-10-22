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
 * manpage_common.c -- example from librpma manpage
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "manpage_common.h"

void
parse_args(int argc, char *argv[], const char **addr, const char **service,
		int *is_server)
{
	if (argc < 3) {
		fprintf(stderr, "usage:\t%s <addr> <service> (<is_server>)\n",
				argv[0]);
		exit(1);
	}

	*addr = argv[1];
	*service = argv[2];
	if (argc == 4)
		*is_server = atoi(argv[3]);
	else
		*is_server = UNDEFINED;
}

void *
alloc_memory(size_t size)
{
	long alignment = sysconf(_SC_PAGESIZE);
	if (alignment < 0) {
		perror("sysconf");
		exit(1);
	}

	void *mem;
	int ret = posix_memalign(&mem, alignment, POOL_SIZE);
	if (ret) {
		fprintf(stderr, "posix_memalign: %s\n", strerror(ret));
		exit(1);
	}
	assert(mem != NULL);

	return mem;
}
