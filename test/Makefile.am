
noinst_PROGRAMS=test_sysio

test_sysio_SOURCES=test.c

AM_CPPFLAGS = -g -W -Wall -std=iso9899:1999 -pedantic \
	-D_POSIX_C_SOURCE=199506L \
	-D_GNU_SOURCE \
	-I $(SYSIO_SRC)/include -I $(top_srcdir)/include

AM_LDFLAGS = -L$(top_srcdir) 
LIBS = -lsysiox
