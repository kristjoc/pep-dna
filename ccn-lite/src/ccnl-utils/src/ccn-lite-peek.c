/*
 * @f util/ccn-lite-peek.c
 * @b request content: send an interest, wait for reply, output to stdout
 *
 * Copyright (C) 2013-15, Christian Tschudin, University of Basel
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * File history:
 * 2013-04-06  created
 * 2014-06-18  added NDNTLV support
 */

#include "ccnl-common.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include <sys/poll.h>

#ifndef assert
#define assert(...) do {} while(0)
#endif

// ----------------------------------------------------------------------
#define NET_SOFTERROR -1
#define NET_HARDERROR -2
#define POLL_READ 0
#define POLL_WRITE 1
#define BUFSIZE 11608    /* 1451 * 8 */
#define MAX_CONNS 65535
unsigned char out[8*CCNL_MAX_PACKET_SIZE];
int outlen;

volatile sig_atomic_t running = 1;
static void sig_handler(int signum) {
    running = 0;
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
        exit(0);
    }

    /* Execution continues only in the child's context. */
    sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }
}

/*
 *   POLL FILE DESCRIPTOR
 */
static int
poll_fd(int fd, int poll_mode, int timeout)
{
    struct pollfd pfd;
    int rc;

    pfd.fd = fd;

    if (poll_mode == POLL_READ) {
        pfd.events = POLLIN;
        do {
            rc = poll(&pfd, (nfds_t)1, timeout);
            if (rc > 0) {
                if (pfd.revents != POLLIN)
                    return NET_HARDERROR;

                return rc;
            } else if (rc < 0) {
                if (errno == EINTR)
                    continue;

                perror("poll");
                return rc;
            } else {
                errno = ETIMEDOUT;
                perror("poll");
                return rc;
            }
        } while(rc < 0 && errno == EINTR);
    } else {
        pfd.events = POLLOUT;
        do {
            rc = poll(&pfd, (nfds_t)1, timeout);
            if (rc > 0) {
                if (pfd.revents != POLLOUT)
                    return NET_HARDERROR;

                return rc;
            } else if (rc < 0) {
                if (errno == EINTR)
                    continue;

                perror("poll");
                return rc;
            } else {
                errno = ETIMEDOUT;
                perror("poll");
                return rc;
            }
        } while(rc < 0 && errno == EINTR);
    }

    return NET_SOFTERROR;
}

/*
 *  SET SOCKET TO O_NONBLOCK
 */
static void
set_blocking(int fd, bool blocking)
{
    if (blocking) {
        if (fcntl(fd, F_SETFL, 0)) {
            perror("fcntl");
            close(fd);
            exit(EXIT_FAILURE);
        }
    } else {
        if (fcntl(fd, F_SETFL, O_NONBLOCK)) {
            perror("fcntl");
            close(fd);
            exit(EXIT_FAILURE);
        }
    }
}

/*
 *  PARSE GET REQUEST
 */
static void
parse_request(char *request, char *content)
{
    char *pos = strstr(request, "GET");
    if (pos) {
        sscanf(pos, "%*s %s", content);
    }
}

static int
frag_cb(struct ccnl_relay_s *relay, struct ccnl_face_s *from,
        unsigned char **data, int *len)
{
    (void)relay;
    (void)from;
    DEBUGMSG(INFO, "frag_cb\n");

    memcpy(out, *data, *len);
    outlen = *len;
    return 0;
}

