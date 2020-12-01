#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "httpapp.h"


volatile sig_atomic_t running = 1;
static void sig_handler(int signum) {
    running = 0;
}
int uflag = 0, dflag = 0;

int main_client_fn(char *_url)
{
    struct timespec start, end;
    char host[32]      = {0};
    int  port          = 80;
    char ip_addr[16]   = {0};
    char file_name[32] = {0};
    char header[1024]  = {0};
    char response[256] = {0};
    int  sock = 0;
    int rc = 0, rd = 0, ru = 0;

    parse_url(_url, host, &port, file_name);

    get_ip_addr(host, ip_addr);
    if (strlen(ip_addr) == 0) {
        fprintf(stderr, "Bad IP address!\n");
        exit(EXIT_FAILURE);
    }

    if (dflag) {
        sprintf(header, \
                "GET %s HTTP/1.1\r\n"\
                "Accept: */*\r\n"\
                "User-Agent: My Wget Client\r\n"\
                "Host: %s\r\n"\
                "Connection: Keep-Alive\r\n"\
                "\r\n"\
                , file_name, host);
    } else if (uflag) {
        struct stat buf;
        if (stat(file_name, &buf) < 0) {
            fprintf(stderr, "'%s' file does not exist!\n", file_name);
            exit(EXIT_FAILURE);
        }

        sprintf(header, \
                "PUT %s HTTP/1.1\r\n"\
                "Content-length: %ld\r\n"\
                "User-Agent: My Wget Client\r\n"\
                "Host: %s\r\n"\
                "Connection: Keep-Alive\r\n"\
                "\r\n"\
                , file_name, (unsigned long)buf.st_size, host);
    }

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip_addr);
    addr.sin_port = htons(port);

    /* clients connects to the server and starts the timer */
    clock_gettime(CLOCK_REALTIME, &start);
    rc = connect(sock, (struct sockaddr *) &addr, sizeof(addr));
    if (rc < 0) {
        perror("connect");
        close(sock);
        exit(EXIT_FAILURE);
    }
    set_blocking(sock, false);

    /* client sends the GET/PUT header to the server */
    rc = write(sock, header, strlen(header));
    if (rc < 0) {
        perror("write");
        close(sock);
        exit(EXIT_FAILURE);
    }

    rc = poll_fd(sock, POLL_READ, 1000);
    if (rc <= 0) {
        fprintf(stderr, "ERROR during poll()\n");
        close(sock);
        exit(EXIT_FAILURE);
    }

    /* client read the HTTP response from server */
    rc = read(sock, response, 256);
    if (rc <= 0) {
        perror("read");
        close(sock);
        exit(EXIT_FAILURE);
    }

    /* client parses HTTP response headers */
    response[rc] = '\0';
    struct HTTP_RES_HEADER resp = parse_res_header(response);

    if (resp.status_code != 200) {
        printf("File cannot be downloaded: %d\n", resp.status_code);
        close(sock);
        exit(EXIT_FAILURE);
    }

    if (dflag) {
        rc = write(sock, "OK", 3);
        if (rc < 0) {
            perror("write");
            close(sock);
            exit(EXIT_FAILURE);
        }

        rd = download(sock, file_name, resp.content_length);
        rc = write(sock, "DONE", 5);
        if (rc < 0) {
            perror("write");
            close(sock);
            exit(EXIT_FAILURE);
        }
    } else if (uflag) {
        ru = upload(sock, file_name, resp.content_length);
        rc = read(sock, response, 256);
        if (rc <= 0) {
            perror("read");
            close(sock);
            exit(EXIT_FAILURE);
        }
    }

    shutdown(sock, SHUT_WR);
    close(sock);

    clock_gettime(CLOCK_REALTIME, &end);

    if ((dflag && rd > 0) || (uflag && ru > 0))
        printf("%.4f\n", diff_time_ms(start, end));

    return 0;
}
int sock_srv_listen(int _port)
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

