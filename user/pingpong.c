#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    if (argc != 1) {
        fprintf(2, "usage: pingpong\n");
        exit(1);
    }

    char *tmp = "";
    int p[2];
    pipe(p);

    if (fork() == 0) {
        read(p[0], tmp, 1);
        printf("%d: received ping\n", getpid());
        close(p[0]);

        write(p[1], "C", 1);
        close(p[1]);

        exit(0);
    }
    else {
        write(p[1], "P", 1);
        close(p[1]);

        read(p[0], tmp, 1);
        printf("%d: received pong\n", getpid());
        close(p[0]);
        
        exit(0);
    }

}