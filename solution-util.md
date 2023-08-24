## Lab: Xv6 and Unix utilities

#### 0. Boot xv6 (easy)

#### 1. sleep (easy)

> Implement the UNIX program sleep for xv6.

1. add a new file user/sleep.c
	```c
	// user/sleep.c
	#include "kernel/types.h"
	#include "kernel/stat.h"
	#include "user/user.h"
	
	int main(int argc, char *argv[]) {
	    if (argc != 2) {
	        fprintf(2, "Usage: sleep [n_ticks]");
	        exit(1);
	    }
	    
	    int time = atoi(argv[1]);
	    sleep(time);
	
	    exit(0);
	}
	```

2. add `sleep` to `UPROGS` in Makefile to compile it when booting xv6
	```c
	// Makefile
	UPROGS=\
		$U/_cat\
		$U/_echo\
		$U/_forktest\
		$U/_grep\
		$U/_init\
		$U/_kill\
		$U/_ln\
		$U/_ls\
		$U/_mkdir\
		$U/_rm\
		$U/_sh\
		$U/_stressfs\
		$U/_usertests\
		$U/_grind\
		$U/_wc\
		$U/_zombie\
		$U/_sleep\      // add this line
	```

#### 2. pingpong (easy)

> Write a program that uses UNIX system calls to ''ping-pong'' a byte between two processes over a pair of pipes, one for each direction.

1. add a new file user/pingpong.c
	```c
	// user/pingpong.c
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
	```

2. add `pingpong`  to `UPROGS` in Makefile to compile it when booting xv6
	```c
	// Makefile
	UPROGS=\
		$U/_cat\
		$U/_echo\
		$U/_forktest\
		$U/_grep\
		$U/_init\
		$U/_kill\
		$U/_ln\
		$U/_ls\
		$U/_mkdir\
		$U/_rm\
		$U/_sh\
		$U/_stressfs\
		$U/_usertests\
		$U/_grind\
		$U/_wc\
		$U/_zombie\
		$U/_sleep\
		$U/_pingpong\	 // add this line
	```


#### 3. primes (moderate)/(hard)
> Write a concurrent version of prime sieve using pipes. 

1. add a new file user/primes.c
	```c
	// user/primes.c
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
	```

2. add `primes` to `UPROGS` in Makefile to compile it when booting xv6
	```c
	// Makefile
	UPROGS=\
		$U/_cat\
		$U/_echo\
		$U/_forktest\
		$U/_grep\
		$U/_init\
		$U/_kill\
		$U/_ln\
		$U/_ls\
		$U/_mkdir\
		$U/_rm\
		$U/_sh\
		$U/_stressfs\
		$U/_usertests\
		$U/_grind\
		$U/_wc\
		$U/_zombie\
		$U/_sleep\
		$U/_pingpong\
		$U/_primes\		// add this line
	```


#### 4. find (moderate)

> Write a simple version of the UNIX find program: find all the files in a directory tree with a specific name.

1. add a new file user/find.c
	```c
	// user/find.c
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
	```

2. add `find` to `UPROGS` in Makefile to compile it when booting xv6
	```c
	// Makefile
	UPROGS=\
		$U/_cat\
		$U/_echo\
		$U/_forktest\
		$U/_grep\
		$U/_init\
		$U/_kill\
		$U/_ln\
		$U/_ls\
		$U/_mkdir\
		$U/_rm\
		$U/_sh\
		$U/_stressfs\
		$U/_usertests\
		$U/_grind\
		$U/_wc\
		$U/_zombie\
		$U/_sleep\
		$U/_pingpong\
		$U/_primes\
		$U/_find\       // add this line
	```

#### 5. xargs (moderate)
> Write a simple version of the UNIX xargs program: read lines from the standard input and run a command for each line, supplying the line as arguments to the command.

1. add a new file user/xargs.c
	```c
	// user/xargs.c
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
	```

2. add `xargs` to `UPROGS` in Makefile to compile it when booting xv6
	```c
	// Makefile
	UPROGS=\
		$U/_cat\
		$U/_echo\
		$U/_forktest\
		$U/_grep\
		$U/_init\
		$U/_kill\
		$U/_ln\
		$U/_ls\
		$U/_mkdir\
		$U/_rm\
		$U/_sh\
		$U/_stressfs\
		$U/_usertests\
		$U/_grind\
		$U/_wc\
		$U/_zombie\
		$U/_sleep\
		$U/_pingpong\
		$U/_primes\
		$U/_find\
		$U/_xargs\      // add this line
	```
