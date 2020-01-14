---
layout: manual
Content-Style: 'text/css'
title: _MP(LIBRPMA, 7)
collection: librpma
header: PMDK
date: rpma API version 1.3
...

[comment]: <> (Copyright 2019, Intel Corporation)

[comment]: <> (Redistribution and use in source and binary forms, with or without)
[comment]: <> (modification, are permitted provided that the following conditions)
[comment]: <> (are met:)
[comment]: <> (    * Redistributions of source code must retain the above copyright)
[comment]: <> (      notice, this list of conditions and the following disclaimer.)
[comment]: <> (    * Redistributions in binary form must reproduce the above copyright)
[comment]: <> (      notice, this list of conditions and the following disclaimer in)
[comment]: <> (      the documentation and/or other materials provided with the)
[comment]: <> (      distribution.)
[comment]: <> (    * Neither the name of the copyright holder nor the names of its)
[comment]: <> (      contributors may be used to endorse or promote products derived)
[comment]: <> (      from this software without specific prior written permission.)

[comment]: <> (THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS)
[comment]: <> ("AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT)
[comment]: <> (LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR)
[comment]: <> (A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT)
[comment]: <> (OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,)
[comment]: <> (SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT)
[comment]: <> (LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,)
[comment]: <> (DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY)
[comment]: <> (THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT)
[comment]: <> ((INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE)
[comment]: <> (OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.)

[comment]: <> (librpma.7 -- man page for librpma)


[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[TARGET NODE ADDRESS FORMAT](#target-node-address-format)<br />
[REMOTE POOL ATTRIBUTES](#remote-pool-attributes)<br />
[SSH](#ssh)<br />
[FORK](#fork)<br />
[CAVEATS](#caveats)<br />
[LIBRARY API VERSIONING](#library-api-versioning-1)<br />
[ENVIRONMENT](#environment)<br />
[DEBUGGING AND ERROR HANDLING](#debugging-and-error-handling)<br />
[EXAMPLE](#example)<br />
[ACKNOWLEDGEMENTS](#acknowledgements)<br />
[SEE ALSO](#see-also)


# NAME #

**librpma** - remote persistent memory access library (EXPERIMENTAL)


# SYNOPSIS #

```c
#include <librpma.h>
cc ... -lrpma
```

##### Library API versioning: #####

```c
const char *rpma_check_version(
	unsigned major_required,
	unsigned minor_required);
```

##### Error handling: #####

```c
const char *rpma_errormsg(void);
```

##### Other library functions: #####

A description of other **librpma** functions can be found on the following
manual pages:

+ **XXX**(3)


# DESCRIPTION #

**librpma** provides low-level support for remote access to
*persistent memory* (pmem) utilizing RDMA-capable RNICs. The library can be
used to remotely read and write a memory region over the RDMA protocol. It utilizes
an appropriate persistency mechanism based on the remote node's platform
capabilities.

This library is for applications that use remote persistent memory directly
to preserve complete control over data transmission process.


# LIBRARY API VERSIONING #

This section describes how the library API is versioned,
allowing applications to work with an evolving API.

The **rpma_check_version**() function is used to see if the installed
**librpma** supports the version of the library API required by an
application. The easiest way to do this is for the application to supply
the compile-time version information, supplied by defines in
**\<librpma.h\>**, like this:

```c
reason = rpma_check_version(RPMA_MAJOR_VERSION,
                             RPMA_MINOR_VERSION);
if (reason != NULL) {
	/* version check failed, reason string tells you why */
}
```

Any mismatch in the major version number is considered a failure, but a
library with a newer minor version number will pass this check since
increasing minor versions imply backwards compatibility.

An application can also check specifically for the existence of an
interface by checking for the version where that interface was
introduced. These versions are documented in this man page as follows:
unless otherwise specified, all nterfaces described here are available
in version 1.0 of the library. Interfaces added after version 1.0 will
contain the text *introduced in version x.y* in the section of this
manual describing the feature.

When the version check performed by **rpma_check_version**() is
successful, the return value is NULL. Otherwise the return value is a
static string describing the reason for failing the version check. The
string returned by **rpma_check_version**() must not be modified or
freed.


# DEBUGGING AND ERROR HANDLING #

If an error is detected during the call to a **librpma** function, the
application may retrieve an error message describing the reason for the failure
from **rpma_errormsg**(). This function returns a pointer to a static buffer
containing the last error message logged for the current thread. The error message buffer is
thread-local; errors encountered in one thread do not affect its value in
other threads. The buffer is never cleared by any library function; its
content is significant only when the return value of the immediately preceding
call to a **librpma** function indicated an error.
The application must not modify or free the error message string, but it may
be modified by subsequent calls to other library functions.

Two versions of **librpma** are typically available on a development
system. The normal version, accessed when a program is linked using the
**-lrpma** option, is optimized for performance. That version skips checks
that impact performance and never logs any trace information or performs any
run-time assertions.

A second version of **librpma**, accessed when a program uses the libraries
under _DEBUGLIBPATH(), contains run-time assertions and trace points. The
typical way to access the debug version is to set the environment variable
**LD_LIBRARY_PATH** to _LDLIBPATH(). Debugging output is
controlled using the following environment variables. These variables have
no effect on the non-debug version of the library.

+ **RPMA_LOG_LEVEL**

The value of **RPMA_LOG_LEVEL** enables trace points in the debug version
of the library, as follows:

+ **0** - This is the default level when **RPMA_LOG_LEVEL** is not set.
No log messages are emitted at this level.

+ **1** - Additional details on any errors detected are logged
(in addition to returning the *errno*-based errors as usual).
The same information may be retrieved using **rpma_errormsg**().

+ **2** - A trace of basic operations is logged.

+ **3** - Enables a very verbose amount of function call
tracing in the library.

+ **4** - Enables voluminous and fairly obscure tracing information
that is likely only useful to the **librpma** developers.

Unless **RPMA_LOG_FILE** is set, debugging output is written to *stderr*.

+ **RPMA_LOG_FILE**

Specifies the name of a file where all logging information should be written.
If the last character in the name is "-", the *PID* of the current process will
be appended to the file name when the log file is created. If
**RPMA_LOG_FILE** is not set, logging output is written to *stderr*.


# EXAMPLE #

The following example uses **librpma** to create a remote pool on given
target node identified by given pool set name. The associated local memory
pool is zeroed and the data is made persistent on remote node. Upon success
the remote pool is closed.

```c
#include <librpma.h>

/* XXX */
```

# NOTE #

The **librpma** API is experimental and may be subject to change in the future.

# ACKNOWLEDGEMENTS #

**librpma** builds on the persistent memory programming model
recommended by the SNIA NVM Programming Technical Work Group:
<http://snia.org/nvmp>


# SEE ALSO #

**XXX**(3)
and **<http://pmem.io>**
