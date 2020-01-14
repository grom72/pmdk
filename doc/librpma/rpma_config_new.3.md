---
layout: manual
Content-Style: 'text/css'
title: _MP(RPMA_CONFIG_NEW, 3)
collection: librpma
header: PMDK
date: rpma API version 1.0
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

[comment]: <> (rpma_config_new.3 -- man page for RPMA config control functions)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**rpma_config_new**(), **rpma_config_delete**(),
**rpma_config_set_addr**(), **rpma_config_set_service**(),
- struct rpma_config setup*


# SYNOPSIS #

```c
#include <librpma.h>

struct rpma_config;

int rpma_config_new(struct rpma_config **config);
int rpma_config_delete(struct rpma_config *config);

int rpma_config_set_addr(struct rpma_config *config, const char *address);
int rpma_config_set_service(struct rpma_config *config, const char *service);
```


# DESCRIPTION #

The **rpma_config** structure stores arguments required for creating **rpma_domain**. For details please see **rpma_domain**(7).

The **rpma_config_new**() function allocates and NULL initializes a **rpma_config** structure. All **rpma_config** structures created using **rpma_config_new**() have to be deleted using **rpma_config_delete**().

The **rpma_config_set_addr**() function allows copying the **address** into the provided **config**. The **address** is required to start listening for upcoming connections on the provided **address** or connect to it. For details on establishing a connection please see **rpma_conn_new**(3).

The **rpma_config_set_service**() function allows copying the **service** into the provided **config**. The **service** is required to start listening for upcoming connections on the provided **service** or connect to it. For details on establishing a connection please see **rpma_conn_new**(3).


# RETURN VALUE #

On success, **rpma_config_new**() returns **RPMA_E_OK** and stores a pointer to the newly created **rpma_config** structure in the provided output argument **config**; otherwise **config** content is undefined and the resulting error code is returned:

- **RPMA_E_XXX** if XXX

On success, **rpma_config_delete**() returns **RPMA_E_OK** and releases the resources related to the provided **config**; otherwise **config** content is undefined and the resulting error code is returned:

- **RPMA_E_XXX** if XXX

On success, **rpma_config_set_addr**() returns **RPMA_E_OK** and copies the **address** into to the **config**; otherwise **config** content is undefined and the resulting error code is returned:

- **RPMA_E_XXX** if XXX

On success, **rpma_config_set_service**() returns **RPMA_E_OK** and copies the **service** into to the **config**; otherwise **config** content is undefined and the resulting error code is returned:

- **RPMA_E_XXX** if XXX


# SEE ALSO #

**rpma_domain_new**(7), **rpma_conn_new**(3) and **<http://pmem.io>**
