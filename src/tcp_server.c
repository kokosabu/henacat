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

typedef struct {
    FILE *socket_fp;
    int fd;
} thread_arg;

char *table[10][2] = {
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

void response_header_200(FILE *socket_fp, int index)
{
    time_t timep;
    struct tm   *time_inf;
    char d[1024];

    timep = time(NULL);
    time_inf = gmtime(&timep);

    fprintf(socket_fp, "HTTP/1.1 200 OK\n");
    //fprintf(socket_fp, "Date: %s\n", asctime(time_inf));
    fprintf(stderr, "Date: %s\n", asctime(time_inf));
    strftime(d, 1024, "%Y年%m月%d日 %H時%M分%S秒", time_inf);
    fprintf(socket_fp, "Date: %s\n", d);
    fprintf(socket_fp, "Server: Modoki/0.1\n");
    fprintf(socket_fp, "Connection: close\n");
    fprintf(socket_fp, "Content-type: %s\n", table[index][1]);
    fprintf(stderr, "Content-type: %s\n", table[index][1]);
    fprintf(socket_fp, "\n");
}

void response_header_301(FILE *socket_fp, int index, char *path)
{
    time_t timep;
    struct tm   *time_inf;
    char d[1024];

    timep = time(NULL);
    time_inf = gmtime(&timep);

    fprintf(socket_fp, "HTTP/1.1 301 Moved Permanently\n");
    //fprintf(socket_fp, "Date: %s\n", asctime(time_inf));
    fprintf(stderr, "Date: %s\n", asctime(time_inf));
    strftime(d, 1024, "%Y年%m月%d日 %H時%M分%S秒", time_inf);
    fprintf(socket_fp, "Date: %s\n", d);
    fprintf(socket_fp, "Server: Modoki/0.1\n");
    fprintf(socket_fp, "Location: %s\n", path);
    fprintf(socket_fp, "Connection: close\n");
    fprintf(socket_fp, "Content-type: %s\n", table[index][1]);
    fprintf(stderr, "Content-type: %s\n", table[index][1]);
    fprintf(socket_fp, "\n");
}

void response_header_404(FILE *socket_fp, int index)
{
    time_t timep;
    struct tm   *time_inf;
    char d[1024];

    timep = time(NULL);
    time_inf = gmtime(&timep);

    fprintf(socket_fp, "HTTP/1.1 404 OK\n");
    //fprintf(socket_fp, "Date: %s\n", asctime(time_inf));
    fprintf(stderr, "Date: %s\n", asctime(time_inf));
    strftime(d, 1024, "%Y年%m月%d日 %H時%M分%S秒", time_inf);
    fprintf(socket_fp, "Date: %s\n", d);
    fprintf(socket_fp, "Server: Modoki/0.1\n");
    fprintf(socket_fp, "Connection: close\n");
    fprintf(socket_fp, "Content-type: %s\n", table[index][1]);
    fprintf(stderr, "Content-type: %s\n", table[index][1]);
    fprintf(socket_fp, "\n");
}
//void thread(FILE *socket_fp, int fd)
void thread(void *p)
{
    FILE *socket_fp;
    int fd;
    char d[1024];
    FILE *file_in_fp;
    char line[1024];
    char file_name[1024];
    char file_name2[1024];
    char real[1024];
    char pathname[1024];
    char location[1024];
    char *ext;
    int index;
    struct stat st;
    int result;

    thread_arg *t = (thread_arg *)p;
    socket_fp = t->socket_fp;
    fd = t->fd;

    while(fgets(line, 1024, socket_fp) != NULL) {
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
            fprintf(stderr, "[%s]\n", file_name);
            fprintf(stderr, "[%s]\n", ext);

            for(index = 0; index < 9; index++) {
                if(ext == NULL) {
                    index = 9;
                    break;
                }
                if(strcmp(ext, table[index][0]) == 0) {
                    break;
                }
            }
        }
    }

    getcwd(pathname, 1024);
    fprintf(stderr, "path: %s\n", pathname);

    realpath(file_name, real);
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

    if(file_in_fp == NULL) {
        fprintf(stderr, "---- 2 [404 file notfound] ----\n");
        fprintf(stderr, "file_name : %s\n", file_name);
        index = 0;
        response_header_404(socket_fp, index);
        file_in_fp = fopen("./404.html", "r");
        while(fgets(line, 1024, file_in_fp) != NULL) {
            fprintf(socket_fp, "%s", line);
        }
        fclose(file_in_fp);
    } if(strncmp(real, pathname, strlen(pathname)) != 0) {
        fprintf(stderr, "---- 3 [404 traversal] ----\n");
        fprintf(stderr, "file_name : %s\n", file_name);
        index = 0;
        response_header_404(socket_fp, index);
        file_in_fp = fopen("./404.html", "r");
        while(fgets(line, 1024, file_in_fp) != NULL) {
            fprintf(socket_fp, "%s", line);
        }
        fprintf(socket_fp, "drectory\n");
        fclose(file_in_fp);
    } else {
        fprintf(stderr, "---- 4 [200 file found] ----\n");
        fprintf(stderr, "file_name : %s\n", file_name);
        response_header_200(socket_fp, index);
        file_in_fp = fopen(file_name, "r");
        while(fgets(line, 1024, file_in_fp) != NULL) {
            fprintf(socket_fp, "%s", line);
        }
        fclose(file_in_fp);
    }

    fclose(socket_fp);
}

int main(int argc, char **argv)
{
    int sock;
    struct sockaddr_in addr;
    int fd;
    FILE *socket_fp;
    int ch;
    pthread_t pthread;
    thread_arg arg;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8001);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    ch = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    ch = listen(sock, 6);

    while(1) {
        fd = accept(sock, NULL, NULL);

        socket_fp = fdopen(fd, "r+");

        arg.socket_fp = socket_fp;
        arg.fd = fd;
        //thread(socket_fp, fd);
        //thread(&arg);
        pthread_create( &pthread, NULL, (void *)&thread, &arg);
    }

    return 0;
}

