// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist; // 其实就是 pagesize大小的链表
} kmems[NCPU];  // lab8 locks. 1. 修改成数组，让每个cpu都有freelist

void
kinit()
{
  // 2. 调整 kinit() ，以前是一个 lock ，现在是 NCPU 个 lock ，当然要全部初始化，
  for(int i=0; i<NCPU; i++)
    initlock(&kmems[i].lock, "kmem");
  freerange(end, (void*)PHYSTOP); //让freerange将所有可用内存分配给运行freerange的CPU。
}


void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  // 3. 修改 kfree
  /** 获取 cpuid 时要关中断，完事之后再把中断开下来 */
  push_off();
  int id = cpuid();
  pop_off();

  /** 将空闲的 page 归还给第 id 个 CPU */
  acquire(&kmems[id].lock);
  r->next = kmems[id].freelist;
  kmems[id].freelist = r;
  release(&kmems[id].lock);
}


// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  // 4. 修改kalloc

  /** 根据提示：
   * 函数cpuid返回当前的核心编号，但只有在中断关闭时调用它并使用其结果才是安全的。
   * 您应该使用push_off()和pop_off()来关闭和打开中断。*/
  push_off();
  int id = cpuid();
  pop_off();

  /** 尝试获取剩余的空闲 page */
  acquire(&kmems[id].lock);
  r = kmems[id].freelist;
  // 调用kalloc就说明《需要新内存》，当前freelist不用判断。
  if(r) { 
    // 此时如果r不是freelist（链表）最后一个节点，把下一个空的page给当前cpu
    kmems[id].freelist = r->next; 
  } else {
    // 此时如果r是freelist（链表）最后一个节点，说明需要偷其他cpu的
    for(int i=0; i<NCPU; i++) {
      if(i == id)
        continue; // 自己不能偷自己

      /** 尝试偷一个其他 CPU 的空闲 page */
      acquire(&kmems[i].lock);
      if(!kmems[i].freelist) {
        release(&kmems[i].lock);
        continue; // 没有空闲 page，偷下一个
      }
      
      // 存在空闲的page
      r = kmems[i].freelist;
      kmems[i].freelist = r->next;
      release(&kmems[i].lock);
      break;
    }
  }
  release(&kmems[id].lock);

  /** 有一种可能：第id个CPU没有空闲 page ，也没偷到 */
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
