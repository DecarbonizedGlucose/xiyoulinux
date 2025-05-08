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

void main_loop() {
    char curr_dir[MAX_DIR_LEN] = { 0 };
    char cmd[MAX_CMD_LEN];
    char host_name[MAX_CMD_LEN] = { 0 };
    gethostname(host_name, sizeof(host_name));
    if (strlen(host_name) == 0) {
        perror("gethostname");
        exit(EXIT_FAILURE);
    }
    if (host_name == NULL) {
        perror("getenv");
        exit(EXIT_FAILURE);
    }
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

        char curr_dir_display[MAX_DIR_LEN];
        if (strstr(curr_dir, "/home/") == 0) {
            snprintf(curr_dir_display, sizeof(curr_dir_display), "~%s", curr_dir + strlen("/home/"));
        } else {
            snprintf(curr_dir_display, sizeof(curr_dir_display), "%s", curr_dir);
        }
        
        printf("\n%s┌──(%s", F_GREEN, CM_END);
        printf("%s%s@%s%s", B_BLUE, pw->pw_name, host_name, CM_END);
        printf("%s)-[%s", F_GREEN, CM_END);
        printf("%s%s%s", B_DFT, curr_dir_display, CM_END);
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

            run(commands[i], in_fd, (i < cmd_count - 1) ? fds[1] : STDOUT_FILENO, 0);

            if (i > 0)
                close(in_fd);

            if (i < cmd_count - 1) {
                close(fds[1]);
                in_fd = fds[0];
            }
        }
    }
}