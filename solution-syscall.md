## Lab: system calls

#### 1. System call tracing (moderate)

> Create a new trace system call that will control tracing. It should take one argument, an integer "mask", whose bits specify which system calls to trace.The trace system call should enable tracing for the process that calls it and any children that it subsequently forks, but should not affect other processes.


1. add `trace` to `UPROGS` in *Makefile* to compile *user/trace.c* when booting xv6
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
   	$U/_trace\      // add this line
   ```

2. create user-space stubs for the system call trace
   - add a prototype for the system call trace in *user/user.h*
     ```c
     // user/user.h
     // system calls
     int fork(void);
     int exit(int) __attribute__((noreturn));
     int wait(int*);
     int pipe(int*);
     int write(int, const void*, int);
     int read(int, void*, int);
     int close(int);
     int kill(int);
     int exec(char*, char**);
     int open(const char*, int);
     int mknod(const char*, short, short);
     int unlink(const char*);
     int fstat(int fd, struct stat*);
     int link(const char*, const char*);
     int mkdir(const char*);
     int chdir(const char*);
     int dup(int);
     int getpid(void);
     char* sbrk(int);
     int sleep(int);
     int uptime(void);
     int trace(int);         // add this line
     ```
   
   - add a stub for system call trace in *user/usys.pl*
     ```c
     // user/usys.pl
     sub entry {
      my $name = shift;
      print ".global $name\n";
      print "${name}:\n";
      print " li a7, SYS_${name}\n";
      print " ecall\n";
      print " ret\n";
     }
     	
     entry("fork");
     entry("exit");
     entry("wait");
     entry("pipe");
     entry("read");
     entry("write");
     entry("close");
     entry("kill");
     entry("exec");
     entry("open");
     entry("mknod");
     entry("unlink");
     entry("fstat");
     entry("link");
     entry("mkdir");
     entry("chdir");
     entry("dup");
     entry("getpid");
     entry("sbrk");
     entry("sleep");
     entry("uptime");
     entry("trace");        // add this line
     ```

   - add a syscall number in *kernel/syscall.h*
     ```c
     // kernel/syscall.h
     // System call numbers
     #define SYS_fork    1
     #define SYS_exit    2
     #define SYS_wait    3
     #define SYS_pipe    4
     #define SYS_read    5
     #define SYS_kill    6
     #define SYS_exec    7
     #define SYS_fstat   8
     #define SYS_chdir   9
     #define SYS_dup    10
     #define SYS_getpid 11
     #define SYS_sbrk   12
     #define SYS_sleep  13
     #define SYS_uptime 14
     #define SYS_open   15
     #define SYS_write  16
     #define SYS_mknod  17
     #define SYS_unlink 18
     #define SYS_link   19
     #define SYS_mkdir  20
     #define SYS_close  21
     #define SYS_trace  22      // add this line
     ```

3. implement the system call trace in the kernel
   
   - add a new variable `trace_mask` to `proc` structure in *kernel/proc.h* （add the last line below)

     ```c
     // kernel/proc.h
     // Per-process state
     struct proc {
       struct spinlock lock;

       // p->lock must be held when using these:
       enum procstate state;        // Process state
       void *chan;                  // If non-zero, sleeping on chan
       int killed;                  // If non-zero, have been killed
       int xstate;                  // Exit status to be returned to parent's wait
       int pid;                     // Process ID

       // wait_lock must be held when using this:
       struct proc *parent;         // Parent process

       // these are private to the process, so p->lock need not be held.
       uint64 kstack;               // Virtual address of kernel stack
       uint64 sz;                   // Size of process memory (bytes)
       pagetable_t pagetable;       // User page table
       struct trapframe *trapframe; // data page for trampoline.S
       struct context context;      // swtch() here to run process
       struct file *ofile[NOFILE];  // Open files
       struct inode *cwd;           // Current directory
       char name[16];               // Process name (debugging)
       
       int trace_mask;              // mask of system calls to be trace(at most 31 bits)
     };
     ```

   - add `sys_trace()` function in *kernel/sysproc.c*
     ```c
     // kernel/sysproc.c
     uint64 sys_trace(void) {
       int n;
       if (argint(0, &n) < 0)
           return -1;
       myproc()->trace_mask = n;
       return 0;
     }
     ```

   - modify `fork()` in *kernel/proc.c* to copy the trace mask from the parent to the child process
       ```c
       // kernel/proc.c
       int
       fork(void)
       {
         int i, pid;
         struct proc *np;
         struct proc *p = myproc();

         // ...

         // copy trace_mask
         np->trace_mask = p->trace_mask;

         // ...

         return pid;
       }
       ```

    - modify the `syscall()` function in *kernel/syscall.c* to print the trace output
       ```c
       // kernel/syscall.c
       extern uint64 sys_trace(void);  // add this line

       static uint64 (*syscalls[])(void) = {
       // ...
       [SYS_trace]   sys_trace,        // add this line
       };

       // add this array of syscall names to index
       char* sys_names[] = {
       [SYS_fork]    "fork",
       [SYS_exit]    "exit",
       [SYS_wait]    "wait",
       [SYS_pipe]    "pipe",
       [SYS_read]    "read",
       [SYS_kill]    "kill",
       [SYS_exec]    "exec",
       [SYS_fstat]   "fstat",
       [SYS_chdir]   "chdir",
       [SYS_dup]     "dup",
       [SYS_getpid]  "getpid",
       [SYS_sbrk]    "sbrk",
       [SYS_sleep]   "sleep",
       [SYS_uptime]  "uptime",
       [SYS_open]    "open",
       [SYS_write]   "write",
       [SYS_mknod]   "mknod",
       [SYS_unlink]  "unlink",
       [SYS_link]    "link",
       [SYS_mkdir]   "mkdir",
       [SYS_close]   "close",
       [SYS_trace]   "trace",
       };

       void
       syscall(void)
       {
         int num;
         struct proc *p = myproc();

         num = p->trapframe->a7;
         if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
           p->trapframe->a0 = syscalls[num]();
           // check whether to trace this system call
           if ( ((1<<num) & p->trace_mask) != 0 ) {
             printf("%d: syscall %s -> %d\n", p->pid, sys_names[num], p->trapframe->a0);     // trace info
           }
         } else {
           printf("%d %s: unknown sys call %d\n",
                   p->pid, p->name, num);
           p->trapframe->a0 = -1;
         }
       }
       ```


#### 2. Sysinfo (moderate)

> Add a system call, sysinfo, that collects information about the running system. 

1. add `sysinfotest` to `UPROGS` in *Makefile* to compile *user/sysinfotest.c* when booting xv6
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
      	$U/_trace\
      	$U/_sysinfotest\    // add this line
   ```
  
