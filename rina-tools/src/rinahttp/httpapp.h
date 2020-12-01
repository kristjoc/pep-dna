#ifndef _HTTPAPP_H_
#define _HTTPAPP_H_

#include <stddef.h>
#include <time.h>

#define NET_SOFTERROR -1
#define NET_HARDERROR -2
#define POLL_READ 0
#define POLL_WRITE 1
#define MAX_CONNS 65535
#define BUFSIZE 11608    /* 1451 * 8 */

struct HTTP_RES_HEADER {
    unsigned long content_length;  //Content-Length: 11683079
    char content_type[128];        //Content-Type: application/gzip
    int status_code;               //HTTP/1.1 '200' OK
};

struct HTTP_PUT_HEADER {
    unsigned long content_length;  /* Content-Length: 11683079 */
    char filename[32];             /* File-Name: */
};

double diff_time_ms(struct timespec, struct timespec);
int poll_fd(int, int, int);
void daemonize(void);
void parse_url(const char *, char *, int *, char *);
unsigned long parse_request(char *, char *);
unsigned long get_file_size(const char *);
struct HTTP_RES_HEADER parse_res_header(const char *);
struct HTTP_PUT_HEADER parse_put_header(const char *);
int Nread(int, unsigned char *, size_t);
int Nwrite(int, unsigned char *, size_t);
void set_blocking(int fd, bool blocking);
int download(int, char *, unsigned long);
void progress_bar(unsigned long, unsigned long);
int upload(int, char *, unsigned long);
void get_ip_addr(char *, char *);

#endif // #ifndef _HTTPAPP_H_
