#
# Makefile for the cl65 compile&link utility
#

# Library dir
COMMON	= ../common

# Type of spawn function to use
SPAWN   = SPAWN_UNIX
ifneq ($(Kickstart),)
SPAWN   = SPAWN_AMIGA
endif


CC=gcc
CFLAGS = -O2 -g -Wall -W -std=c89 -I$(COMMON) -D$(SPAWN)
EBIND  = emxbind
LDFLAGS=

OBJS =	error.o	 	\
	global.o 	\
	main.o

LIBS = $(COMMON)/common.a

EXECS = cl65


.PHONY: all
ifeq (.depend,$(wildcard .depend))
all : $(EXECS)
include .depend
else
all:	depend
	@$(MAKE) -f make/gcc.mak all
endif


cl65:	$(OBJS) $(LIBS)
	$(CC) $(LDFLAGS) -o cl65 $(OBJS) $(LIBS)
	@if [ $(OS2_SHELL) ] ;	then $(EBIND) cl65 ; fi

clean:
	$(RM) *~ core

zap:	clean
	$(RM) *.o $(EXECS) .depend


# ------------------------------------------------------------------------------
# Make the dependencies

.PHONY: depend dep
depend dep:	$(OBJS:.o=.c)
	@echo "Creating dependency information"
	$(CC) $(CFLAGS) -D$(SPAWN) -MM $^ > .depend



