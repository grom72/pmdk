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
 * comm_server.c -- librpma-based communicator server
 */

#include "comm_server.h"

static int
ml_monitor(void *argc) // the message log monitor
{
	while (!exiting) {
		// sem_wait(ml_sem, 0);
		if (ML.W != ML.R) {
			// sem_set_value(distrubtion_sem, C)
			for (int i = 0; i < C; ++i) {
				// SEND (ML_UPDATE (ML.W, ML.R))
			}
			// sem_wait(distribution_sem, 0);
			ML.R = ML.W;
		}
		// sem_set_value(ml_sem, 1);
	}
}

static int
cq_notify(/* XXX */) // the CQ notification callback
{
	// check CS for changes
	ASSERTeq(CS[x], MSG_READY);
	// append the MSG BUFF to the Message Log (ML)
	// sem_dec(ml_sem, 0);
	CS[x] = MSG_DONE;
	return NTFY_ACK; // SEND is done by the library
}

static int
cq_recv_callback()
{
	// if RECV(bye bye message)
		// print the bye bye message
		// set exiting
		// cq break
	// if RECV(ML_UPDATE_ACK)
		// sem_dec(distribution_sem, 1);
}

static int
void conn_thread(void *arg)
{
	// rpma_conn = arg

	// send the hello message
	// - client row for the client (CV[x])
	// recv the hello message ACK

	// register recv callback
	// register notify callback
	// CQ loop
	// - handles emulated Atomic Writes
	// - handles notify messages
}

static int
eq_event_callback()
{
	switch (event) {
	case RPMA_EVENT_CONNECT:
		// accept the incomming connection
		// spawn a thread for the connection handling
		// clear eq timeout callback
		;
	case RPMA_EVENT_DISCONNECT:
		// set the connection as closing
		// join the connection thread
		// if # of conn == 0 then
		// register EQ timeout callback
		;
	default:
		return RPMA_E_UNHANDLED_EVENT;
	}
}

static int
eq_timeout_callback()
{
	// eq_loop_break
}

int
main(int argc, char *argv[])
{
	// mmap the message log - ML (persistent)
	// mmap the client vector - CV (persistent)
	// register CV row for each client
	// spawn CS monitor thread
	// spawn ML monitor thread

	// start listening
	// register EQ event callback
	// register EQ timeout callback
	// start EQ loop

	// join CS monitor thread
	// join ML monitor thread
	return 0;
}
