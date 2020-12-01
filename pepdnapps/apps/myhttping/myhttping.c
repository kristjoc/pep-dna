#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <stddef.h>
#include <math.h>
#include <fcntl.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#define NET_SOFTERROR -1
#define NET_HARDERROR -2
#define POLL_READ 0
#define POLL_WRITE 1
#define MAX_CONNS 65535
#define BUFSIZE 16384

volatile sig_atomic_t running = 1;
static void sig_handler(int signum) {
	running = 0;
}

char ip_addr[16] = {0};
int port = 0;
int concurrent_connection = 0;
int incremental = 0;
int persistent = 1;
double data[101] = {0};
double pdata[101] = {0};
int count = 4;
int j = 0;

/* Turn this program into a daemon process. */
static void daemonize(void)
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

    if (chdir("/var/www/web/"))
        exit(EXIT_FAILURE);
}

static double diff_time_ms(struct timespec start, struct timespec end)
{
    double s, ms, ns;

    s  = (double)end.tv_sec  - (double)start.tv_sec;
    ns = (double)end.tv_nsec - (double)start.tv_nsec;

    if (ns < 0) { // clock underflow
        --s;
        ns += 1000000000;
    }

    ms = ((s) * 1000 + ns/1000000.0);

    return ms;
}

static double calc_min(double *array)
{
    double min = 999;

    for (int i = 0; i < 101; ++i) {
        if (array[i] != 0) {
            if (array[i] < min)
                min = array[i];
        }
    }

    return min;
}

static double calc_max(double *array)
{
    double max = array[0];

    for (int i = 0; i < 101; ++i) {
        if (array[i] != 0) {
            if (array[i] > max)
                max = array[i];
        }
    }

    return max;
}

static double calc_mean(double *array)
{
    double sum = 0, mean = 0;
    int n = 0;

    for (int i = 0; i < 101; ++i) {
        if (array[i] != 0) {
            n++;
            sum += array[i];
        }
    }
    mean = sum / n;

    return mean;
}

static double stddev(double *array)
{
    double mean = 0, stddev = 0;
    int n = 0;

    mean = calc_mean(array);

    for (int i = 0; i < 101; ++i) {
        if (array[i] != 0) {
            n++;
            stddev += pow(array[i] - mean, 2);
        }
    }

    return sqrt(stddev / n);
}

static void print_stats(double *array)
{
    printf("\n--- %s:%d myhttping statistics ---\n", ip_addr, port);
    printf("round-trip min/avg/max/stddev = %.4f/%.4f/%.4f/%.4f ms\n",
            calc_min(array), calc_mean(array), calc_max(array), stddev(array));
}


/*
 *  SET SOCKET TO O_BLOCK/O_NONBLOCK
 */
static void set_blocking(int fd, bool blocking)
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
 *    NWRITE FUNCTION
 */
static int Nwrite(int fd, void *buf, size_t len)
{
    size_t nleft = len;
    int rc = 0;

    while (nleft > 0) {
        /* rc = poll_fd(fd, POLL_WRITE, 3000); */
        /* if (rc <= 0) { */
        /*         perror("poll_fd(WRITE)"); */
        /*         return NET_SOFTERROR; */
        /* } */

        rc = write(fd, buf, nleft);
        if (rc > 0) {
            nleft -= rc;
            buf   += rc;
            continue;
        } else if (rc < 0) {
            switch (errno) {
                case EINTR:
                case EAGAIN:
#if EWOULDBLOCK != EAGAIN
                case EWOULDBLOCK:
#endif
                    continue;

                case ENOBUFS:
                    return NET_SOFTERROR;

                default:
                    return NET_HARDERROR;
            }
        } else // rc = 0
            return NET_SOFTERROR;
    }
    return len;
}

/*
 *   NREAD FUNCTION
 */
static int Nread(int fd, void *buf, size_t len)
{
    size_t  nleft = len;
    int rc = 0;

    while (nleft > 0) {
        /* rc = poll_fd(fd, POLL_READ, 3000); */
        /* if (rc <= 0) { */
        /*         perror("poll_fd(READ)"); */
        /*         return NET_SOFTERROR; */
        /* } */

        rc = read(fd, buf, nleft);
        if (rc > 0) {
            nleft -= rc;
            buf   += rc;
            continue;
        } else if (rc < 0) {
            switch(errno) {
                case EINTR:
                case EAGAIN:
#if EWOULDBLOCK != EAGAIN
                case EWOULDBLOCK:
#endif
                    continue;

                default:
                    return NET_HARDERROR;
            }
        } else // ret = 0
            break;
    }
    return len - nleft;
}

