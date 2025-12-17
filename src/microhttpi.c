#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include "sha256.h"

#define PORT 8001
#define ROOT "./www"

#define MAXLINE 1024

void handle_request(int c, char *line) {
    fprintf(stdout, "[REQ] %s\n", line);
    // 改行除去
    char *nl = strpbrk(line, "\r\n");
    if (nl) *nl = 0;

    // パスとパラメータ分離
    char path[256], params[512] = {0};
    int scanned = sscanf(line, "%255s %511[^\n]", path, params);
    if (scanned < 1) {
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
    char key[128], value[128];
    int keys_nonce = 0, keys_ts = 0, keys_hmac = 0;

    char *pair = strtok(params, " ");
    while (pair) {
        if (sscanf(pair, "%127[^=]=%127s", key, value) != 2) {
            write(c, "ERR bad param format\n", 21);
            return;
        }

        if (strcmp(key, "nonce") == 0) {
            keys_nonce += 1;
            for (int i = 0; value[i]; i++)
                if (!isxdigit(value[i])) keys_nonce += 10;
        } else if (strcmp(key, "ts") == 0) {
            keys_ts += 1;
            for (int i = 0; value[i]; i++)
                if (!isdigit(value[i])) keys_ts += 10;
        } else if (strcmp(key, "hmac") == 0) {
            keys_hmac += 1;
            if (strlen(value) != 64) keys_hmac += 10;
            for (int i = 0; value[i]; i++)
                if (!isxdigit(value[i])) keys_hmac += 10;
        } else {
            write(c, "ERR unknown key\n", 16);
            return;
        }

        pair = strtok(NULL, " ");
    }

    dprintf(c, "header code: %d %d %d\n", keys_nonce, keys_ts, keys_hmac);
    dprintf(1, "header code: %d %d %d\n", keys_nonce, keys_ts, keys_hmac);

    if (keys_nonce != 1 || keys_ts != 1 || keys_hmac != 1) {
        write(c, "ERR auth failed\n", 16);
        return;
    }

    /* --- ファイル処理 --- */
    char full[512];
    snprintf(full, sizeof(full), "%s%s", ROOT, path);

    const char *ext = strrchr(full, '.');

    // .sh は実行
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

    // 通常ファイル
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

/* --- 非ブロッキング設定 --- */
int make_nonblock(int fd) {
    int fl = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

/* --- 1 行読む（raw TCP 用、改行まで） --- */
int read_line(int fd, char *buf, int maxlen) {
    int total = 0;
    while (total < maxlen - 1) {
        char c;
        int n = read(fd, &c, 1);
        if (n == 0) break;       // EOF
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;         // 非ブロッキングなので再試行
            return -1;
        }
        buf[total++] = c;
        if (c == '\n') break;
    }
    buf[total] = 0;
    return total;
}

/* --- メイン --- */
int main() {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(s, 5) < 0) { perror("listen"); return 1; }

    make_nonblock(s);

    fd_set rfds;
    printf("MicroHttpi raw TCP server started on port %d\n", PORT);

    while (1) {
        FD_ZERO(&rfds);
        FD_SET(s, &rfds);

        if (select(s + 1, &rfds, NULL, NULL, NULL) < 0)
            continue;

        if (FD_ISSET(s, &rfds)) {
            int c = accept(s, NULL, NULL);
            if (c < 0) continue;

            make_nonblock(c);

            char req[MAXLINE];
            int n = read_line(c, req, sizeof(req));
            if (n <= 0) {
                close(c);
                continue;
            }

            handle_request(c, req);

            close(c);
        }
    }

    return 0;
}
