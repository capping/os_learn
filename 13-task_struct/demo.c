/*
    # 进程数据结构(中)
    ① 运行统计信息
    ② 进程亲缘关系
    ③ 进程权限
    ④ 内存管理
    ⑤ 文件与文件系统
    总结时刻

    ------------------------------------------------------------
    | ① 运行统计信息
    | 在进程的运行过程中，会有一些统计量，具体你可以看下面的列表。这里面有进程在用户态和内核态消耗的时间、上下文切换的次数等。
    | ```
    | u64               utime;  // 用户态消耗的CPU时间
    | u64               stime;  // 内核态消耗的CPU时间
    | unsigned long     nvcsw;  // 自愿(voluntary)上下文切换计数
    | unsigned long     nivcsw; // 非自愿(involuntary)上下文切换计数
    | u64               start_time; // 进程启动时间，不包含睡眠时间
    | u64               real_start_time;    // 进程启动时间，包含睡眠时间
    | ```
    ------------------------------------------------------------

    ------------------------------------------------------------
    | ② 进程亲缘关系
    | 任何一个进程都有父进程。所以，整个进程其实就是一棵进程树。而拥有同一父进程的所有进程都具有兄弟关系。
    | ```
    | struct task_struct __rcu      *real_parent;       // real parent process
    | struct task_struct __rcu      *parent;            // recipient of SIGCHLD, wait4() reports
    | struct list_head              children;           // list of my children
    | struct list_head              sibling;            // linkage in my parent's children list
    | ```
    | - parent 指向其父进程。当它终止时，必须向它的父进程发送信号。
    | - children 表示链表的头部。链表中的所有元素都是它的子进程。
    | - sibling 用于把当前进程插入到兄弟链表中。
    |
    | 通常情况下，real_parent和parent是一样的，但是也会有另外的情况存在。例如，bash创建一个进程，那进程的parent和real_parent
    | 就都是bash。如果在bash上使用GDB来debug一个进程，这个时候GDB是real_parent, bash是这个进程的parent。
    ------------------------------------------------------------

    ------------------------------------------------------------
    | ③ 进程权限
    | ```
    | // Objective and real subjective task credentials (COW):
    | const struct cred __rcu       *real_cred;
    | // Effective (overridable) subjective task credentials (COW):
    | const struct cred __rcu       *cred;
    | ```
    | 这个结构的注释里，有两个名词比较拗口，Objective和Subjective。事实上，所谓的权限，就是我能操纵谁，谁能操纵我。
    | 
    | real_cred说明谁能操纵我这个进程。cred说明我这个进程能够操纵谁。
    | ```
    | struct cred {
    |    kuid_t                 uid;    // real UID of the task
    |    kgid_t                 gid;    // reap GID of the task
    |    kuid_t                 suid;   // saved UID of the task
    |    kgid_t                 sgid;   // saved GID of the task
    |    kuid_t                 euid;   // effective UID of the task
    |    kgid_t                 egid;   // effective GID of the task
    |    kuid_t                 fsuid;  // UID for VFS ops
    |    kgid_t                 fsgod;  // GID for VFS ops
    |    kernel_cap_t           cap_inheritable;    // caps our children can inherit
    |    kernel_cap_t           cap_permitted;      // caps we're permitted
    |    kernel_cap_t           cap_effective;      // caps we can actually use
    |    kernel_cap_t           cap_bset;           // capability bounding set
    |    kernel_cap_t           cap_ambient;        // Ambient capability set
    | } __randomize_layout;
    | ```
    | 从这里的定义可以看出，大部分是关于用户和用户所属的用户组信息。
    |
    | 第一个是uid和gid，注释是real user/group id。一般情况下，谁启动的进程，就是谁的ID。但是权限审核的时候，往往不比较这两个，
    | 也就是说不大起作用。
    |
    | 第二个是euid和egid，注释是effective user/group id。一看这个名字，就知道是起"作用"的。当这个进程要操作消息队列、共享内存、
    | 信号量等对象的时候，其实就是在比较这个用户和组是否有权限。
    |
    | 第三个是fsuid和fsgid，也就是filesystem user/group id。这个是对文件操作会审核的权限。
    | 
    | 一般来说，fsuid、euid，和uid是一样的，fsgid、egid，和gid也是一样的。因为谁启动的进程，就应该审核启动的用户到底有没有这个权限。
    | ......
    | 
    | 除了以用户和用户组控制权限，Linux还有另外一个机制就是capabilities。
    |
    | 原来进程控制的权限，要么是高权限的root用户，要么是一般权限的普通用户，这时候的问题是，root用户权限太大，而普通用户权限太小。有时候
    | 一个普通用户想要做一点高权限的事情，必须给他整个root的权限。这个太不安全了。
    |
    | 于是，这个引入新的机制capabilities，用位图表示权限，在capability.h可以找到定义的权限。这里列举几个：
    | ```
    | #define CAP_CHOWN             0;
    | #define CAP_KILL              5;
    | #define CAP_NET_BIND_SERVICE  10;
    | #define CAP_NET_RAW           13;
    | #define CAP_SYS_MODULE        16;
    | #define CAP_SYS_RAWIO         17;
    | #define CAP_SYS_BOOT          22;
    | #define CAP_AUDIT_READ        37;
    | #define CAP_LAST_CAP          CAP_AUDIT_READ;
    | ```
    | 对于普通用户运行的进程，当有这个权限的时候，就能做这些操作；没有的时候，就不能做，这样粒度要小很多。
    |
    | cap_permitted 表示进程能够使用的权限。但真正起作用的是cap_effective。cap_permitted中可以包含cap_effective中没有的权限。
    | 一个进程可以在必要的时候，放弃自己的某些权限，这样更加安全。假设自己因为代码漏洞被攻破了，但是如果啥也干不了，就没办法进一步突破。
    |
    | cap_inheritable表示当可执行文件的扩展属性设置了inheritable位时，调用exec执行该程序会继承调用者的inheritable集合，并将其
    | 加入到permitted集合。但在非root用户下执行exec时，通常不会保留inheritable集合，但是往往又是非root用户，才想保留权限，所以非
    | 常鸡肋。
    |
    | cap_bset, 也就是说capability bounding set, 是系统中所有进程允许保留的权限。如果这个集合中不存在某个权限，那么系统中的所有
    | 进程都没有这个权限。即使以超级用户权限执行的进程，也是一样的。
    |
    | 这样有很多好处。例如，系统启动以后，将加载内核模块的权限去掉，那所有进程都不能加载内核模块。这样，即便这台机器被攻破，也做不了太多
    | 有害的事情。
    |
    | cap_ambient是比较新加入内核的，就是为了解决cap_inheritable鸡肋的情况，也就是，非root用户进程使用exec执行一个程序的时候，如何
    | 保留权限的问题。当执行exec的时候，cap_ambient会被添加到cap_permitted中，同时设置cap_effective中。
    ------------------------------------------------------------

    ------------------------------------------------------------
    | ④ 内存管理
    | 每个进程都有自己独立的虚拟内存空间，这需要有一个数据结构来表示，就是mm_struct。这个我们在内存管理那一节详细讲述。
    | ```
    | struct mm_struct      *mm;
    | struct mm_struct      *active_mm;
    | ```
    ------------------------------------------------------------

    ------------------------------------------------------------
    | ⑤ 文件与文件系统
    | 每个进程有一个文件系统的数据结构，还有一个打开文件的数据结构。这个我们在文件系统那一节详细讲述
    | ```
    | // Filesystem information:
    | struct fs_struct              *fs;
    | // Open file information
    | struct files_struct           *files;
    | ```
    ------------------------------------------------------------
 */