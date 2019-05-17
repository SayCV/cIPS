# $Id: Makefile,v 1.11 2012/09/10 21:17:22 jdavid Exp $
# Copyright (c) 2008, Jean-Marc David (George Kelk Corp.)
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
#  * Neither the name of the author nor the names of its contributors
# may be used to endorse or promote products derived from this software
# without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
#  AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Author: Jean-Marc David



COMPILER=
ARCHIVER=
CP=cp
COMPILER_FLAGS= 
EXTRA_COMPILER_FLAGS=
CFLAGS=-Wall -D__BIG_ENDIAN__ -O3
TOPDIR=.
LIB=libcips.a
RELEASEDIR=../../../lib
INCLUDEDIR=../../../include/cips

INCLUDES= \
	-I$(TOPDIR)/app_options \
	-I$(TOPDIR)/include \
	-I$(INCLUDEDIR) 

INCLUDEFILES= $(TOPDIR)/include/ \
	$(TOPDIR)/app_options/application_options.h
 
LIBSOURCES=$(TOPDIR)/err.c \
          $(TOPDIR)/netif.c \
          $(TOPDIR)/tcp.c \
          $(TOPDIR)/udp.c \
	    $(TOPDIR)/arp.c \
          $(TOPDIR)/ip.c \
	    $(TOPDIR)/inet.c 


LIBOBJS=$(LIBSOURCES:.c=.o)

.c.o:
	@echo "Building object file: $@"
	$(COMPILER) -c $(CFLAGS) $(INCLUDES) $< -o $@

#Library generation
libs: $(LIB)

all: include libs install
.PHONY: all include lib install



$(LIB): $(LIBOBJS)
	@echo "Compiling cIPS"
	@echo "Archiving. Copy object files into a single one, $@"
	$(ARCHIVER) $(ARFLAGS) $@ $?
	@echo "Copy $(LIB): to ${RELEASEDIR}"
	${CP} $(LIB) $(RELEASEDIR)/

install: include libs
	@echo "Copy $(LIB): to ${RELEASEDIR}"
	${CP} $(LIB) $(RELEASEDIR)/

include:
	@echo "Copy header files to ${INCLUDEDIR}" 
	@for i in $(INCLUDEFILES); do \
	echo ${CP} -r $$i ${INCLUDEDIR}; \
	${CP} -r $$i ${INCLUDEDIR}; \
	done

clean:
	@echo "Cleaning up..."
	@echo ""
	@rm -f *.o *.a src


help:
	@echo " "
	@echo "  Makefile to generate the cIPS library ."
	@echo ""
	@echo "   Usage:"
	@echo ""
	@echo "  make or make libs"
	@echo "    Compile the library."
	@echo ""
	@echo "  make include"	
	@echo "    Copies the include files into the Xilinx EDK directory "
	@echo "    $(INCLUDEDIR) if exists."
	@echo ""
	@echo "  make install"
	@echo "    Generate the library and copy it and the include files"
	@echo "    into the Xilinx EDK directory $(INCLUDEDIR) if exists."
	@echo ""
	@echo "  make clean"
	@echo "    remove the object and library files."
	@echo ""

