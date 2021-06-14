#
# Makefile.am
#
# Written by: Francesco Salvestrini <f DOT salvestrini AT nextworks DOT it>
# Modified by Steve Bunch @ trianetworksystems.com
# Modified by Kr1stj0n C1k0 <kristjoc@ifi.uio.no>
#

bin_PROGRAMS                       =
AM_INSTALLCHECK_STD_OPTIONS_EXEMPT =

rinahttp_SOURCES  =				\
	httpapp.c		httpapp.h 	\
	main.c
rinahttp_LDADD    = $(LIBRINA_LIBS) -lrt \
	-lrina-api
rinahttp_CFLAGS =			\
	-std=gnu11				\
	$(LIBRINA_CFLAGS)			\
	-I$(srcdir)/../common -lpthread
rinahttp_CXXFLAGS =			\
	$(CPPFLAGS_EXTRA)

bin_PROGRAMS            += rinahttp
AM_INSTALLCHECK_STD_OPTIONS_EXEMPT += rinahttp
