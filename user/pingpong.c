#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char* argv[])
{
    int p2c[2], c2p[2]; // 两个管道：p2c 用于父进程到子进程，c2p 用于子进程到父进程

    // 创建管道
    if (pipe(p2c) == -1 || pipe(c2p) == -1) {
        fprintf(2, "Error: pipe() error.\n");
        exit(1);
    }
    
    // 创建子进程
    int pid = fork();
    if (pid == -1) {
        fprintf(2, "Error: fork() error.\n");
        exit(1);
    }
    
    // 子进程
    if (pid == 0) {
        char buffer[1];
        close(p2c[1]); // 关闭 p2c 的写端
        close(c2p[0]); // 关闭 c2p 的读端

        // 从父进程读取数据
        if (read(p2c[0], buffer, 1) == -1) {
            fprintf(2, "Error: read() error in child.\n");
            exit(1);
        }
        
        close(p2c[0]); // 关闭 p2c 的读端
        printf("%d: received ping\n", getpid()); // 打印接收到的信息
        
        // 写数据回父进程
        if (write(c2p[1], buffer, 1) == -1) {
            fprintf(2, "Error: write() error in child.\n");
            exit(1);
        }
        close(c2p[1]); // 关闭 c2p 的写端

        exit(0);
    } 
    // 父进程
    else {
        char buffer[1];
        buffer[0] = 'a'; // 要发送的数据

        close(p2c[0]); // 关闭 p2c 的读端
        close(c2p[1]); // 关闭 c2p 的写端

        // 向子进程写数据
        if (write(p2c[1], buffer, 1) == -1) {
            fprintf(2, "Error: write() error in parent.\n");
            exit(1);
        }
        close(p2c[1]); // 关闭 p2c 的写端
        wait(0); // 等待子进程完成

        // 从子进程读取数据
        if (read(c2p[0], buffer, 1) == -1) {
            fprintf(2, "Error: read() error in parent.\n");
            exit(1);
        }
        close(c2p[0]); // 关闭 c2p 的读端

        // 打印接收到的信息
        printf("%d: received pong\n", getpid());

        exit(0);
    }
}

