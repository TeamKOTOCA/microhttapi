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

void handle_request(int c, const char *req) {
    printf("%s\n", req);

    char firstline[512];
    sscanf(req, "%511[^\r\n]", firstline);

    char method[8], path[256];
    if (sscanf(firstline, "%7s %255s", method, path) != 2) {
        write(c, JSON_500, strlen(JSON_500));
        return;
    }

    if (strcmp(method, "GET") != 0) {
        write(c, JSON_404, strlen(JSON_404));
        return;
    }

    char *q = strchr(path, '?');
    if (q) *q = 0;

    if (strstr(path, "..") != NULL) {
        write(c, JSON_404, strlen(JSON_404));
        return;
    }

    if (strstr(path, "//") != NULL) {
        write(c, JSON_404, strlen(JSON_404));
        return;
    }

    if (strcmp(path, "/") == 0)
        strcpy(path, "/index.json");

    char full[512];
    snprintf(full, sizeof(full), "%s%s", ROOT, path);

    struct stat st;
    if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
        snprintf(full, sizeof(full), "%s%s/index.json", ROOT, path);
    }

    const char *ext = strrchr(full, '.');

    //クエリ分離
    char *qmark = strchr(firstline, '?');
    char query[256];
    if (qmark) {
        size_t plen = qmark - firstline;
        strncpy(path, firstline, plen);
        path[plen] = 0;
        strcpy(query, qmark + 1);
    } else {
        strcpy(path, firstline);
    }

    char key[128], value[128];
    char *pair = strtok(query, "&");
    //以下のヘッダーの検証処理でだめな点があれば増える
    //一の位...そのキーが存在するか。
    //１０の位...そのキーの形式が正しいか。
    int bad_header = 0;
    while (pair) {
        if (sscanf(pair, "%127[^=]=%127s", key, value) == 2) {
            if (strcmp(key, "nonce") == 0) {
                bad_header += 1;
                //16進文字列か確認
                for (int i = 0; value[i]; i++) {
                    if (!isxdigit(value[i])){
                        bad_header += 10;
                    };
                }
            } else if (strcmp(key, "ts") == 0) {
                bad_header += 2;
                // ts は数字のみか確認
                for (int i = 0; value[i]; i++) {
                    if (!isdigit(value[i])){
                        bad_header += 20;
                    };
                }
            } else if (strcmp(key, "hmac") == 0) {
                bad_header += 3;
                // hmac は64文字の16進文字列（SHA256ハッシュ）か確認
                if (strlen(value) != 64){
                    bad_header += 40;
                };
                for (int i = 0; value[i]; i++) {
                    if (!isxdigit(value[i])){
                        bad_header += 30;
                    };
                }
            }
        }
        pair = strtok(NULL, "&");
    }
    char error_message[256];
    int len;
    len = snprintf(error_message, sizeof(error_message), 
                    "header code: %d\n", bad_header);
    if (len > 0) {
        write(1, error_message, (size_t)len);
    }
    if(bad_header != 6){
        return;
    }

    //.shの処理
    if (ext && strcmp(ext, ".sh") == 0) {
        FILE *fp = popen(full, "r");
        if (!fp) {
            write(c, JSON_500, strlen(JSON_500));
            return;
        }

        char header[256];
        int hn = snprintf(header, sizeof(header),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Connection: close\r\n\r\n");
        write(c, header, hn);

        const char *startmsg = "{'status' : 'started'}\n";
        char szbuf[32];
        int n = snprintf(szbuf, sizeof(szbuf), "%x\r\n", (int)strlen(startmsg));
        write(c, szbuf, n);
        write(c, startmsg, strlen(startmsg));
        write(c, "\r\n", 2);

        char buf[256];
        while (fgets(buf, sizeof(buf), fp)) {
            n = strlen(buf);
            int cn = snprintf(szbuf, sizeof(szbuf), "%x\r\n", n);
            write(c, szbuf, cn);
            write(c, buf, n);
            write(c, "\r\n", 2);
        }

        pclose(fp);
        write(c, "0\r\n\r\n", 5);
        return;
    }

    // 通常ファイルの処理
    FILE *f = fopen(full, "rb");
    if (!f) {
        write(c, JSON_404, strlen(JSON_404));
        return;
    }

    if (ext && strcmp(ext, ".json") == 0) send_header(c, "application/json");
    else send_header(c, "application/octet-stream");

    char buf[512];
    int n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        write(c, buf, n);
    }

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

    printf("MicroHttpi async minimal server.\n");

    while (1) {
        FD_ZERO(&rfds);
        FD_SET(s, &rfds);

        // listen のみ監視
        if (select(s + 1, &rfds, NULL, NULL, NULL) < 0)
            continue;

        if (FD_ISSET(s, &rfds)) {
            int c = accept(s, NULL, NULL);
            if (c < 0) continue;

            make_nonblock(c);

            // ▼ ここ重要：クライアントのデータ到着を少し待つ
            fd_set cfds;
            FD_ZERO(&cfds);
            FD_SET(c, &cfds);

            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 30000;   // 30ms（十分短い）

            int rdy = select(c + 1, &cfds, NULL, NULL, &tv);

            char req[1024];
            int n = 0;

            if (rdy > 0 && FD_ISSET(c, &cfds)) {
                n = read(c, req, sizeof(req) - 1);
            }

            if (n > 0) {
                req[n] = 0;
                handle_request(c, req);
            }

            close(c);
        }
    }

    return 0;
}
