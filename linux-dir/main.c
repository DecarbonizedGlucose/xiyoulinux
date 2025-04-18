#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#define MAX_SIZE 100000

char buffer[MAX_SIZE + 1];

int main() {
    while (1) {
        ssize_t numRead = read(STDIN_FILENO, buffer, MAX_SIZE);
        buffer[numRead] = 0;
        write(STDOUT_FILENO, buffer, numRead);
    }
    return 0;
}