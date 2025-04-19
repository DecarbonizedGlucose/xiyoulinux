#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <bits/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <grp.h>
#include <pwd.h>
#include <time.h>

/* ls
 显示什么
 -a 显示所有项目
 -R 递归遍历目录
 显示的顺序
 -t 按时间排序，最新的最前
 -r 逆序输出
 -s 按大小排序，大的最前 显示大小
 显示的格式
 -l 长列表输出 显示大小
 -i 显示每个文件索引编号（i-node编号）
*/

#define MAX_BUFFER_SIZE 1023
#define MAX_PEND_NUM 1024

// 00 -> 0 (default)
// 01 -> 1 -a
// 10 -> 2 -R
// 11 -> 3 -a -R
int displayContent;

// 000 -> 0 dict order (default)
// 001 -> 1 -t
// 010 -> 2 -s
// 011 -> 3 -t -s
// 1xx -> -r (+4)
int displayOrder;

// 00 -> 0 normal (default)
// 01 -> 1 -l
// 10 -> 2 -i
// 11 -> 3 -i -l
int displayFormat;

char cwdBuffer[MAX_BUFFER_SIZE + 1];
char globalPendingObject[MAX_PEND_NUM][MAX_BUFFER_SIZE + 1];
int pendingObjectCount;

void analyseArgs(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-a") == 0) {
            displayContent &= 1;
        }
        else if (strcmp(argv[i], "-R") == 0) {
            displayContent &= 2;
        }
        else if (strcmp(argv[i], "-t") == 0) {
            displayOrder &= 1;
        }
        else if (strcmp(argv[i], "-s") == 0) {
            displayOrder &= 2;
        }
        else if (strcmp(argv[i], "-r") == 0) {
            displayOrder &= 4;
        }
        else if (strcmp(argv[i], "-l") == 0) {
            displayFormat &= 1;
        }
        else if (strcmp(argv[i], "-i") == 0) {
            displayFormat &= 2;
        }
        else {
            strcpy(globalPendingObject[pendingObjectCount++], argv[i]);
        }
    }
}

int fileCmp(char* path1, char* path2) {
    int res = 0; // return -1, 0, 1
    struct stat stat1, stat2;
    stat(path1, &stat1);
    stat(path2, &stat2);
    if (displayOrder & 1) {
        if (stat1.st_mtime != stat2.st_mtime) {
            res =  (stat1.st_mtime > stat2.st_mtime) ? -1 : 1;
        }
    }
    if (displayOrder & 2) {
        if (stat1.st_size != stat2.st_size) {
            res = (stat1.st_size > stat2.st_size) ? -1 : 1;
        }
    }
    res = strcmp(path1, path2);
    if (displayOrder & 4) {
        res = -res;
    }
    return res;
}

void printColorName(char* name, int type) { // 最后写
    char cstart[MAX_BUFFER_SIZE];
    char cend[] = "\033[0m";
    switch (type) {
    case 0: // 普通文件
        sprintf(cstart, "");
        sprintf(cend, "");
        break;
    case 1: // 目录
        sprintf(cstart, "\033[1;34m");
        break;
    case 2: // 字符设备文件 - ?
        sprintf(cstart, "");
        break;
    case 3: // 块设备 - ?
        sprintf(cstart, "");
        break;
    case 4: // FIFO或管道 - ?
        sprintf(cstart, "\033[0;33;43m");
        break;
    case 5: // socket - ?
        sprintf(cstart, "");
        break;
    case 6: // 软连接
        sprintf(cstart, "\033[1;36m");
        break;
    default:
        // 这对吗？
        fprintf(stderr, "错误的文件类型");
    }
    printf("%s%s%s", cstart, name, cend);
}

void filePermStr(mode_t perm, char* strbuf, int* filetype) {
    char str[11] = {0};
    if (S_ISREG(perm)) {
        str[0] = '-';
        *filetype = 0;
    }
    else if (S_ISDIR(perm)) {
        str[0] = 'd';
        *filetype = 1;
    }
    else if (S_ISCHR(perm)) {
        str[0] = 'c';
        *filetype = 2;
    }
    else if (S_ISBLK(perm)) {
        str[0] = 'b';
        *filetype = 3;
    }
    else if (S_ISFIFO(perm)) {
        str[0] = 'p';
        *filetype = 4;
    }
    else if ((perm & __S_IFMT) == __S_IFSOCK) {
        str[0] = 's';
        *filetype = 5;
    }
    else if (S_ISLNK(perm)) {
        str[0] = 'l';
        *filetype = 6;
    }
    snprintf(str + 1, 10, "%c%c%c%c%c%c%c%c%c",
        (perm & S_IRUSR) ? 'r' : '-', (perm & S_IWUSR) ? 'w' : '-',
        (perm & S_IXUSR) ? 'x' : '-', (perm & S_IRGRP) ? 'w' : '-',
        (perm & S_IWGRP) ? 'w' : '-', (perm & S_IXGRP) ? 'x' : '-',
        (perm & S_IROTH) ? 'r' : '-', (perm & S_IWOTH) ? 'w' : '-',
        (perm & S_IXOTH) ? 'x' : '-'
    );
    strncpy(strbuf, str, MAX_BUFFER_SIZE);
}

void modifyTimeStr(time_t mtime, char* strbuf) {
    struct tm* timeinfo = localtime(&mtime);
    strftime(strbuf, MAX_BUFFER_SIZE, "%Y-%m-%d %H:%M:%S", timeinfo);
}

void showSingleFileInLine(char* name, int disiNode, struct stat* st) {
    char mode[MAX_BUFFER_SIZE + 1];
    char timestr[MAX_BUFFER_SIZE + 1];
    int filetype;
    filePermStr(st->st_mode, mode, &filetype);
    struct passwd* pw = getpwuid(st->st_uid);
    struct group* gr = getgrgid(st->st_gid);
    timestr, modifyTimeStr(st->st_mtime, timestr);
    if (disiNode) {
        printf("%d ", st->st_ino);
    }
    printf("%s %d %s %s %d %s ", mode, st->st_nlink,
        pw->pw_name, gr->gr_name, st->st_size, timestr
    );
    printColorName(name, filetype);
    putchar('\n');
}

int main(int argc, char *argv[]) {
    getcwd(cwdBuffer, MAX_BUFFER_SIZE);
    analyseArgs(argc, argv);
    
    return 0;
}