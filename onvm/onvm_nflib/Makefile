#                    openNetVM
#      https://github.com/sdnfv/openNetVM
#
# BSD LICENSE
#
# Copyright(c)
#          2015-2016 George Washington University
#          2015-2016 University of California Riverside
#          2010-2014 Intel Corporation.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in
# the documentation and/or other materials provided with the
# distribution.
# Neither the name of Intel Corporation nor the names of its
# contributors may be used to endorse or promote products derived
# from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overriden by command line or environment
include $(RTE_SDK)/mk/rte.vars.mk

RTE_TARGET ?= x86_64-native-linuxapp-gcc

# binary name
LIB    = onvm_nflib.a

# all source are stored in SRCS-y
SRCS-y := onvm_nflib.c
#SRCS-y += histogram.c 
#SRCS-y += onvm_pkt_helper.c onvm_sc_common.c onvm_sc_mgr.c onvm_flow_table.c onvm_flow_dir.c
#INC := histogram.h
CFLAGS += -DUSE_HISTOGRAM_AS_LIB

CFLAGS += $(WERROR_FLAGS) -O3 $(USER_FLAGS)
CFLAGS += -I$(SRCDIR)/../shared

#LDFLAGS += $(SRCDIR)/../shared/shared/$(RTE_TARGET)/onvm_pkt_helper.o
#LDFLAGS += $(SRCDIR)/../shared/shared/$(RTE_TARGET)/onvm_sc_common.o
#LDFLAGS += $(SRCDIR)/../shared/shared/$(RTE_TARGET)/onvm_sc_mgr.o

LDFLAGS += -L/usr/local/lib
LDLIBS+= -lzmq
LDFLAGS += ./libzmq.a

#include $(RTE_SDK)/mk/rte.extobj.mk
include $(RTE_SDK)/mk/rte.extlib.mk

#add other .o files in shared folder to this
#SHARED_OBJS := " $(SRCDIR)/../shared/shared/$(RTE_TARGET)/onvm_pkt_helper.o" 
#SHARED_OBJS += " $(SRCDIR)/../shared/shared/$(RTE_TARGET)/onvm_sc_common.o" 
#SHARED_OBJS += " $(SRCDIR)/../shared/shared/$(RTE_TARGET)/onvm_sc_mgr.o" 
#SHARED_OBJS += " $(SRCDIR)/../shared/shared/$(RTE_TARGET)/onvm_flow_table.o" 
#SHARED_OBJS += " $(SRCDIR)/../shared/shared/$(RTE_TARGET)/onvm_flow_dir.o"
#cp onvm_nflib.a libnf2.a
#echo $SHARED_OBJS
#ar r libnf.a ${SHARED_OBJS}
