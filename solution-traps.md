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

