// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.
#define NBUCKET 13  // 可以根据需要选择适当的桶数

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock;// 定义一个自旋锁，用于保护缓存的全局访问
  // 定义一个缓冲区头部数组，大小为NBUCKET，用于缓存的管理T
  struct buf head[NBUCKET];
  struct buf hash[NBUCKET][NBUF]; // 定义一个哈希表，每个桶中包含NBUF个缓冲区
  // 为每个桶定义一个自旋锁，用于保护该桶的访问
  struct spinlock hashlock[NBUCKET];  
} bcache;

void
binit(void) // 初始化缓冲区缓存
{
  struct buf *b; // 定义一个指向缓冲区结构的指针
  initlock(&bcache.lock, "bcache"); // 初始化全局缓存锁
  for (int i = 0; i < NBUCKET; i++) { // 遍历所有桶
    initlock(&bcache.hashlock[i], "bcache"); // 初始化每个桶的自旋锁
    // 创建缓冲区的双向链表
    // 头部的前一个指针指向自身，形成环
    bcache.head[i].prev = &bcache.head[i]; 
    // 头部的下一个指针指向自身，形成环
    bcache.head[i].next = &bcache.head[i]; 
    // 遍历每个桶中的所有缓冲区
    for(b = bcache.hash[i]; b < bcache.hash[i]+NBUF; b++){ 
    // 缓冲区的下一个指针指向头部的下一个指针
      b->next = bcache.head[i].next; 
      b->prev = &bcache.head[i]; // 缓冲区的前一个指针指向头部
      initsleeplock(&b->lock, "buffer"); // 初始化缓冲区的睡眠锁
      // 头部下一个指针的前一个指针指向当前缓冲区
      bcache.head[i].next->prev = b; 
      bcache.head[i].next = b; // 头部的下一个指针指向当前缓冲区
    }
  }
}



// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno) // 获取指定设备和块号的缓冲区
{
  struct buf *b; // 定义一个指向缓冲区结构的指针

  uint hashcode = blockno % NBUCKET; // 计算块号的哈希值，确定桶的位置
  acquire(&bcache.hashlock[hashcode]); // 获取该桶的自旋锁

  // 块是否已经被缓存？
  for(b = bcache.head[hashcode].next; b != &bcache.head[hashcode]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){ // 如果找到匹配的设备和块号
      b->refcnt++; // 增加引用计数
      release(&bcache.hashlock[hashcode]); // 释放桶的自旋锁
      acquiresleep(&b->lock); // 获取缓冲区的睡眠锁
      return b; // 返回找到的缓冲区
    }
  }

  // 未缓存
  // 回收最近最少使用（LRU）的未使用缓冲区
  for(b = bcache.head[hashcode].prev; b != &bcache.head[hashcode]; b = b->prev){
    if(b->refcnt == 0) { // 找到引用计数为0的缓冲区
      b->dev = dev; // 设置设备号
      b->blockno = blockno; // 设置块号
      b->valid = 0; // 标记缓冲区无效
      b->refcnt = 1; // 设置引用计数为1
      release(&bcache.hashlock[hashcode]); // 释放桶的自旋锁
      acquiresleep(&b->lock); // 获取缓冲区的睡眠锁
      return b; // 返回回收的缓冲区
    }
  }
  release(&bcache.hashlock[hashcode]); // 释放桶的自旋锁
  panic("bget: no buffers"); // 如果没有可用的缓冲区，触发panic
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}


void
brelse(struct buf *b) // 释放缓冲区
{
  if(!holdingsleep(&b->lock)) // 检查当前是否持有缓冲区的睡眠锁
    panic("brelse"); // 如果没有持有锁，触发panic

  releasesleep(&b->lock); // 释放缓冲区的睡眠锁

  uint hashcode = b->blockno % NBUCKET; // 计算块号的哈希值，确定桶的位置
  acquire(&bcache.hashlock[hashcode]); // 获取该桶的自旋锁
  b->refcnt--; // 减少引用计数
  if (b->refcnt == 0) { // 如果引用计数为0
    // 没有人在等待它
    b->next->prev = b->prev; // 将当前缓冲区从链表中移除
    b->prev->next = b->next; // 将当前缓冲区从链表中移除
    b->next = bcache.head[hashcode].next; // 将当前缓冲区插入到桶的头部
    b->prev = &bcache.head[hashcode]; // 将当前缓冲区插入到桶的头部
    // 更新头部的下一个缓冲区的前一个指针
    bcache.head[hashcode].next->prev = b; 
    bcache.head[hashcode].next = b; // 更新头部的下一个指针
  }

  release(&bcache.hashlock[hashcode]); // 释放桶的自旋锁
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