2. create user-space stubs for the system call sysinfo
   - add a prototype for system call `sysinfo`
     ```c
     // user/user.h
     struct sysinfo;
     int sysinfo(struct sysinfo*);
     ```
  
   - add a stub in *user/usys.pl*
     ```c
     // usys.pl
     entry("sysinfo");
     ```
  
   - add a syscall number
     ```c
     // kernel/syscall.h
     #define SYS_sysinfo  23
     ```

3. implement the system call sysinfo in the kernel
   - add `syscalls[SYS_sysinfo]` in *kernel/syscall.c*

     ```c
     // kernel/syscall.c
     extern uint64 sys_sysinfo(void);    // add this line

     static uint64 (*syscalls[])(void) = {
     // ...
     [SYS_trace]   sys_trace,
     [SYS_sysinfo] sys_sysinfo,          // add this line
     };
     ```

   - add `sys_sysinfo()` function in *kernel/sysproc.c*

     ```c
     // kernel/sysproc.c
     #include "sysinfo.h"    // add this head file

     uint64 sys_sysinfo(void) {
       uint64 addr;
       if (argaddr(0, &addr) < 0 || addr < 0 || addr >= MAXVA)     // check if the virtual address provided by argument is legitimate
           return -1;
       
       struct sysinfo sinfo;
       sinfo.freemem = freemem();
       sinfo.nproc = nproc();

       struct proc *p = myproc();
       copyout(p->pagetable, addr, (char *)&sinfo, sizeof(sinfo));

       return 0;
     }
     ```


   - add `freemem()` function to collect the amount of free memory in *kernel/kalloc.c*

     ```c
     // kernel/kalloc.c
     // Collect the amount of free memory (bytes)
     uint64 freemem() {
       uint64 page_num = 0;
       for(struct run *p = kmem.freelist; p; p = p->next) {
           page_num += 1;
       }
       return page_num * PGSIZE;
     }
     ```


   - add `nproc()` function to collect the number of processes in *kernel/proc.c*

     ```c
     // kernel/proc.c
     // Collect the number of processes whose state is not UNUSED.
     uint64 nproc(void) {
       uint64 num = 0;
       for (struct proc *p = proc; p <= &proc[NPROC]; p++) {
           if (p->state != UNUSED)
               num += 1;
       }
       return num;
     }
     ```

   - add `freemem()` and `nproc()` to *kernel/defs.h*

      ```c
      // kernel/defs.h
      uint64          freemem(void);
      uint64          nproc(void);
      ```



