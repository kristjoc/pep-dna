#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#ifndef TCP_FASTOPEN
#define TCP_FASTOPEN   23
#endif

#ifndef MSG_FASTOPEN
#define MSG_FASTOPEN   0x20000000
#endif

#ifndef IP_TRANSPARENT
#define IP_TRANSPARENT      19
#endif

static void die_usage(void)
{
	fputs("Usage: tcprdr [ -4 | -6 ] [ -f ] [ -l ] [ -t [ -T ]] [ -L listen address ] localport host [ remoteport ]\n", stderr);
	exit(1);
}


/* Turn this program into a daemon process. */
static void
daemonize(void)
{
    pid_t pid = fork();
    pid_t sid;

    if (pid < 0) {
        perror("fork(daemonize)");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        /* This is the parent. We can terminate it. */
        exit(EXIT_SUCCESS);
    }

    /* Execution continues only in the child's context. */
    sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }
}

static int wanted_pf = PF_UNSPEC;
static bool loop = false;
static bool tproxy = false;
static bool tproxy_trans = false; /* use non-local, original client ip for outgoing connection */
static bool fastopen = false;
static char *listenaddr = NULL;

static const char *getxinfo_strerr(int err)
{
	const char *errstr;
	if (err == EAI_SYSTEM)
		errstr = strerror(errno);
	else
		errstr = gai_strerror(err);
	return errstr;
}


static void xgetaddrinfo(const char *node, const char *service,
			const struct addrinfo *hints,
			struct addrinfo **res)
{
	int err = getaddrinfo(node, service, hints, res);
	if (err) {
		const char *errstr = getxinfo_strerr(err);
		fprintf(stderr, "Fatal: getaddrinfo(%s:%s): %s\n", node ? node: "", service ? service: "", errstr);
	        exit(1);
	}
}


static void xgetnameinfo(const struct sockaddr *sa, socklen_t salen,
			char *host, size_t hostlen,
			char *serv, size_t servlen, int flags)
{
	int err = getnameinfo(sa, salen, host, hostlen, serv, servlen, flags);
	if (err) {
		const char *errstr = getxinfo_strerr(err);
		fprintf(stderr, "Fatal: getnameinfo(): %s\n", errstr);
	        exit(1);
	}
}


static void ipaddrtostr(const struct sockaddr *sa, socklen_t salen, char *resbuf, size_t reslen, char *port, size_t plen)
{
	xgetnameinfo(sa, salen, resbuf, reslen, port, plen, NI_NUMERICHOST|NI_NUMERICSERV);
}


static void logendpoints(int dest, int origin)
{
	struct sockaddr_storage ss1, ss2;
	int ret;
	char buf1[INET6_ADDRSTRLEN];
	char buf2[INET6_ADDRSTRLEN];

	socklen_t sa1len, sa2len;

	sa1len = sa2len = sizeof(ss1);

	ret = getpeername(origin, (struct sockaddr *) &ss1, &sa1len);
	if (ret == -1) {
		perror("getpeername");
		return;
	}
	ret = getpeername(dest, (struct sockaddr *) &ss2, &sa2len);
	if (ret == -1) {
		perror("getpeername");
		return;
	}
	ipaddrtostr((const struct sockaddr *) &ss1, sa1len, buf1, sizeof(buf1), NULL, 0);
	ipaddrtostr((const struct sockaddr *) &ss2, sa2len, buf2, sizeof(buf2), NULL, 0);

	fprintf(stderr, "Handling connection from %s to %s", buf1, buf2);
	if (tproxy) {
		char port[8];

		sa1len = sizeof(ss1);
		ret = getsockname(origin, (struct sockaddr *) &ss1, &sa1len);
		if (ret) {
			perror("getsockname");
			return;
		}
		ipaddrtostr((const struct sockaddr *) &ss1, sa1len, buf1, sizeof(buf1), port, sizeof(port));
		fprintf(stderr, " (original destination was %s:%s)", buf1, port);
	}
	fputc('\n', stderr);
}


