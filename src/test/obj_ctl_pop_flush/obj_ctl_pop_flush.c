// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2024, Intel Corporation */

/*
 * obj_ctl_arenas.c -- tests for the ctl entry points
 * usage:
 * obj_ctl_arenas <file> n - test for heap.narenas.total
 *
 * obj_ctl_arenas <file> s - test for heap.arena.[idx].size
 * and heap.thread.arena_id (RW)
 *
 * obj_ctl_arenas <file> c - test for heap.arena.create,
 * heap.arena.[idx].automatic and heap.narenas.automatic
 * obj_ctl_arenas <file> a - mt test for heap.arena.create
 * and heap.thread.arena_id
 *
 * obj_ctl_arenas <file> f - test for POBJ_ARENA_ID flag,
 *
 * obj_ctl_arenas <file> g - test for POBJ_ARENA_ID with
 * non-exists arena id
 *
 * obj_ctl_arenas <file> q - test for programmatic change of
 *	heap.arenas_assignment_type (RW)
 *
 * obj_ctl_arenas <file> p - test for config change of
 *	heap.arenas_assignment_type for global type (RW)
 *
 * obj_ctl_arenas <file> d - test for config change of
 *	heap.arenas_assignment_type for thread key type (RW)
 *
 * obj_ctl_arenas <file> b - test for config change of
 *	heap.arenas_default_max
 */

#include <sched.h>
#include "libpmemobj/atomic_base.h"
#include "libpmemobj/ctl.h"
#include "os_thread.h"
#include "sys_util.h"
#include "unittest.h"
#include "util.h"


static void
test_obj_ctl_pop_flush(PMEMobjpool *pop)
{
	int ret;

	ret = pmemobj_ctl_exec(pop, "pop.flush", NULL);
	UT_ASSERTeq(ret, 0);
	// UT_ASSERTeq(pattern, PALLOC_CTL_DEBUG_NO_PATTERN);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_ctl_debug");

	if (argc < 2)
		UT_FATAL("usage: %s filename", argv[0]);

	const char *path = argv[1];

	PMEMobjpool *pop;

	if ((pop = pmemobj_create(path, "pmemobj_ctl_exec", PMEMOBJ_MIN_POOL,
		S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_open: %s", path);

	test_obj_ctl_pop_flush(pop);

	pmemobj_close(pop);

	DONE(NULL);
}
