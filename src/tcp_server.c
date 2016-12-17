#define _POSIX_C_SOURCE 1 /* glibcでfdopenを使うための定義 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/uio.h>
#include <time.h>

#include <pthread.h>

typedef struct {
    FILE *socket_fp;
    int fd;
} thread_arg;

char *table[9][2] = {
    {"html", "text/html"},
    {"htm",  "text/html"},
    {"txt",  "text/plain"},
    {"css",  "text/css"},
    {"png",  "image/png"},
    {"jpg",  "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"gif",  "image/gif"},
    {"ico",  "image/ico"}
};

//void thread(FILE *socket_fp, int fd)
void thread(void *p)
{
    FILE *socket_fp;
    int fd;
    char d[1024];
    FILE *file_in_fp;
    time_t timep;
    char line[1024];
    char file_name[1024];
    char file_name2[1024];
    char *ext;
    struct tm   *time_inf;
    int index;

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
                if(strcmp(ext, table[index][0]) == 0) {
                    break;
                }
            }
        }
    }

    socket_fp = fdopen(fd, "w");
    file_in_fp = fopen(file_name, "r");

    timep = time(NULL);
    time_inf = gmtime(&timep);

    fprintf(socket_fp, "HTTP/1.1 200 OK\n");
    fprintf(socket_fp, "Server: Modoki/0.1\n");
    fprintf(socket_fp, "Connection: close\n");
    //fprintf(socket_fp, "Content-type: text/html\n");
    fprintf(socket_fp, "Content-type: %s\n", table[index][1]);
    fprintf(stderr, "Content-type: %s\n", table[index][1]);
    fprintf(socket_fp, "\n");

    while(fgets(line, 1024, file_in_fp) != NULL) {
        fprintf(socket_fp, "%s", line);
    }

    fclose(file_in_fp);
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

    while(1) {
        ch = listen(sock, 6);
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

