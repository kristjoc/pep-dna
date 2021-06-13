#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>

#include <arpa/inet.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "httpapp.h"

extern volatile sig_atomic_t running;
extern int uflag;
extern int dflag;

double diff_time_ms(struct timespec start, struct timespec end) {
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

/* Turn this program into a daemon process. */
void daemonize(void)
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

    if (chdir("/var/www/web/")) {
        exit(EXIT_FAILURE);
    }
}

/*
 *   POLL FILE DESCRIPTOR
 */
int poll_fd(int fd, int poll_mode, int timeout)
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
void set_blocking(int fd, bool blocking)
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
 * PARSE URL FUNCTION
 */
void parse_url(const char *url, char *host, int *port, char *file_name)
{
    int start = 0;
    *port     = 80;
    int j     = 0;
    char *proto[] = {"http://", "https://", NULL};

    for (int i = 0; proto[i]; i++)
        if (strncmp(url, proto[i], strlen(proto[i])) == 0)
            start = strlen(proto[i]);

    for (int i = start; url[i] != '/' && url[i] != '\0'; i++, j++)
        host[j] = url[i];
    host[j] = '\0';

    char *pos = strstr(host, ":");
    if (pos)
        sscanf(pos, ":%d", port);

    for (int i = 0; i < (int)strlen(host); i++) {
        if (host[i] == ':') {
            host[i] = '\0';
            break;
        }
    }

    j = 0;
    for (int i = start; url[i] != '\0'; i++) {
        if (url[i] == '/') {
            if (i !=  strlen(url) - 1)
                j = 0;
            continue;
        } else
            file_name[j++] = url[i];
    }
    file_name[j] = '\0';
}

/*
 *  PARSE GET REQUEST
 */
unsigned long parse_request(char *request, char *filename)
{
    struct HTTP_PUT_HEADER put;
    struct stat buf;

    char *pos = strstr(request, "GET");
    if (pos) {
        dflag = 1;
        uflag = 0;
        sscanf(pos, "%*s %s", filename);

        if (stat(filename, &buf) < 0)
            return 0;
        return (unsigned long)buf.st_size;
    }
    else if (!pos) {
        pos = strstr(request, "PUT");
        if (pos) {
            uflag = 1;
            dflag = 0;
            put = parse_put_header(request);
            strcpy(filename, put.filename);
                return put.content_length;
        }
    }
    return 0;
}

/*
 *   NREAD FUNCTION
 */
int Nread(int fd, unsigned char *buf, size_t count)
{
    size_t  nleft = count;
    int rc = 0;

    while (nleft > 0) {
        rc = poll_fd(fd, POLL_READ, 1000);
        if (rc <= 0) {
            perror("poll_fd(READ)");
            return NET_SOFTERROR;
        }

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
    return count - nleft;
}

/*
 *    NWRITE FUNCTION
 */
int Nwrite(int fd, unsigned char *buf, size_t count)
{
    size_t nleft = count;
    int rc = 0;

    while (nleft > 0) {
        rc = poll_fd(fd, POLL_WRITE, 1000);
        if (rc <= 0) {
            perror("poll_fd(WRITE)");
            return NET_SOFTERROR;
        }

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
    return count;
}

/*
 *    UPLOAD FILE
 */
int upload(int sock, char *file_name, unsigned long file_size)
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
    if (hassent == file_size) {
        /* printf("It is finished! :-)\n"); */
        return 1;
    } else {
        /* printf("Download failed! :-(\n"); */
        return -1;
    }
}


/*
 * PARSE RESPONSE
 */
struct HTTP_RES_HEADER parse_res_header(const char *response)
{
    struct HTTP_RES_HEADER resp;

    char *pos = strstr(response, "HTTP/");
    if (pos)
        sscanf(pos, "%*s %d", &resp.status_code);

    pos = strstr(response, "Content-Type:");
    if (pos)
        sscanf(pos, "%*s %s", resp.content_type);

    pos = strstr(response, "Content-length:");
    if (pos)
        sscanf(pos, "%*s %ld", &resp.content_length);

    return resp;
}

/*
 * PARSE HTTP PUT REQUEST
 */
struct HTTP_PUT_HEADER parse_put_header(const char *header)
{
    struct HTTP_PUT_HEADER put;

    char *pos = strstr(header, "PUT");
    if (pos)
        sscanf(pos, "%*s %s", put.filename);

    pos = strstr(header, "Content-length:");
    if (pos)
        sscanf(pos, "%*s %ld", &put.content_length);

    return put;
}

/*
 *  GET HOST BY NAME
 */
void get_ip_addr(char *host_name, char *ip_addr)
{
    struct hostent *host = gethostbyname(host_name);
    if (!host) {
        ip_addr = NULL;
        return;
    }

    for (int i = 0; host->h_addr_list[i]; i++) {
        strcpy(ip_addr, inet_ntoa( * (struct in_addr*) host->h_addr_list[i]));
        break;
    }
}


/*
 *  GET FILE SIZE
 */
unsigned long get_file_size(const char *filename)
{
    struct stat buf;

    if (stat(filename, &buf) < 0)
        return 0;

    return (unsigned long)buf.st_size;
}


/*
 *  PROGRESS BAR
 */
void progress_bar(unsigned long cur_size, unsigned long total_size)
{
    float percent = (float) cur_size / total_size;
    const int numTotal = 50;
    int numShow = (int)(numTotal * percent);

    if (numShow == 0)
        numShow = 1;

    if (numShow > numTotal)
        numShow = numTotal;

    char sign[51] = {0};
    memset(sign, '=', numTotal);

    printf("\r%d%%[%-*.*s] %d/%dMiB", (int)(percent * 100), numTotal, numShow,
            sign, (int)(cur_size / 1024.0 / 1024.0),
            (int)(total_size / 1024.0 / 1024.0));
    fflush(stdout);

    if (numShow == numTotal)
        printf("\n");
}


/*
 *  DOWNLOAD FUNCTION
 */
int download(int sock, char *file_name, unsigned long content_length)
{
    unsigned long hasrecieve = 0;
    unsigned char buf[BUFSIZE] = {0};
    int len = 0;

    int fp = open(file_name, O_CREAT | O_WRONLY, S_IRWXG | S_IRWXO | S_IRWXU);
    if (fp < 0) {
        perror("fopen");
        return -1;
    }

    set_blocking(sock, true);
    if (content_length) {
	while (hasrecieve < content_length) {
	    len = read(sock, buf, BUFSIZE);
	    if (len > 0) {
		if ((write(fp, buf, len)) < 0)
		    break;
		hasrecieve += len;
		/* progress_bar(hasrecieve, content_length); */
	    } else
		break;
	}
    } else {
	do {
	    len = read(sock, buf, BUFSIZE);
	    if (len > 0) {
		if ((write(fp, buf, len)) < 0)
		    break;
		hasrecieve += len;
		/* progress_bar(hasrecieve, content_length); */
	    } else
		break;
	} while (len > 0);
    }

    close(fp);

    /* printf("\nDownloaded  %lu / %lu bytes.\n", hasrecieve, content_length); */
    if (hasrecieve == content_length || !content_length) {
        /* printf("It is finished! :-)\n"); */
        return 1;
    } else {
        /* printf("Download failed! :-(\n"); */
        remove(file_name);
        return -1;
    }
}
