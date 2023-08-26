## Lab: traps

#### 1. RISC-V assembly (easy)

> Read the code in call.asm for the functions g, f, and main. Answer following questions.


1. Which registers contain arguments to functions? For example, which register holds 13 in main's call to printf?
   - a0-a7 registers contain arguments to functions.
   - a2 register holds 13 in main's call to printf.


2. Where is the call to function f in the assembly code for main? Where is the call to g? (Hint: the compiler may inline functions.)
   - g(x) is inlined within f(x) and f(x) is further inlined into main()


3. At what address is the function printf located?
   - 0x34 + 1510 = 0x616 (pc-relative calculated)


4. What value is in the register ra just after the jalr to printf in main?
   - 0x38, the next instruction's address after printf()


5. Run the following code.

     ```c
   	unsigned int i = 0x00646c72;
   	printf("H%x Wo%s", 57616, &i);   
     ```   
  
    What is the output?
    - He110 World (little endian)
    - big endian: `unsigned int i = 0x726c6400;`
    - do not need to change 57616


6. In the following code, what is going to be printed after 'y='? (note: the answer is not a specific value.) Why does this happen?
   ```c
   printf("x=%d y=%d", 3);
   ```
   - a random value stored in a2 register, as the caller did not store the third argument into a2, while the callee tries to load it from a2.


#### 2. Backtrace (moderate)
> Implement a backtrace() function to show a list of the function calls on the stack above the point at which the backtrace() is called.

1. implement the `backtrace()` function in *kernel/printf.c*
   ```c
   // kernel/printf.c
   void backtrace(void) {
     uint64 fp = r_fp();
     uint64 stacktop = PGROUNDUP(fp);
     uint64 stackbottom = PGROUNDDOWN(fp);
     uint64 ra = 0;

     printf("backtrace:\n");
     while (fp < stacktop && fp > stackbottom) {
       ra = *(uint64*)(fp-8);
       fp = *(uint64*)(fp-16);
       printf("%p\n", ra);
     }
   }
   ```

   add the prototype for `backtrace` to *kernel/defs.h*
   ```c
   // kernel/defs.h
   void backtrace(void);
   ```

2. add `r_fp()` function which can read value from s0 register
   ```c
   // kernel/riscv.h
   static inline uint64
   r_fp()
   {
     uint64 x;
     asm volatile("mv %0, s0" : "=r" (x) );
     return x;
   }
   ```

3. insert the call to `backtrace()` in `sys_sleep()` to test it
   ```c
   // kernel/sysproc.c
   uint64
   sys_sleep(void)
   {
     // ...
     backtrace();
     return 0;
   }
   ```
   call it from `panic()` for following debugging
   ```c
   // kernel/printf.c
   void
   panic(char *s)
   {
     // ...
     backtrace();
     // ...
   }
   ```


#### 3. Alarm (hard)
> Add a feature to xv6 that periodically alerts a process as it uses CPU time. You should add a new sigalarm(interval, handler) system call. If an application calls sigalarm(n, fn), then after every n "ticks" of CPU time that the program consumes, the kernel should cause application function fn to be called. When fn returns, the application should resume where it left off.


#### test0: invoke handler

1. modify *Makefile* to cause *alarmtest.c* to be compiled as an xv6 user program
   ```c
   // Makefile
   UPROGS=\
	// ...
	$U/_alarmtest\
   ```

2. add system call stubs for `sigalarm` and `sigreturn`
   - add declaration
     ```c
     // user/user.h
     int sigalarm(int interval, void(*handler)());
     int sigreturn(void);
     ```
   
   - add entry to jump in the kernel
     ```c
     // user/usys.pl
     entry("sigalarm");
     entry("sigreturn");
     ```

   - add syscall number
     ```c
     // kernel/syscall.h
     #define SYS_sigalarm  22
     #define SYS_sigreturn 23
     ```

   - add syscall interface
     ```c
     // kernel/syscall.c
     extern uint64 sys_sigalarm(void);
     extern uint64 sys_sigreturn(void);

     static uint64 (*syscalls[])(void) = {
     // ...
     [SYS_sigalarm]  sys_sigalarm,
     [SYS_sigreturn] sys_sigreturn,
     };
     ```

3. define and initiate variables needed
   ```c
   // kernel/proc.h
   struct proc {
     // ...
     int interval;
     uint64 handler;
     int ticks;
   };
   ```

   ```c
   // kernel/proc.c
   static struct proc*
   allocproc(void)
   {
     // ...
     p->interval = 0;
     p->handler = 0;
     p->ticks = 0;

     return p;
   }
   ```

4. define `sys_sigalarm` and `sys_sigreturn` function

   ```c
   // kernel/sysproc.c
   uint64
   sys_sigalarm(void)
   {
     int interval;
     uint64 handler;

     if (argint(0, &interval) < 0)
       return -1;
     if (argaddr(1, &handler) < 0)
       return -1;

     myproc()->interval = interval;
     myproc()->handler = handler;
     myproc()->ticks = 0;

     return 0;
   }

   uint64
   sys_sigreturn(void)
   {
     return 0;
   }
   ```

5. process timer interrupts in `usertrap()`

   ```c
   // kernel/trap.c
   void
   usertrap(void)
   {
     // ...
     
     if (which_dev == 2) {
       if (p->interval != 0 && ++(p->ticks) == p->interval) {
         p->ticks = 0;
         p->trapframe->epc = p->handler;
       }
     }

     // ...
   }
   ```