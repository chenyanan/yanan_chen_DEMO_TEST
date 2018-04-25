//
//  6_2.cpp
//  123
//
//  Created by chenyanan on 2017/5/5.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <iostream>
#include <thread>
#include <mutex>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//6.2.2 使用锁和条件变量的线程安全队列

//清单6.2中重现了第4章中提到的线程安全对垒，类似于栈是以std::stack<>建模的，队列是以std::queue<>来建模的，此外，它的接口与标准容器适配器的结构是不同的，因为他要满足其数据将诶够对于来自多线程的并发访问是安全的这一约束

//清单6.2 使用条件变量的线程安全队列的完整类定义

#include <queue>

template<typename T>
class threadsafe_queue
{
private:
    mutable std::mutex mut;
    std::queue<T> data_queue;
    std::condition_variable data_cond;
public:
    threadsafe_queue() {};
    
    void push(T new_vlaue)
    {
        std::lock_guard<std::mutex> lk(mut);
        data_queue.push(std::move(new_vlaue));
        data_cond.notify_one();   //①
    }
    
    void wait_and_pop(T& value)   //②
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk, [this] { return !data_queue.empty(); });
        value = std::move(data_queue.front());
        data_queue.pop();
    }
    
    std::shared_ptr<T> wait_and_pop()   //③
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk, [this] { return !data_queue.empty(); });   //④
        std::shared_ptr<T> res(std::make_shared<T>(std::move(data_queue.front())));
        data_queue.pop();
        return res;
    }
    
    bool try_pop(T& value)
    {
        std::lock_guard<std::mutex> lk(mut);
        if (data_queue.empty())
            return false;
        value = std::move(data_queue.front());
        data_queue.pop();
        return true;
    }
    
    std::shared_ptr<T> try_pop()
    {
        std::lock_guard<std::mutex> lk(mut);
        if (data_queue.empty())
            return std::shared_ptr<T>();   //⑤
        std::shared_ptr<T> res(std::make_shared<T>(std::move(data_queue.front())));
        data_queue.pop();
        return res;
    }
    
    bool empty() const
    {
        std::lock_guard<std::mutex> lk(mut);
        return data_queue.empty();
    }
};

//清单6.2中所示的队列实现的结构与清单6.1中所示的栈的结构是类似的，除了push()①中调用的data_cond.notify_one()以及wait_and_pop()函数②、③，try_pop()的两种重载形式与清单6.1中的pop()函数的两种重载形式基本上是相同的，却被在于当队列为空的时候，try_pop()函数不引发异常，try_pop()函数要么返回一个表明是否取回一个值得bool值，要么在返回指针的重载⑤没能取到值得时候返回NULL，这也是实现栈的有效方式，因此，如果不包括wait_and_pop()函数，为栈引用所做的分析同样适用此

//新的wait_and_pop()函数是一种解决等待队列入口问题的方法，与反复调用empty()函数不同，等待中的线程可以通过调用wait_and_pop()函数，然后数据结构使用条件变量来处理这种等待状态，对data_cond.wait()的调用直到下层队列中至少有一个元素时，才会返回，因此你不用担心在代码的这个地方队列为空的可能性，数据仍然被互斥元上的锁保护着，这些函数不会增加任何新的竞争条件和死锁的可能性，并且维持了不变量

//当一个元素进入队列时，如果不止一个线程处于等待状态，关于异常安全就会有点崎岖，只有一个线程会被data_cond.notify_one()的调用唤醒，然而，如果被唤醒的线程在wait_and_pop()中引发异常，例如构造std::shared_ptr<>的时候④，那么就没有线程将被唤醒，如果不能接受这一点，那么可以用data_cond.notify_all()来代替data_cond.notify_one()，前者将唤醒所有在等待的线程，代价就是当最后它们发现队列为空的时候，其中的大部分线程都要重新进入睡眠状态，第二种替代方法就是，当引发异常的时候，让wait_and_pop()调用notify_one()，这样另一个线程可以尝试获取所存储的值，第三种替代方法，是将std::shared_ptr<>的初始化移动到push()调用，并且存储std::shared_ptr<>实例而不是直接存储值，将内部的std::queue<>复制到std::shared_ptr<>并不会引发异常，因此wait_and_pop()有是安全的了，清单6.3展示了使用这一思想重新修订以后的队列实现

//清单6.3 包含std::shared_ptr<>实例的线程安全队列

template<typename T>
class threadsafe_shared_ptr_queue
{
private:
    mutable std::mutex mut;
    std::queue<std::shared_ptr<T>> data_queue;
    std::condition_variable data_cond;
public:
    threadsafe_shared_ptr_queue() {}
    
    void wait_and_pop(T& value)
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk, [this] { return !data_queue.empty(); });
        value = std::move(*data_queue.front());   //①
        data_queue.pop();
    }
    
    bool try_pop(T& value)
    {
        std::lock_guard<std::mutex> lk(mut);
        if (data_queue.empty())
            return false;
        value = std::move(*data_queue.front());   //②
        data_queue.pop();
        return true;
    }
    
    std::shared_ptr<T> wait_and_pop()
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk, [this] { return !data_queue.emtpy(); });
        std::shared_ptr<T> res = data_queue.front();   //③
        data_queue.pop();
        return res;
    }
    
    std::shared_ptr<T> try_pop()
    {
        std::lock_guard<std::mutex> lk(mut);
        if (data_queue.empty())
            return std::shared_ptr<T>();
        std::shared_ptr<T> res = data_queue.front();   //④
        data_queue.pop();
        return res;
    }
    
    void push(T new_value)
    {
        std::shared_ptr<T> data(std::make_shared<T>(std::move(new_value)));   //⑤
        std::lock_guard<std::mutex> lk(mut);
        data_queue.push(data);
        data_cond.notify_one();
    }
    
    bool empty() const
    {
        std::lock_guard<std::mutex> lk(mut);
        return data_queue.empty();
    }
};

//通过std::shared_ptr<>持有数据的基本效果是直观的:接受一个对变量的引用来获取新值得pop函数，现在必须得解引用所存储的指针①、②，返回一个std::shared_ptr<>实例的pop函数可以在返回调用前从队列中取得这个数③、④

//如果数据由std::shared_ptr<>持有，那么有一个额外的好处:可以在push()的锁外面完成此新实例的分配⑤，然而砸清单6.2中，就必须在pop()持有锁的情况下，才能这样做，因为内存分配通常是很昂贵的操作，这种范式就有助于提高队列的性能，因为它也很少有吃由互斥元的时间，允许其他线程同时在队列上执行操作

//就像之前栈的例子一样，是用互斥元来保护整个数据结构限制了队列所支持的并发，尽管多个线程可能在不同的成员函数中被阻塞，但是一次只有一个线程可以进行操作，尽管如此，部分限制来自于在视线中使用std::queue<>通过使用标准容器，你基本上有了一个受到保护或者没收到保护的数据项，通过控制数据结构的详细实现，可以提供更细粒度的锁定，并且实现更高级别的并发

#pragma clang diagnostic pop

