
VPATH=$(SYSIO_SRC)/src
TESTDIR=$(top_srcdir)/test

include $(top_srcdir)/src/module.mk
include $(top_srcdir)/aio-emulation/module.mk

lib_LIBRARIES=libsysiox.a

libsysiox_a_SOURCES = $(SRCDIR_SRCS) $(OPTIONAL_ESRCS)

AM_CPPFLAGS = -g -W -Wall -std=iso9899:1999 -pedantic \
	-D_POSIX_C_SOURCE=199506L \
	-D_GNU_SOURCE \
	-I $(SYSIO_SRC)/include -I $(top_srcdir)/include

tests: $(lib_LIBRARIES) FORCE
	$(MAKE) -C $(TESTDIR) 

clean: clean-am
	$(MAKE) -C $(TESTDIR) clean

FORCE:
