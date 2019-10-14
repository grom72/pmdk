---
layout: manual
Content-Style: 'text/css'
title: _MP(RPMA_DOMAIN_NEW, 3)
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

[comment]: <> (rpma_domain_new.3 -- man page for RPMA domain control functions)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**rpma_domain_new**(), **rpma_domain_delete**()


# SYNOPSIS #

```c
#include <librpma.h>

struct rpma_domain;

int rpma_domain_new(struct rpma_config *cfg, struct rpma_domain **domain);
int rpma_domain_delete(struct rpma_domain *domain);
```


# DESCRIPTION #

The **rpma_domain_new**() function creates a new **domain** using parameters passed by the **config** structure. After successfully creating the **domain** it is allowed to release the **config** structure with the **rpma_config_delete**(3) function. A **domain** created using **rpma_domain_new**() has to be released using **rpma_domain_delete**().

A successfully created **domain** allows creating new memory regions and establishing connections. For details please see **rpma_mr_new**(3) and **rpma_conn_new**(3) respectively.


# RETURN VALUE #

On success, **rpma_domain_new**() returns **RPMA_E_OK** and stores a pointer to the newly created **domain** in the provided output argument **domain**; otherwise **domain** content is undefined and the resulting error code is returned:

- **RPMA_E_XXX** if XXX

On success, **rpma_domain_delete**() returns **RPMA_E_OK** and releases the resources related to the provided **domain**; otherwise **domain** content is undefined and the resulting error code is returned:

- **RPMA_E_XXX** if XXX


# SEE ALSO #

**rpma_config_new**(3), **rpma_mr_new**(3), **rpma_conn_new**(3) and **<http://pmem.io>**
