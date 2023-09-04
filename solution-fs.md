## Lab: file system

#### 1. large files (moderate)

> Increase the maximum size of an xv6 file by supporting a "doubly-indirect" block in each inode, containing 256 addresses of singly-indirect blocks, each of which can contain up to 256 addresses of data blocks. The result will be that a file will be able to consist of up to 65803 blocks, or 256*256+256+11 blocks

1. modify `dinode` and `inode` structure

   ```c
   // kernel/fs.h
   #define NDIRECT 11
   #define MAXFILE (NDIRECT + NINDIRECT + NINDIRECT * NINDIRECT)

   struct dinode {
     // ...
     uint addrs[NDIRECT+2];  // Data block addresses
   };
   ```

   ```c
   // kernel/file.h
   struct inode {
     // ...
     uint addrs[NDIRECT+2];
   };
   ```

2. modify `bmap()` and `itrunc()` function to support doubly-indirect block

   ```c
   // kernel/fs.c
   static uint
   bmap(struct inode *ip, uint bn)
   {
     // ...
     bn -= NINDIRECT;
     if(bn < MAXFILE){
       // Load doubly-indirect block, allocating if necessary.
       if((addr = ip->addrs[NDIRECT+1]) == 0)
         ip->addrs[NDIRECT+1] = addr = balloc(ip->dev);
       bp = bread(ip->dev, addr);
       a = (uint*)bp->data;

       if((addr = a[bn/NINDIRECT]) == 0){
         a[bn/NINDIRECT] = addr = balloc(ip->dev);
         log_write(bp);
       }
       brelse(bp);
       bp = bread(ip->dev, addr);
       a = (uint*)bp->data;
       
       if((addr = a[bn%NINDIRECT]) == 0){
         a[bn%NINDIRECT] = addr = balloc(ip->dev);
         log_write(bp);
       }
       brelse(bp);
       return addr;
     }
     // ...
   }
   ```

   ```c
   // kernel/fs.c
   void
   itrunc(struct inode *ip)
   {
     // ...
     if(ip->addrs[NDIRECT+1]){
       bp = bread(ip->dev, ip->addrs[NDIRECT+1]);
       a = (uint*)bp->data;
       for(j = 0; j < NINDIRECT; ++j){
         if(a[j]){
           struct buf *nbp = bread(ip->dev, a[j]);
           uint *na = (uint*)nbp->data;
           for(int k = 0; k < NINDIRECT; ++k){
             if(na[k])
               bfree(ip->dev, na[k]);
           }
           brelse(nbp);
           bfree(ip->dev, a[j]);
         }
       }
       brelse(bp);
       bfree(ip->dev, ip->addrs[NDIRECT+1]);
       ip->addrs[NDIRECT+1] = 0;
     }
     // ...
   }
   ```

#### 2. symbolic links (moderate)
> Add symbolic links to xv6. Symbolic links (or soft links) refer to a linked file by pathname; when a symbolic link is opened, the kernel follows the link to the referred file. Symbolic links resembles hard links, but hard links are restricted to pointing to file on the same disk, while symbolic links can cross disk devices. Although xv6 doesn't support multiple devices, implementing this system call is a good exercise to understand how pathname lookup works.

1. add variables related to the system call `symlink()`

   ```c
   // user/user.h
   int symlink(char*, char*);
   ```

   ```c
   // user/usys.pl
   entry("symlink");
   ```

   ```c
   // kernel/syscall.h
   #define SYS_symlink 22
   ```

   ```c
   // kernel/syscall.c
   extern uint64 sys_symlink(void);

   static uint64 (*syscalls[])(void) = {
   // ...
   [SYS_symlink] sys_symlink,
   };
   ```

2. implement `sys_symlink()` function

   ```c
   // kernel/stat.h
   #define T_SYMLINK 4   // Symlink
   ```

   ```c
   // kernel/fcntl.h
   #define O_NOFOLLOW 0x800
   ```

   ```c
   // kernel/sysfile.c
   uint64
   sys_symlink(void){
     char target[MAXPATH], path[MAXPATH];
     struct inode *ip;

     if(argstr(0, target, MAXPATH) < 0 || argstr(1, path, MAXPATH) < 0)
       return -1;

     begin_op();
     if((ip = create(path, T_SYMLINK, 0, 0)) == 0){
       end_op();
       return -1;
     }
     if(writei(ip, 0, (uint64)target, 0, MAXPATH) != MAXPATH){
       end_op();
       return -1;
     }

     iunlockput(ip);
     end_op();
     return 0;
   }

3. modify `sys_open()` to support symbolic link

   ```c
   uint64
   sys_open(void)
   {
     // ...
     if(ip->type == T_SYMLINK && !(omode & O_NOFOLLOW)){
       char target[MAXPATH];
       int depth = 0;
       while(ip->type == T_SYMLINK){
         readi(ip, 0, (uint64)target, 0, MAXPATH);
         iunlockput(ip);
         if((ip = namei(target)) == 0){
           end_op();
           return -1;
         }
         ilock(ip);
         if(++depth == 10){
           end_op();
           return -1;
         }
       }
     }
     // ...
   }
   ```

4. add `symlinktest.c` to Makefile to compile it

   ```c
   // Makefile
   UPROGS=\
   	// ...
   	$U/_symlinktest\
   ```