/*
 *   POLL FILE DESCRIPTOR
 */
static int poll_fd(int fd, int poll_mode, int timeout)
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

static int prepare_client_socket(void)
{
    struct sockaddr_in caddr;
    int sock = 0, rc = 0;

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&caddr, 0, sizeof(caddr));
    caddr.sin_family = AF_INET;
    caddr.sin_addr.s_addr = inet_addr(ip_addr);
    caddr.sin_port = htons(port);

    rc = connect(sock, (struct sockaddr *) &caddr, sizeof(caddr));
    if (rc < 0) {
        perror("connect");
        close(sock);
        exit(EXIT_FAILURE);
    }

    set_blocking(sock, true);

    return sock;
}

/*****************************************************************************/
static void nonincremental_persistent_client_httping(int sock)
{
    struct timespec start, end;
    char ping[128]= {0};
    char pong[128]= {0};
    int rc = 0;

    for (int i = 0; i < count + 1;  i++) {
        clock_gettime(CLOCK_REALTIME, &start);
        rc = write(sock, ping, sizeof(ping));
        if (rc < 0) {
            perror("write");
            close(sock);
            exit(EXIT_FAILURE);
        }

        rc = read(sock, pong, sizeof(pong));
        if (rc <= 0) {
            perror("read");
            close(sock);
            exit(EXIT_FAILURE);
        }
        clock_gettime(CLOCK_REALTIME, &end);
        if (i != 0) {
            data[i] = diff_time_ms(start, end);
            pdata[j++] = data[i];
            printf("sent %lu bytes to %s:%d time %.4f\n",
                    sizeof(ping),
                    ip_addr, port,
                    diff_time_ms(start, end));
        }
        sleep(1);
    }

    print_stats(data);
}

static void nonincremental_nonpersistent_client_httping(void)
{
    struct timespec start, end;
    char ping[128]= {0};
    char pong[128]= {0};
    int rc = 0;

    for (int i = 0; i < count + 1;  i++) {
        clock_gettime(CLOCK_REALTIME, &start);
        int sock = prepare_client_socket();
        rc = write(sock, ping, sizeof(ping));
        if (rc < 0) {
            perror("write");
            close(sock);
            exit(EXIT_FAILURE);
        }

        rc = read(sock, pong, sizeof(pong));
        if (rc <= 0) {
            perror("read");
            close(sock);
            exit(EXIT_FAILURE);
        }
        close(sock);
        clock_gettime(CLOCK_REALTIME, &end);
        if (i != 0) {
            data[i] = diff_time_ms(start, end);
            pdata[j++] = data[i];
            printf("sent %lu bytes to %s:%d time %.4f\n",
                    sizeof(ping),
                    ip_addr, port,
                    diff_time_ms(start, end));
        }
        sleep(1);
    }
}

static void incremental_persistent_client_httping(int sock)
{
    struct timespec start, end;
    int body_size = 0, rc = 0;
    char tmp[16] = {0};
    char *buffer = (char *)calloc(8192, sizeof(char));
    if (NULL == buffer) {
        perror("calloc");
        close(sock);
        exit(EXIT_FAILURE);
    }

    for (int i = -1; i < 7;  i++) {
        body_size = (int)pow(2, 10 + (3 * i));
        buffer = (char *)realloc(buffer, body_size);
        if (NULL == buffer) {
            perror("realloc");
            close(sock);
            exit(EXIT_FAILURE);
        }

        memset(buffer, 'X', body_size);

        clock_gettime(CLOCK_REALTIME, &start);
        rc = Nwrite(sock, buffer, body_size);
        if (rc < 0) {
            perror("write");
            close(sock);
            free(buffer);
            exit(EXIT_FAILURE);
        }

        rc = read(sock, tmp, 8);
        if (rc <= 0) {
            perror("Nread");
            close(sock);
            free(buffer);
            exit(EXIT_FAILURE);
        }
        clock_gettime(CLOCK_REALTIME, &end);
        if (i != -1)
            printf("sent %d bytes to %s:%d time %.4f\n", body_size, ip_addr,
                   port, diff_time_ms(start, end));
        sleep(1);
    }
    free(buffer);
}

