/*
    # 线程：如何让复杂的项目并行执行
    ① 为什么要有线程
    ② 如何创建线程
    ③ 线程的数据
    ④ 数据的保护
    总结时刻

    ---------------------------------------------------------------------------------------
    | ## ① 为什么要有线程
    |
    | 进程默认有一个主线程，线程是负责执行二进制指令的。
    | 进程除了执行指令之外，内存、文件系统等等都要它管着（管的更宽）
    | 所以，进程相当于一个项目，而线程是为了完成项目需求，而建立的一个个任务。
    |
    | (两种工作模式) 1.一个人从头做到尾(主线程); 2.将无关联的任务拆分,并行执行(多线程) 快！快！！快！！！
    | 
    | 使用多进程并行执行的问题:
    | 1. 创建进程占用资源太多
    | 2. 进程之间的通信需要数据在不同的内存空间传来传去，无法共享。
    |
    | 按照任务性质，可以将不同的任务放到不同的线程，保证(互！不！！耽！！！误 ！！！！)
    ---------------------------------------------------------------------------------------

    ---------------------------------------------------------------------------------------
    | ## ② 如何创建线程
    | 
    | 例子：将N个视频下载，拆分成N个任务
    |
    | 当一个线程退出的时候，就会发信号给其他所有同进程的线程。有一个线程使用pthread_join获取这个线程退出的
    | 返回值。线程的返回值通过pthread_join传给主线程，这样子线程就将自己下载文件所消耗的时间，告诉给主线程。
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NUM_OF_TASKS 5

void *downloadfile(void *filename)          // void类型的指针,用于接收任何类型的参数
{
    printf("I am downloading the file %s\n", (char *)filename);
    sleep(10);
    long downloadtime = rand() % 100;
    printf("I finish downloading the file within %ld minutes!\n", downloadtime);
    pthread_exit((void *)downloadtime);     // 下载结束，pthread_exit()退出线程, (void *)是线程退出的返回值
}

int main(int argc, char *argv[])
{
    char files[NUM_OF_TASKS][20] = {"file1.avi", "fiel2.rmvb", "file3.mp4", "file4.wmv", "file5.flv"};
    pthread_t threads[NUM_OF_TASKS];        // 线程对象
    int rc;
    int t;
    int downloadtime;
    pthread_attr_t thread_attr;             // 线程属性
    pthread_attr_init(&thread_attr);        // 初始化线程属性
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE); // 属性 PTHREAD_CREATE_JOINABLE,表示将来主线程等待这个线程的结束，并获取退出时的状态

    for (t=0; t<NUM_OF_TASKS; t++) {
        printf("creating thread %d, please help me to download %s\n", t, files[t]);
        rc = pthread_create(&threads[t], &thread_attr, downloadfile, (void *)files[t]); // pthread_create()创建线程,四个参数(线程对象, 线程属性, 线程运行函数, 线程运行函数的参数)。 返回值是0表示成功,其它都是失败
        if (rc) {
            printf("ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
    }

    pthread_attr_destroy(&thread_attr);

    for (t=0; t<NUM_OF_TASKS; t++) {
        pthread_join(threads[t], (void**)&downloadtime);
        printf("Thread %d downloads the file %s in %d minutes.\n", t, files[t], downloadtime);
    }

    pthread_exit(NULL);
}

/*
    ---------------------------------------------------------------------------------------
    | ③ 线程的数据
    | 1. 线程栈上的本地数据
    | 函数执行过程中的局部变量，函数的调用会使用栈的模型，这在线程里面是一样的。只不过每个线程都有自己的栈空间。
    | 栈的大小可以通过命令ulimit -a查看，默认情况下线程栈大小为8192(8MB),可以通过ulimit -s修改。
    | 对于线程栈，可以通过下面这个函数pthread_attr_t，修改线程栈的大小
    |
    | > int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
    |
    | 主线程在内存中有一个栈空间，其它线程也拥有独立的栈空间。为了避免线程之间的栈空间踩踏，线程栈之间还会有小块区域，
    | 用来隔离保护各自的栈空间。一旦另一个线程踏入到这个隔离区，就会引发段错误。
    |
    | 2. 整个进程里共享的全局数据
    | 例如全局变量，虽然在不同的进程中是隔离的，但是在一个进程中是共享的。如果同一个全局变量，两个线程一起修改，那肯定
    | 会有问题，有可能把数据改的面目全非。这就需要有一种机制来保护它们
    |
    | 3. 线程私有数据
    | 可以通过一下函数创建：
    | > int pthread_key_create(pthread_key_t *key, void (*destructor)(void*))
    | 可以看到，创建一个key, 所有线程都可以访问它，但各线程可根据自己的需要往key中填入不同的值，这就相当于提供了一个同名
    | 而不同值的全局变量。
    | 我们可以通过下面的函数设置key对应的value。
    | > int pthread_setspecific(pthread_key_t key, const void *value)
    | 我们还可以通过下面的函数获取key对应的value
    | > void *pthread_getspecific(pthread_key_t key)
    | 而等到线程退出的时候，就会调用析构函数释放value。
    ---------------------------------------------------------------------------------------

    ---------------------------------------------------------------------------------------
    | ④ 数据的保护
    | 共享数据的保护问题：
    | 1. 方式一：互斥(Mutex, Mutual Exclusion)
    | 例子: ./transfer.c
    | 2. 方式二：条件变量
    | 条件变量和互斥锁是配合使用的
    | 例子: ./coder.c
 */