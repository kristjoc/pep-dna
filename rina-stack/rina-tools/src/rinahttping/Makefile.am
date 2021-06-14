#
# Makefile.am
#
# Written by: Francesco Salvestrini <f DOT salvestrini AT nextworks DOT it>
# Modified by Steve Bunch @ trianetworksystems.com
# Modified by Kr1stj0n C1k0 <kristjoc@ifi.uio.no>
#

bin_PROGRAMS                       =
AM_INSTALLCHECK_STD_OPTIONS_EXEMPT =

rinahttping_SOURCES  =				\
	rinahttping.c
rinahttping_LDADD    = $(LIBRINA_LIBS) -lrt \
	-lrina-api
rinahttping_CFLAGS =			\
	-std=gnu11				\
	$(LIBRINA_CFLAGS)			\
	-I$(srcdir)/../common -lm -lpthread
rinahttping_CXXFLAGS =			\
	$(CPPFLAGS_EXTRA)

bin_PROGRAMS            += rinahttping
AM_INSTALLCHECK_STD_OPTIONS_EXEMPT += rinahttping
