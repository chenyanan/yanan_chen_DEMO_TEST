//
//  3_10.cpp
//  123
//
//  Created by chenyanan on 2017/4/24.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <iostream>
#include <thread>
#include <mutex>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//3.2.8 锁定在恰当的粒度
//所粒度是我在之前曾提到过的，在3.2.3节中:锁粒度是一个文字属于，用来描述由单个锁所保护的数据量，细粒度锁保护着少量的数据，粗粒度锁保护着大量的数据，选择一个足够粗的锁粒度，来确保所需的数据都被保护是很重要的，不仅如此，同样重要的是，确保只在真正需要锁的操作中持有锁，我们都知道，带着谩骂安一车杂货在超市排队结账，只因为正在结账的人突然意识到自己忘了一些小红莓酱，然后就跑去找，而让大家都等着，或者收银员已经准备好收钱，顾客才开始在自己的手提包里翻找钱包，是很令人抓狂的，如果每个人去结账时都拿到了他们想要的，并准备好了适当的支付方式，一切都更容易进行

//这同样适用于线程，如果很多歌线程正等待着同一个资源(收银台的收银员)，然后，如果任意线程持有锁的时间比所需时间长，就会增加等待所花费的总时间(不要等到你已经到了收银台才开始寻找小红莓酱)，如果可能，仅在实际访问共享数据的时候锁定互斥元，尝试在锁的外面做任意的数据处理，特别地，在持有锁时，不要做任何确实很耗时的活动，比如文件I/O，文件I/O通常比从内存中读取或写入相同大小的数据量要慢上数百倍(如果不是数千倍)，因此，除非这个锁是真的想保护对文件的访问，否则在持有锁时进行I/O会不必要的延迟其他线程(因为它们在等待获取锁时会阻塞)，潜在地消除了使用多线程带来的性能提升

//std::unique_lock在这种情况下运作良好，因为能够在代码不再需要访问共享数据时调用unlock()，然后在代码中有需要访问时再次调用lock()

static std::mutex the_mutex;

typedef int result_type;

static int data_to_process;
static int some_class_data_to_process;
static int get_next_data_chunk() { return 0; }
static int process(int data_to_process) { return 0; }
static void write_result(int data_to_process, result_type result) {}

static void get_and_process_data()
{
    std::unique_lock<std::mutex> my_lock(the_mutex);
    some_class_data_to_process = get_next_data_chunk();
    my_lock.unlock();   //①在对process的调用中不需要锁定互斥元
    result_type result = process(data_to_process);
    my_lock.lock();   //②重新锁定互斥元以回写结果
    write_result(data_to_process, result);
}

//在调用process()过程中不需要锁定互斥元，所以手动地将其在调用前解锁①，并在之后再次锁定②

//希望这是显而易见的，如果你让一个互斥锁保护整个数据结构，不仅可能会有更多的对锁的竞争，锁被持有的时间也可能会减少，更多的操作步骤会需要在同一个互斥元上的锁，所以所必须被持有更长的时间，这种成本上的双重打击，也是尽可能走向细粒度锁定的双重激励

//如这个例子所示，锁定在恰当的粒度不仅关乎锁定的数据量，这也是关系到锁会被持有多长时间，以及在持有锁时执行哪些操作，一般情况下，只有应该以执行要求的擦偶作所需的最小可能时间而去持有锁，这也意味着耗时的操作，比如获取另一个锁(即便你知道它不会死锁)或是等待I/O完成，都不应该在持有锁的时候去做，除非绝对必要。

//在清单3.6和清单3.9中，需要锁定两个互斥锁的操作是交换操作，这显然是需要并发访问两个对象，假设取而代之，你试图去比较仅为普通int的简单数据成员，这会有区别吗？int可以轻易被复制，所以你可以很容易地为每个待比较的对象复制器数据，同时只用持有该对象的锁，然后比较已复制数值，这意味着你在每个互斥元上持有锁的时间最短，并且你也没有在持有一个锁的时候去锁定另外一个，清单3.10展示了这样一个类Y，以及相等比较运算符的示例实现

//清单3.10 在比较运算符中每次锁定一个互斥元

class Y {
private:
    int some_detail;
    mutable std::mutex m;
    
    int get_detail() const
    {
        std::lock_guard<std::mutex> lock_a(m);   //①
        return some_detail;
    }
public:
    Y(int sd) : some_detail(sd) {}
    friend bool operator==(const Y& lhs, const Y& rhs)
    {
        if (&lhs == &rhs)
            return true;
        const int lhs_value = lhs.get_detail();   //②
        const int rhs_value = rhs.get_detail();   //③
        return lhs_value == rhs_value;   //④
    }
};

//在这种情况下，比较运算符首先通过调用get_detail()成员函数获取要进行比较的值②和③，此函数在获取值得同时用一个锁来保护它①，比较运算符接着比较获取到的值④，但是请注意，这同样会减少锁定的时间，而且每次只持有一个锁(从而消除了死锁的可能性),与同时持有两个锁相比，这巧妙地改变了操作语义，在清单3.10中，如果运算符返回true，意味着lhs.some_detail在另一个时间点的值相等，这两个值能够在两次读取之中以任何方式改变，例如，这两个值可能在②和③之间进行了交换，从而使这个比较变得毫无意义，这个相等比较可能返回true来表示值是相等的，即使这两个值在某个瞬间从未真正地相等的，即使这两个值在某个瞬间从未真正地相等过，因此，当进行这样的改变时消息注意是很重要的，操作的语义不能以有问题的方式而被改变，如果你不能在操作的整个持续时间中持有所需的锁，你就把自己暴露在竞争条件中

//有时，根本就没有一个合适IDE粒度级别，因为并非所有的数据结构的访问都要求同样级别的保护，因为并非所有的对数据结构的访问都要求同样级别的保护，在这种情况下，使用替代机制来代替普通的std::mutex可能才是恰当的



#pragma clang diagnostic pop
