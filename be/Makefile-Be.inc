#---------------------------------------------------------------------
# This Makefile-Include for BeOS is part of PRCS.
# Copyright (C) 1998 Lars Duening
#
# This file is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published
# by the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This file is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this library; see the file COPYING.  If not, write to
# the Free Software Foundation, 59 Temple Place - Suite 330, Boston,
# MA  02111-1307, USA.
#----------------------------------------------------------------------
#
# Usage: make [CPU=ppc|x86] [<target>]
#
# The variable CPU determines the platform for which the program is
# compiled. If left unspecified, the executing platform is targeted.
# The created objects and binaries are located in subdirectories with
# the name 'obj.<CPU>'.
#
# Target is one of the typical GNU makefile targets.
#----------------------------------------------------------------------

# Determine the CPU if not specified on the command line.
#   set MACHINE to the host machine
#   set NATIVE  to the host cpu
#   set CROSS   to the non-host cpu

MACHINE =$(shell uname -m)

ifndef CPU
ifeq ($(MACHINE), BePC)
    CPU = x86
else
    CPU = ppc
endif
endif
ifeq ($MACHINE), BePC)
    NATIVE = x86
    CROSS = ppc
else
    NATIVE = ppc
    CROSS = x86
endif

# Set the object directory
    OBJ := obj.$(CPU)

# Specify the directory for libraries
    BELIBS = /boot/develop/lib/$(CPU)

# Specify the default libraries
    DEFAULT_LIBS = libbe.so libroot.so libnet.so

# The path to the headers
    BEHEADERS = /boot/develop/headers
    
# Specify the common tools and their initial flags
    MIMESET = mimeset

    CC = mwcc$(CPU)
    CXX = mwcc$(CPU)

    LD = mwld$(CPU)
    LDFLAGS += -sym full -xma -nodefaults \
              $(BELIBS)/glue-noinit.a \
              $(BELIBS)/init_term_dyn.o \
              $(BELIBS)/start_dyn.o
    
# Platform specific settings
ifeq ($(CPU), x86)
    LIB = .LIB
    CFLAGS += -inline off -O4
    CXXFLAGS += -inline off -O4
endif

ifeq ($(CPU), ppc)
    LIB = 
    CFLAGS += -opt full
    CXXFLAGS += -opt full
    LDFLAGS += -nodup -export pragma -init _init_routine_ -term _term_routine_
endif

# set the full directory variable if not specified
ifeq ($(FULL_DIR),)
    FULL_DIR := $(shell pwd)
endif

# Create the list of libraries to use
    LIBS_TO_USE = $(addprefix $(BELIBS)/, $(addsuffix $(LIB), $(DEFAULT_LIBS) ))

# Complete the linker flags
    LDFLAGS += -L$(FULL_DIR) -L$(BELIBS) $(LIBS_TO_USE)

