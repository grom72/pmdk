# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2024, Intel Corporation

#
# src/test/pmem2_movnt/Makefile -- build pmem2_movnt test
#
TOP = ../../..
vpath %.c $(TOP)/src/test/_unittest
INCS += -I$(TOP)/src/libpmem2

TARGET = pmem2_movnt
OBJS = pmem2_movnt.o\
	ut_pmem2_utils.o\
	ut_pmem2_config.o\
	ut_pmem2_source.o

LIBPMEM2=y

include ../Makefile.inc
