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
 * pstructs.c -- persistent ops for librpma-based communicator
 */

#include <assert.h>
#include <fcntl.h>

#include <libpmem.h>

#include "pstructs.h"

/*
 * pmem_restore -- (internal) restore from the persistent state
 */
static void
pmem_restore(struct root_obj *root)
{
	// XXX
}

/*
 * pmem_init -- map the server root object
 */
void
pmem_init(struct root_obj **root_ptr, const char *path, size_t min_size)
{
	size_t total_size = 0;
	struct root_obj *root = pmem_map_file(path, min_size,
			PMEM_FILE_CREATE, O_RDWR, &total_size, NULL);
	if (!root->total_size) {
		// initialize persistent state
		const size_t ml_offset = offsetof(struct root_obj, ml);
		size_t ml_size = total_size - ml_offset;
		ml_init(&root->ml, ml_size);

		// valid total size indicates the persistent state is valid
		root->total_size = total_size;
		pmem_persist(&root->total_size, sizeof(total_size));
	} else {
		assert(root->total_size == total_size);
		pmem_restore(root);
	}

	*root_ptr = root;
}

/*
 * pmem_fini -- unmap the persistent part
 */
void
pmem_fini(struct root_obj *root)
{
	pmem_unmap(root, root->total_size);
}

/*
 * get_empty_client_row -- find first empty client row
 */
struct client_row *
p_get_empty_client_row(struct client_row rows[], uint64_t n_rows)
{
	for (uint64_t i = 0; i < n_rows; ++i) {
		if (rows[i].status == NO_CLIENT)
			return &rows[i];
	}

	return NULL;
}

/*
 * p_client_init -- initialize client row's persistent state
 */
void
p_client_init(struct client_row *cr)
{
	cr->status = CLIENT_NO_MSG;
	pmem_persist(&cr->status, sizeof(cr->status));
}

/*
 * p_client_pending -- mark the client's msg is pending
 */
void
p_client_pending(struct client_row *cr)
{
	cr->status = CLIENT_MSG_PENDING;
	pmem_persist(&cr->status, sizeof(cr->status));
}

/*
 * p_client_done -- mark the client's msg is done
 */
void
p_client_done(struct client_row *cr)
{
	cr->status = CLIENT_MSG_DONE;
	pmem_persist(&cr->status, sizeof(cr->status));
}

int
ml_init(struct msg_log *ml, size_t size)
{
	return -1;
}

int
ml_ready(struct msg_log *ml)
{
	return -1;
}

int
mlog_append(struct msg_log *ml, struct client_row *cr)
{
	return -1;
}

uintptr_t
ml_get_wptr(struct msg_log *ml)
{
	return UINTPTR_MAX;
}

int
ml_set_rptr(struct msg_log *ml, uintptr_t rptr)
{
	return -1;
}

int
ml_set_wptr(struct msg_log *ml, uintptr_t wptr)
{
	return -1;
}

size_t
ml_offset(struct msg_log *ml, uintptr_t ptr)
{
	return SIZE_MAX;
}

uintptr_t
ml_read(struct msg_log *ml)
{
	return UINTPTR_MAX;
}
