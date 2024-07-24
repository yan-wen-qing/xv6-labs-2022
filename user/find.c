#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

// 递归函数，用于在目录及其子目录中查找具有特定名称的文件
void find(char *path, char *target) {
    char buf[512], *p;
    int fd;
    struct dirent de; // 目录条目结构
    struct stat st; // 文件状态结构

    // 尝试打开由 'path' 指定的目录
    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    // 获取目录状态
    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    // 检查是否是目录
    if (st.type != T_DIR) {
        fprintf(2, "find: %s is not a directory\n", path);
        close(fd);
        return;
    }

    // 将路径复制到缓冲区，并准备附加文件名
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';

    // 一次读取一个目录条目
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        // 跳过 inum 为 0 的条目（无效条目）
        if (de.inum == 0)
            continue;

        // 跳过 "." 和 ".." 条目
        if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
            continue;

        // 将目录条目名称复制到缓冲区
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0; // 添加空字符终止符

        // 获取目录条目的状态
        if (stat(buf, &st) < 0) {
            fprintf(2, "find: cannot stat %s\n", buf);
            continue;
        }

        // 如果名称与目标名称匹配，打印完整路径
        if (strcmp(de.name, target) == 0) {
            printf("%s\n", buf);
        }

        // 如果是目录，递归查找
        if (st.type == T_DIR) {
            find(buf, target);
        }
    }

    // 关闭目录
    close(fd);
}

int main(int argc, char *argv[]) {
    // 检查提供的参数数量是否正确
    if (argc < 3) {
        fprintf(2, "Usage: find <path> <filename>\n");
        exit(1);
    }
    // 使用提供的路径和目标文件名调用 find 函数
    find(argv[1], argv[2]);
    exit(0);
}

