# c11_threadpool
C++模拟线程池，可自定义线程数量与任务队列的长度

线程池（thread pool）是一种线程使用模式。线程过多或者频繁创建和销毁线程会带来调度开销，进而影响缓存局部性和整体性能。而线程池维护着多个线程，等待着管理器分配可并发执行的任务。这避免了在处理短时间任务时创建与销毁线程的代价，以及保证了线程的可复用性。线程池不仅能够保证内核的充分利用，还能防止过分调度。

## 线程池的原理
预先创建好指定数量的线程，这些线程的身份相当于是消费者；任务队列中的任务相当于生产者。每当有任务被加入队列时，线程们就会去竞争任务，每次只有一个线程能够得到任务；得到任务的线程会执行该任务，执行完毕后继续去竞争新的任务。

## 线程池的好处
- 避免了在高并发情况下,不断开辟线程造成的进程资源消耗

  > 假设一个服务器完成一项任务所需时间为：T1 创建线程时间，T2 在线程中执行任务的时间，T3 销毁线程时间。 如果：T1 + T3 远大于 T2，则可以采用线程池，以提高服务器性能。

- 保证多个任务可以并发地执行

  > 例如，一个任务队列有n个任务，可以通过线程池的调度分配到m个线程上去并发执行。

## 一个线程池基本组成

1. 线程池管理器（ThreadPool）：用于创建并管理线程池，包括：创建线程池，销毁线程池，添加新任务； 

2. 工作线程（PoolWorker）：线程池中的线程，在没有任务时处于等待状态，可以循环地执行任务； 

3. 任务接口（Task）：每个任务必须实现的接口，以供工作线程调度任务的执行，它主要规定了任务的入口，任务执行完后的收尾工作，任务的执行状态等； 

4. 任务队列（taskQueue）：用于存放没有处理的任务，提供一种缓冲机制。

# 代码实现

**代码地址：**

<https://github.com/metang326/c11_threadpool>

## 流程
1. 初始化线程池实例，新生成指定数量的线程，加入线程数组；由于此时没有任务加入，任务队列为空，因此所有的线程陷入睡眠

2. 添加任务队列，每添加一个任务就唤醒一个线程，被唤醒线程从任务队列中取出任务并执行

3. 任务队列已满时，需要等待之前的任务执行完毕，队列不再满时才能加入新任务

4. 删除线程池时，is_working置为false，并且唤醒所有睡眠中的线程，使用join


## 使用方法

**Linux命令行：**

> g++  main.cpp -otest -pthread;./test

**测试函数：线程池中有10个线程，任务队列最长为100个任务，总共下发了5000个任务。**

```
int main(int argc, char *argv[]) {
    ThreadPool *thread_pool = new ThreadPool(10, 100);
    for (int i = 0; i < 5000; i++) {
        thread_pool->add_task([=] { return print(i); });
    }
    this_thread::sleep_for(chrono::milliseconds(2000));
    delete thread_pool;
    return 0;
}
```

**输出结果：**

```
------------delete pool--------------
[-] join thread 140290998769408, finished task_num =331
[-] join thread 140290990376704, finished task_num =555
[-] join thread 140290981984000, finished task_num =535
[-] join thread 140290973591296, finished task_num =583
[-] join thread 140290965198592, finished task_num =624
[-] join thread 140290956805888, finished task_num =350
[-] join thread 140290948413184, finished task_num =596
[-] join thread 140290940020480, finished task_num =495
[-] join thread 140290931627776, finished task_num =473
[-] join thread 140290923235072, finished task_num =458
[✔] delete ThreadPool finished!

```

## 编码过程的思考
### 0. notify_one的唤醒对象
- 工作线程陷入睡眠的条件：任务队列为空（没有活要干）；
- 增加任务的主线程陷入睡眠的条件：任务队列满了（没有人来干活）。

**虚假唤醒现象**

当任务数量很少的情况下，例如有10个工作线程，但一共只有5个任务，那么领取到这5个任务的工作线程，完成任务之后，唤醒对象并不是主线程（因为主线程已经把自己需要添加的5个任务都放进队列了，没有陷入睡眠），唤醒对象是没有领到任务的工作线程。

把代码中增加一些输出可以看出来，task4是最后一个任务，执行完毕后还有5个线程被唤醒了，**但这是一种虚假的唤醒，因为任务队列依然是空的，所以工作线程仍然会陷入睡眠**。
```
while (is_working and tasks.empty()) {
                cond.wait(lk);
            }
```

从输出可看出来，被“虚假唤醒”的线程，醒了之后由于队列为空所以再次陷入while循环中的睡眠状态：

