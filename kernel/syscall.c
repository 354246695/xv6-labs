#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"
#include "fcntl.h"
// #include "spinlock.h"
#include "proc.h"
// #include "sleeplock.h"
#include "file.h"

// Fetch the uint64 at addr from the current process.
int
fetchaddr(uint64 addr, uint64 *ip)
{
  struct proc *p = myproc();
  if(addr >= p->sz || addr+sizeof(uint64) > p->sz)
    return -1;
  if(copyin(p->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
    return -1;
  return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Returns length of string, not including nul, or -1 for error.
int
fetchstr(uint64 addr, char *buf, int max)
{
  struct proc *p = myproc();
  int err = copyinstr(p->pagetable, buf, addr, max);
  if(err < 0)
    return err;
  return strlen(buf);
}

static uint64
argraw(int n)
{
  struct proc *p = myproc();
  switch (n) {
  case 0:
    return p->trapframe->a0;
  case 1:
    return p->trapframe->a1;
  case 2:
    return p->trapframe->a2;
  case 3:
    return p->trapframe->a3;
  case 4:
    return p->trapframe->a4;
  case 5:
    return p->trapframe->a5;
  }
  panic("argraw");
  return -1;
}

// Fetch the nth 32-bit system call argument.
int
argint(int n, int *ip)
{
  *ip = argraw(n);
  return 0;
}

// Retrieve an argument as a pointer.
// Doesn't check for legality, since
// copyin/copyout will do that.
int
argaddr(int n, uint64 *ip)
{
  *ip = argraw(n);
  return 0;
}

// Fetch the nth word-sized system call argument as a null-terminated string.
// Copies into buf, at most max.
// Returns string length if OK (including nul), -1 if error.
int
argstr(int n, char *buf, int max)
{
  uint64 addr;
  if(argaddr(n, &addr) < 0)
    return -1;
  return fetchstr(addr, buf, max);
}

extern uint64 sys_chdir(void);
extern uint64 sys_close(void);
extern uint64 sys_dup(void);
extern uint64 sys_exec(void);
extern uint64 sys_exit(void);
extern uint64 sys_fork(void);
extern uint64 sys_fstat(void);
extern uint64 sys_getpid(void);
extern uint64 sys_kill(void);
extern uint64 sys_link(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_mknod(void);
extern uint64 sys_open(void);
extern uint64 sys_pipe(void);
extern uint64 sys_read(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_sleep(void);
extern uint64 sys_unlink(void);
extern uint64 sys_wait(void);
extern uint64 sys_write(void);
extern uint64 sys_uptime(void);
extern uint64 sys_mmap(void);
extern uint64 sys_munmap(void);


static uint64 (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
// lab 10
[SYS_mmap]    sys_mmap,
[SYS_munmap]  sys_munmap,
};

void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    p->trapframe->a0 = syscalls[num]();
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}

// lab 10
// ���� ������ʵ��
// 1. mmap����ʵ�֣��ҵ����õ�ַ����Ԥ������
uint64
sys_mmap(void)
{
  uint64 addr;
  int len, prot, flags, fd, offset;
  struct file* file;
  struct vma* vma = 0;

  // ��ȡϵͳ���ò���
  // ͨ���Ĵ���������caller����
  if(argaddr(0, &addr)<0 
    || argint(1, &len)<0
    || argint(2, &prot)<0 
    || argint(3, &flags)<0
    || argfd(4, &fd, &file)<0 
    || argint(5, &offset)<0)
    return -1;

  // TODO: ��ʦֻҪ������ addr == 0 �������������һ��
  if (addr != 0) return -1;
  // TODO: ��ʦֻҪ������ offset == 0
  if (offset != 0) return -1;

  /** ����Ȩ�޳�ͻ 
   * ���� �ж� �ļ��ɷ����� �Ƿ� �� caller��Ҫ���Ȩ�޳�ͻ */
  // �ļ�����Ϊ������д��ʱ��prot Ӧ��ͬʱ����Ϊ��д�� flags Ӧ�ò�Ϊ ��MAP_SHARED��ӳ���ڴ���޸�Ӧд���ļ�����
  if(file->writable==0 && (prot & PROT_WRITE) && flags==MAP_SHARED)
    return -1;

  struct proc* p = myproc();
  len = PGROUNDUP(len);

  if(p->sz+len > MAXVA) // ������������ռ��������
    return -1;

  if(offset<0 || offset%PGSIZE) 
    return -1;

  // ѭ���ڽ��̵�����ռ䣬Ѱ��һ�����е� vma
  for(int i=0; i<NVMA; i++) {
    if(p->vmas[i].addr)
      continue;

    vma = &p->vmas[i]; // �ҵ����࣬����
    break;
  }

  if(!vma) /** �� vm ��û�ҵ����Ա�����ӳ��Ŀ������� */
    return -1;

  if(addr == 0) 
    vma->addr = p->sz;
  else  /** Caller ָ��ӳ�����ʼ��ַ */
    vma->addr = addr;
  
  // ���� Ԫ����
  vma->len = len;
  vma->prot = prot;
  vma->flags = flags;
  vma->fd = fd;
  vma->offset = offset;
  vma->file = file;

  // ����file ���ô���
  filedup(file);

  p->sz += len;
  return vma->addr;
}

// lab 10
// 3. ӳ����ͷŻ�������
uint64
sys_munmap(void)
{
  uint64 addr;
  int len;
  struct vma* vma = 0;
  struct proc* p = myproc();

  if(argaddr(0, &addr)<0 || argint(1, &len)<0)
    return -1;

  addr = PGROUNDDOWN(addr);
  len = PGROUNDUP(len);

  for(int i=0; i<NVMA; i++) {
    if(p->vmas[i].addr && addr>=p->vmas[i].addr 
      && addr+len<=p->vmas[i].addr+p->vmas[i].len) {
      vma = &p->vmas[i];
      break;
    }
  }

  if(!vma)
    return -1;

  if(addr != vma->addr)
    return -1;

  /** ����ͷ� file ӳ���� vm �е� pages */
  vma->addr += len;
  vma->len -= len;

  // Caller �� flags �� MAP_SHARED �Ļ�������Ҫ�� in-memory �еĸ������ݻ�д�� disk ��
  // ���� MAP_SHARED �����������ļ��ǹ���ģ��Ǳ�ȻҪ��֤�ļ���������������µģ�
  // ����ļ��������޸Ĺ�����ôһ��Ҫ֪ͨ���еĸ���������
  if(vma->flags & MAP_SHARED)
    filewrite(vma->file, addr, len);
  uvmunmap(p->pagetable, addr, len/PGSIZE, 1);

  return 0;  
}
