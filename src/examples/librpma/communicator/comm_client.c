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
 * comm_client.c -- librpma-based communicator client
 */

#include "comm_client.h"

static int
ml_monitor(void *argc) // the message log monitor
{
	while (!exiting) {
		if (ML.W != ML.R) {
			// get stdout lock
			// printf messages
			// unlock stdout
			ML.R = ML.W;
		}
		// sleep(timeout);
	}
}

static void
cs_monitor()
{
	while (!exiting) {
		if (CS[x] == MSG_DONE) // monitor local CS
			break;
	}
}

static void
send_messages()
{
	while(!exiting) {
		scanf(""); // to the message source buffer
		if (msg == "exit") {
			// set exiting
			// send bye bye message
			continue;
		}
		// atomic write to CS[x] on the server
		// CS monitor
	}
}

static void
eq_timeout_callback()
{
	// eq loop break
}

static void
eq_disconnect_callback()
{
	// set exiting
	// eq loop break
}

static void
eq_monitor(void *argc)
{
	// register on disconnect callback
	// register on timeout callback
	// eq loop
}

int
main(int argc, char *argv[])
{
	// mmap the message log - ML
	// mmap the client status cell - CS
	// register the client status
	// register the message log
	// spawn ML monitor thread

	// spawn the EQ monitor thread
	// connect
	// recv the hello message
	// - client status memory region on the server CS[x]
	// send the hello message ACK
	// - the client's message source buffer
	// - the client's message log (ML)

	// send the messages

	// join the EQ monitor thread
	// join the ML monitor thread
	return 0;
}
