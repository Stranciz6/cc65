#
# gcc Makefile for the binutils common stuff
#

CFLAGS 	= -g -O2 -Wall -W
CC	= gcc
LDFLAGS	=
LIB	= common.a



OBJS =	abend.o		\
	bitops.o	\
	chartype.o	\
	check.o		\
	cmdline.o	\
	coll.o		\
        cpu.o           \
        debugflag.o     \
	exprdefs.o	\
	filepos.o	\
	fname.o		\
	hashstr.o	\
        matchpat.o      \
	print.o		\
        segdefs.o       \
        segnames.o      \
	strbuf.o	\
	strutil.o	\
	target.o	\
	tgttrans.o	\
	xmalloc.o	\
	xsprintf.o


# ------------------------------------------------------------------------------
# Dummy targets

.PHONY: all
ifeq (.depend,$(wildcard .depend))
all:	lib
include .depend
else
all:	depend
	@$(MAKE) -f make/gcc.mak all
endif

.PHONY:	lib
lib:	$(LIB)

$(LIB):	$(OBJS)
	$(AR) rs $(LIB) $?

clean:
	rm -f *~ core *.map

zap:	clean
	rm -f *.o $(LIB) .depend

# ------------------------------------------------------------------------------
# Make the dependencies

.PHONY: depend dep
depend dep:	$(OBJS:.o=.c)
	@echo "Creating dependency information"
	$(CC) -MM $^ > .depend


