#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

const int INT_LEN = sizeof(int);

void get_prime(int*);

int main(int argc, char *argv[])
{
    // 创建一个管道，传输原始数据
    int p0[2];
    pipe(p0);

    for (int i = 2; i < 35; i++)
    {
        write(p0[1], &i, INT_LEN); // 写入原始数据
    }

    if (fork() == 0)
    {                  // 创建子进程
        get_prime(p0); // 调用素数获取函数
        wait(0);
    }
    else{
        close(p0[0]);
        close(p0[1]);
    }
    exit(0);
}

void get_prime(int *p0) // 传入 指针
{
    int p1[2]; // 创建管道1，用于传输素数
    pipe(p1);

    close(p0[1]); // 写段不需要，关闭

    // 读出数据，处理第一个数
    int tmp;
    int first;
    if (read(p0[0], &tmp, INT_LEN) > 0)
    {
        // 输出管道第一个数，他是素数
        printf("prime %d\n", tmp);

        // 记录管道第一个数，用作筛选后续数据
        first = tmp;

        // 筛选
        while (read(p0[0], &tmp, INT_LEN) > 0)
        {
            // 能被除尽的不要
            if (tmp % first != 0)
            {
                write(p1[1], &tmp, INT_LEN);
            }
        }

        if(fork() == 0)
        {
            // 递归
            get_prime(p1);
            exit(0);
        }
        else exit(0);//错误直接结束
    }
    else{
        close(p1[0]);
        close(p1[1]);
        exit(0);
    }
}
