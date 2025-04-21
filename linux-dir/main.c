#define _BSD_SOURCE 1
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <grp.h>
#include <pwd.h>
#include <time.h>
#include <stdlib.h>

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
#define PRI_PEND_NUM 1024

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

// 当前目录路径
char cwdBuffer[MAX_BUFFER_SIZE + 1];

// 要处理的对象列表
char globalPendingObject[PRI_PEND_NUM][MAX_BUFFER_SIZE + 1];
struct stat pendingObjectStat[PRI_PEND_NUM];
int pendingObjectCount;
int idxArr[PRI_PEND_NUM];

// 不存在的对象/无法处理的
char unavaObject[PRI_PEND_NUM][MAX_BUFFER_SIZE + 1];
int unavaCount;

void analyseArgs(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-a") == 0) {
            displayContent |= 1;
        }
        else if (strcmp(argv[i], "-R") == 0) {
            displayContent |= 2;
        }
        else if (strcmp(argv[i], "-t") == 0) {
            displayOrder |= 1;
        }
        else if (strcmp(argv[i], "-s") == 0) {
            displayOrder |= 2;
        }
        else if (strcmp(argv[i], "-r") == 0) {
            displayOrder |= 4;
        }
        else if (strcmp(argv[i], "-l") == 0) {
            displayFormat |= 1;
        }
        else if (strcmp(argv[i], "-i") == 0) {
            displayFormat |= 2;
        }
        else if (argv[i][0] == '-') {
            int len = strlen(argv[i]);
            for (int j = 1; j < len; ++j) {
                switch (argv[i][j]) {
                case 'a':
                    displayContent |= 1;
                    break;
                case 'R':
                    displayContent |= 2;
                    break;
                case 't':
                    displayOrder |= 1;
                    break;
                case 's':
                    displayOrder |= 2;
                    break;
                case 'r':
                    displayOrder |= 4;
                    break;
                case 'l':
                    displayFormat |= 1;
                    break;
                 case 'i':
                    displayFormat |= 2;
                    break;
                default:
                    fprintf(stderr, "错误的参数：%s\n", argv[i]);
                }
            }
        }
        else {
            int isAva = lstat(argv[i], pendingObjectStat+pendingObjectCount);
            if (isAva == 0) {
                strcpy(globalPendingObject[pendingObjectCount++], argv[i]);
            }
            else {
                strcpy(unavaObject[unavaCount++], argv[i]);
            }
        }
    }
    if (pendingObjectCount == 0 && unavaCount == 0) {
        lstat(cwdBuffer, pendingObjectStat + 0);
        strcpy(globalPendingObject[pendingObjectCount++], cwdBuffer);
    }
}

int fileCmp(int idx1, int idx2, char objList[][MAX_BUFFER_SIZE + 1], struct stat statList[]) {
    int res = 0; // return -1, 0, 1
    if (displayOrder & 1) {
        if (statList[idx1].st_mtime != statList[idx2].st_mtime) {
            res =  (statList[idx1].st_mtime > statList[idx2].st_mtime) ? -1 : 1;
        }
    }
    if ((displayOrder & 2) && (res == 0)) {
        if (pendingObjectStat[idx1].st_size != pendingObjectStat[idx2].st_size) {
            res = (statList[idx1].st_size > statList[idx2].st_size) ? -1 : 1;
        }
    }
    if (!(displayOrder & 3) || (res == 0)) {
        res = strcmp(objList[idx1], objList[idx2]);
    }
    if (displayOrder & 4) {
        res = -res;
    }
    return res;
}

void printColorName(char* name, int type) {
    char cstart[MAX_BUFFER_SIZE] = {0};
    char cend[] = "\033[0m";
    switch (type) {
    case 0: // 普通文件
    case 2: // 字符设备文件
    case 3: // 块设备文件
    case 5: // socket
        cstart[0] = '\0';
        cend[0] = '\0';
        break;
    case 1: // 目录
        sprintf(cstart, "\033[1;34m");
        break;
    case 4: // FIFO或管道 - ?
        sprintf(cstart, "\033[0;33;43m");
        break;
    case 6: // 软连接
        sprintf(cstart, "\033[1;36m");
        break;
    default:
        // 这对吗？
        // fprintf(stderr, "错误的文件类型");
    }
    printf("%s%s%s", cstart, name, cend);
}

int getFileType(mode_t perm) {
    if (S_ISREG(perm)) {
        return 0;
    }
    else if (S_ISDIR(perm)) {
        return 1;
    }
    else if (S_ISCHR(perm)) {
        return 2;
    }
    else if (S_ISBLK(perm)) {
        return 3;
    }
    else if (S_ISFIFO(perm)) {
        return 4;
    }
    else if ((perm & __S_IFMT) == __S_IFSOCK) {
        return 5;
    }
    else if (S_ISLNK(perm)) {
        return 6;
    }
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
    modifyTimeStr(st->st_mtime, timestr);
    if (disiNode) {
        printf("%lu ", st->st_ino);
    }
    printf("%s %lu %s %s %lu %s ", mode, st->st_nlink,
        pw->pw_name, gr->gr_name, st->st_size, timestr
    );
    printColorName(name, filetype);
    putchar('\n');
}

void swapInt(int* a, int* b) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

