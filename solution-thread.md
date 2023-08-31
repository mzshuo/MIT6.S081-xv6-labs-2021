## Lab: Multithreading

#### 1. uthread: switching between threads (moderate)

> Design the context switch mechanism for a user-level threading system, and then implement it.

1. define thread context and add it to `struct thread`
   ```c
   // user/uthread.c
   struct tcontext {
     uint64 ra;
     uint64 sp;

     // callee-saved
     uint64 s0;
     uint64 s1;
     uint64 s2;
     uint64 s3;
     uint64 s4;
     uint64 s5;
     uint64 s6;
     uint64 s7;
     uint64 s8;
     uint64 s9;
     uint64 s10;
     uint64 s11;
   };

   struct thread {
     char             stack[STACK_SIZE]; /* the thread's stack */
     int              state;             /* FREE, RUNNING, RUNNABLE */
     struct tcontext  tcontext;          /* thread context */
   };
   ```

2. set context infomation especially `ra` and `sp` for each thread

   ```c
   // user/uthread.c
   void 
   thread_create(void (*func)())
   {
     struct thread *t;

     for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
       if (t->state == FREE) break;
     }
     t->state = RUNNABLE;

     // YOUR CODE HERE
     // set thread context to prepare for the first time to be scheduled
     t->tcontext.ra = (uint64)func;
     t->tcontext.sp = (uint64)(t->stack + STACK_SIZE);
   }
   ```

3. call `thread_switch` in thread scheduler

   ```c
   // user/uthread.c
   void 
   thread_schedule(void)
   {
     // ...

     if (current_thread != next_thread) {         /* switch threads?  */
       next_thread->state = RUNNING;
       t = current_thread;
       current_thread = next_thread;
       /* YOUR CODE HERE
        * Invoke thread_switch to switch from t to next_thread:
        * thread_switch(??, ??);
        */
       thread_switch((uint64)&t->tcontext, (uint64)&next_thread->tcontext);
     } else
       next_thread = 0;
   }
   ```

4. implement thread context switch

   ```c
   .text

   	/*
   	* save the old thread's registers,
   	* restore the new thread's registers.
   	*/

   .globl thread_switch
   thread_switch:
   	/* YOUR CODE HERE */

   	sd ra, 0(a0)
   	sd sp, 8(a0)
   	sd s0, 16(a0)
   	sd s1, 24(a0)
   	sd s2, 32(a0)
   	sd s3, 40(a0)
   	sd s4, 48(a0)
   	sd s5, 56(a0)
   	sd s6, 64(a0)
   	sd s7, 72(a0)
   	sd s8, 80(a0)
   	sd s9, 88(a0)
   	sd s10, 96(a0)
   	sd s11, 104(a0)

   	ld ra, 0(a1)
   	ld sp, 8(a1)
   	ld s0, 16(a1)
   	ld s1, 24(a1)
   	ld s2, 32(a1)
   	ld s3, 40(a1)
   	ld s4, 48(a1)
   	ld s5, 56(a1)
   	ld s6, 64(a1)
   	ld s7, 72(a1)
   	ld s8, 80(a1)
   	ld s9, 88(a1)
   	ld s10, 96(a1)
   	ld s11, 104(a1)

   	ret    /* return to ra */
   ```

#### 2. using threads (moderate)
> In this assignment you will explore parallel programming with threads and locks using a hash table. This assignment uses the UNIX pthread threading library.

1. create a mutex lock for each table, initialize them and protect critical section

   ```c
   // notxv6/ph.c
   pthread_mutex_t tablelock[NBUCKET];

   int
   main(int argc, char *argv[])
   {
     // ...

     for (int i = 0; i < NBUCKET; ++i)
       pthread_mutex_init(&tablelock[i], NULL);

     // ...
   }

   static 
   void put(int key, int value)
   {
     int i = key % NBUCKET;

     // is the key already present?
     struct entry *e = 0;

     pthread_mutex_lock(&tablelock[i]);             // lock
     for (e = table[i]; e != 0; e = e->next) {
       if (e->key == key)
         break;
     }
     if(e){
       // update the existing key.
       e->value = value;
     } else {
       // the new is new.
       insert(key, value, &table[i], table[i]);
     }
     pthread_mutex_unlock(&tablelock[i]);           // unlock

   }
   ```

#### 3. barrier (moderate)
> In this assignment you'll implement a barrier: a point in an application at which all participating threads must wait until all other participating threads reach that point too. You'll use pthread condition variables, which are a sequence coordination technique similar to xv6's sleep and wakeup.

1. use `pthread_cond_t` variable to count threads, cooperating with sleep and wakeup mechanism.

   ```c
   static void 
   barrier()
   {
     // YOUR CODE HERE
     //
     // Block until all threads have called barrier() and
     // then increment bstate.round.
     //
     pthread_mutex_lock(&bstate.barrier_mutex);
     if(++bstate.nthread < nthread){
       pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
     } else {
       bstate.nthread = 0;
       ++bstate.round;
       pthread_cond_broadcast(&bstate.barrier_cond);
     }
     pthread_mutex_unlock(&bstate.barrier_mutex);
   }
   ```