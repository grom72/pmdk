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
 * pstructs.h -- persistent structures
 */

#ifndef COMM_PSTRUCTS_H
#define COMM_PSTRUCTS_H 1

#include <stddef.h>
#include <stdint.h>

/* status of the client row */
enum client_status_t {
	NO_CLIENT,
	CLIENT_NO_MSG,
	CLIENT_MSG_PENDING,
	CLIENT_MSG_DONE
};

#define USER_NAME_MAX (32)
#define MSG_SIZE_MAX (4096)

/* a single message for the message log */
struct msg_row {
	char user[USER_NAME_MAX];
	char msg[MSG_SIZE_MAX];
	size_t msg_size;
};

/* persistent space reserved for a client */
struct client_row {
	enum client_status_t status;
	struct msg_row msg;
};

/* the message log root object */
struct msg_log {
	uint64_t write_ptr;
	uint64_t read_ptr;
	uint64_t capacity;
	struct msg_row msgs[];
};

/* size of the message log in bytes */
#define MSG_LOG_SIZE(CAPACITY) \
	(sizeof(struct msg_log) + sizeof(struct msg_row) * (CAPACITY))

int ml_init(struct msg_log *ml, size_t size);

int ml_ready(struct msg_log *ml);

int mlog_append(struct msg_log *ml, struct client_row *cr);

uintptr_t ml_get_wptr(struct msg_log *ml);

int ml_set_rptr(struct msg_log *ml, uintptr_t rptr);

int ml_set_wptr(struct msg_log *ml, uintptr_t wptr);

size_t ml_offset(struct msg_log *ml, uintptr_t ptr);

uintptr_t ml_read(struct msg_log *ml);

/* root object - common for server and client */
struct root_obj {
	size_t total_size; /* total size of the memory pool */
	struct msg_log ml;
};

void pmem_init(struct root_obj **root_ptr, const char *path, size_t min_size);

void pmem_fini(struct root_obj *root);

struct client_row *p_get_empty_client_row(struct client_row rows[],
		uint64_t n_rows);

void p_client_init(struct client_row *cr);

void p_client_pending(struct client_row *cr);

void p_client_done(struct client_row *cr);

#endif /* pstructs.h */
