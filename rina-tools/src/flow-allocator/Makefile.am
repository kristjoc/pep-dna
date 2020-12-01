#
# Makefile.am
#
# Written by: Francesco Salvestrini <f DOT salvestrini AT nextworks DOT it>
# Edited by: Kr1stj0n C1k0 <kristjoc@ifi.uio.no>
#

bin_PROGRAMS                       =
AM_INSTALLCHECK_STD_OPTIONS_EXEMPT =

flow_allocator_SOURCES  =					\
	common.cc			common.h		\
	flow-allocator-server.cc	flow-allocator-server.h	\
	flow-allocator-client.cc	flow-allocator-client.h	\
	main.cc
flow_allocator_LDADD    = $(LIBRINA_LIBS) -lrt	\
	../common/librinaapp.la
flow_allocator_CPPFLAGS =			\
	$(LIBRINA_CFLAGS) -std=c++11		\
	$(CPPFLAGS_EXTRA)			\
	-I$(srcdir)/../common 			\
	-I/usr/include/libnl3
flow_allocator_CXXFLAGS =			\
	$(CPPFLAGS_EXTRA)

bin_PROGRAMS            += flow-allocator
AM_INSTALLCHECK_STD_OPTIONS_EXEMPT += flow-allocator
