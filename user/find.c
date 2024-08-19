#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

// 用于分割出带有/路径的文件名
// input:a/b/filename
// output:filename
char *
my_fmtname(char *path)
{
  char *p;
  // 从path最后一个内存地址开始（path+strlen(path)）
  // 字符串本质上是字符数组，而数组名 path 可以作为指向数组首元素的指针使用。
  // 因此，path + strlen(path) 这种运算实际上是在进行指针算术。
  // strlen(path) 返回的是字符串中字符的数量，以字符为单位的。
  // C语言中，字符通常占用 1 个字节，
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  return p;
}

void find(char *path, char *target_filename)
{
  char buf[512];    /* 用于存储路径或文件名的临时缓冲区 */
  char *p;          /* 指向缓冲区中的字符串的指针，用于处理路径或文件名 */
  int fd;           // 文件描述符，用于后续的文件或目录操作。
  struct dirent de; // 一个 dirent 结构体，用于读取目录中的条目。
  struct stat st;   // 一个 stat 结构体，用于存储文件或目录的状态信息。

  // 尝试打开目录
  if ((fd = open(path, 0)) < 0)
  {                                             // 0 表示只读方式打开
    fprintf(2, "find: cannot open %s\n", path); // 向标准错误输出find命令无法打开指定路径的信息
    return;
  }

  // 尝试获取状态信息，
  if (fstat(fd, &st) < 0)
  { // 参数：文件描述符；struct stat变量指针，记录得到的文件属性信息
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch (st.type)
  {
  //
  case T_FILE:
    // 当文件类型为普通文件时，比较文件名是否与目标文件名匹配
    // C语言中不能像python一样使用“==”对字符串进行比较，而应当使用strcmp()
    if (strcmp(my_fmtname(path), target_filename) == 0)
    {
      // 文件名匹配则打印文件路径
      // 表示文件名一样
      printf("%s\n", path);
    }
    break;

  case T_DIR:
    // 当文件类型为目录时，检查路径长度是否超过缓冲区大小
    if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf)
    {
      // 路径过长则打印错误信息并退出当前循环
      printf("find: path too long\n");
      break;
    }

    // 复制路径到缓冲区，并为目录名预留空间
    strcpy(buf, path);
    // 在p最后加上'/'
    p = buf + strlen(buf);
    *p++ = '/';

    // 遍历目录中的每个条目
    // 循环读取目录中的每个条目，并检查其索引节点号
    while (read(fd, &de, sizeof(de)) == sizeof(de))
    {
      // 忽略空目录项和当前目录、父目录符号链接
      if (de.inum == 0)
        continue;

      // 不在“.”和“..”目录中递归
      if (!strcmp(de.name, ".") || !strcmp(de.name, ".."))
        continue;

      // 将目录项名称复制到缓冲区末尾，并确保字符串以空字符结尾
      memmove(p, de.name, DIRSIZ);//将 de.name（目录项的名称）中的 DIRSIZ 个字节复制到指针 p 指向的位置
      p[DIRSIZ] = 0;//复制的字符串之后立即放置一个空字符（'\0'）

      // 获取当前目录项的详细信息
      if (stat(buf, &st) < 0)
      {
        // 获取信息失败则打印错误信息并继续下一个目录项
        printf("find: cannot stat %s\n", buf);
        continue;
      }

      // 递归调用find函数，搜索当前目录项中的文件或子目录
      // 递归！！！
      find(buf, target_filename);
    }
    break;
  }
  // 关闭目录文件描述符
  close(fd);
}

void
main(int argc, char *argv[])
{
  
    if(argc != 3){
        printf("usage: %s <dir> <filename>",argv[0]);
        exit(0);
    }
    find(argv[1], argv[2]);
    exit(0);
}
