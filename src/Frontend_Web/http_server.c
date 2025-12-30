#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <uv.h>
#include <picohttpparser.h>
/*
#define BUFFER_SIZE 8192

typedef struct {
    uv_tcp_t handle;
    char buffer[BUFFER_SIZE];
    size_t buffer_len;
} client_t;

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = malloc(suggested_size);
    buf->len = suggested_size;
}

void handle_request(client_t *client, ssize_t nread) {
    const char *method, *path;
    int minor_version;
    struct phr_header headers[32];
    size_t method_len, path_len, num_headers = 32;

    int ret = phr_parse_request(client->buffer, nread, &method, &method_len,
                                &path, &path_len, &minor_version,
                                headers, &num_headers, 0);

    if (ret > 0) {
        printf("Method: %.*s\n", (int)method_len, method);
        printf("Path: %.*s\n", (int)path_len, path);
        printf("HTTP/1.%d\n", minor_version);

        for (size_t i = 0; i < num_headers; i++) {
            printf("%.*s: %.*s\n",
                   (int)headers[i].name_len, headers[i].name,
                   (int)headers[i].value_len, headers[i].value);
        }

        // Aqui você poderia enfileirar para o worker
        // uv_queue_work(...);
    } else if (ret == -1) {
        printf("Erro no parse HTTP\n");
    }
}

void read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    client_t *client = stream->data;

    if (nread > 0) {
        if (nread > BUFFER_SIZE) nread = BUFFER_SIZE;
        memcpy(client->buffer, buf->base, nread);
        client->buffer_len = nread;
        handle_request(client, nread);
    } else if (nread < 0) {
        if (nread != UV_EOF)
            fprintf(stderr, "Read error: %s\n", uv_err_name(nread));
        uv_close((uv_handle_t*)stream, NULL);
    }

    free(buf->base);
}

void on_new_connection(uv_stream_t *server, int status) {
    if (status < 0) {
        fprintf(stderr, "New connection error: %s\n", uv_strerror(status));
        return;
    }

    uv_loop_t *loop = server->loop;
    client_t *client = malloc(sizeof(client_t));
    uv_tcp_init(loop, &client->handle);
    client->handle.data = client;

    if (uv_accept(server, (uv_stream_t*)&client->handle) == 0) {
        uv_read_start((uv_stream_t*)&client->handle, alloc_buffer, read_cb);
    } else {
        uv_close((uv_handle_t*)&client->handle, NULL);
    }
}
*/
int FrontendWebHttpServer(uv_loop_t *loop, uint32_t port) {
    /*static uv_tcp_t server;
    uv_tcp_init(loop, &server);

    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", port, &addr);
    uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);

    int r = uv_listen((uv_stream_t*) &server, 128, on_new_connection);
    if (r) {
        fprintf(stderr, "Listen error: %s\n", uv_strerror(r));
        return 1;
    }

    printf("Server listening on http://0.0.0.0:%d\n", port);
    return 0;*/
}
