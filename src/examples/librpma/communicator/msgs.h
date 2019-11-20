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
 * msgs.h -- messages
 */

#ifndef COMM_MSGS_H
#define COMM_MSGS_H 1

#define MSG_TYPE_ACK		1
#define MSG_TYPE_HELLO		2
#define MSG_TYPE_MLOG_UPDATE	3
#define MSG_TYPE_BYE_BYE	4

/* base message type */
struct msg_base_t {
	uint64_t type;
};

/* generic ACK message */
struct msg_ack_t {
	uint64_t original_msg_type;
	uint64_t status;
};

/* hello message - sending the required identifiers */
struct msg_hello_t {
	struct rpma_memory_id cr_id; /* client-row id */
	struct rpma_memory_id ml_id; /* the message log id */
};

/* message log update */
struct msg_mlog_update {
	uintptr_t wptr;
};

/* unified message type */
struct msg_t {
	struct msg_base_t base;
	union {
		struct msg_ack_t ack;
		struct msg_hello_t hello;
		struct msg_mlog_update update;
	};
};

#endif /* msgs.h */
