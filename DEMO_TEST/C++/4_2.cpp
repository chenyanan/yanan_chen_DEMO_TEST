//
//  4_2.cpp
//  123
//
//  Created by chenyanan on 2017/4/26.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <vector>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//4.1.2 使用条件变量建立一个线程安全队列
//如果你需要设计一个泛型队列，话几分钟考虑一下可能需要的操作是值得的，就像你在3.2.3节对线程安全堆栈所做的那样，让我们看一看C++标准库来寻找灵感，以清单4.2所示的std::queue<>的容器适配器的形式

//清单4.2 std::queue接口

template<typename T, typename Container = std::deque<T>>
class Queue {
    explicit Queue(const Container&);
    explicit Queue(Container&& = Container());
    
    template <typename Alloc> explicit Queue(const Alloc&);
    template <typename Alloc> Queue(const Container&, const Alloc&);
    template <typename Alloc> Queue(Container&&, const Alloc&);
    template <typename Alloc> Queue(Queue&&, const Alloc&);
    
    void swap(Queue& q);
    
    bool empty() const;
    size_t size() const;
    
    T& front();
    const T& front() const;
    
    T& back();
    const T& back() const;
    
    void push(const T& x);
    void push(T&& x);
    
    void pop();
    template <typename...Args> void emplate(Args&&... args);
};

//如果忽略构造函数，赋值和交换操作，那么还剩下3组操作，查询整个队列的状态(empty()和size())，查询队列的元素(front()和back())以及修改队列(push(),pop(),emplace())，这些操作与你之前在3.2.3节中对堆栈的操作是相同的，因此你也遇到相同的有关接口中固有的竞争条件的问题，所以，你需要将front()和pop()组合到单个函数调用中，就像你为了堆栈而组合top()和pop()那样，清单4.1中的代码增加了新的席位差别，但是，当使用队列在线程间传递数据时，接受线程往往需要等待数据，我们为pop()提供了两个变体:try_pop()，它试图从队列中弹出值，但总是立即返回(带有失败指示符),即使没有能获取到值，以及wait_and_pop()，他会一直等待，自导有值要获取,如果将栈示例中的特征带到此处，则接口看起来如清单4.3所示

//清单4.3 threadsafe_queue的接口

#include <memory>   //为了std::shared_ptr

template<typename T>
class threadsafe_queue_one
{
public:
    threadsafe_queue_one();
    threadsafe_queue_one(const threadsafe_queue_one&);
    threadsafe_queue_one& operator=(const threadsafe_queue_one&) = delete;   //为简单起见不允许赋值
    void push(T new_value);
    bool try_pop(T& value);   //①
    std::shared_ptr<T> try_pop();   //②
    void wait_and_pop(T& value);
    std::shared_ptr<T> wait_and_pop();
    bool empty() const;
};

//就像你为堆栈做的那样，减少构造函数并消除赋值以简化代码，如以前一样，还提供了try_pop()和wait_and_pop()的两个版本，try_pop()的第一个重载①将获取到的值存储在引用变量中，所以它可以将返回值用作状态，如果它获取到值就返回true,则返回false,第二个重载②不能这么做，因为它直接返回获取到的值，但是如果没有值可被获取，则返回的指针可以设置为NULL

//那么，所有这一切如何与清单4.1关联起来呢？嗯，你可以从那里提取push()以及wait_and_pop()的代码，如清单4.4所示

//清单4.4 从清单4.1中提取push()和wait_and_pop()

#include <queue>
#include <mutex>
#include <condition_variable>

struct data_chunk {};
static void process(const data_chunk& data) {}
static bool more_data_to_prepare() { return true; }
static data_chunk prepare_data() { return data_chunk(); }
static bool is_last_chunk(const data_chunk& data) { return true; }

template <typename T>
class threadsafe_queue_two
{
private:
    std::mutex mut;
    std::queue<T> data_queue;
    std::condition_variable data_cond;
public:
    void push(T new_value)
    {
        std::lock_guard<std::mutex> lk(mut);
        data_queue.push(new_value);
        data_cond.notify_one();
    }
    
    void wait_and_pop(T& value)
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk, [this] { return !data_queue.empty(); });
        value = data_queue.front();
        data_queue.pop();
    }
};

