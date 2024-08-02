#ifdef __linux__
#define _GNU_SOURCE

#include <stdint.h>
#endif

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <errno.h>

#define BUFSIZE 10 * 1024 * 1024

#ifdef __linux__
#define getdents getdents64
#define dirent linux_dirent64

struct linux_dirent64 {
    uint64_t d_ino;
    int64_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[0];
};
#endif

int main(int argc, char *argv[]) {
    const char *path = (argc > 1? argv[1] : ".");
    int dir = open(path, O_RDONLY | O_DIRECTORY);
    if (dir == -1) {
        perror(path);
        return 1;
    }

    char *buf = malloc(BUFSIZE);
    if (buf == NULL) {
        perror("malloc()");
        return 1;
    }

    int count = 0;
    while (1) {
        ssize_t bytes_read = getdents(dir, buf, BUFSIZE);
        if (bytes_read == -1) {
            perror("getdents()");
            return 1;
        }

        if (bytes_read == 0) {
            break;
        }

        size_t pos = 0;
        while (pos < bytes_read) {
            struct dirent *ent = (struct dirent *)(buf + pos);
            if (ent->d_type == DT_REG) {
                count++;
            }
            pos += ent->d_reclen;
        }
    }

    if (errno != 0) {
        perror("readdir()");
        return 1;
    }

    printf("%d\n", count);

    return 0;
}
