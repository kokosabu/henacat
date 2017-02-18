#define _POSIX_C_SOURCE 1 /* glibcでfdopenを使うための定義 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/uio.h>
#include <time.h>
#include <errno.h>

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

#define TABLE_SIZE (sizeof(table)/sizeof(table[0]))

static const char *document_root = "htdocs";
static const char *server_name = "Henacat";
static const char *server_version = "0.0";

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

void get_filename(char *line, char *file_name)
{
    while(*line != ' ') line++;
    while(*line == ' ') line++;
    *file_name = *line;
    while(*line != ' ') {
        *file_name = *line;
        file_name++;
        line++;
    }
    *file_name = '\0';
}

unsigned char hex2int(char b1, char b2)
{
    int digit;

    if(b1 >= 'A') {
        digit = (b1 & 0xDF) - 'A' + 10; /* 小文字を大文字に変換 */
    } else {
        digit = (b1 - '0');
    }
    digit <<= 4;
    if(b2 >= 'A') {
        digit |= (b2 & 0xDF) - 'A' + 10;
    } else {
        digit |= (b2 - '0');
    }

    return digit;
}

void decode(char *dest, char *src, char *enc)
{
    int dest_index;
    int i;
   
    dest_index = 0;
    for(i = 0; i < strlen(src); i++) {
        if(src[i] == '%') {
            dest[dest_index] = hex2int(src[i+1], src[i+2]);
            i += 2;
        } else {
            dest[dest_index] = src[i];
        }
        dest_index++;
    }
}

char *search_ext(char *file_name)
{
    int len;
    char *ext;

    ext = file_name + strlen(file_name) - 1;
    while(ext != file_name) {
        if(*ext == '/') {
            ext = NULL;
            break;
        } else if(*ext == '.') {
            ext++;
            break;
        }
        ext--;
    }

    return ext;
}

int request(FILE *socket_fp, char *file_name)
{
    char line[BUF_SIZE];
    char file[BUF_SIZE];
    char encfile[BUF_SIZE];
    char *ext;
    int index;

    while(fgets(line, BUF_SIZE, socket_fp) != NULL) {
        if (strcmp(line, "\r\n") == 0 || strcmp(line, "\n") == 0) {
            break;
        }
        if (strncmp(line, "GET", 3) == 0) {
            get_filename(line, file);
            decode(encfile, file, "UTF-8");
            sprintf(file_name, "./%s%s", document_root, encfile);
            ext = search_ext(file_name);

            if(ext == NULL) {
                return TABLE_SIZE - 1;
            }
            for(index = 0; index < (TABLE_SIZE-1); index++) {
                if(strcmp(ext, table[index][EXT]) == 0) {
                    break;
                }
            }
        }
    }

    return index;
}

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
    fprintf(socket_fp, "Server: %s/%s\n", server_name, server_version);
    fprintf(socket_fp, "Connection: close\n");
    fprintf(socket_fp, "Content-type: %s\n", table[index][CONTENT_TYPE]);
    fprintf(socket_fp, "\n");
}

void response_header_301(FILE *socket_fp, char *path)
{
    char d[BUF_SIZE];

    make_date(d, BUF_SIZE);

    fprintf(socket_fp, "HTTP/1.1 301 Moved Permanently\n");
    fprintf(socket_fp, "Date: %s\n", d);
    fprintf(socket_fp, "Server: %s/%s\n", server_name, server_version);
    fprintf(socket_fp, "Location: %s\n", path);
    fprintf(socket_fp, "Connection: close\n");
    fprintf(socket_fp, "Content-type: %s\n", table[0][CONTENT_TYPE]);
    fprintf(socket_fp, "\n");
}

void response_header_404(FILE *socket_fp)
{
    char d[BUF_SIZE];

    make_date(d, BUF_SIZE);

    fprintf(socket_fp, "HTTP/1.1 404 OK\n");
    fprintf(socket_fp, "Date: %s\n", d);
    fprintf(socket_fp, "Server: %s/%s\n", server_name, server_version);
    fprintf(socket_fp, "Connection: close\n");
    fprintf(socket_fp, "Content-type: %s\n", table[0][CONTENT_TYPE]);
    fprintf(socket_fp, "\n");
}

void response_body(FILE *socket_fp, FILE *file_in_fp)
{
    char line[BUF_SIZE];

    while(fgets(line, BUF_SIZE, file_in_fp) != NULL) {
        fprintf(socket_fp, "%s", line);
    }
}

void thread(void *p)
{
    FILE *socket_fp;
    int fd;
    FILE *file_in_fp;
    char file_name[BUF_SIZE];
    char real[BUF_SIZE];
    char pathname[BUF_SIZE];
    char location[BUF_SIZE];
    int index;
    struct stat st;

    socket_fp = ((thread_arg *)p)->socket_fp;
    fd        = ((thread_arg *)p)->fd;

    index = request(socket_fp, file_name);
    socket_fp = fdopen(fd, "w");

    getcwd(pathname, BUF_SIZE);
    realpath(file_name, real);

    if(file_name[strlen(file_name)-1] == '/') {
        index = 0;
        sprintf(file_name, "%s/index.html", document_root);
    } else {
        (void)stat(real, &st);
        if ((st.st_mode & S_IFMT) == S_IFDIR) {
            sprintf(location, "http://localhost:%d/%s/", PORT_NUMBER, file_name);
            response_header_301(socket_fp, location);
            return;
        }
    }

    file_in_fp = fopen(file_name, "r");

    if(    (file_in_fp == NULL)
        || (strncmp(real, pathname, strlen(pathname)) != 0)) {
        response_header_404(socket_fp);
        file_in_fp = fopen("./htdocs/404.html", "r");
        response_body(socket_fp, file_in_fp);
    } else {
        response_header_200(socket_fp, index);
        response_body(socket_fp, file_in_fp);
    }

    fclose(file_in_fp);
    fclose(socket_fp);
}

int main(int argc, char **argv)
{
    int sock;
    struct sockaddr_in addr;
    pthread_t pthread;
    thread_arg arg;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(PORT_NUMBER);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // WebApplication app = WebApplication.createInstance("testbbs");
    // app.addServlet("/ShowBBS", "ShowBBS");
    // app.addServlet("/PostBBS", "PostBBS");

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        fprintf(stderr, "%s\n", strerror(errno));
        return -1;
    }
    if(listen(sock, 6) == -1) {
        fprintf(stderr, "%s\n", strerror(errno));
        return -1;
    }

    fprintf(stderr, "listen\n");
    while(1) {
        arg.fd = accept(sock, NULL, NULL);
        if(arg.fd == -1) {
            fprintf(stderr, "%s\n", strerror(errno));
            continue;
        }
        arg.socket_fp = fdopen(arg.fd, "r+");
        if(arg.socket_fp == NULL) {
            fprintf(stderr, "%s\n", strerror(errno));
            continue;
        }
        fprintf(stderr, "accept\n");
        pthread_create( &pthread, NULL, (void *)&thread, &arg);
    }

    return 0;
}

