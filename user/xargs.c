#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

/* Read a line from standard input and store it into buf[]. 
 * Return -1 if read failed else 0. 
 */
int readline(char *buf) {
    char *p = buf;
    while (read(0, p, 1) == 1) {
        if (*p == '\n') {
            return 0;
        }
        p++;
    }
    return -1;
}

/* Parse arguments from the line and store them into args */
void parseline(char *args[], char* line) {
    char *p = line;
    int curr_arg = 0;
    int len = 0;

    while (1) {
        if (*p == ' ' || *p == '\n') {
            if (args[curr_arg] != 0) free(args[curr_arg]);
            args[curr_arg] = malloc(len+1);
            memmove(args[curr_arg], p-len, len);
            args[curr_arg][len] = '\0';

            len = 0;
            curr_arg++;
            if (*p == '\n') {
                args[curr_arg] = 0;
                return;
            }
            while (*p == ' ') { p++; }
        }
        else {
            len++;
            p++;
        }
    }
}

int main(int argc, char *argv[]) {
    char *cmd = argv[1];
    char *args[MAXARG];
    
    for (int i = 1; i < argc; ++i) {
        args[i-1] = argv[i];
    }

    char buf[1024];
    while (readline(buf) != -1) {
        parseline(args + argc - 1, buf);
        if (fork() == 0) {
            exec(cmd, args);
        }
        else {
            wait(0);
        }
    }

    exit(0);
}