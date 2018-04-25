//
//  8_4.cpp
//  123
//
//  Created by chenyanan on 2017/5/15.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <iostream>
#include <thread>
#include <mutex>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//8.4 为并发设计时的额外考虑

//本章我们看了一些在线程间划分工作的方法，影响性能的因素，以及这些因素是如何影响你选择哪种数据读取模式和数据结构的，但是，设计并发代码需要考虑更多，你需要考虑的事情例如异常安全以及可扩展性，如果当系统中处理核心增加时性能(无论是从减少执行时间还是从增加吞吐量方面来说)也增加的话，那么代码就是可扩展的，从理论上说，性能增加是线性的，因此一个有100个处理器的系统的性能比只有一个处理器的系统好100倍

//即使代码不是可扩展的，他也是可以工作的，例如，单线程应用不是可扩展的，异常安全是与正确性有关的，如果你的代码不是异常安全的，就可能会以破碎的不变量或者竞争条件计数，或者你的应用可能因为一个操作抛出异常而突然终止，考虑到这些，我们将首先考虑异常安全

//8.4.1 并行算法中的异常安全

//异常安全是好的C++代码的一个基本方面，使用并发性的代码也不例外，实际上，并行算法通常比普通线性算法需要你考虑更多异常方面的问题，如果线性算法中的操作抛出异常，该算法只要考虑确保它能够处理好以避免资源泄漏和破碎的不变量，它可以允许扩大异常给调用者来处理，相比之下，在并行算法中，很多操作在不同的线程上运行，在这种情况下，就不允许扩大异常了，因为它在错误的调用栈中，如果一个函数大量产生一异常结束的新线程，那么该应用就会被终止

//作为一个具体的例子，我们来回顾清单2.8中的parallel_accumulate函数，清单8.2中会做一些修改

//清单8.2 std::accumulate的并行版本(来自清单2.8)

#include <numeric>

template<typename Iterator, typename T>
struct accumulate_block
{
    void operator() (Iterator first, Iterator last, T& result)
    {
        result = std::accumulate(first, last, result);   //①
    }
};

template<typename Iterator, typename T>
T parallel_accumulate(Iterator first, Iterator last, T init)
{
    unsigned long const length = std::distance(first, last);   //②
    
    if (!length)
        return init;
    
    unsigned long const min_per_thread = 25;
    unsigned long const max_threads = (length + min_per_thread-1) / min_per_thread;
    unsigned long const hardware_threads = std::thread::hardware_concurrency();
    unsigned long const num_threads = std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);
    unsigned long const block_size = length / num_threads;
    
    std::vector<T> results(num_threads);   //③
    std::vector<std::thread> threads(num_threads - 1);   //④
    
    Iterator block_start = first;   //⑤
    for (unsigned long i = 0; i < (num_threads - 1); ++i)
    {
        Iterator block_end = block_start;   //⑥
        std::advance(block_end, block_size);
        threads[i] = std::thread(accumulate_block<Iterator, T>(), block_start, block_end, std::ref(results[i]));   //⑦
        block_start = block_end;   //⑧
    }
    accumulate_block()(block_start, last, results[num_threads - 1]);   //⑨
    
    std::for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));
    
    return std::accumulate(results.begin(), results.end(), init);   //⑩
}

//现在我们检查并且确定抛出异常的位置:总的来说，任何调用函数的地方或者在用户定义的类型上执行操作的地方都可能抛出异常

//首先，你调用distance②，它在用户定义的迭代器类型上执行操作，因为你还没有进行任何工作，并且是这是在调用线程上，所以这是没问题的，下一步，你分配了results迭代器③和threads迭代器④，同样，这是在调用线程上，并且你没有做任何工作或者产生任何线程，因此这是没问题的，当然，如果threads构造函数抛出异常，那么就必须清楚为results分配内存，析构函数将为你完成它

//跳过block_start的初始化⑤因为这是安全的，就到了产生线程的循环中的操作⑥、⑦、⑧，一旦在⑦中创造了第一个线程，如果抛出异常的话就会很麻烦，你的新std::thread对象的析构函数会调用std::terminate然后中止程序

//调用accumulate_block⑨也可能会抛出异常，你的线程对象将被销毁并且调用std::terminate另一方面，最后调用std::acumulate⑩的时候也可能抛出异常并且不导致任何困难，因为所有线程将在此处汇合

//这不是对于主线程来说的，但是也可能抛出异常，在新线程上调用accumulate_block可能抛出异常①，这里没有任何catch块，因此该异常将被稍后处理并且导致库调用std::terminate()来终止程序

//即使不是显而易见的，这段代码也不是异常安全的



#pragma clang diagnostic pop