（以为有活要干所以起床了，发现还是没有活，白醒了，接着睡觉去了）
```
[-] task 3 working in thread 0x70000cbe5000
0x70000cbe5000 going to notify_one
[-] task 4 working in thread 0x70000cbe5000
0x70000cd6e000 was wanken up to work
0x70000cd6e000 thread going to sleep
0x70000cceb000 was wanken up to work
0x70000cceb000 thread going to sleep
0x70000cdf1000 was wanken up to work
0x70000cdf1000 thread going to sleep
0x70000cc68000 was wanken up to work
0x70000cc68000 thread going to sleep
0x70000ce74000 was wanken up to work
0x70000ce74000 thread going to sleep
0x70000cbe5000 thread going to sleep
```

但既然使用线程池，那么任务数少于线程数这种情况其实不太合理了，说明有多余线程。

**对工作线程来说，只有来自主线程的唤醒才是有效的，说明又有新任务可以领取了。**

一般情况下，任务数量是会更多的，因为我们设置线程池就是为了让工作线程能够被循环使用，一直保持忙碌状态，干完活之后再去领取任务。

增加任务的工作，其实执行起来是更快的，只需要在主线程把任务的函数地址、参数加入队列，一个简单的append操作，所以主线程增加任务的速度是比工作线程完成任务的速度更快的，往往是主线程把任务加满了，没有空闲的工作线程来干活。

**工作线程=程序员，主线程=PM，任务=需求**

> PM提需求的速度>程序员实现需求的速度

> PM的需求总是源源不断的，排期很满，愁的不是没有需求，而是程序员还在做之前的需求，没人来做新需求

### 1. 退出时无法join线程，问题排查

**正确情况：**

```c++
void work() {
        while (is_working) {
            unique_lock<mutex> lk(task_mutex);
            while (is_working and tasks.empty()) {
                cond.wait(lk);
            }
            if (tasks.size() == 0) {
                continue;
            }
            Task cur_task;
            cur_task = tasks.front();
            tasks.pop();
            cond.notify_one();
            int task_id=cur_task();
        }
    }
```

**会造成退出时，join线程失败的情况：**

```c++
void work() {
        while (is_working) {
            unique_lock<mutex> lk(task_mutex);
            while (tasks.empty()) {
                cond.wait(lk);
            }
            if (tasks.size() == 0) {
                continue;
            }
            Task cur_task;
            cur_task = tasks.front();
            tasks.pop();
            cond.notify_one();
            int task_id=cur_task();
        }
    }
```

上面的代码中，进入wait的条件一开始我没有设置为：**while (this->is_working and tasks.empty())，**而是直接**while (tasks.empty())**。因为当时的直觉是，这是进入了while循环后的代码，那么一定是满足了**is_working=true**。

**但实际的情况是：**

- 当任务全部执行结束，此时tasks.empty()=true，所以线程们会陷入sleep，等到有新的任务加入时才被唤醒。
- 当我们关闭线程池的时候，会把is_working=false，并且发出cond.notify_all();来把所有的睡眠状态线程都唤醒，唤醒后的线程才可以join。
- 然而，当之前陷入睡眠的线程虽然因为notify_all()被唤醒了，**但还是会因为tasks.empty()=true再次陷入睡眠，这时已经不会再有人来唤醒它们了，因此导致所有的线程都无法被join。**
- 这些无法被唤醒的线程，还是处于**之前的is_working=true的睡眠过程中**，所以无法进入is_working=false的那次循环。

```
thread 140153839867648 is sleeping
thread 140153823082240 is sleeping
thread 140153797904128 is sleeping
thread 140153831474944 is sleeping
thread 140153814689536 is sleeping
thread 140153781118720 is sleeping
thread 140153856653056 is sleeping
thread 140153789511424 is sleeping
thread 140153806296832 is sleeping
thread 140153848260352 is sleeping
thread 140153823082240 is sleeping
thread 140153839867648 is sleeping
------------delete pool--------------
[-] join thread 140153856653056, finished task_num =11
```


**不能以平时写单线程代码的思维来写多线程的代码**

比如下面这里任务队列为空的判断，虽然进入上一层while循环的条件是is_working=true，但在wait导致陷入睡眠的过程中，is_working有可能已经变成了false，那么我们退出当前while循环的条件是：is_working=false,this->tasks.empty()可能为true。如果this->tasks.empty()=true，那么直接执行下面的执行任务代码是会出错的。以此还是需要对任务队列是否为空的判断。

```
            while (this->is_working and this->tasks.empty()) {
                this->cond.wait(lk);
            }
//            if (this->tasks.empty()) {
//                continue;
//            }
```



### 2. 任务内容简单且数量少的情况，所有的任务都被第一个创建的线程抢夺了。

当任务仅为输出一行字符时，执行任务的开销会远小于线程被唤醒的开销。第一个线程最先被唤醒，在它循环的过程中，任务充足，不会陷入睡眠状态，所以在尝试运行几次例子之后，基本上都是由第一个线程抢占了所有的任务。

**任务：**

