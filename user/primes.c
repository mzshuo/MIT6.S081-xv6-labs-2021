#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void prime(int p_left[2]) {
    close(p_left[1]);

    int first_prime = 0;
    int read_a_prime = read(p_left[0], &first_prime, 4);

    if (read_a_prime == 0) {
        close(p_left[0]);
        return;
    }

    int p_right[2];
    pipe(p_right);

    if (fork() == 0) {
        // child process
        prime(p_right);
    }
    else {
        // parent process
        printf("prime %d\n", first_prime);
        close(p_right[0]);

        int num = 0;
        while (read(p_left[0], &num, 4) != 0) {
            if (num % first_prime != 0) {
                write(p_right[1], &num, 4);
            }
        }

        close(p_right[1]);
        wait((int *)0);
    }
    exit(0);

}

int main(int argc, char *argv[]) {
    if (argc != 1) {
        fprintf(2, "usage: primes\n");
        exit(1);
    }

    int p[2];
    pipe(p);

    if (fork() == 0) {
        prime(p);
    }
    else {
        close(p[0]);
        for (int num = 2; num <= 35; ++num) {
            write(p[1], &num, 4);
        }
        close(p[1]);

        wait((int *)0);
        exit(0);
    }
    return 0;
}