static void incremental_nonpersistent_client_httping(int con)
{
    struct timespec start, end;
    int body_size = 0, rc = 0;
    char tmp[16] = {0};
    char *buffer = (char *)calloc(8192, sizeof(char));
    if (NULL == buffer) {
        perror("calloc");
        exit(EXIT_FAILURE);
    }

    if (con < 7) {
        body_size = (int)pow(2, 10 + (3 * con));
        buffer = (char *)realloc(buffer, body_size);
        if (NULL == buffer) {
            perror("realloc");
            exit(EXIT_FAILURE);
        }

        memset(buffer, 'X', body_size);

        clock_gettime(CLOCK_REALTIME, &start);
        int sock = prepare_client_socket();
        rc = Nwrite(sock, buffer, body_size);
        if (rc < 0) {
            perror("write");
            close(sock);
            free(buffer);
            exit(EXIT_FAILURE);
        }

        rc = read(sock, tmp, 8);
        if (rc <= 0) {
            perror("Nread");
            close(sock);
            free(buffer);
            exit(EXIT_FAILURE);
        }
        close(sock);
        clock_gettime(CLOCK_REALTIME, &end);
        if (con != -1)
            printf("sent %d bytes to %s:%d time %.4f\n", body_size, ip_addr,
                   port, diff_time_ms(start, end));
        sleep(1);
    }
    free(buffer);
}

static void client(void)
{
    int sock = 0, n = count, con = 0;

    if (!persistent) {
        if (!incremental) { /* nonincremental_nonpersistent */
            for (int i = 0; i < n; i++) {
                count = 1;
                nonincremental_nonpersistent_client_httping();
            }
            print_stats(pdata);
            return;
        } else { /* incremental_nonpersistent */
            for (con = -1; con < 7; con++)
                incremental_nonpersistent_client_httping(con);
            return;
        }
    } else { /* persistent */
        sock = prepare_client_socket();

        if (incremental) /* incremental_persistent */
            incremental_persistent_client_httping(sock);
        else 		 /* incremental_persistent */
            nonincremental_persistent_client_httping(sock);

        close(sock);
    }
}


static void *cthread_cpumem_fn(void *data)
{
    int asock = *(int *)data;
    int burst = 128 * 1024; // approx. 128KB
    int rc = 0;

    char *buffer = (char *)calloc(burst, sizeof(char));
    time_t endwait;
    int seconds = 25; // end loop after this time has elapsed
    endwait = time(NULL) + seconds;

    while (running && (time(NULL) < endwait)) {
        /* set_blocking(asock, false); */
        memset(buffer, 'X', burst);
        rc = Nwrite(asock, buffer, burst);
        if (rc < 0) {
            perror("write");
            close(asock);
            free(buffer);
            return NULL;
        }
        usleep(100000);
    }

    shutdown(asock, SHUT_RDWR);
    close(asock);
    free(buffer);

    return NULL;
}


static void cpumem_cln_perf()
{
    pthread_t thread[MAX_CONNS];
    int asock[MAX_CONNS];
    int id = 0, rc = 0;

    for (id = 0; id < count; ++id) {
        asock[id] = prepare_client_socket();
        usleep(1000);
    }

    for (id = 0; id < count; ++id) {
        rc = pthread_create(&thread[id], NULL, cthread_cpumem_fn,
                            (void *)&asock[id]);
        if (rc < 0) {
            perror("pthread_create");
            close(asock[id]);
        }

    }

    for (id = 0; id < count; ++id)
        pthread_join(thread[id], NULL);
}
/*****************************************************************************/
/*
 *  PARSE GET REQUEST
 */
unsigned long parse_get_request(char *request, char *filename)
{
    struct stat buf;

    char *pos = strstr(request, "GET");

    if (pos)
        sscanf(pos, "%*s %s", filename);

    if (stat(filename, &buf) < 0)
        return 0;

    return (unsigned long)buf.st_size;
}

static int upload(int sock, char *file_name, unsigned long file_size)
{
    unsigned long hassent = 0;
    unsigned char buf[BUFSIZE];
    int len = 0, rc = 0;

    FILE *fp = fopen(file_name, "rb");
    if (fp == NULL) {
        perror("fopen");
        return -1;
    }

    set_blocking(sock, true);

    while (running && (len = fread(buf, 1, BUFSIZE, fp)) > 0 ) {
        rc = write(sock, buf, len);
        if (rc < 0)
            break;
        hassent += rc;
    }

    fclose(fp);

    /* printf("Uploaded  %lu / %lu bytes.\n", hassent, file_size); */
    if (hassent == file_size)
        return 1;
    else
        return 0;
}