static void
send_to_relay(char *_uri, int _suite, unsigned int _chunknum, char *_ux,
	      int sock, struct sockaddr _sa, float _wait, int csock)
{
    struct ccnl_buf_s *buf = NULL;
    int rc, socksize;

    struct ccnl_prefix_s *prefix = ccnl_URItoPrefix(_uri, _suite,
	    _chunknum == UINT_MAX ? NULL : &_chunknum);

    for (int cnt = 0; cnt < 3; cnt++) {
        int32_t nonce = (int32_t) random();
        int rc;
        struct ccnl_face_s dummyFace;
        ccnl_interest_opts_u int_opts;
#ifdef USE_SUITE_NDNTLV
        int_opts.ndntlv.nonce = nonce;
#endif

        DEBUGMSG(TRACE, "sending request, iteration %d\n", cnt);

        memset(&dummyFace, 0, sizeof(dummyFace));

        buf = ccnl_mkSimpleInterest(prefix, &int_opts);
        if (!buf) {
            fprintf(stderr, "Failed to create interest.\n");
            myexit(1); // FIXME
        }

        DEBUGMSG(DEBUG, "interest has %zd bytes\n", buf->datalen);
/*
        {
            int fd = open("outgoing.bin", O_WRONLY|O_CREAT|O_TRUNC);
            write(fd, out, len);
            close(fd);
        }
*/
        if (_ux) {
            socksize = sizeof(struct sockaddr_un);
        } else {
            socksize = sizeof(struct sockaddr_in);
        }
        rc = sendto(sock, buf->data, buf->datalen, 0, (struct sockaddr*)&_sa, socksize);
        if (rc < 0) {
            perror("sendto");
            myexit(1);
        }
        DEBUGMSG(DEBUG, "sendto returned %d\n", rc);

        for (;;) { // wait for a content pkt (ignore interests)
            unsigned char *cp = out;
            int32_t enc;
            int suite2;
            size_t len2;
            DEBUGMSG(TRACE, "  waiting for packet\n");

            if (block_on_read(sock, wait) <= 0) { // timeout
                break;
            }
            len = recv(sock, out, sizeof(out), 0);

            DEBUGMSG(DEBUG, "received %d bytes\n", len);
/*
            {
                int fd = open("incoming.bin", O_WRONLY|O_CREAT|O_TRUNC, 0700);
                write(fd, out, len);
                close(fd);
            }
*/
            suite2 = -1;
            len2 = len;
            while (!ccnl_switch_dehead(&cp, &len2, &enc)) {
                suite2 = ccnl_enc2suite(enc);
            }
            if (suite2 != -1 && suite2 != suite) {
                DEBUGMSG(DEBUG, "  unknown suite %d\n", suite);
                continue;
            }

#ifdef USE_FRAG
	    ccnl_isFragmentFunc isFragment = ccnl_suite2isFragmentFunc(_suite);
            if (isFragment && isFragment(cp, len2)) {
                int t;
                int len3;
                DEBUGMSG(DEBUG, "  fragment, %d bytes\n", len2);
                switch(suite) {
                case CCNL_SUITE_CCNTLV: {
                    struct ccnx_tlvhdr_ccnx2015_s *hp;
                    hp = (struct ccnx_tlvhdr_ccnx2015_s *) out;
                    cp = out + sizeof(*hp);
                    len2 -= sizeof(*hp);
                    if (ccnl_ccntlv_dehead(&cp, &len2, (unsigned*)&t, (unsigned*) &len3) < 0 ||
                        t != CCNX_TLV_TL_Fragment) {
                        DEBUGMSG(ERROR, "  error parsing fragment\n");
                        continue;
                    }
                    /*
                    rc = ccnl_frag_RX_Sequenced2015(frag_cb, NULL, &dummyFace,
                                      4096, hp->fill[0] >> 6,
                                      ntohs(*(uint16_t*) hp->fill) & 0x03fff,
                                      &cp, (int*) &len2);
                    */
                    rc = ccnl_frag_RX_BeginEnd2015(frag_cb, NULL, &dummyFace,
                                      4096, hp->fill[0] >> 6,
                                      ntohs(*(uint16_t*) hp->fill) & 0x03fff,
                                      &cp, (int*) &len3);
                    break;
                }
                default:
                    continue;
                }
                if (!outlen)
                    continue;
                len = outlen;
            }
#endif

/*
        {
            int fd = open("incoming.bin", O_WRONLY|O_CREAT|O_TRUNC);
            write(fd, out, len);
            close(fd);
        }
*/
            rc = ccnl_isContent(out, len, suite);
            if (rc < 0) {
                DEBUGMSG(ERROR, "error when checking type of packet\n");
                goto done;
            }
            if (rc == 0) { // it's an interest, ignore it
                DEBUGMSG(WARNING, "skipping non-data packet\n");
                continue;
            }
	    //send back to PEP-DNA
            write(csock, out, len);
            myexit(0);
        }
        if (cnt < 2)
            DEBUGMSG(WARNING, "re-sending interest\n");
    }
    fprintf(stderr, "timeout\n");

done:
    close(sock);
}

static void
handle_new_connection(int csock, unsigned int _chunknum, char *_udp, int _suite,
		      int sock, struct sockaddr _sa, char *_ux, float _wait);
{
    char content[32] = {0};
    char request[2048] = {0};
    int rc = 0;

    set_blocking(csock, false);

    rc = poll_fd(csock, POLL_READ, 1000);
    if (rc <= 0) {
        perror("poll_fd(READ)");
        close(csock);
        return;
    }

    rc = read(csock, request, 2048);
    if (rc <= 0) {
        perror("read");
        close(csock);
        return NULL;
    }
    request[rc] = '\0';

    parse_request(request, content);

    send_to_relay(content, _suite, _chunknum, _ux, sock, _sa, wait, csock);
}

static int
sock_srv_listen(int _port)
{
    struct sockaddr_in saddr;
    int sock = 0, opt = 1;
    /* open socket descriptor */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* allows us to restart server immediately */
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt, sizeof(int));

    /* bind port to socket */
    bzero((char *) &saddr, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons((unsigned short)_port);

    if (bind(sock, (struct sockaddr *) &saddr, sizeof(saddr)) < 0) {
        perror("bind");
        close(sock);
        exit(EXIT_FAILURE);
    }

    /* get us ready to accept connection requests */
    if (listen(sock, MAX_CONNS) < 0) {
        perror("listen");
        close(sock);
        exit(EXIT_FAILURE);
    }

    return sock;
}

