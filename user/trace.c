#include "kernel/param.h" 
#include "kernel/types.h" 
#include "kernel/stat.h"  
#include "user/user.h"      

int
main(int argc, char *argv[])  // 主函数，接收命令行参数
{
  int i;                     // 循环计数器
  char *nargv[MAXARG];       // 定义一个新的参数数组，用于exec系统调用

  // 检查命令行参数数量是否正确，以及第一个参数是否为数字
  if (argc < 3 || (argv[1][0] < '0' || argv[1][0] > '9')) {
    fprintf(2, "Usage: %s mask command\n", argv[0]);  // 如果不正确，打印用法信息到标准错误
    exit(1);                                   // 退出程序并返回错误码1
  }

  // 调用trace系统调用，传入第一个参数作为掩码
  if (trace(atoi(argv[1])) < 0) {  // atoi将字符串转换为整数
    fprintf(2, "%s: trace failed\n", argv[0]);  // 如果trace失败，打印错误信息
    exit(1);                             // 退出程序并返回错误码1
  }
  
  // 构建新的参数数组，用于exec系统调用
  for (i = 2; i < argc && i < MAXARG; i++) {
    nargv[i - 2] = argv[i];  // 将命令行参数复制到新数组
  }
  nargv[i - 1] = 0;          // 将exec函数需要的最后一个空指针设置好

  // 使用exec系统调用执行用户指定的命令
  exec(nargv[0], nargv);     // 第一个参数是要执行的命令的路径，第二个参数是参数数组

  exit(0);                    // 如果exec成功，不会执行到这里，这里只是防止未定义行为
}