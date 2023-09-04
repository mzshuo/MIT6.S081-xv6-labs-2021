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