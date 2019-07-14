/*
    # 进程数据结构(上)
    在Linux里面，无论是进程，还是线程，到了内核里面，我们统一都叫任务(Task)，由一个统一的结构task_struct进行管理。
    任务管理，使用链表将所有的task_struct串起来。
    > struct list_head tasks;
    接下来，我们来看每个任务都应该包含哪些字段。
    ① 任务ID
    ② 信号处理
    ③ 任务状态
    ④ 进程调度
    总结时刻

    ------------------------------------------------------------
    | ① 任务ID
    | ```
    | pid_t pid;                // process id
    | pid_t tgid;               // thread group id
    | struct task_struct *group_leader
    | ```
    | 疑问：既然是ID，有一个就足以做唯一标识了，这个怎么看起来这么麻烦？
    | 这是因为，上面的进程和线程到了内核这里，统一变成了任务，这就带来两个问题
    | 1. 任务展示
    | 2. 给任务下命令
    |
    | 任何一个进程，如果只有主线程，那pid是自己，tgid是自己，group_leader指向的还是自己。
    | 如果一个进程创建了其他线程，线程有自己的pid, tgid就是进程的主线程的pid, group_leader指向的就是进程的主线程。
    | 有了tgid，我们就知道task_struct代表的是一个进程还是代表一个线程了。
    -----------------------------------------------------------------

    -----------------------------------------------------------------
    | ② 信号处理
    | ```
    | struct signal_struct  *signal;
    | struct sighand_struct *sighand;
    | sigset_t              blocked;
    | sigset_t              real_blocked;
    | sigset_t              saved_sigmask;
    | struct sigpending     pending;
    | unsigned long         sas_ss_sp;
    | size_t                sas_ss_size;
    | unsigned int          sas_ss_flags;
    | ```
    | 
    | 这里定义了哪些信号被阻塞暂不处理(blocked), 哪些信号尚等待处理(pending), 哪些信号正在通过信号处理函数进行处理(sighand)。
    |
    | 信号处理函数默认使用用户态的函数栈，当然也可以开辟新的栈专门用于信号处理, 这就是sas_ss_xxx这三个变量的作用。
    |
    | task_struct 里面有一个struct sigpending pending。如果我们进入struct signal_struct *signal去看的话，还有一个
    | struct sigpending shared_pending。它们一个是本任务的，一个是线程组共享的。
    -----------------------------------------------------------------

    -----------------------------------------------------------------
    | ③ 任务状态
    | 在task_struct里面，涉及任务状态的是下面这几个变量
    | ```
    | volatile long state;      // -1 unrunable, 0 runable, >0 stopped
    | int exit_state;
    | unsigned int flags;
    | ```
    | state(状态)可以取的值定义在include/linux/sched.h头文件中
    | ```
    | // Used in tsk->state:
    | #define TASK_RUNNING          0           // 进程在时刻准备运行的状态，并不是说进程正在运行
    | #define TASK_INTERRUPTIBLE    1           // 可中断的睡眠状态
    | #define TASK_UNINTERRUPIBLE   2           // 不可中断的睡眠状态
    | #define __TASK_STOPPED        4
    | #define __TASK_TRACED         8
    | // Used in tsk->exit_state:
    | #define EXIT_DEAD             16
    | #define EXIT_ZOMBIE           32
    | #define EXIT_TRACE            (EXIT_ZOMBIE | EXIT_DEAD)
    | // Used in tsk->state again:
    | #define TASK_DEAD             64
    | #define TASK_WAKEKILL         128
    | #define TASK_WAKING           256
    | #define TASK_PARKED           512
    | #define TASK_NOLOAD           1024
    | #define TASK_NEW              2048
    | #define TASK_STATE_MAX        4096
    | ```
    | 从定义的数值很容易看出来，flags是通过bitset的方式设置的，也就是说，当前是什么状态，哪一位就置一
    |
    | 两种睡眠状态:
    | 1. 可中断的睡眠状态:
    | 虽然在睡眠，等待I/O完成，但是这个时候一个信号来的时候，进程还是要被唤醒。只不过唤醒后，不是继续刚才的操作，而是进行信号处理。
    | 当然程序员可以根据自己的意愿，来写信号处理函数，例如收到某些信号，就放弃等待这个I/O操作完成，直接退出，也可收到某些信息，继续等待
    |
    | 2. 不可中断的睡眠状态
    | 这是一种深度睡眠状态，不可被唤醒，只能死等I/O操作完成。因此这是一个比较危险的事情，除非程序员极其有把握，不然还是不要成TASK_UNINTERRUPIBLE。
    |
    | 3. 可以终止的新睡眠状态
    | 进程处于这种状态中，它的运行原理类似TASK_UNINTERRUPIBLE，只不过可以响应致命信号。
    |
    | 从定义上可以看出，TASK_WAKEKILL用于在接受到致命信号时唤醒进程，而TASK_KILLABLE相当于这两位都设置啦。
    | > #define TASK_KILLABLE       (TASK_WAKEKILL | TASK_UNINTERRUPTIBLE)
    |
    | TASK_STOPPED是在进程接受到SIGSTOP、SIGTTIN、SIGTSTP或者SIGTTOU信号之后进入该状态。
    | TASK_TRACED表示进程被debugger等进程监视，进程执行被调试程序所停止。当一个进程被另外一个进程所监视，每一个信号都会让进程进入该状态。
    |
    | 一旦一个进程要结束，先进入的是EXIT_ZOMBLE状态，但是这个时候它的父进程还没有使用wait()等系统调用来获知它的终止信息，此时进程就是僵尸进程。
    |
    | EXIT_DEAD是进程的最终状态
    |
    | EXIT_DEAD和EXIT_ZOMBLE也可以用于exit_state。
    |
    | 上面的进程状态和进程的运行、调度有关系，还有其他的一些状态，我们称为标志。放在flags字段中，这些字段都被定义称为宏，以PF开头。例子:
    | ```
    | #define PF_EXITING        0x00000004          // 表示正在退出
    | #define PF_VCPU           0x00000010          // 表示进程运行在虚拟CPU上
    | #define PF_FORKNOEXEC     0x00000040          // 表示fork完了，还没有exec
    | ```
    -----------------------------------------------------------------

    -----------------------------------------------------------------
    | ④ 进程调度
    | 进程的切换往往设计调度，下面这些字段都是用于调度的。
    | ```
    | // 是否在运行队列上
    | int                       on_rq;
    | // 优先级
    | int                       prio;
    | int                       static_prio;
    | int                       normal_prio;
    | unsigned int              rt_priority;
    | // 调度器类
    | const struct sched_class  *sched_class;
    | // 调度实体
    | struct sched_entity       se;
    | struct sched_rt_entity    tr;
    | struct sched_dl_entity    dl;
    | // 调度策略
    | unsigned int              policy;
    | // 可以使用哪些CPU
    | int                       nr_cpus_allowed;
    | cpumask_t                 cpus_allowed;
    | struct sched_info         sched_info;
    | ```
 */