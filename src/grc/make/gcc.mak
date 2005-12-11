#
# gcc Makefile for grc
#

COMMON = ../common

CFLAGS 	= -g -O2 -Wall -W -std=c89 -I$(COMMON)
CC	= gcc
LDFLAGS	=
EBIND	= emxbind

OBJS =  grc.o

EXECS = grc

LIBS = $(COMMON)/common.a

.PHONY: all
ifeq (.depend,$(wildcard .depend))
all : $(EXECS)
include .depend
else
all:	depend
	@$(MAKE) -f make/gcc.mak all
endif



grc:	$(OBJS) $(LIBS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)
	@if [ $(OS2_SHELL) ] ;	then $(EBIND) $@ ; fi

clean:
	$(RM) *~ core *.lst

zap:	clean
	$(RM) *.o $(EXECS) .depend

# ------------------------------------------------------------------------------
# Make the dependencies

.PHONY: depend dep
depend dep:	$(OBJS:.o=.c)
	@echo "Creating dependency information"
	$(CC) $(CFLAGS) -MM $^ > .depend


