#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find(char *path, char *file_name) {
    int fd = open(path, 0);
    if (fd < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    struct dirent de;
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0) continue;
        if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) continue;

        char subpath[512];
        strcpy(subpath, path);
        subpath[strlen(path)] = '/';
        memmove(subpath+strlen(path)+1, de.name, DIRSIZ);

        struct stat st;
        int subfd = open(subpath, 0);
        fstat(subfd, &st);

        switch (st.type) {
            case T_FILE:
                if (strcmp(de.name, file_name) == 0) {
                    printf("%s\n", subpath);
                }
                break;
            case T_DIR:
                find(subpath, file_name);
        }
        close(subfd);
    }

    close(fd);
    return;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(2, "usage: find [path] [file_name]\n");
        exit(1);
    }

    find(argv[1], argv[2]);
    exit(0);
}