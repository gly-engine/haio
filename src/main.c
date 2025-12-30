#include <stdio.h>
#include <string.h>

#include "img.h"

int main(int argc, char* argv[]) {
    if (argc <= 1) {
        printf("subcommands: convert, server, help\n");
        return 1;
    }

    if (strcmp(argv[1], "convert") == 0) {
        return FrontendConvertCli(argc - 1, &argv[1]);
    }
    
    if(strcmp(argv[1], "server") == 0) {
        return FrontendServerCli(argc - 1, &argv[1]);
    }
    
    printf("subcommand not exist: %s\n", argv[1]);
    return 1;
}