static int sock_listen_tcp(const char * const listenaddr, const char * const port)
{
	int sock;
	struct addrinfo hints = {
		.ai_protocol = IPPROTO_TCP,
		.ai_socktype = SOCK_STREAM,
		.ai_flags = AI_PASSIVE | AI_NUMERICHOST
	};

	hints.ai_family = wanted_pf;

	struct addrinfo *a, *addr;
	int one = 1;

	xgetaddrinfo(listenaddr, port, &hints, &addr);

	for (a = addr; a != NULL ; a = a->ai_next) {
		sock = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
		if (sock < 0) {
			perror("socket");
			continue;
		}

		if (-1 == setsockopt(sock, SOL_SOCKET,SO_REUSEADDR,&one,sizeof one))
			perror("setsockopt");


		if (bind(sock, a->ai_addr, a->ai_addrlen) == 0)
			break; /* success */

		perror("bind");
		close(sock);
		sock = -1;
	}

	if ((sock >= 0) && listen(sock, 65535))
		perror("listen");

	freeaddrinfo(addr);
	return sock;
}

static int
connect_fastopen(int txfd, char *buf, int blen, const struct sockaddr *dest_addr, socklen_t alen)
{
	int ret;
	/* implicit connect() */
	ret = sendto(txfd, buf, blen, MSG_FASTOPEN, dest_addr, alen);

	if (ret != blen)
		return -1;
	return 0;
}

static int
connect_fastopen_prepare(int fd, char *buf, size_t len)
{
	int r = recv(fd, buf, len, MSG_DONTWAIT);

	if (r >= 0)
		return r;
	return (errno == EAGAIN || errno == EINTR) ? -1 : 0;
}

static int sock_connect_tcp(const char * const remoteaddr, const char * const port, int rs)
{
	char buf[1024];
	int buflen;
	int sock;
        int mark = 333;
	struct addrinfo hints = {
		.ai_protocol = IPPROTO_TCP,
		.ai_socktype = SOCK_STREAM,
	};
	struct addrinfo *a, *addr;
	struct sockaddr_storage sa;
	socklen_t salen = sizeof(sa);

	if (fastopen) {
		buflen = connect_fastopen_prepare(rs, buf, sizeof(buf));
		if (buflen == 0)
			return -1;
		else if (buflen < 0)
			buflen = 0;
	}

	hints.ai_family = wanted_pf;

	xgetaddrinfo(remoteaddr, port, &hints, &addr);
	if (tproxy_trans) {
		if (getpeername(rs, (void *) &sa, &salen) != 0) {
			perror("getpeername");
			tproxy_trans = false;
		}
	}

	for (a=addr; a != NULL; a = a->ai_next) {
		sock = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
		if (sock < 0) {
			perror("socket");
			continue;
		}

		if (tproxy_trans) {
			static const int one = 1;

			if (setsockopt(sock, SOL_IP, IP_TRANSPARENT, &one, sizeof(one)))
				perror("setsockopt(IP_TRANSPARENT2)");

	                if (setsockopt(sock, SOL_SOCKET, SO_MARK, &mark, sizeof(mark)))
                                perror("setsockopt(SO_MARK)");

			if (bind(sock, (void *) &sa, salen))
				perror("fake bind");
		}

		if (fastopen && buflen > 0) {
			if (connect_fastopen(sock, buf, buflen, a->ai_addr, a->ai_addrlen) == 0)
				break; /* success */
		} else {
			if (connect(sock, a->ai_addr, a->ai_addrlen) == 0)
				break; /* success */
		}

		perror("connect()");
		close(sock);
		sock = -1;
	}

	freeaddrinfo(addr);
	return sock;
}


static size_t do_write(const int fd, char *buf, const size_t len)
{
	size_t offset = 0;

	while (offset < len) {
		size_t written;
		ssize_t bw = write(fd, buf+offset, len - offset);
		if (bw < 0 ) {
			perror("write");
			return 0;
		}

		written = (size_t) bw;
		offset += written;
	}
	return offset;
}