```c++
void print(int task_id) {
    cout << "[-] task " << task_id << " working in thread " << this_thread::get_id() << endl;
}

void work() {
        while (this->is_working) {
//            this_thread::sleep_for(chrono::milliseconds(1));
            unique_lock<mutex> lk(task_mutex);
            while (this->is_working and tasks.empty()) {
                cond.wait(lk);
            }
            if (tasks.size() == 0) {
                continue;
            }
            Task cur_task;
            cur_task = tasks.front();
            tasks.pop();
            cond.notify_one();
            cur_task();
        }
    }
```

**{ 线程数量3，最大任务队列6，总任务量9 }  对应输出：**

```c++
tangmy@dell-mgt-01:~/tmy_repos/threadpool$ g++  main.cpp -otest -pthread;./test 
[✔] create ThreadPool finished!
[-] task 0 working in thread 140421144192768
[-] task 1 working in thread 140421144192768
[-] task 2 working in thread 140421144192768
[-] task 3 working in thread 140421144192768
[-] task 4 working in thread 140421144192768
[-] task 5 working in thread 140421144192768
[-] task 6 working in thread 140421144192768
[-] task 7 working in thread 140421144192768
[-] task 8 working in thread 140421144192768
------------delete pool--------------
[-] join thread 140421144192768
[-] join thread 140421135800064
[-] join thread 140421127407360
[✔] delete ThreadPool finished!

```



### 3. 在work()增加一毫秒的sleep能够维持均匀的线程调度

> 在work函数增加  this_thread::sleep_for(chrono::milliseconds(1));  
>
> 这种情况，在第1个线程结束了任务后，会进入一个sleep，在它sleep的过程中，就可以由第2个线程来抢新任务，然后进入sleep,.....

```c++
void work() {
    while (this->is_working) {
        this_thread::sleep_for(chrono::milliseconds(1));
        unique_lock<mutex> lk(task_mutex);
        while (this->is_working and tasks.empty()) {
            cond.wait(lk);
        }
        if (tasks.size() == 0) {
            continue;
        }
        Task cur_task;
        cur_task = tasks.front();
        tasks.pop();
        cond.notify_one();
        cur_task();
    }
}
```

**{ 线程数量3，最大任务队列6，总任务量9 }  输出：**

```c++
tangmy@dell-mgt-01:~/tmy_repos/threadpool$ g++  main.cpp -otest -pthread;./test 
[✔] create ThreadPool finished!
[-] task 0 working in thread 140195294820096
[-] task 1 working in thread 140195278034688
[-] task 2 working in thread 140195286427392
[-] task 3 working in thread 140195294820096
[-] task 4 working in thread 140195286427392
[-] task 5 working in thread 140195278034688
[-] task 6 working in thread 140195294820096
[-] task 7 working in thread 140195286427392
[-] task 8 working in thread 140195294820096
------------delete pool--------------
[-] join thread 140195294820096
[-] join thread 140195286427392
[-] join thread 140195278034688
[✔] delete ThreadPool finished!

```

> 当把每个线程需要执行的任务计算量增大的时候，也能实现相对均衡的线程任务分配：

```c++
void print(int task_id) {
    cout << "[-] task " << task_id << " working in thread " << this_thread::get_id() << endl;
    long long res=0;
    for(int i=0;i<1000000000;i++){
        res++;
    }
}
```

**{ 线程数量3，最大任务队列6，总任务量9 }  输出：**

```c++
tangmy@dell-mgt-01:~/tmy_repos/threadpool$ g++  main.cpp -otest -pthread;./test 
[✔] create ThreadPool finished!
[-] task 0 working in thread 140681668404992
[-] task 1 working in thread 140681651619584
[-] task 2 working in thread 140681668404992
[-] task 3 working in thread 140681651619584
[-] task 4 working in thread 140681668404992
[-] task 5 working in thread 140681668404992
[-] task 6 working in thread 140681660012288
------------delete pool--------------
[-] join thread 140681668404992
[-] task 7 working in thread 140681651619584
[-] task 8 working in thread 140681668404992
[-] join thread 140681660012288
[-] join thread 140681651619584
[✔] delete ThreadPool finished!

```

### 4. 任务队列与总数较大时均衡性更好

**{ 线程数量10，最大任务队列100，总任务量10000 } 输出：**  

```c++
------------delete pool--------------
[-] join thread 139730486896384, finished task_num =961
[-] join thread 139730478503680, finished task_num =1026
[-] join thread 139730470110976, finished task_num =948
[-] join thread 139730461718272, finished task_num =1012
[-] join thread 139730453325568, finished task_num =1024
[-] join thread 139730444932864, finished task_num =951
[-] join thread 139730436540160, finished task_num =984
[-] join thread 139730428147456, finished task_num =1027
[-] join thread 139730419754752, finished task_num =1050
[-] join thread 139730411362048, finished task_num =1017
[✔] delete ThreadPool finished!

```
