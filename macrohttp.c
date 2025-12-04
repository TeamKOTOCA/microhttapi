/*
 * tinyhttp.c - 超ミニマルHTTPサーバー
 * 機能:
 * - GETのみ
 * - ./www/ 配下にマップ
 * - .sh は /bin/sh で実行して標準出力を返す
 * - ディレクトリなら index.html を返す
 * - 認証は URL に ?key=secret があるかだけ
 * - 例: http://host/file.sh?key=secret
 *
 * ビルド:
 *   musl-gcc -static -Os -s tinyhttp.c -o tinyhttp
 *
 * 注意:
 * セキュリティはほぼ無い。  
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>

#define PORT 8080
#define ROOT "./www"

void send_404(int c) {
    write(c, "HTTP/1.1 404 Not Found\r\n\r\n", 26);
}

void send_500(int c) {
    write(c, "HTTP/1.1 500 Internal Error\r\n\r\n", 31);
}

void send_header(int c, const char *type) {
    char buf[256];
    snprintf(buf, sizeof(buf),
        "HTTP/1.1 200 OK\r\nContent-Type: %s\r\n\r\n", type);
    write(c, buf, strlen(buf));
}

int main() {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(s, (struct sockaddr*)&addr, sizeof(addr));
    listen(s, 5);

    while (1) {
        int c = accept(s, NULL, NULL);
        char req[1024] = {0};
        read(c, req, 1023);

        // ────────────────────────────────
        // 1. GET のパスを取り出す
        // ────────────────────────────────
        char method[8], path[256];
        if (sscanf(req, "%7s %255s", method, path) != 2) {
            send_500(c);
            close(c);
            continue;
        }

        // GET 以外は無視
        if (strcmp(method, "GET") != 0) {
            send_404(c);
            close(c);
            continue;
        }

        // ────────────────────────────────
        // 2. 簡易認証 ?key=secret
        // ────────────────────────────────
        if (strstr(path, "?key=secret") == NULL) {
            write(c, "HTTP/1.1 403 Forbidden\r\n\r\n", 26);
            close(c);
            continue;
        }

        // パラメータ除去
        char *q = strchr(path, '?');
        if (q) *q = 0;

        // ルートの場合 → /index.html に変換
        if (strcmp(path, "/") == 0) strcpy(path, "/index.html");

        // 実際のファイルパス
        char full[512];
        snprintf(full, sizeof(full), "%s%s", ROOT, path);

        // ディレクトリなら index.html
        struct stat st;
        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
            snprintf(full, sizeof(full), "%s%s/index.html", ROOT, path);
        }

        // 拡張子判定
        const char *ext = strrchr(full, '.');

        // ────────────────────────────────
        // 3. .sh の場合 → sh で実行して返す
        // ────────────────────────────────
        if (ext && strcmp(ext, ".sh") == 0) {
            FILE *fp;
            char cmd[600];
            snprintf(cmd, sizeof(cmd), "/bin/sh %s", full);

            fp = popen(cmd, "r");
            if (!fp) {
                send_500(c);
                close(c);
                continue;
            }

            send_header(c, "text/plain");

            char out[256];
            while (fgets(out, sizeof(out), fp)) {
                write(c, out, strlen(out));
            }
            pclose(fp);
            close(c);
            continue;
        }

        // ────────────────────────────────
        // 4. 通常の静的ファイル返却
        // ────────────────────────────────
        FILE *f = fopen(full, "rb");
        if (!f) {
            send_404(c);
            close(c);
            continue;
        }

        // Content-Type とりあえず最低限
        if (ext && strcmp(ext, ".html") == 0) send_header(c, "text/html");
        else if (ext && strcmp(ext, ".css") == 0) send_header(c, "text/css");
        else if (ext && strcmp(ext, ".js") == 0) send_header(c, "text/javascript");
        else send_header(c, "application/octet-stream");

        char buf[512];
        int n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
            write(c, buf, n);
        }

        fclose(f);
        close(c);
    }

    return 0;
}