static void
main_server_fn(int _listen_port, unsigned int _chunknum, char *_udp, int _suite,
	       char *_addr, int _port, char *_ux, float _wait)
{
    struct sockaddr sa;
    struct sockaddr_in caddr;
    socklen_t clen = sizeof(caddr);
    int lsock, sock, csock, rc;

    lsock = sock_srv_listen(_listen_port);

    if (_ux) { // use UNIX socket
        struct sockaddr_un *su = (struct sockaddr_un*) &sa;
        su->sun_family = AF_UNIX;
        strncpy(su->sun_path, ux, sizeof(su->sun_path));
        sock = ux_open();
    } else { // UDP
        struct sockaddr_in *si = (struct sockaddr_in*) &sa;
        si->sin_family = PF_INET;
        si->sin_addr.s_addr = inet_addr(_addr);
        si->sin_port = htons(_port);
        sock = udp_open();
    }

    while (running) {
        /* wait for a connection request */
        csock = accept(lsock, (struct sockaddr *) &caddr, &clen);
        if (csock < 0) {
            perror("accept");
            running = 0;
            break;
        }

	handle_new_connection(csock, _chunknum, _udp, _suite, sock, sa, _ux,
			      _wait);
    }
    close(csock);
}

int
main(int argc, char *argv[])
{
    int opt, rc, port, suite = CCNL_SUITE_NDNTLV;
    int listen_port = 8080;
    char *addr = NULL, *udp = NULL, *ux = NULL;
    float wait = 3.0;
    unsigned int chunknum = UINT_MAX;
    struct sigaction sa;
    /* Set some signal handler */
    /* Ignore SIGPIPE
     * allow the server main thread to continue even after the client ^C */
    signal(SIGPIPE, SIG_IGN);

    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = sig_handler;
    rc = sigaction(SIGINT, &sa, NULL);
    if (rc) {
        perror("sigaction(SIGINT)");
        exit(EXIT_FAILURE);
    }
    rc = sigaction(SIGTERM, &sa, NULL);
    if (rc) {
        perror("sigaction(SIGTERM)");
        exit(EXIT_FAILURE);
    }

    while ((opt = getopt(argc, argv, "hn:s:u:v:w:x:p:")) != -1) {
        switch (opt) {
        case 'n': {
            errno = 0;
            unsigned long chunknum_ul = strtoul(optarg, (char **) NULL, 10);
            if (errno || chunknum_ul > UINT_MAX) {
                goto usage;
            }
            chunknum = (unsigned int) chunknum_ul;
            break;
        }
        case 's':
            suite = ccnl_str2suite(optarg);
            if (!ccnl_isSuite(suite))
                goto usage;
            break;
        case 'u':
            udp = optarg;
            break;
        case 'v':
#ifdef USE_LOGGING
            if (isdigit(optarg[0]))
                debug_level =  (int)strtol(optarg, (char **)NULL, 10);
            else
                debug_level = ccnl_debug_str2level(optarg);
#endif
            break;
        case 'w':
            wait = strtof(optarg, (char**) NULL);
            break;
        case 'x':
            ux = optarg;
            break;
        case 'p':
            if (strchr(optarg, '-') != NULL)
                goto usage;
            listen_port = atoi(optarg);
            break;
        case 'h':
        default:
usage:
            fprintf(stderr, "usage: %s [options] URI\n"
            "  -n CHUNKNUM      positive integer for chunk interest\n"
            "  -s SUITE         (ccnb, ccnx2015, ndn2013)\n"
            "  -u a.b.c.d/port  UDP destination (default is suite-dependent)\n"
#ifdef USE_LOGGING
            "  -v DEBUG_LEVEL (fatal, error, warning, info, debug, verbose, trace)\n"
#endif
            "  -w timeout       in sec (float)\n"
            "  -x ux_path_name  UNIX IPC: use this instead of UDP\n"
            "  -p listen_port  listen port for PEP-DNA incoming requests\n"
            "Examples:\n"
            "%% peek /ndn/edu/wustl/ping             (classic lookup)\n"
            "%% peek /rpc/site \"call 1 /test/data\"   (lambda RPC, directed)\n",
            argv[0]);
            exit(1);
        }
    }

    srandom(time(NULL));

    if (ccnl_parseUdp(udp, suite, &addr, &port) != 0) {
        exit(-1);
    }
    DEBUGMSG(TRACE, "using udp address %s/%d\n", addr, port);

    /* Wait for tcp incoming connections from PEP-DNA */
    daemonize();
    main_server_fn(listen_port, chunknum, udp, suite, addr, port, ux, wait);

    return 0; // avoid a compiler warning
}

// eof
