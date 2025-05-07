#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "analyse.h"

#define MAX_CMD_LEN 1024
#define MAX_PIPES 10
#define MAX_DIR_LEN 4096

#define CM_END "\033[0m"
#define B_BLUE "\033[34;1m"
#define B_RED "\033[31;1m"
#define B_DFT "\033[1m"
#define F_GREEN "\033[36m"

void analyse() {
    static char curr_dir[MAX_DIR_LEN] = "/";
    char cmd[MAX_CMD_LEN];
    char *host_name = getenv("HOSTNAME");
    uid_t uid = getuid();
    struct passwd* pw;

    if ((pw = getpwuid(uid)) == NULL) {
        perror("getpwuid");
        exit(EXIT_FAILURE);
    }

    while (true) {
        if (getcwd(curr_dir, MAX_DIR_LEN) == NULL) {
            perror("getcwd");
            exit(EXIT_FAILURE);
        }
        
        printf("\n%s┌──(%s", F_GREEN, CM_END);
        printf("%s%s@%s%s", B_BLUE, pw->pw_name, host_name, CM_END);
        printf("%s)-[%s", F_GREEN, CM_END);
        printf("%s%s%s", B_DFT, curr_dir, CM_END);
        printf("%s]%s\n", F_GREEN, CM_END);
        printf("%s└─%s", F_GREEN, CM_END);
        printf("%s$%s ", B_BLUE, CM_END);

        fflush(stdout);
        if (!fgets(cmd, MAX_CMD_LEN, stdin))
            break;
        cmd[strcspn(cmd, "\n")] = 0;

        char* commands[MAX_PIPES];
        int cmd_count = 0;
        char* token = strtok(cmd, "|");
        while (token) {
            commands[cmd_count++] = token;
            token = strtok(NULL, "|");
        }

        int in_fd = STDIN_FILENO;
        int fds[2];

        for (int i = 0; i < cmd_count; i++) {
            if (i < cmd_count - 1)
                pipe(fds);

            run(commands[i], in_fd,
                            (i < cmd_count - 1) ? fds[1] : STDOUT_FILENO, 0);

            if (i > 0)
                close(in_fd);

            if (i < cmd_count - 1) {
                close(fds[1]);
                in_fd = fds[0];
            }
        }
    }
}