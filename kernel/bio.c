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


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

// struct {
//   struct spinlock lock;
//   struct buf buf[NBUF];

//   // Linked list of all buffers, through prev/next.
//   // Sorted by how recently the buffer was used.
//   // head.next is most recent, head.prev is least.
//   struct buf head;
// } bcache;

// lab8.2 
// 1 定义bcache数组
struct {
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf buckets[NBUCKET];
  struct spinlock lks[NBUCKET];
} bcache;

// 3 使用自己定义的hash
static int
myhash(int x){
  return x%NBUCKET; // 取余求哈希值
}

// 2 修改init函数
void
binit(void)
{
  struct buf *b;

  for(int i=0; i<NBUCKET; i++) 
    initlock(&bcache.lks[i], "bcache");

  // Create linked list of buffers
  for(int i=0; i<NBUCKET; i++) {
    // 要保持每个哈希桶是双链表的特性，所以将每个哈希桶的 prev 和 next 都指向了自己
    bcache.buckets[i].prev = &bcache.buckets[i];
    bcache.buckets[i].next = &bcache.buckets[i];
  }

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    // 将所有空闲 buf 都挂载到第 0 号哈希桶上，方便后续接济其他哈希桶，
    b->next = bcache.buckets[0].next;
    b->prev = &bcache.buckets[0];
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[0].next->prev = b;
    bcache.buckets[0].next = b;
  }
}

// 辅助函数
static void
bufinit(struct buf* b, uint dev, uint blockno)
{
  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int id = myhash(blockno);
  
  acquire(&bcache.lks[id]);
  // Is the block already cached?
  for(b = bcache.buckets[id].next; b != &bcache.buckets[id]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lks[id]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  /**
   * 检查没有发现cached的 buf，那么就根据 ticks LRU 策略
   * 选择第 id 号哈希桶中 ticks 最小的淘汰，ticks 最小意味着
   * 该 buf 在众多未被使用到（ b->refcnt==0 ）的 buf 中是距今最远的
   * */

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  struct buf *victm = 0;
  uint minticks = ticks;
  for(b = bcache.buckets[id].next; b != &bcache.buckets[id]; b = b->next){
    if(b->refcnt==0 && b->lastuse<=minticks) {
      minticks = b->lastuse;
      victm = b;
    }
  }

  if(!victm) 
    goto steal;

  /** 
   * 直接覆盖待淘汰的buf，无需再将其中的旧内容写回至disk中 
   * 标记位valid置0，为了保证能够读取到最新的数据 
   * */
  bufinit(victm, dev, blockno);

  release(&bcache.lks[id]);
  acquiresleep(&victm->lock);
  return victm;

steal:
  /** 到别的哈希桶挖 buf */
  for(int i=0; i<NBUCKET; i++) {
    if(i == id)
      continue;

    acquire(&bcache.lks[i]);
    minticks = ticks;
    for(b = bcache.buckets[i].next; b != &bcache.buckets[i]; b = b->next){
      if(b->refcnt==0 && b->lastuse<=minticks) {
        minticks = b->lastuse;
        victm = b;
      }
    }

    if(!victm) {
      release(&bcache.lks[i]);
      continue;
    }

    bufinit(victm, dev, blockno);
    
    /** 将 victm 从第 i 号哈希桶中取出来 */
    victm->next->prev = victm->prev;
    victm->prev->next = victm->next;
    release(&bcache.lks[i]);

    /** 将 victm 接入第 id 号中 */
    victm->next = bcache.buckets[id].next;
    bcache.buckets[id].next->prev = victm;
    bcache.buckets[id].next = victm;
    victm->prev = &bcache.buckets[id];

    release(&bcache.lks[id]);
    acquiresleep(&victm->lock);
    return victm;
  }

  release(&bcache.lks[id]);
  panic("bget: no buf");
}


// bread() 和 bwrite() 无需我们修改：
// bread() 是提供给上层的接口，
// 当 kernel 要从 Buffer cache 中读取数据时，就会调用 bread() 。
// bread() 又会去调用 bget() ，尝试在 Buffer cache 中定位到所需的数据。
// 若 bget() 顺利定位，那么 bread() 直接将结果返回给 kernel ；
// 反之，bread() 就需要去 disk 中读取新鲜的数据，再将其返回。

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  /** 如果buf中的数据过时了，那么需要重新读取 */
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

// Release a locked buffer.
// Move to the head of the most-recently-used list.

// 3. 修改 brelse()
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int id = myhash(b->blockno);
  acquire(&bcache.lks[id]);
  b->refcnt--;
  if(b->refcnt == 0)
    b->lastuse = ticks; // 根据提示：记录最近被使用的时间戳（改为标记 kernel/trap.c中的ticks）
  release(&bcache.lks[id]);
}

void
bpin(struct buf *b) 
{
  int id = myhash(b->blockno);

  acquire(&bcache.lks[id]);
  b->refcnt++;
  release(&bcache.lks[id]);
}

void
bunpin(struct buf *b) 
{
  int id = myhash(b->blockno);

  acquire(&bcache.lks[id]);
  b->refcnt--;
  release(&bcache.lks[id]);
}

