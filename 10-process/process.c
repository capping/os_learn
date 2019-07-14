/* -----------------------
    # 进程
    ① 写代码:用系统调用创建进程
    ② 进行编译:程序的二进制格式
    ③ 运行程序为进程
    ④ 进程树

    ## 写代码:用系统调用创建进程
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

extern int create_process(char* program, char** arg_list);

int create_process(char* program, char** arg_list)
{
    pid_t child_pid;
    child_pid = fork();
    if (child_pid != 0) {
        return child_pid;
    } else {
        execvp(program, arg_list);
        abort();
    }
}

/* --------------------
    ## ② 进行编译:程序的二进制格式
        在Linux下面，二进制的程序也要有严格的格式，这个格式我们称为ELF(Executeable and Linkable Format，可执行与可链接格式)
        1) 可重定位文件
        2) 可执行文件
        
 */