/* credential-server.c — Minimal git credential server over Unix socket.
 *
 * Reads a GitHub PAT from a file and serves it via git credential protocol
 * over a Unix domain socket. Only responds to github.com requests.
 *
 * Security: validates host= at line boundaries to prevent substring injection.
 * Uses per-connection read timeout to prevent DoS from slow clients.
 *
 * Usage: credential-server /path/to/token /path/to/socket
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <time.h>

#define MAX_TOKEN 512
#define MAX_LINE 1024
#define CLIENT_TIMEOUT_SEC 5

static char token[MAX_TOKEN];
static int token_len;

/* Check if "host=github.com" appears as a complete line in the buffer.
 * Must be either at the start of buffer or preceded by \n.
 * Prevents injection via "fake-host=github.com" or "xhost=github.com".
 */
static int has_github_host(const char *buf) {
    const char *target = "host=github.com";
    const char *p = buf;
    while ((p = strstr(p, target)) != NULL) {
        /* Check it's at line start (beginning of buf or after \n) */
        if (p == buf || *(p - 1) == '\n') {
            /* Check it ends at line boundary (\n, \r, or end of string) */
            char after = p[strlen(target)];
            if (after == '\n' || after == '\r' || after == '\0')
                return 1;
        }
        p++;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <token-file> <socket-path>\n", argv[0]);
        return 1;
    }

    /* Read token */
    FILE *f = fopen(argv[1], "r");
    if (!f) {
        perror("Cannot open token file");
        return 1;
    }
    if (!fgets(token, MAX_TOKEN, f)) {
        fprintf(stderr, "Empty token file\n");
        fclose(f);
        return 1;
    }
    fclose(f);
    /* Strip trailing newline */
    token_len = strlen(token);
    while (token_len > 0 && (token[token_len-1] == '\n' || token[token_len-1] == '\r'))
        token[--token_len] = '\0';

    /* Ignore SIGPIPE (client may disconnect mid-write) */
    signal(SIGPIPE, SIG_IGN);

    /* Create socket */
    unlink(argv[2]);
    int srv = socket(AF_UNIX, SOCK_STREAM, 0);
    if (srv < 0) { perror("socket"); return 1; }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, argv[2], sizeof(addr.sun_path) - 1);

    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(srv, 5) < 0) { perror("listen"); return 1; }

    /* Serve forever */
    for (;;) {
        int client = accept(srv, NULL, NULL);
        if (client < 0) continue;

        /* Set read timeout to prevent slow-client DoS */
        struct timeval tv = { .tv_sec = CLIENT_TIMEOUT_SEC, .tv_usec = 0 };
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        /* Read request lines, look for host=github.com */
        char buf[MAX_LINE];
        int is_github = 0;
        ssize_t n;
        while ((n = read(client, buf, sizeof(buf) - 1)) > 0) {
            buf[n] = '\0';
            if (has_github_host(buf))
                is_github = 1;
            /* Empty line or newline-only terminates the request */
            if (strstr(buf, "\n\n") || n == 1)
                break;
        }

        if (is_github) {
            dprintf(client,
                "protocol=https\n"
                "host=github.com\n"
                "username=x-access-token\n"
                "password=%s\n"
                "\n", token);
        } else {
            dprintf(client, "\n");
        }
        close(client);
    }
}
