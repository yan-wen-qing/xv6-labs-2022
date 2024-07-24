#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// 实现质数筛选逻辑的函数
void prime_sieve(int p[2]) {
    int prime;
    int n;
    close(p[1]); // 关闭当前管道的写端
    
    // 从管道中读取第一个数，这个数就是质数
    if(read(p[0], &prime, sizeof(prime)) == 0) {
        // 如果没有读取到数，关闭读端并退出
        // 递归结束
        close(p[0]);
        exit(0);
    }
    printf("prime %d\n", prime);
   
    // 创建一个新的管道
    int next_pipe[2];
    pipe(next_pipe);

    // 创建一个新的进程
    if(fork() == 0) {
        // 子进程
        close(p[0]);
        prime_sieve(next_pipe); // 递归调用
    } 
    else {
        // 父进程
        close(next_pipe[0]);
        
        // 从当前管道中读取数字
        // 过滤掉质数的倍数
        // 将剩下的数字写入下一个管道
        while(read(p[0], &n, sizeof(n)) > 0) {
            if(n % prime != 0) {
                write(next_pipe[1], &n, sizeof(n));
            }
        }
        
        close(p[0]);
        close(next_pipe[1]);
        wait(0); // 等待子进程完成
    }
}

int main(int argc, char *argv[]) {
    // 创建初始管道
    int p[2];
    pipe(p);
    
    // 创建第一个筛选进程
    if(fork() == 0) {
        // 子进程：使用初始管道开始筛选
        prime_sieve(p);
    } else {
        // 父进程
        close(p[0]); // 关闭初始管道的读端
        
        // 向管道中写入数据
        for(int i = 2; i <= 35; i++) {
            write(p[1], &i, sizeof(i));
        }

        close(p[1]); // 关闭初始管道的写端
        wait(0); // 等待筛选进程完成
        exit(0);
    }
    return 0;
}

