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

#define CLIENT_MSG_READY	1
#define CLIENT_MSG_DONE	2

#define MSG_SIZE_MAX (4096)

struct client_row {
	uint64_t status;
	size_t msg_size;
	char msg[MSG_SIZE_MAX];
};

struct msg_row {
	uint64_t client_id;
	size_t msg_size;
	char msg[MSG_SIZE_MAX];
};

struct msg_log {
	uint64_t write_ptr;
	uint64_t read_ptr;
	uint64_t capacity;
	struct msg_row msgs[];
};

#define MSG_LOG_SIZE(CAPACITY) \
	(sizeof(struct msg_log) + sizeof(struct msg_row) * (CAPACITY))

#endif /* pstructs.h */