static threadsafe_queue_two<data_chunk> data_queue;   //①

static void data_preparetion_thread()
{
    while (more_data_to_prepare())
    {
        const data_chunk data = prepare_data();
        data_queue.push(data);   //②
    }
}

static void data_processing_thread()
{
    while (true)
    {
        data_chunk data;
        data_queue.wait_and_pop(data);   //③
        process(data);
        if (is_last_chunk(data))
            break;
    }
}

//互斥元和条件变量现在包含在threadsafe_queue的实例中，所以不再需要单独的变量①，并且调用push()不再需要外部的同步②，此外，wait_and_pop()负责条件变量等待③

//wait_and_pop()的另一个重载现在很容易编写，其余的函数几乎可以一字不差地从清单3.5中的栈实例中复制过来，清单4.5展示了最终的队列实现

//清单4.5 使用条件变量的线程安全队列的完整类定义

#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>

template <typename T>
class threadsafe_queue_three
{
private:
    mutable std::mutex mut;   //①互斥元必须是可变的
    std::queue<T> data_queue;
    std::condition_variable data_cond;
public:
    threadsafe_queue_three() {};
    
    threadsafe_queue_three(const threadsafe_queue_three& other)
    {
        std::lock_guard<std::mutex> lk(other.mut);
        data_queue = other.data_queue;
    }
    
    void push(T new_value)
    {
        std::lock_guard<std::mutex> lk(mut);
        data_queue.push(new_value);
        data_cond.notify_one();
    }
    
    void wait_and_pop(T& value)
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk, [this] { return !data_queue.empty(); });
        value = data_queue.front();
        data_queue.pop();
    }
    
    std::shared_ptr<T> wait_and_pop()
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk, [this] { return !data_queue.empty(); });
        std::shared_ptr<T> res{std::make_shared<T>(data_queue.front())};
        data_queue.pop();
        return res;
    }
    
    bool try_pop(T& value)
    {
        std::lock_guard<std::mutex> lk(mut);
        if (data_queue.empty())
            return false;
        value = data_queue.front();
        data_queue.pop();
        return true;
    }
    
    std::shared_ptr<T> try_pop()
    {
        std::lock_guard<std::mutex> lk(mut);
        if (data_queue.empty())
            return std::shared_ptr<T>();
        std::shared_ptr<T> res{std::make_shared<T>(data_queue.frong())};
        data_queue.pop();
        return res;
    }
    
    bool empty() const
    {
        std::lock_guard<std::mutex> lk(mut);
        return data_queue.empty();
    }
};

//虽然empty()是一个const成员函数，并且拷贝构造函数的other参数是一个const引用，但是其他线程可能会有到该对象的非const引用，并调用可变的成员函数，所以我们仍然需要锁定互斥元，由于锁定互斥元是一种可变的操作，故互斥元对象必须标记为mutable①，以便其可以被锁定在empty()和拷贝构造函数中

//条件变量在多个线程等待同一个时间的合作也是很有用的，如果线程被用于划分工作负载，那么应该只有一个线程去响应通知，可以使用与清单4.1中完全相同的结构，只要运行多个数据处理线程的实例，当新的数据准备就绪，notify_one()的调用将触发其中一个正在执行wait()的线程去检查其条件，然后从wait()返回(因为你刚向data_queue中增加了意向),谁也不能保证哪个线程会被通知到，即使是某个线程正在等待通知，所有的处理线程可能仍在处理数据

//另一种可能性是，多个线程正等待着同一事件，并且它们都需要作出响应，这可能发生在共享数据正在初始化的场合下，处理线程都可以使用同一个数据，但需要等待期初始化完成(虽然有比这更好的机制,参见第3章中的3.3.1节)，或者是县城需要等待共享数据更新的地方，比如周期性的重新初始化，在这些案例中，准备数据的线程可以在条件变量上调用notify_all()成员函数而不是notify_one()，贵名思议，这将导致所有当前执行着wait()的线程检查其等待中的条件

//如果等待线程只打算等待一次，那么当条件为true时它就不会再等待这个条件变量了，条件变量未必是同步机制的最佳选择，如果所等待的条件是一个特定数据块的可用性，这尤其正确，在这个场景中，使用期望可能会更合适

#pragma clang diagnostic pop