static void *sthread_fn(void *data)
{
    struct timespec start, end;
    int sock = *(int *)data;
    unsigned long file_size;
    char response[256] = {0};
    char file_name[32] = {0};
    char request[2048] = {0};
    int rc = 0;

    clock_gettime(CLOCK_REALTIME, &start);

    /* Uncomment this to log the fct at the server side */
    /* FILE *fp = NULL; */
    /* fp = fopen("/home/ocarina/kristjoc/fs.dat", "ab+"); */
    /* if (NULL == fp) { */
    /*     perror("fopen"); */
    /*     close(csock); */
    /*     return NULL; */
    /* } */

    set_blocking(sock, false);

    rc = poll_fd(sock, POLL_READ, 1000);
    if (rc <= 0) {
        perror("poll_fd(READ)");
        close(sock);
        return NULL;
    }

    rc = read(sock, request, 2048);
    if (rc <= 0) {
        perror("read");
        close(sock);
        return NULL;
    }
    request[rc] = '\0';

    file_size = parse_request(request, file_name);
    if (file_size > 0) {
        sprintf(response, \
                "HTTP/1.1 200 OK\n"\
                "Content-length: %ld\n"\
                "\r\n"\
                ,file_size);
    } else {
        sprintf(response, \
                "HTTP/1.1 404 Not Found\n"\
                "Content-length: %ld\n"\
                "\r\n"\
                ,file_size);
    }

    rc = write(sock, response, strlen(response));
    if (rc < 0) {
        perror("write");
        close(sock);
        return NULL;
    }

    if (dflag) {
        rc = poll_fd(sock, POLL_READ, 1000);
        if (rc <= 0) {
            perror("poll_fd(READ)");
            close(sock);
            return NULL;
        }

        rc = read(sock, response, 256);
        if (rc <= 0) {
            perror("read");
            close(sock);
            return NULL;
        }

        if (upload(sock, file_name, file_size) < 0) {
           perror("upload");
           close(sock);
           return NULL;
        }
        rc = read(sock, response, 256);
        if (rc <= 0) {
            perror("read");
            close(sock);
            return NULL;
        }

        shutdown(sock, SHUT_RDWR);
        close(sock);

        clock_gettime(CLOCK_REALTIME, &end);

        /* if (ru > 0) */
        /*     fprintf(fp, "%.4f\n", diff_time_ms(start, end)); */
        /* fclose(fp); */

        return NULL;
    } else if (uflag) {
        if (download(sock, file_name, file_size) < 0) {
            perror("download");
            close(sock);
            return NULL;
        }
        rc = write(sock, "DONE", 5);
        if (rc < 0) {
            perror("write");
            close(sock);
            return NULL;
        }
    }
    return NULL;
}

static void main_server_fn(int _port)
{
    struct sockaddr_in caddr;
    socklen_t clen = sizeof(caddr);
    pthread_t th_id[MAX_CONNS];
    int sock, csock, rc;
    int th_nr = 0;

    sock = sock_srv_listen(_port);

    while (running) {
        /* wait for a connection request */
        csock = accept(sock, (struct sockaddr *) &caddr, &clen);
        if (csock < 0) {
            perror("accept");
            running = 0;
            break;
        }

        if (th_nr >= MAX_CONNS) {
            perror("Max connection reached");
            close(csock);
            running = 0;
            break;
        }

        rc = pthread_create(&th_id[th_nr++], NULL, sthread_fn, (void *)&csock);
        if (rc < 0) {
            perror("pthread_create");
            close(csock);
        }
    }

    for (int i = 0; i < th_nr; i++)
        pthread_join(th_id[i], NULL);

    close(sock);
}

/******************************************************************************
 * MAIN
 ******************************************************************************/
int main(int argc, char *argv[])
{
    int cflag = 0, sflag = 0;
    struct sigaction sa;
    char url[64] = "127.0.0.1";
    int srvport = 80;
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

    while ((opt = getopt(argc, argv, "usc:dp:")) != -1) {
        switch (opt) {
        case 's':
            if (cflag == 1) {
                fprintf(stderr, "%s can't run both as client & server\n",
                        argv[0]);
                exit(EXIT_FAILURE);
            }
            sflag = 1;
            break;
        case 'c':
            if (strchr(optarg, '-') != NULL) {
                    fprintf(stderr, "[-c url] -- wrong option\n");
                    exit(EXIT_FAILURE);
                }
            if (sflag == 1) {
                fprintf(stderr, "%s cannot run both as client and server\n",
                        argv[0]);
                exit(EXIT_FAILURE);
            }
            cflag = 1;
            if (strstr(optarg, "http"))
                strcpy(url, optarg);
            else {
                fprintf(stderr, "URL should start with http\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 'p':
            if (strchr(optarg, '-') != NULL) {
                fprintf(stderr, "[-p port] -- wrong option\n");
                exit(EXIT_FAILURE);
            }
            srvport = atoi(optarg);
            break;
        case 'u':
            uflag = 1;
            break;
        case 'd':
            dflag = 1;
            break;
        default: /* '?' */
            fprintf(stderr, "Usage: %s\n"
                            "[-s] server\n"
                            "[-c url]\n"
                            "[-p server port]\n"
                            "[-u] upload\n"
                            "[-d] download\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (optind < argc) {
        fprintf(stderr, "Expected argument after option %s\n", argv[optind]);
        exit(EXIT_FAILURE);
    }
    if (!cflag && !sflag) {
        fprintf(stderr, "Client or Server flag is mandatory!\n");
    }

    if (cflag) {
        if (!uflag && !dflag) {
            fprintf(stderr, "Download or Upload flag is mandatory!\n");
            exit(EXIT_FAILURE);
        }
        if (chdir("/var/www/web/")) {
            exit(EXIT_FAILURE);
        }
        main_client_fn(url);
    } else if (sflag) {
        daemonize();
        main_server_fn(srvport);
    }

    exit(EXIT_SUCCESS);
}
