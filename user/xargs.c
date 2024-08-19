#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

/*
https://www.ruanyifeng.com/blog/2019/08/xargs-tutorial.html
例子1：
$ echo "1\n2" | xargs -n 1 echo line
line 1
line 2

管道命令（|）。管道命令的作用：
是将左侧命令（echo "1\n2" ）的标准输出转换为标准输入，
提供给右侧命令（echo line）作为参数。

-n参数指定每次将多少项，作为命令行参数。
（xargs -n 1）命令指定将每一项（-n 1）标准输入作为命令行参数

xargs命令的作用，是将标准输入转为命令行参数。
*/
int main(int argc, char *argv[])
{
    if (argc == 1)
        exit(0);
    else if(argc - 1 >= MAXARG)
        {
            // fprintf 输出到标准错误 2 中
            fprintf(2, "xargs: too many arguments\n");
            exit(1);
        }

    // 从标准输入读取数据；（例子1：1/n2）
    //由于 read 函数是低级 I/O 函数，它不会对输入数据进行特殊处理，包括换行符。
    //read 函数会将输入数据原样读取到缓冲区stdIn。
    char stdIn[512];
    int size = read(0, stdIn, sizeof stdIn);

    // 从标准输入读取的数据 分行存放
    int i= 0,j=0;
    int line = 0;
    for (int k = 0; k < size; k++) // 统计行数
    {
        if(stdIn[k] == '\n'){
            ++line;
        }
    }
    // 创建用来存储的字符串二维数组
    char output[line][MAXARG]; // 列数为最大参数
    for (int k = 0; k < size; k++){
        output[i][j++] = stdIn[k];
        if (stdIn[k] == '\n'){
            output[i][j - 1] = 0; // 用0覆盖掉换行符。C语言没有字符串类型，char类型的数组中，'0'表示字符串的结束
            ++i; // 行数增加，下行保存
            j=0; // 列数清零
        }
    }

    // 定义一个数组用于存储命令行参数
    char *arguments[MAXARG]; 

    // 将原始命令行参数复制到arguments数组
    for (j = 0; j < argc - 1; ++j) {
        arguments[j] = argv[j + 1]; // 从argv[1]开始复制参数
    }
    i=0;
    while (i < line) {
        arguments[j] = output[i++]; //将每一行数据作为一个新的参数（第二行会覆盖第一行）
        if (fork() == 0) {
            exec(argv[1], arguments);
            exit(0);
        }
        wait(0);
    }
    exit(0);
}
    
