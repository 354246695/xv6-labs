#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    /* code */
    // 两个管道
    int p0[2];
    int p1[2];
    
    // 定义用于 读写的 字符数组，大小是1
    char buf[1] = "0";

    // 创建管道
    pipe(p0);
    pipe(p1); 
    
    if(fork()==0){//子进程
        // 关闭p0 写，p1 读
        // 以下close需要: 1.防止资源泄漏 2.避免文件描述符达上限
        close(p0[1]);
        close(p1[0]);

        printf("%d: received ping\n", getpid());

        write(p1[1],buf, 1); //向管道1 写入 字节

        // 关闭p0，p1余下功能
        // 以下close需要: 1.防止资源泄漏 2.避免文件描述符达上限
        close(p0[0]);
        close(p1[1]);

    }else {//父进程，向子进程发送 字节
        // 关闭 不用的读写
        // 以下close需要: 1.防止资源泄漏 2.避免文件描述符达上限
        close(p0[0]);
        close(p1[1]);

        write(p0[1],buf, 1); //向管道0 写入 字节
        
        wait(0); //等待子进程结束

        //读取子进程传出的数据：从管道1 读取 字节
        read(p1[0],buf, 1); 

        printf("%d: received pong\n", getpid());

        // 关闭 不用的余下的读写
        // 以下close需要: 1.防止资源泄漏 2.避免文件描述符达上限
        close(p0[1]);
        close(p1[0]);

        }

    exit(0);
}