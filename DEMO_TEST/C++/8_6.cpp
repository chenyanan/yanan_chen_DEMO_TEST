//
//  8_6.cpp
//  DEMO_TEST
//
//  Created by chenyanan on 2017/6/12.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <iostream>
#include <numeric>
#include <thread>
#include <mutex>
#include <future>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//2.std::async()的异常安全

//你已经知道了当处理器时需要什么来实现异常安全，我们来看看使用std::async()时需要做的同样的事情，你已经看到了，在这种情况下库为你处理这些线程，并且当future是就绪的时候，产生的任何线程都完成了，需要注意到关键事情就是异常安全，如果销毁future的时候没有等待它，析构函数将等待线程完成，这就避免了仍然在执行以及持有数据引用的泄漏线程的问题，清单8.5所示就是使用std::async()的异常安全实现

//清单8.5 使用std::async的std::accumulate的异常安全并发版本

template<typename Iterator, typename T>
T parallel_accumulate(Iterator first, Iterator last, T init)
{
    unsigned long const length = std::distance(first, last);   //①
    unsigned long const max_chunk_size = 25;
    if (length < max_chunk_size)
    {
        return std::accumulate(first, last, init);   //②
    }
    else
    {
        Iterator mid_point = first;
        std::advance(mid_point, length / 2);   //③
        std::future<T> first_half_result = std::async(parallel_accumulate<Iterator, T>, first, mid_point, init);   //④
        
        T second_half_result = parallel_accumulate(mid_point, last, T());   //⑤
        return first_half_result.get() + second_half_result;   //⑥
    }
}

//这个版本使用递归将数据划分为块而不是冲洗计算将数据划分为块，但是它比之前的版本要简单一些，并且是异常安全的，如以前一样，你以计算序列长度开始①，如果它比最大的块尺寸小的话，就直接调用std::accumulate②，如果它的元素比块尺寸大的话，就找到重点③然后产生一个异步任务来处理前半部分，序号④范围内的第二部分就用一个直接的递归调用来处理⑤，然后将这两个块的结果累加⑥，库确保了std::async调用使用了可获得的硬件线程，并且没有创造很多线程，一些"异步"调用将在调用get()的时候被异步执行⑥

//这种做法的好处在于它不仅可以利用硬件并发，而且它也是异常安全的，如果递归调用抛出异常⑤，当异常传播时，调用std::async④创造的future就会被销毁，他会轮流等待异步线程结束，因此避免了悬挂线程，另一方面，如果异步调用抛出异常，就会被future捕捉，并且调用get()⑥将再次抛出异常

//当设计并发代码的时候还需要考虑哪些方面呢？让我们来看看可扩展性，如果将你的代码迁移到更多处理器系统中会提高多少性能呢？

#pragma clang diagnostic pop
