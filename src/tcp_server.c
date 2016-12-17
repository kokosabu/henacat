#define _POSIX_C_SOURCE 1 /* glibcでfdopenを使うための定義 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/uio.h>
#include <time.h>

int main(int argc, char **argv)
{
    int sock;
    struct sockaddr_in addr;
    int fd;
    FILE *socket_fp;
    FILE *file_in_fp;
    int ch;
    char line[1024];
    char d[1024];
    char file_name[1024];
    time_t timep;
    struct tm   *time_inf;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8001);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    ch = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    ch = listen(sock, 5);
    fd = accept(sock, NULL, NULL);

    socket_fp = fdopen(fd, "r+");

    while(fgets(line, 1024, socket_fp) != NULL) {
        if (strcmp(line, "\r\n") == 0 || strcmp(line, "\n") == 0) {
            break;
        }
        if (strncmp(line, "GET", 3) == 0) {
            /* fprintf(stderr, "[%s]", line); */
            strtok(line, " ");
            strcpy(file_name, ".");
            strcat(file_name, strtok(NULL, " "));
            /* fprintf(stderr, "[%s]\n", file_name); */
        }
    }

    socket_fp = fdopen(fd, "w");
    file_in_fp = fopen(file_name, "r");

    timep = time(NULL);
    time_inf = gmtime(&timep);

    fprintf(socket_fp, "HTTP/1.1 200 OK\n");
    /* fprintf(stderr, "Date: %s\n", asctime(time_inf)); */
    strftime(d, 1024, "%Y年%m月%d日 %H時%M分%S秒", time_inf);
    /* fprintf(stderr, "Date: %s\n", d); */
    fprintf(socket_fp, "Date: %s\n", d);
    fprintf(socket_fp, "Server: Modoki/0.1\n");
    fprintf(socket_fp, "Connection: close\n");
    fprintf(socket_fp, "Content-type: text/html\n");
    fprintf(socket_fp, "\n");

    while(fgets(line, 1024, file_in_fp) != NULL) {
        fprintf(socket_fp, "%s", line);
    }

    fclose(file_in_fp);
    fclose(socket_fp);

    return 0;
}