static void *pthread_fn(void *data)
{
    int asock = *(int *)data;
    char response[1468] = {0};
    char file_name[16] = {0};
    unsigned long file_size;
    char request[256] = {0};
    int rc = 0;

    set_blocking(asock, false);

    while (1) {
        rc = poll_fd(asock, POLL_READ, 1000);
        if (rc <= 0) {
            /* perror("poll_fd(READ)"); */
            close(asock);
            return NULL;
        }

        rc = read(asock, request, 256);
        if (rc <= 0) {
            /* perror("read"); */
            close(asock);
            return NULL;
        }
        request[rc] = '\0';

        file_size = parse_get_request(request, file_name);
        if (file_size > 0) {
            sprintf(response, \
                    "HTTP/1.1 200 OK\r\n"\
                    "Content-Type: text/html\r\n"\
                    "Accept-Ranges: bytes\r\n"\
                    "Content-length: %ld\n"\
                    "Server: myhttperf 1.1 (Unix)\r\n"\
                    "\r\n"\
                    , file_size);
        } else {
            sprintf(response, \
                    "HTTP/1.1 404 Not Found\n"\
                    "Content-length: %d\n"\
                    "\r\n"\
                    , 0);
        }

        rc = write(asock, response, strlen(response));
        if (rc < 0) {
            perror("write");
            close(asock);
            return NULL;
        }

        rc = upload(asock, file_name, file_size);
        if (!rc)
            printf("Error during upload\n");
    }

    shutdown(asock, SHUT_RDWR);
    close(asock);

    return NULL;
}

/******************************************************************************
 * SERVER side
 ******************************************************************************/
static int prepare_server_socket(void)
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
    saddr.sin_family      = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port        = htons((unsigned short)port);

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

static void pong(int sock)
{
    char ping[128] = {0};
    char pong[128] = {0};
    int rc = 0;

    for (int i = 0; i < 101;  i++) {
        rc = read(sock, ping, sizeof(ping));
        if (rc <= 0) {
            /* perror("read"); */
            close(sock);
            return;
        }
        rc = write(sock, pong, sizeof(pong));
        if (rc < 0) {
            /* perror("write"); */
            close(sock);
            return;
        }
    }
}

static void incremental_persistent_server_httping(int sock)
{
    int body_size = 0, rc = 0;
    char *buffer = (char *)calloc(8192, sizeof(char));
    if (NULL == buffer) {
        perror("calloc");
        close(sock);
        exit(EXIT_FAILURE);
    }

    for (int i = -1; i < 7;  i++) {
        body_size = (int)pow(2, 10 + (3 * i));
        buffer = (char *)realloc(buffer, body_size);
        if (NULL == buffer) {
            perror("realloc");
            close(sock);
            free(buffer);
            return;
        }
        memset(buffer, 0, body_size);

        rc = Nread(sock, buffer, body_size);
        if (rc <= 0) {
            perror("Nread");
            close(sock);
            free(buffer);
            return;
        }
        rc = write(sock, "HTTP-OK", 8);
        if (rc < 0) {
            perror("Nwrite");
            close(sock);
            free(buffer);
            return;
        }
    }
    free(buffer);
}

static void incremental_nonpersistent_server_httping(int sock, int con)
{
	int body_size = 0, rc = 0;
	char *buffer = (char *)calloc(8192, sizeof(char));
	if (NULL == buffer) {
		perror("calloc");
		close(sock);
		exit(EXIT_FAILURE);
	}

	if (con < 7) {
		body_size = (int)pow(2, 10 + (3 * con));
		buffer = (char *)realloc(buffer, body_size);
		if (NULL == buffer) {
			perror("realloc");
			close(sock);
			free(buffer);
			return;
		}

		memset(buffer, 0, body_size);

		rc = Nread(sock, buffer, body_size);
		if (rc <= 0) {
			perror("Nread");
			close(sock);
			free(buffer);
			return;
		}

		rc = write(sock, "HTTP-OK", 8);
		if (rc < 0) {
			perror("Nwrite");
			close(sock);
			free(buffer);
			return;
		}
	}

	free(buffer);
}

static void server(void)
{
    struct sockaddr_in caddr;
    socklen_t clen = sizeof(caddr);
    int sock = 0, asock = 0, con = -1;

    sock = prepare_server_socket();

    while (running) {
        /* wait for a connection request */
        asock = accept(sock, (struct sockaddr *) &caddr, &clen);
        if (asock < 0) {
            /* perror("accept"); */
            running = 0;
            break;
        }

        set_blocking(asock, true);

        if (incremental && persistent)
            incremental_persistent_server_httping(asock);
        else if (incremental && !persistent)
            incremental_nonpersistent_server_httping(asock, con++);
        else
            pong(asock);

        close(asock);
    }
}

