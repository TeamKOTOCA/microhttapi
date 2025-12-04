#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>

#define PORT 8080
#define ROOT "./www"
#define AUTH_KEY "key"

const char *HTML_404_PAGE = "HTTP/1.1 404 Not Found\r\n" "Content-Type: text/html\r\n" "Connection: close\r\n" "\r\n" "<!DOCTYPE html><html lang=\"ja\"><head><link rel=\"stylesheet\" href=\"/errors.css\"><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><meta charset=\"UTF-8\"><title>403 Forbidden</title></head><body>" "<div><h1>404</h1><h2>ページが見つかりません</h2><p>ページが存在していない可能性があります。</p><a href=\"/\">トップへ戻る</a></div>" "</body></html>";
const char *HTML_403_PAGE = "HTTP/1.1 403 Forbidden\r\n" "Content-Type: text/html\r\n" "Connection: close\r\n" "\r\n" "<!DOCTYPE html><html lang=\"ja\"><head><link rel=\"stylesheet\" href=\"/errors.css\"><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><meta charset=\"UTF-8\"><title>403 Forbidden</title></head><body>" "<div><h1>403</h1><h2>アクセスが拒否されました</h2><p>このリソースを閲覧する権限がありません。</p><a href=\"/\">トップへ戻る</a></div>" "</body></html>";
const char *HTML_500_PAGE = "HTTP/1.1 500 Internal Server Error\r\n" "Content-Type: text/html\r\n" "Connection: close\r\n" "\r\n" "<!DOCTYPE html><html lang=\"ja\"><head><link rel=\"stylesheet\" href=\"/errors.css\"><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><meta charset=\"UTF-8\"><title>500 Server Error</title></head><body>" "<div><h1>500</h1><h2>サーバーエラー</h2><p>予期せぬエラーが発生しました。時間を置いて再度お試しください。</p><a href=\"/\">トップへ戻る</a></div>" "</body></html>";

void send_header(int c, const char *type) {
    char buf[256];
    snprintf(buf, sizeof(buf),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Connection: close\r\n\r\n",
        type
    );
    write(c, buf, strlen(buf));
}

int main() {
    printf("MicroHttp server is started.\n");

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

        char method[8], path[256];
        if (sscanf(req, "%7s %255s", method, path) != 2) {
            write(c, HTML_500_PAGE, strlen(HTML_500_PAGE));
            close(c);
            continue;
        }

        if (strcmp(method, "GET") != 0) {
            write(c, HTML_404_PAGE, strlen(HTML_404_PAGE));
            close(c);
            continue;
        }

        char raw_path[256];
        strcpy(raw_path, path);

        char *q = strchr(path, '?');
        if (q) *q = 0;

        if (strcmp(path, "/") == 0)
            strcpy(path, "/index.html");

        char full[512];
        snprintf(full, sizeof(full), "%s%s", ROOT, path);

        struct stat st;
        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
            snprintf(full, sizeof(full), "%s%s/index.html", ROOT, path);
        }

        const char *ext = strrchr(full, '.');

        if (ext && strcmp(ext, ".sh") == 0) {

            char required_auth[256];
            snprintf(required_auth, sizeof(required_auth), "key=%s", AUTH_KEY);

            if (strstr(raw_path, required_auth) == NULL) {
                write(c, HTML_403_PAGE, strlen(HTML_403_PAGE));
                close(c);
                continue;
            }

            char cmd[600];
            snprintf(cmd, sizeof(cmd), "/bin/sh %s", full);

            FILE *fp = popen(cmd, "r");
            if (!fp) {
                write(c, HTML_500_PAGE, strlen(HTML_500_PAGE));
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

        FILE *f = fopen(full, "rb");
        if (!f) {
            write(c, HTML_404_PAGE, strlen(HTML_404_PAGE));
            close(c);
            continue;
        }

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
