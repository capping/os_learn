/*
    # 调度(上)
    ① 调度策略与调度类
    ② 实时调度策略
    ③ 普通调度策略
    ④ 完全公平调度算法
    ⑤ 调度队列与调度实体
    ⑥ 调度类是如何工作的？
    总结时刻

    对于操作系统来讲，它面对的CPU的数量是有限的，干活儿都是他们，但是进程数目远远超过CPU的数目，因而就需要进行进程的调度，
    有效地分配CPU的时间，既要保证进程的最快响应，也要保证进程之间的公平。这也是一个非常复杂的、需要平衡的事情。

    ------------------------------------------------------------
    | ① 调度策略与调度类
    | 在Linux里面，进程大概可以分为两种。
    |
    | 1. 实时进程:也就是需要尽快执行返回结果的那种
    | 2. 普通进程:大部分的进程其实都是这种
    |
    | 那很显然，对于这两种进程，我们的调度策略肯定是不同的。
    |
    | 在task_struct中，有一个成员变量，我们叫调度策略
    | ```
    | unsigned int policy;
    | ```
    | 它有以下几个定义：
    | ```
    | #define SCHED_NORMAL          0
    | #define SCHED_FIFO            1
    | #define SCHED_RR              2
    | #define SCHED_BATCH           3
    | #define SCHED_IDLE            5
    | #define SCHED_DEADLINE        6
    | ```
    | 配合调度侧策略的，还有我们刚才说的优先级，也在task_struct中。
    | ```
    | int prio, static_prio, normal_prio;
    | unsigned int rt_priority;
    | ```
    | 优先级其实就是就是一个数值，对于实时进程，优先级的范围是0~99；对普通进程，优先级的范围是100~139。数值越小，优先级
    | 越高。从这里可以看出，所有的实时进程都比普通进程优先级高。
    ------------------------------------------------------------

    ------------------------------------------------------------
    | ② 实时调度策略
    | 对于调度策略，其中SCHED_FIFO、SCHED_RR、SCHED_DEADLINE是实时进程的调度策略。
    | 1. SCHED_FIFO，先来先服务，但是有的加钱多，可以分配更高的优先级，也就是说，高优先级的进程可以抢占低优先级的进程，而
    | 相同优先级的进程，我们遵循先来先得。
    | 2. SCHED_RR，轮流调度算法，采用时间片，相同优先级的任务当用完时间片会被放到队列尾部，以保证公平性，而高优先级的任务也
    | 是可以抢占低优先级的任务。
    | 3. SCHED_DEADLINE，按照任务的deadline进行调度。当产生一个调度的时候，DL调度器总是选择其deadline距离当前时间点最近
    | 的那个任务，并调度它执行。
    ------------------------------------------------------------

    ------------------------------------------------------------
    | ③ 普通调度策略
    | 对于普通进程的调度策略有，SCHED_NORMAL、SCHED_BATCH、SHCED_IDLE。
    | 1. SCHED_NORMAL，普通的进程
    | 2. SHCED_BATCH，后台进程，几乎不需要和前端进行交互。
    | 3. SCHED_IDLE，是特别空闲的时候才跑的进程。
    | 
    | 上面无论是policy【调度策略】还是priority【优先级】，都设置了一个变量，变量仅仅表示了应该这样这样干，但事情总要有人干，谁呢？
    | 在task_struct里面，还有这样的成员变量：
    | ```
    | const struct sched_class *sched_class;
    | ```
    | 调度策略的执行逻辑，就封装在这里，它是真正干活的那个。
    |
    | sched_class有几种实现：
    | - stop_sched_class优先级最高的任务会使用这种策略，会中断所有其他线程，且不会被其他任务打断；
    | - dl_sched_class就对应上面的deadline调度策略。
    | - rt_sched_class就对应RR算法或者FIFO算法的调度策略，具体调度策略由进程task_struct->policy指定。
    | - fair_sched_class就是普通的调度策略。
    | - idle_sched_class就是空闲进程的调度策略。
    |
    | 这里实时进程的调度策略RR和FIFO相对简单一些，而且由于咱们平时常遇到的都是普通进程，在这里，咱们就重点分析普通进程的调度问题。普通进程
    | 使用的调度策略是fair_sched_class，顾名思义，对于普通进程来讲，公平是最重要的。
    ------------------------------------------------------------
    ------------------------------------------------------------
    | ④ 完全公平调度算法
    | 在Linux里面，实现了一个基于CFS的调度算法。CFS全称Completely Fair Scheduling，叫完全公平调度。听起来很“公平”。那这个算法的原理是
    | 什么呢？
    |
    | 首先，你需要记录下进程的运行时间。CPU会提供一个时钟，过一段时间就触发一个时钟中断。就像咱们的表滴答一下，这个我们叫做Tick。CFS会为每一
    | 个进程安排一个虚拟运行时间vruntime。如果进程在运行，随着时间的增长，也就是一个个tick的到来，进程的vruntime将不断增大。没有得到执行的
    | 进程vruntime不变
    |
    | 显然，那些vruntime少的，原来受到了不公平的对待，需要给它补上，所以会优先运行这样的进程。
    | 
    | 如何给优先级高的进程多分时间呢？
    | 这个简单，就相当于N个口袋，优先级高的袋子大，优先级低的袋子小。这样球就不能按照个数分配了，要按比例来，大口袋的放一半和小口袋放一半，里面
    | 球数目虽然差很多，也认为是公平的。
    | 
    | 在更新进程运行的统计量的时候，我们其实就可以看出这个逻辑。
    | ```
    | // Update the current task's runtime statistics.
    | static void update_curr(struct cfs_rq *cfs_rq) {
    |     struct sched_entity *curr = cfs_rq->curr;
    |     u64 now = rq_lock_task(rq_of(cfs_rq));
    |     u64 delta_exec;
    |     ...
    |     delta_exec = now - curr->exec_start;
    |     ...
    |     curr->exec_start = now;
    |     ...
    |     curr->sum_exec_runtime += delta_exec;
    |     ...
    |     curr_vruntime += calc_delta_fair(delta_exec, curr);
    |     update_min_vruntime(cfs_rq);
    | }
    |
    | static inline u64 calc_delta_fair(u64 delta, struct sched_entity *se) {
    |      if (unlikely(se->load.weight != NICE_0_LOAD)) {
    |           delta = __calc_delta(delta, NICE_0_LOAD, &se->load);
    |      }
    |      return delta;
    | }
    | ```
    | 在这里得到当前时间，以及这次的时间片开始的时间，两者相减就是这次运行的时间delta_exec，但是得到的这个时间其实是实际运行的时间，需要做一定
    | 的转化才作为虚拟运行时间vruntime。转化方式如下：
    | > (虚拟运行时间) vruntime += (实际运行时间) delta_exec * NICE_0_LOAD / 权重
    ------------------------------------------------------------

    ------------------------------------------------------------
    | ⑤ 调度队列与调度实体
    | 看来CFS需要一个数据结构来对vruntime进行排序，找出最小的那个。这个能够排序的数据结构不但需要查询的时候，能够快速找到最小的，更新的时候也需
    | 要能够快速的调整排序，要知道vruntime可是经常在变的，变了再插入这个数据结构，就需要重新排序。
    |
    | 能够平衡查询和更新速度的树，在这里使用的是红黑树。
    | 红黑树的节点是应该包含vruntime的，称为调度实体。
    |
    | 在task_struct中有这样的成员变量：
    | ```
    | struct sched_entity    se;
    | struct sched_rt_entity rt;
    | struct sched_dl_entity dl;
    | ```
    | 这里有实时调度实体sched_rt_entity，Deadline调度实体sched_dl_entity，以及完全公平算法调度实体sched_entity。
    |
    | 看来不光CFS调度策略需要有一个数据结构进行排序，其他的调度策略也同样有自己的数据结构进行排序，因为任何一个策略调度的时候，
    | 都是要区分谁先运行与谁后运行。
    | 
    | 而进程根据自己的是实时的，还是普通的类型，通过这个成员变量，将自己挂在某一个数据结构里面，和其他的进程排序，等待被调度。
    | 如果这个进程是普通进程，则通过sched_entity，将自己挂在这棵红黑树上。
    |
    | 对于普通进程的调度实体定义如下，这里面包含了vruntime和权重load_weight，以及对于运行时间的统计。
    | ```
    | struct sched_entity {
    |     struct load_weight        load;
    |     struct rb_node            run_node;
    |     struct list_head          group_node;
    |     unsigned int              on_rq;
    |     u64                       exec_start;   
    |     u64                       sum_exec_runtime;   
    |     u64                       vruntime;   
    |     u64                       pre_sum_exec_runtime;   
    |     u64                       nr_migrations;   
    |     struct seched_statistics  statistics;   
    | };
    | ```
    | 所有可运行的进程通过不断地插入操作最终都存储在以时间为顺序的红黑树中，vruntime最小的在树的左侧，vruntime最多的在树的右侧。
    | CFS调度策略会选择红黑树最左边的叶子节点作为下一个将获得CPU的任务。
    |
    | 这棵红黑树放在那里呢？
    | 每个CPU有自己的struct rq结构，其用于描述在此CPU上所运行的的所有进程，其包括一个实时进程队列rt_rq和一个CFS运行队列cfs_rq,
    | 在调度时，调度器首先会先去实时进程队列找是否有实时进程需要运行，如果没有才会去CFS运行队列找是否有进程需要运行。
    | ```
    | struct rq {
    |     // runqueue lock:
    |     raw_spinlock_t lock;
    |     unsigned int nr_running;
    |     unsigned long cpu_load[CPU_LOAD_IDX_MAX];
    |     ...
    |     struct load_weigth load;
    |     unsigned long nr_load_updates;
    |     u64   nr_switches;
    |     ...
    |     struct cfs_rq cfs;
    |     struct rt_rq rt;
    |     struct dl_rq dl;
    |     ...
    |     struct task_struct *curr, *idle, *stop;   
    | };
    | ```
    | 对于普通公平队列cfs_rq，定义如下：
    | ```
    | // CFS-related fields in a runqueue
    | struct cfs_rq {
    |     struct load_weigth load;
    |     unsigned int nr_running, h_nr_running;
    |     u64 exec_lock;
    |     u64 min_vruntime;
    | #ifndef CONFIG_64BIT
    |     u64 min_vruntime_copy;
    | #endif
    |     struct rb_root tasks_timeline;
    |     struct rb_node *rb_leftmost;
    |     struct sched_entity *curr, *next, *last, *skip;   
    | };
    | ```
    | 这里面rb_root指向的就是红黑树的根节点，这个红黑树在CPU看起来就是一个队列，不断的取下一个应该运行的进程。
    | rb_leftmost指向的是最左面的节点。
    ------------------------------------------------------------

    ------------------------------------------------------------
    | ⑥ 调度类是如何工作的？
    | 凑够了数据结构，接下来我们看调度类是如何工作的。
    | ```
    | struct sched_class {
    |     const struct sched_class *next;
    |     
    |     void (*enqueue_task) (struct rq *rq, struct task_struct *p, int flags);
    |     void (*dequeue_task) (struct rq *rq, struct task_struct *p, int flags);
    |     void (*yield_task) (struct rq *rq);
    |     bool (*yield_to_task) (struct rq *rq, struct task_struct *p, bool preempt);
    |     void (*check_preempt_curr) (struct rq *rq, struct task_struct *p, int flags);
    |
    | }
    | ```
    | 这个结构定义了很多种方法，用于在队列上操作任务。这里请大家注意第一个成员变量，是一个指针，指向下一个调度类。
    | 
    | 上面我们讲了，调度类分为下面这几种：
    | ```
    | extern const struct sched_class stop_sched_class;
    | extern const struct sched_class dl_sched_class;
    | extern const struct sched_class rt_sched_class;
    | extern const struct sched_class fair_sched_class;
    | extern const struct sched_class idle_sched_class;
    | ```
    | 它们其实是放在一个链表上的。这里我们以调度最常见的操作，取下一个任务为例，来解析一下。可以看到，这里面有一个for_each_class循环，
    | 沿着上面的顺序，依次调用每个调用类的方法。
    | ```
    | static inline struct task_struct *;
    | pick_next_task(struct rq *rq, struct task_struct *prev, struct rq_flags *rf) {
    |     const struct sched_class *class;
    |     struct task_struct *p;
    |     for_each_class(class) {
    |         p = class->pick_next_task(rq, prev, rf);
    |         if (p) {
    |             if (unlikely(p == RETRY_TASK)) 
    |                 goto again;
    |              return p;
    |         }
    |     } 
    | }
    | ```
    | 这就说明，调度的时候是从优先级最高的调度类到优先级低的调度类，依次执行。而对于每种调度类，有自己的实现。
    | ```
    | const struct sched_class fair_sched_class = {
    |     .next                 = &idle_sched_class,
    |     .enqueue_task         = enqueue_task_fair,
    |     .dequeue_task         = dequeue_task_fair,
    |     .yield_task           = yield_task_fair,
    |     .yield_to_task        = yield_to_task_fair,
    |     .check_preempt_curr   = check_preempt_wakeup,
    |     .pick_next_task       = pick_next_task_fair,
    |     .put_prev_task        = put_prev_task_fair,
    |     .set_curr_task        = set_curr_task_fair,
    |     .task_tick            = task_tick_fair,
    |     .task_fork            = task_fork_fair,
    |     .prio_changed         = prio_changed_fair,
    |     .switched_from        = switched_from_fair,
    |     .switched_to          = switched_to_fair,
    |     .get_rr_interval      = get_rr_interval_fair,
    |     .update_curr          = update_curr_fair,
    | };
    | ```
    | 对于同样的pick_next_task选取下一个要运行的任务这个动作，不同的调度类有自己的实现。fair_sched_class的实现
    | 是pick_next_task_fair, rt_sched_class的实现是pick_next_task_rt。
    |
    | 我们会发现这两个函数是操作不同的队列，pick_next_task_rt操作的是rt_rq，pick_next_task_fair操作的是cfs_rq。
    | ```
    | static struct task_struct *;
    | pick_next_task_rt(struct rq *rq, struct task_struct *prev, struct rq_flags *rf) 
    | {
    |     struct task_struct *p;
    |     struct rt_rq *rt_rq = &rq->rt;
    |     ...
    | }
    | static struct task_struct *;
    | pick_next_task_fair(struct rq *rq, struct task_struct *prev, struct rq_flags *rf) 
    | {
    |     struct cfs_rq *cfs_rq = &rq->cfs;
    |     struct sched_entity   *se;
    |     struct task_struct    *p;
    | }
    | ```
    | 这样整个运行的场景就串起来了，在每个CPU上都有一个队列rq，这个队列里面包含多个子队列，例如rt_rq和cfs_rq，不同的队列有
    | 不同的实现方式，cfs_rq就是用红黑树实现的。
    |
    | 当有一天，某个CPU需要找下一个任务执行的时候，会按照优先级依次调用调度类，不同的调度类操作不同的队列。当然rt_sched_class
    | 会先被调用，它会在rt_rq上找下一个任务，只有找不到的时候，才轮到fair_sched_class被调度，它会在cfs_rq上找下一个任务。这
    | 样保证了实时任务的优先级永远大于普通任务。
    |
    | 下面我们仔细看一下sched_class定义的与调度有关的函数。
    | - enqueue_task 向就绪队列中添加一个进程，当某个进程进入可运行状态时，调用这个函数；
    | - dequeue_task 将一个进程从就绪队列中删除；
    | - pick_next_task 选择接下来要运行的进程；
    | - put_prev_task 用另一个进程代替当前运行的的进程；
    | - set_curr_task 用于修改调度策略；
    | - task_tick 每次周期性时钟到的时候，这个函数被调用，可能触发调度。
    |
    | 在这里，我们重点看fair_sched_class对于pick_next_task的实现pick_next_task_fair，获取下一个进程。调用路径如下：
    | pick_next_task_fair->pick_next_entity->__pick_first_entity。
    | ```
    | struct sched_entity *__pick_first_entity(struct cfs_rq *cfs_rq) {
    |     struct rb_node *left = rb_first_cached(&cfs_rq->tasks_timeline);
    |     if (!left) {
    |           return NULL;
    |     } 
    |     return rb_entity(left, struct sched_entity, run_node);
    | }
    | ```
    | 从这个函数的实现可以看出，就是从红黑树里面取最左面的节点。
    ------------------------------------------------------------
 */
