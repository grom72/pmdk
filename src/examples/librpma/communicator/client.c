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
on_recive(void *argc) // RECV(R,W) pair from the server
{
	if (!exiting) {
		// break the loop
		return;
	}

	// READ (R, W-R) -> local ML
	// ML.W = W
	// SEND (ACK)
	// printf messages
}

static void
notify_ack()
{
	wait_for_notify = false;
}

static void
send_messages()
{
	if (!exiting && !wait_for_notify) {
		scanf(""); // to the message source buffer
		if (msg == "exit") {
			// set exiting
			// send bye bye message
			continue;
		}
		wait_for_notify = true;
		// write the message (commit)
		// commit
		// atomic write to CV[x] status
		// commit + notify
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
	// mmap the client vector row - CV
	// register the client vector row
	// register the message log
	// spawn ML monitor thread

	// spawn the EQ monitor thread
	// connect
	// recv the hello message
	// - client status memory region on the server CV[x]
	// send the hello message ACK

	// register on_timeout callback -> send the messages
	// register on_notify_ack callback
	// register on_recv callback
	// CQ loop

	// join the EQ monitor thread
	// join the ML monitor thread
	return 0;
}
