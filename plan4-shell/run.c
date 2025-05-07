#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "run.h"

#define MAX_ARGS 64
#define FILE_MODE 0644
#define MAX_DIR_LEN 4096

static int redire_pf(const char* filename, int flags, int std_fd) {
    int fd = open(filename, flags, FILE_MODE);
    if (fd < 0) {
        perror("open");
        return -1;
    }
    if (dup2(fd, std_fd) < 0) {
        perror("dup2");
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

void run(char* cmd, int in_fd, int out_fd, int background) {
    static char prev_dir[MAX_DIR_LEN] = "";
    char* args[MAX_ARGS];
    int argc = 0;
    char *infile = NULL, *outfile = NULL;
    int append = 0;

    char* saveptr;
    char* token = strtok_r(cmd, " ", &saveptr);
    while (token) {
        if (strcmp(token, "&") == 0) {
            background = 1;
        } else if (strcmp(token, "<") == 0) {
            infile = strtok_r(NULL, " ", &saveptr);
        } else if (strcmp(token, ">>") == 0) {
            outfile = strtok_r(NULL, " ", &saveptr);
            append = 1;
        } else if (strcmp(token, ">") == 0) {
            outfile = strtok_r(NULL, " ", &saveptr);
        } else {
            args[argc++] = token;
        }
        token = strtok_r(NULL, " ", &saveptr);
    }
    args[argc] = NULL;

    if (argc == 0)
        return;

    if (strcmp(args[0], "cd") == 0) {
        char current_dir[MAX_DIR_LEN];
        if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
            perror("getcwd");
            return;
        }

        char* target = argc > 1 ? args[1] : getenv("HOME");
        if (target && strcmp(target, "-") == 0) {
            if (*prev_dir == '\0') {
                fprintf(stderr, "cd: OLDPWD not set\n");
                return;
            }
            target = prev_dir;
        }

        if (chdir(target) != 0) {
            perror("cd");
            return;
        }

        strncpy(prev_dir, current_dir, MAX_DIR_LEN - 1);
        prev_dir[MAX_DIR_LEN - 1] = '\0';
        return;
    }

    if (strcmp(args[0], "exit") == 0) {
        exit(EXIT_SUCCESS);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return;
    }

    if (pid == 0) {
        if (infile && redire_pf(infile, O_RDONLY, STDIN_FILENO) < 0) {
            exit(EXIT_FAILURE);
        }

        if (outfile) {
            int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
            if (redire_pf(outfile, flags, STDOUT_FILENO) < 0) {
                exit(EXIT_FAILURE);
            }
        }

        if (in_fd != STDIN_FILENO) {
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
        }

        if (out_fd != STDOUT_FILENO) {
            dup2(out_fd, STDOUT_FILENO);
            close(out_fd);
        }

        execvp(args[0], args);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else {
        if (!background) {
            waitpid(pid, NULL, 0);
        }
    }
}