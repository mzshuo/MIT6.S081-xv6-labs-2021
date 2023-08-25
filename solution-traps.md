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


#### 2. backtrace (moderate)
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