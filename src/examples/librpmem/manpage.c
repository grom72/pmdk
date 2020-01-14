/*
 * Copyright 2016-2019, Intel Corporation
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
 * manpage.c -- example from librpmem manpage
 */
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <librpmem.h>

#define POOL_SIGNATURE	"MANPAGE"
#define POOL_SIZE	(32 * 1024 * 1024)
#define NLANES		4

#define DATA_OFF	4096
#define DATA_SIZE	(POOL_SIZE - DATA_OFF)

static void
parse_args(int argc, char *argv[], const char **target, const char **poolset)
{
	if (argc < 3) {
		fprintf(stderr, "usage:\t%s <target> <poolset>\n", argv[0]);
		exit(1);
	}

	*target = argv[1];
	*poolset = argv[2];
}

static void *
alloc_memory()
{
	long pagesize = sysconf(_SC_PAGESIZE);
	if (pagesize < 0) {
		perror("sysconf");
		exit(1);
	}

	/* allocate a page size aligned local memory pool */
	void *mem;
	int ret = posix_memalign(&mem, pagesize, POOL_SIZE);
	if (ret) {
		fprintf(stderr, "posix_memalign: %s\n", strerror(ret));
		exit(1);
	}

	assert(mem != NULL);

	return mem;
}

int
main(int argc, char *argv[])
{
	const char *target, *poolset;
	parse_args(argc, argv, &target, &poolset);

	unsigned nlanes = NLANES;
	void *pool = alloc_memory();
	int ret;

	/* fill pool_attributes */
	struct rpmem_pool_attr pool_attr;
	memset(&pool_attr, 0, sizeof(pool_attr));
	strncpy(pool_attr.signature, POOL_SIGNATURE, RPMEM_POOL_HDR_SIG_LEN);

	/* remove old pool */
	if (rpmem_remove(target, poolset, 0)) {
		fprintf(stderr, "removing pool failed: %s\n", rpmem_errormsg());
//		return 1;
	}

	/* create a remote pool */
	RPMEMpool *rpp = rpmem_create(target, poolset, pool, POOL_SIZE,
			&nlanes, &pool_attr);
	if (!rpp) {
		fprintf(stderr, "rpmem_create: %s\n", rpmem_errormsg());
		return 1;
	}

	/* store data on local pool */
	memset(pool, 0, POOL_SIZE);

	/* make local data persistent on remote node */
#if 0
	ret = rpmem_persist(rpp, DATA_OFF, DATA_SIZE, 0, 0);
	if (ret) {
		fprintf(stderr, "rpmem_persist: %s\n", rpmem_errormsg());
		return 1;
	}
	ret = rpmem_persist(rpp, DATA_OFF, DATA_SIZE, 0, 0);
	if (ret) {
		fprintf(stderr, "rpmem_persist: %s\n", rpmem_errormsg());
		return 1;
	}
#endif
#if 1
fprintf(stderr, "before flush\r\n");
	{
		int ii;
//		for(ii = 1024*35; ii < DATA_SIZE; ii = ii+1024)
//		for(ii = 1024*34; ii < DATA_SIZE; ii = ii+1)
		for(ii = 35415; ii < DATA_SIZE; ii = ii+1)
		{
//			ret = rpmem_flush(rpp, DATA_OFF, ii, 0,RPMEM_FLUSH_RELAXED);
			ret = rpmem_flush(rpp, DATA_OFF, ii, 0,0);
fprintf(stderr, "after flush %d %d\r\n", ii/1024, ii);
			if (ret) {
                        fprintf(stderr, "rpmem_flush: %s\n", rpmem_errormsg());
                        return 1;
			}
			ret = rpmem_drain(rpp,0,0);
			if (ret) {
                        fprintf(stderr, "rpmem_drain: %s\n", rpmem_errormsg());
                        return 1;
			}
		}
	}
#endif
#if 0
        /* deep flush on target. NOT WORKING AT ALL!*/
      ret = rpmem_deep_persist(rpp, DATA_OFF, DATA_SIZE, 0);
        if (ret) {
                        fprintf(stderr, "rpmem_deep_persist: %s\n", rpmem_errormsg());
                        //return 1;
        }
#endif
	/* close the remote pool */
	ret = rpmem_close(rpp);
	if (ret) {
		fprintf(stderr, "rpmem_close: %s\n", rpmem_errormsg());
		return 1;
	}

	free(pool);

	return 0;
}