static void webperf()
{
    struct sockaddr_in caddr;
    socklen_t clen = sizeof(caddr);
    pthread_t thread[MAX_CONNS];
    int sock = 0, asock[MAX_CONNS];
    int id = 0, rc = 0;

    sock = prepare_server_socket();

    while (running) {
        /* wait for a connection request */
        asock[id] = accept(sock, (struct sockaddr *) &caddr, &clen);
        if (asock[id] < 0) {
            /* perror("accept"); */
            running = 0;
            break;
        }

        rc = pthread_create(&thread[id], NULL, pthread_fn, (void *)&asock[id]);
        if (rc < 0) {
            perror("pthread_create");
            close(asock[id]);
        }

        id++;
    }

    for (int i = 0; i < id; i++)
        pthread_join(thread[i], NULL);

    close(sock);
}

static void *sthread_cpumem_fn(void *data)
{
    int asock = *(int *)data;
    char buffer[8192] = {0};
    int rc = 0;

    /* set_blocking(asock, false); */

    while (running) {
        rc = read(asock, buffer, 8192);
        if (rc <= 0) {
            /* perror("read"); */
            close(asock);
            return NULL;
        }
    }

    shutdown(asock, SHUT_RDWR);
    close(asock);

    return NULL;
}

static void cpumem_srv_perf()
{
    pthread_t thread[MAX_CONNS];
    int sock = 0, asock[MAX_CONNS];
    int id = 0, rc = 0;

    sock = prepare_server_socket();

    while (running) {
        /* wait for a connection request */
        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);
        asock[id] = accept(sock, (struct sockaddr *) &caddr, &clen);
        if (asock[id] < 0) {
            /* perror("accept"); */
            running = 0;
            break;
        }

        rc = pthread_create(&thread[id], NULL, sthread_cpumem_fn,
                            (void *)&asock[id]);
        if (rc < 0) {
            perror("pthread_create");
            close(asock[id]);
        }

        id++;
    }

    for (int i = 0; i < id; i++)
        pthread_join(thread[i], NULL);

    close(sock);
}

/******************************************************************************
 * MAIN
 ******************************************************************************/
int main(int argc, char *argv[])
{
    int cflag = 0, sflag = 0, perflag = 0, cpumemflag = 0;
    struct sigaction sa;
    int opt, rc;

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

    while ((opt = getopt(argc, argv, "sfmc:p:ian:")) != -1) {
        switch (opt) {
            case 's':
                if (cflag == 1) {
                    fprintf(stderr, "%s can't run both as client & server\n",
                            argv[0]);
                    exit(EXIT_FAILURE);
                }
                sflag = 1;
                break;
            case 'f':
                perflag = 1;
                break;
            case 'm':
                cpumemflag = 1;
                break;
            case 'c':
                if (strchr(optarg, '-') != NULL) {
                    fprintf(stderr, "[-c ip] -- wrong option\n");
                    exit(EXIT_FAILURE);
                }
                if (sflag == 1) {
                    fprintf(stderr, "%s cannot run both as client and server\n",
                            argv[0]);
                    exit(EXIT_FAILURE);
                }
                cflag = 1;
                strcpy(ip_addr, optarg);
                break;
            case 'p':
                if (strchr(optarg, '-') != NULL) {
                    fprintf(stderr, "[-p port] -- wrong option\n");
                    exit(EXIT_FAILURE);
                }
                port = atoi(optarg);
                break;
            case 'i':
                incremental = 1;
                break;
            case 'a':
                persistent = 0;
                break;
            case 'n':
                count = atoi(optarg);
                break;
            default: /* '?' */
                fprintf(stderr, "Usage: %s [-s] server"
                        " [-f] httperf"
                        " [-m] cpumem"
                        " [-c ip]"
                        " [-p port]"
                        " [-i] incremental "
                        " [-a] nonpersistent"
                        " [-n count]\n",
                        argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (optind < argc) {
        fprintf(stderr, "Expected argument after option %s\n", argv[optind]);
        exit(EXIT_FAILURE);
    }

    if (cflag) {
        if (cpumemflag)
            cpumem_cln_perf();
        else
            client();
    } else if (sflag) {
        if (perflag) {
            daemonize();
            webperf();
        } else if (cpumemflag) {
            daemonize();
            cpumem_srv_perf();
        } else {
            daemonize();
            server();
        }
    }

    exit(EXIT_SUCCESS);
}
