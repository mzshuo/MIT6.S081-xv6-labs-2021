## Lab: mmap (hard)

> Implement Unix `mmap()` and `munmap()` function:
> 
> `void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);`
> `int munmap(addr, length);`
>
> mmap can be called in many ways, but this lab requires only a subset of its features relevant to memory-mapping a file. You can assume that addr will always be zero, meaning that the kernel should decide the virtual address at which to map the file. mmap returns that address, or 0xffffffffffffffff if it fails. length is the number of bytes to map; it might not be the same as the file's length. prot indicates whether the memory should be mapped readable, writeable, and/or executable; you can assume that prot is PROT_READ or PROT_WRITE or both. flags will be either MAP_SHARED, meaning that modifications to the mapped memory should be written back to the file, or MAP_PRIVATE, meaning that they should not. You don't have to implement any other bits in flags. fd is the open file descriptor of the file to map. You can assume offset is zero (it's the starting point in the file at which to map).
>
> munmap(addr, length) should remove mmap mappings in the indicated address range. If the process has modified the memory and has it mapped MAP_SHARED, the modifications should first be written to the file. An munmap call might cover only a portion of an mmap-ed region, but you can assume that it will either unmap at the start, or at the end, or the whole region (but not punch a hole in the middle of a region).

1. add system call interface

```c
// user/user.h
void* mmap(void *addr, int, int, int, int, int);
int munmap(void *addr, int);
```

```c
// user/usys.pl
entry("mmap");
entry("munmap");
```

```c
// kernel/syscall.h
#define SYS_mmap   22
#define SYS_munmap 23
```

```c
// kernel/syscall.c
extern uint64 sys_mmap(void);
extern uint64 sys_munmap(void);

static uint64 (*syscalls[])(void) = {
// ...
[SYS_mmap]    sys_mmap,
[SYS_munmap]  sys_munmap,
};

```
