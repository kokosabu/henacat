#define _POSIX_C_SOURCE 1 /* glibcでfdopenを使うための定義 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/uio.h>
#include <time.h>

#include <pthread.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>

enum {
    PORT_NUMBER  = 8001,
    BUF_SIZE     = 1024,
    EXT          = 0,
    CONTENT_TYPE = 1
};

typedef struct {
    FILE *socket_fp;
    int fd;
} thread_arg;

char *table[][2] = {
    {"html", "text/html"},
    {"htm",  "text/html"},
    {"txt",  "text/plain"},
    {"css",  "text/css"},
    {"png",  "image/png"},
    {"jpg",  "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"gif",  "image/gif"},
    {"ico",  "image/ico"},
    {NULL,   "text/plain"}
};

void make_date(char d[], int buf_size)
{
    time_t timep;
    struct tm *time_inf;

    timep = time(NULL);
    time_inf = gmtime(&timep);
    strftime(d, buf_size, "%a, %b %d %H:%M:%S %G", time_inf);
}

void response_header_200(FILE *socket_fp, int index)
{
    char d[BUF_SIZE];

    make_date(d, BUF_SIZE);

    fprintf(socket_fp, "HTTP/1.1 200 OK\n");
    fprintf(socket_fp, "Date: %s\n", d);
    fprintf(socket_fp, "Server: Modoki/0.1\n");
    fprintf(socket_fp, "Connection: close\n");
    fprintf(socket_fp, "Content-type: %s\n", table[index][CONTENT_TYPE]);
    fprintf(socket_fp, "\n");
}

void response_header_301(FILE *socket_fp, int index, char *path)
{
    char d[BUF_SIZE];

    make_date(d, BUF_SIZE);

    fprintf(socket_fp, "HTTP/1.1 301 Moved Permanently\n");
    fprintf(socket_fp, "Date: %s\n", d);
    fprintf(socket_fp, "Server: Modoki/0.1\n");
    fprintf(socket_fp, "Location: %s\n", path);
    fprintf(socket_fp, "Connection: close\n");
    fprintf(socket_fp, "Content-type: %s\n", table[index][CONTENT_TYPE]);
    fprintf(socket_fp, "\n");
}

void response_header_404(FILE *socket_fp, int index)
{
    char d[BUF_SIZE];

    make_date(d, BUF_SIZE);

    fprintf(socket_fp, "HTTP/1.1 404 OK\n");
    fprintf(socket_fp, "Date: %s\n", d);
    fprintf(socket_fp, "Server: Modoki/0.1\n");
    fprintf(socket_fp, "Connection: close\n");
    fprintf(socket_fp, "Content-type: %s\n", table[index][CONTENT_TYPE]);
    fprintf(socket_fp, "\n");
}

void response_body(FILE *socket_fp, char *file_name)
{
    FILE *file_in_fp;
    char line[BUF_SIZE];

    file_in_fp = fopen(file_name, "r");
    while(fgets(line, BUF_SIZE, file_in_fp) != NULL) {
        fprintf(socket_fp, "%s", line);
    }
    fclose(file_in_fp);
}

void thread(void *p)
{
    FILE *socket_fp;
    int fd;
    FILE *file_in_fp;
    char line[BUF_SIZE];
    char file_name[BUF_SIZE];
    char file_name2[BUF_SIZE];
    char real[BUF_SIZE];
    char pathname[BUF_SIZE];
    char location[BUF_SIZE];
    char *ext;
    int index;
    struct stat st;
    int result;

    socket_fp = ((thread_arg *)p)->socket_fp;
    fd        = ((thread_arg *)p)->fd;

    while(fgets(line, BUF_SIZE, socket_fp) != NULL) {
        if (strcmp(line, "\r\n") == 0 || strcmp(line, "\n") == 0) {
            break;
        }
        if (strncmp(line, "GET", 3) == 0) {
            fprintf(stderr, "[%s]", line);
            strtok(line, " ");
            strcpy(file_name, ".");
            strcat(file_name, strtok(NULL, " "));
            strcpy(file_name2, file_name);
            ext = strtok(file_name2, ".");
            ext = strtok(NULL, ".");

            for(index = 0; index < ((sizeof(table)/sizeof(table[0]))-1); index++) {
                if(ext == NULL) {
                    index = sizeof(table)/sizeof(table[0]) - 1;
                    break;
                }
                if(strcmp(ext, table[index][EXT]) == 0) {
                    break;
                }
            }
        }
    }

    getcwd(pathname, BUF_SIZE);
    realpath(file_name, real);
    fprintf(stderr, "path: %s\n", pathname);
    fprintf(stderr, "real: %s\n", real);

    if(file_name[strlen(file_name)-1] == '/') {
        index = 0;
        strcat(file_name, "index.html");
    } else {
        result = stat(real, &st);
        if ((st.st_mode & S_IFMT) == S_IFDIR) {
            fprintf(stderr, "---- 1 [301] ----\n");
            index = 0;
            strcpy(location, "http://localhost:8001/");
            strcat(location, file_name);
            strcat(location, "/");
            fprintf(stderr, "trav file_name : %s\n", location);
            response_header_301(socket_fp, index, location);
            return;
        }
    }

    socket_fp = fdopen(fd, "w");
    file_in_fp = fopen(file_name, "r");
    fprintf(stderr, "file_name : %s\n", file_name);

    if(file_in_fp == NULL) {
        fprintf(stderr, "---- 2 [404 file notfound] ----\n");
        index = 0;
        response_header_404(socket_fp, index);
        response_body(socket_fp, "./404.html");
    } else if(strncmp(real, pathname, strlen(pathname)) != 0) {
        fprintf(stderr, "---- 3 [404 traversal] ----\n");
        index = 0;
        response_header_404(socket_fp, index);
        response_body(socket_fp, "./404.html");
    } else {
        fprintf(stderr, "---- 4 [200 file found] ----\n");
        response_header_200(socket_fp, index);
        response_body(socket_fp, file_name);
    }

    fclose(socket_fp);
}

int main(int argc, char **argv)
{
    int sock;
    struct sockaddr_in addr;
    int ch;
    pthread_t pthread;
    thread_arg arg;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT_NUMBER);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    ch = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    ch = listen(sock, 6);

    while(1) {
        arg.fd        = accept(sock, NULL, NULL);
        arg.socket_fp = fdopen(arg.fd, "r+");
        pthread_create( &pthread, NULL, (void *)&thread, &arg);
    }

    return 0;
}

