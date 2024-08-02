#include <stdio.h>
#include <dirent.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    const char *path = (argc > 1? argv[1] : ".");
    DIR *dir = opendir(path);
    if (dir == NULL) {
        perror(path);
        return 1;
    }

    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(dir))) {
        if (ent->d_type == DT_REG) {
            count++;
        }
    }

    if (errno != 0) {
        perror("readdir()");
        return 1;
    }

    closedir(dir);

    printf("%d\n", count);

    return 0;
}