void quickSort(int* array, int l, int r, char objList[][MAX_BUFFER_SIZE + 1], struct stat statList[])
{
	if (l >= r) return;
	int begin = l, end = r, key = l;
	while (begin < end)
	{
		while (begin < end && fileCmp(array[begin], array[key], objList, statList) <= 0)
			++begin;
		while (begin < end && fileCmp(array[end], array[key], objList, statList) >= 0)
			--end;
        if (begin < end)
		    swapInt(array + begin, array + end);
	}
    if (fileCmp(array[begin], array[key], objList, statList) < 0) {
        swapInt(array + begin, array + key);
    }
	quickSort(array, l, begin - 1, objList, statList);
	quickSort(array, end + 1, r, objList, statList);
}

void printIdxObj(int idx, int flag, char objList[][MAX_BUFFER_SIZE + 1], struct stat statList[], char* prepath) {
    if (!S_ISDIR(statList[idx].st_mode)) {
        printColorName(objList[idx], getFileType(statList[idx].st_mode));
        putchar('\n');
        if (flag) {
            printf("\n\n");
        }
        return;
    }

    char newDir[MAX_BUFFER_SIZE + 1];
    sprintf(newDir, "%s%s", prepath, objList[idx]);
    int sl = strlen(prepath) + strlen(objList[idx]);
    if (newDir[sl - 1] != '/') {
        newDir[sl] = '/';
        newDir[sl + 1] = '\0';
    }
    else {
        newDir[sl] = '\0';
    }
    char fileFullPath[MAX_BUFFER_SIZE + 1];    

    DIR* dirp = opendir(newDir);
    
    if (dirp == NULL) {
        printf("权限不足，打开目录失败:%s\n", newDir);
        return;
    }

    if (flag ||displayContent & 2) {
        printf("%s:\n", newDir);
    }

    int capacity = PRI_PEND_NUM;
    int localPendingCount = 0;
    char (*localPendingObject)[MAX_BUFFER_SIZE + 1] = malloc(capacity * sizeof(*localPendingObject));
    struct stat* localObjStat = malloc(capacity * sizeof(struct stat));
    int* localIdxArr = malloc(capacity * sizeof(int));

    if (!localPendingObject || !localObjStat || !localIdxArr) {
        perror("内存分配失败");
        closedir(dirp);
        free(localPendingObject);
        free(localObjStat);
        free(localIdxArr);
        return;
    }

    struct dirent* dp;

    while ((dp = readdir(dirp)) != NULL) {
        if ((strcmp(dp->d_name, ".")==0 || strcmp(dp->d_name, "..")==0) && !(displayContent & 1) && objList == globalPendingObject) {
            continue;
        }
        if (dp->d_name[0] == '.' && !(displayContent & 1)) {
            continue;
        }

        if (localPendingCount >= capacity) {
            capacity *= 2;
            localPendingObject = realloc(localPendingObject, capacity * sizeof(*localPendingObject));
            localObjStat = realloc(localObjStat, capacity * sizeof(struct stat));
            localIdxArr = realloc(localIdxArr, capacity * sizeof(int));

            if (!localPendingObject || !localObjStat || !localIdxArr) {
                perror("内存扩展失败");
                closedir(dirp);
                free(localPendingObject);
                free(localObjStat);
                free(localIdxArr);
                return;
            }
        }
        strcpy(localPendingObject[localPendingCount++], dp->d_name);
    }

    for (int i=0; i<localPendingCount; ++i) {
        localIdxArr[i] = i;
        sprintf(fileFullPath, "%s%s", newDir, localPendingObject[i]);
        lstat(fileFullPath, localObjStat + i);
    }
    quickSort(localIdxArr, 0, localPendingCount - 1, localPendingObject, localObjStat);
    if (displayFormat & 1) {
        for (int i=0; i<localPendingCount; ++i) {
            showSingleFileInLine(localPendingObject[localIdxArr[i]], displayFormat & 2, localObjStat + localIdxArr[i]);
        }
    }
    else {
        for (int i=0; i<localPendingCount; ++i) {
            if (displayFormat & 2) {
                printf("%lu ", localObjStat[localIdxArr[i]].st_ino);
            }
            printColorName(localPendingObject[localIdxArr[i]], getFileType(localObjStat[localIdxArr[i]].st_mode));
            printf("  ");
            if ((i + 1) % 8 == 0 || i == localPendingCount - 1) {
                putchar('\n');
            }
        }
    }

    closedir(dirp);

    if (displayContent & 2) {
        for (int i=0; i<localPendingCount; ++i) {
            if (S_ISDIR(localObjStat[localIdxArr[i]].st_mode) && strcmp(localPendingObject[localIdxArr[i]], ".") != 0 && strcmp(localPendingObject[localIdxArr[i]], "..") != 0) {
                printIdxObj(localIdxArr[i], flag, localPendingObject, localObjStat, newDir);
            }
        }
    }

    free(localPendingObject);
    free(localObjStat);
    free(localIdxArr);
    if (flag) {
        printf("\n");
    }
}

void pendObjects() {
    for (int i=0; i<unavaCount; ++i) {
        printf("没有这个文件或目录或没有权限打开：%s\n", unavaObject[i]);
    }
    if (unavaCount)
        putchar('\n');
    int flag = pendingObjectCount - 1;
    for (int i=0; i<pendingObjectCount; ++i) {
        printIdxObj(idxArr[i], flag, globalPendingObject, pendingObjectStat, "");
        putchar('\n');
    }
}

int main(int argc, char *argv[]) {
    getcwd(cwdBuffer, MAX_BUFFER_SIZE);
    analyseArgs(argc, argv);
    for (int i=0; i<pendingObjectCount; ++i) {
        idxArr[i] = i;
    }
    quickSort(idxArr, 0, pendingObjectCount - 1, globalPendingObject, pendingObjectStat);
    pendObjects();
    return 0;
}