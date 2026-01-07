#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <uv.h>

int frontendWebHttpServer(uv_loop_t *loop, uint32_t port);

int FrontendServerCli(int argc, char* argv[]) {
    static uv_loop_t loop;
    int32_t port = 8080;

    uv_loop_init(&loop);

    int err = frontendWebHttpServer(&loop, port);
    if (!err) {
        uv_run(&loop, UV_RUN_DEFAULT);
    }

    return err;
}
