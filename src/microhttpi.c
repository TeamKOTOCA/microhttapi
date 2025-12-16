#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include "sha256.h"

#define PORT 8080
#define ROOT "./www"

const char *JSON_404 =
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Type: application/json\r\n"
    "Connection: close\r\n\r\n"
    "{ \"error\": 404 }";

const char *JSON_500 =
    "HTTP/1.1 500 Internal Server Error\r\n"
    "Content-Type: application/json\r\n"
    "Connection: close\r\n\r\n"
    "{ \"error\": 500 }";

void send_header(int c, const char *type) {
    char buf[128];
    int n = snprintf(buf, sizeof(buf),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Connection: close\r\n\r\n",
        type
    );
    write(c, buf, n);
}

int make_nonblock(int fd) {
    int fl = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

void handle_request(int c, char *line) {
    printf("REQ: %s\n", line);

    // 改行除去
    char *nl = strpbrk(line, "\r\n");
    if (nl) *nl = 0;

    // 先頭はパス
    char path[256];
    char params[512];

    if (sscanf(line, "%255s %511[^\n]", path, params) < 1) {
        write(c, "ERR bad format\n", 15);
        return;
    }

    if (strstr(path, "..") || strstr(path, "//")) {
        write(c, "ERR invalid path\n", 17);
        return;
    }

    if (strcmp(path, "/") == 0)
        strcpy(path, "/index.json");

    /* --- クエリ解析 --- */

    char query[512] = {0};
    if (strchr(line, ' '))
        strcpy(query, params);

    char key[128], value[128];
    int keys_nonce = 0, keys_ts = 0, keys_hmac = 0;

    char *pair = strtok(query, " ");
    while (pair) {
        if (sscanf(pair, "%127[^=]=%127s", key, value) == 2) {
            if (strcmp(key, "nonce") == 0) {
                keys_nonce += 1;
                for (int i = 0; value[i]; i++)
                    if (!isxdigit(value[i])) keys_nonce += 10;
            }
            else if (strcmp(key, "ts") == 0) {
                keys_ts += 1;
                for (int i = 0; value[i]; i++)
                    if (!isdigit(value[i])) keys_ts += 10;
            }
            else if (strcmp(key, "hmac") == 0) {
                keys_hmac += 1;
                if (strlen(value) != 64) keys_hmac += 10;
                for (int i = 0; value[i]; i++)
                    if (!isxdigit(value[i])) keys_hmac += 10;
            }
        }
        pair = strtok(NULL, " ");
    }

    dprintf(c, "header code: %d %d %d\n", keys_nonce,keys_ts,keys_hmac);

    if (keys_nonce != 1 && keys_ts != 1 && keys_hmac != 1 ) {
        write(c, "ERR auth failed\n", 16);
        return;
    }

    /* --- ファイル処理 --- */

    char full[512];
    snprintf(full, sizeof(full), "%s%s", ROOT, path);

    const char *ext = strrchr(full, '.');

    if (ext && strcmp(ext, ".sh") == 0) {
        FILE *fp = popen(full, "r");
        if (!fp) {
            write(c, "ERR exec failed\n", 16);
            return;
        }

        char buf[256];
        while (fgets(buf, sizeof(buf), fp))
            write(c, buf, strlen(buf));

        pclose(fp);
        return;
    }

    FILE *f = fopen(full, "rb");
    if (!f) {
        write(c, "ERR not found\n", 14);
        return;
    }

    char buf[512];
    int n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        write(c, buf, n);

    fclose(f);
}

int main() {
    int s = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(s, (struct sockaddr*)&addr, sizeof(addr));
    listen(s, 5);

    make_nonblock(s);

    fd_set rfds;
    int maxfd = s;

    printf("MicroHttpi started.\n");

    while (1) {
        FD_ZERO(&rfds);
        FD_SET(s, &rfds);

        if (select(s + 1, &rfds, NULL, NULL, NULL) < 0)
            continue;

        if (FD_ISSET(s, &rfds)) {
            int c = accept(s, NULL, NULL);
            if (c < 0) continue;

            make_nonblock(c);

            fd_set cfds;
            FD_ZERO(&cfds);
            FD_SET(c, &cfds);

            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 50000; //50ms

            int rdy = select(c + 1, &cfds, NULL, NULL, &tv);

            char req[1024];
            int n = 0;
            if (rdy > 0 && FD_ISSET(c, &cfds)) {
                n = read(c, req, sizeof(req) - 1);
            }
            if (n <= 0) {
                close(c);
                continue;
            }

            close(c);
        }
    }

    return 0;
}
