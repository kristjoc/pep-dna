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
#include "rina/api.h"
#include "/rina/librina/src/ctrl.h"

volatile sig_atomic_t running = 1;
static void sig_handler(int signum) {
    running = 0;
}
int uflag = 0, dflag = 0;

static int rina_connect(char *difName, char *remoteAPN) {
    struct rina_flow_spec flowspec;
    char localAPN[9] = "rinawget"; int sd = 0;

    /* Issue a non-blocking flow allocation request, asking for a reliable
     * flow without message boundaries (TCP-like). */
    rina_flow_spec_unreliable(&flowspec);
    flowspec.max_sdu_gap       = 0; /* reliable QoS cube */
    flowspec.in_order_delivery = 1;
    flowspec.msg_boundaries    = 0;

    sd = rina_flow_alloc(difName, localAPN, remoteAPN, &flowspec, 0);
    if (sd < 0) {
        perror("rina_flow_alloc");
        return -1;
    }
    set_blocking(sd, false);

    return sd;
}

static int main_client_fn(char *_url, char *difName, char *localAPN)
{
    struct timespec start, end;
    char host[32]      = {0};
    int  port          = 80;
    char ip_addr[16]   = {0};
    char file_name[32] = {0};
    char header[1024]  = {0};
    char response[256] = {0};
    int  sock = 0, rc = 0, rd = 0, ru = 0;

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


    /* clients connects to the server and starts the timer */
    clock_gettime(CLOCK_REALTIME, &start);
    sock = rina_connect(difName, localAPN);
    if (sock < 0) {
        perror("rina_connect");
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

    close(sock);

    clock_gettime(CLOCK_REALTIME, &end);

    if ((dflag && rd > 0) || (uflag && ru > 0))
        printf("%.4f\n", diff_time_ms(start, end));

    return 0;
}
static int sock_srv_listen(int *regfd, char *difName, char *localAPN)
{
    int sock = rina_open();
    if (sock < 0) {
        perror("rina_open");
        exit(EXIT_FAILURE);
    }

    *regfd = rina_register(sock, difName, localAPN, 0);
    if (*regfd < 0) {
        perror("rina_register");
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
    int rc = 0, rd = 0, ru = 0;

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

        ru = upload(sock, file_name, file_size);
        rc = read(sock, response, 256);
        if (rc <= 0) {
            perror("read");
            close(sock);
            return NULL;
        }

        close(sock);
        clock_gettime(CLOCK_REALTIME, &end);

        /* if (ru > 0) */
        /*     fprintf(fp, "%.4f\n", diff_time_ms(start, end)); */
        /* fclose(fp); */

        return NULL;
    } else if (uflag) {
        rd = download(sock, file_name, file_size);
        rc = write(sock, "DONE", 5);
        if (rc < 0) {
            perror("write");
            close(sock);
            exit(EXIT_FAILURE);
        }
    }
    return NULL;
}

static void main_server_fn(char *difName, char *localAPN)
{
    struct sockaddr_in caddr;
    socklen_t clen = sizeof(caddr);
    pthread_t th_id[MAX_CONNS];
    int sock = 0, csock = 0, regfd = 0;
    int th_nr = 0, rc = 0;

    sock = sock_srv_listen(&regfd, difName, localAPN);

    while (running) {
        /* wait for a connection request */
        csock = rina_flow_accept(sock, NULL, NULL, 0);
        if (csock < 0) {
            perror("rina_flow_accept");
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

    close(regfd);
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
    char difName[12] = "normal.DIF";
    char localAPN[9] = "rinahttp";
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

    while ((opt = getopt(argc, argv, "usc:d")) != -1) {
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
        main_client_fn(url, difName, localAPN);
    } else if (sflag) {
        daemonize();
        main_server_fn(difName, localAPN);
    }

    exit(EXIT_SUCCESS);
}