static void copyfd_io(int fd_zero, int fd_one)
{
	struct pollfd fds[] = { { .events = POLLIN }, { .events = POLLIN }};

	fds[0].fd = fd_zero;
	fds[1].fd = fd_one;

	for (;;) {
		int readfd, writefd;

		readfd = -1;
		writefd = -1;

		switch(poll(fds, 2, -1)) {
		case -1:
			if (errno == EINTR)
				continue;
			perror("poll");
			return;
		case 0:
			/* should not happen, we requested infinite wait */
			fputs("Timed out?!", stderr);
			return;
		}

		if (fds[0].revents & POLLHUP) return;
		if (fds[1].revents & POLLHUP) return;

		if (fds[0].revents & POLLIN) {
			readfd = fds[0].fd;
			writefd = fds[1].fd;
		} else if (fds[1].revents & POLLIN) {
			readfd = fds[1].fd;
			writefd = fds[0].fd;
		}

		if (readfd>=0 && writefd >= 0) {
			char buf[4096];
			ssize_t len;

			len = read(readfd, buf, sizeof buf);
			if (!len) return;
			if (len < 0) {
				if (errno == EINTR)
					continue;

				perror("read");
				return;
			}
			if (!do_write(writefd, buf, len)) return;
		} else {
			/* Should not happen,  at least one fd must have POLLHUP and/or POLLIN set */
			fputs("Warning: no useful poll() event", stderr);
		}
	}
}


static int parse_args(int argc, char *const argv[])
{
	int i;
	for (i = 0; i < argc; i++) {
		if (argv[i][0] != '-')
			return i;
		switch(argv[i][1]) {
			case '4': wanted_pf = PF_INET; break;
			case '6': wanted_pf = PF_INET6; break;
			case 't': tproxy = true; break;
			case 'T': tproxy_trans = true; break;
			case 'f': fastopen = true; break;
			case 'l': loop = true; break;
			case 'L':
				if (i + 1 < argc)
					listenaddr = argv[++i];
				else
					die_usage();
				break;
			default:
				die_usage();
		}
	}
	return i;
}


/* try to chroot; don't complain if chroot doesn't work */
static void do_chroot(void)
{
	if (chroot("/var/empty") == 0) {
		/* chroot ok, chdir, setuid must not fail */
		if (chdir("/")) {
			perror("chdir /var/empty");
			exit(1);
		}
		setgid(65535);
		if (setuid(65535)) {
			perror("setuid");
			exit(1);
		}
	}
}

int main_loop(int listensock, const char *host, const char *port)
{
	struct sockaddr sa;
	socklen_t salen = sizeof(sa);
        int remotesock, connsock;

 again:
	while ((remotesock = accept(listensock, &sa, &salen)) < 0)
		perror("accept");

	connsock = sock_connect_tcp(host, port, remotesock);
	if (connsock < 0)
	       return 1;

	if (!loop)
		do_chroot();

	logendpoints(connsock, remotesock);
	copyfd_io(connsock, remotesock);
	close(connsock);
	close(remotesock);

	if (loop)
		goto again;
	return 0;
}

int main(int argc, char *argv[])
{
	int args;
	int listensock;
	const char *host, *port;

	if (argc < 3)
		die_usage();

	--argc;
	++argv;

	args = parse_args(argc, argv);

	argc -= args;
	argv += args;

	if (argc < 2) /* we need at least 2 more arguments (srcport, hostname) */
		die_usage();

	listensock = sock_listen_tcp(listenaddr, argv[0]);
	if (listensock < 0)
		return 1;

	if (tproxy) {
		static int one = 1;
		if (setsockopt(listensock, SOL_IP, IP_TRANSPARENT, &one, sizeof(one)))
			perror("setsockopt(IP_TRANSPARENT)");
	}

	if (fastopen) {
		int tmp = 32;
		if (setsockopt(listensock, SOL_TCP, TCP_FASTOPEN, &tmp, sizeof(tmp)))
			perror("setsockopt(TCP_FASTOPEN)");
		if (setsockopt(listensock, SOL_TCP, TCP_DEFER_ACCEPT, &tmp, sizeof(tmp)))
			perror("setsockopt(TCP_DEFER_ACCEPT)");
	}
	host = argv[1];
	/* destport given? if no, use srcport */
	port = argv[2] ? argv[2] : argv[0];

        daemonize();
	return main_loop(listensock, host, port);
}

