//
//  8_5.cpp
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

//1.增加异常安全性

//好了，我们识别出了所有可能抛出异常的地方以及异常所造成的不好影响，那么如何处理它呢？我们先来解决在新线程上抛出异常的问题

//在第4章中介绍了完成此工作的工具，如果你仔细考虑在新线程中想获得什么，那么很明显当允许代码抛出异常的时候，你试图计算结果来返回，std::packaged_task和std::future的组合设计是恰好的，如果你重新设计代码来使用std::packaged_task就以清单8.3中的代码结束

//清单8.3 使用std::packaged_task的std:accumulate的并行版本

template<typename Iterator, typename T>
struct accumulate_block
{
    T operator() (Iterator first, Iterator last)   //①
    {
        return std::accumulate(first, last, T());   //②
    }
};

template<typename Iterator, typename T>
T parallel_accumulate(Iterator first, Iterator last, T init)
{
    unsigned long const length = std::distance(first, last);
    
    if (!length)
        return init;
    
    unsigned long const min_per_thread = 25;
    unsigned long const max_threads = (length+min_per_thread-1)/min_per_thread;
    unsigned long const hardware_threads = std::thread::hardware_concurrency();
    unsigned long const num_threads = std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);
    unsigned long const block_size = length/num_threads;
    
    std::vector<std::future<T>> futures(num_threads-1);   //③
    std::vector<std::thread> threads(num_threads-1);
    
    Iterator block_start=first;
    for (unsigned long i = 0; i < (num_threads-1); ++i)
    {
        Iterator block_end=block_start;
        std::advance(block_end,block_size);
        std::packaged_task<T(Iterator,Iterator)> task(accumulate_block<Iterator,T>());   //④
        futures[i] = task.get_future();   //⑤
        threads[i] = std::thread(std::move(task), block_start, block_end);   //⑥
        block_start = block_end;
    }
    
    T last_result = accumulate_block()(block_start, last);   //⑦
    
    std::for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));
    
    T result = init;   //⑧
    
    for (unsigned long i = 0; i < (num_threads - 1); ++i)
    {
        result += futures[i].get();   //⑨
    }
    result += last_result;   //⑩
    return result;
}

//第一个改变就是，函数调用accumulate_block操作直节返回结果，而不是返回储存地址的引用①，你使用std::packaged_task和std::future来保证异常安全，因此你也可以使用它来移动结果，这就需要你调用std::accumulate②明确使用默认构造函数T而不是重新使用提供的result值，不过这只是一个小小的改变

//下一个改变就是你用futures向量③，而不是用结果为每个生成的线程储存一个std::future<T>，在生成线程的循环中，你首先为accumulate_block创造一个任务④，std::packaged_task<T(Iterator,Iterator)>声明了有两个Iterator并且返回一个T的任务，这就是你的函数所做的，人后你将得到任务的future⑤，并且在一个新的线程上运行这个任务，输入要处理的块的起点和终点，当运行任务的时候，将在future中捕捉结果，也会捕捉任何抛出的异常

//既然你已经使用了future，就不再有结果数组了，因此必须将最后一块的记过储存在一个变量中⑦而不是储存在数组的一个位置中，同样，因为你将从future中得到值，使用基本的for循环比使用std::accumulate要简单，已提供的初始值开始⑧，并且将每个future的结果累加起来⑨，如果响应的任务抛出异常，就会在future中捕捉到并且调用get()时会再次抛出异常，最后，在返回总的记过给调用者之前要叫上最后一个块的结果⑩

//因此，这就去除了一个可能的问题，工作线程中抛出的异常会在主线程汇中再次被抛出，如果多于一个工作线程抛出异常，只有一个异常会被传播，但是这也不是一个大问题，如果确实有关的话，可以使用类似std::nested_exception来捕捉所有的异常然后抛出它

//如果在你产生第一个线程和你加入它们之间抛出异常的话，那么剩下的问题就是线程泄漏，最简单的方法就是捕获所有异常，并且将它们融合到调用joinable()的线程中，然后再次抛出异常

try
{
    for(unsigned long i = 0; i < (num_threads-1); ++i)
    {
        //... as before
    }
    T last_result = accumulate_block()(block_start,last);
    
    std::for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));
}
catch(...)
{
    for (unsigned long i = 0; i < (num_thread - 1); ++i)
    {
        if (threads[i].joinable())
            threads[i].join();
    }
    throw;
}

//现在它起作用了，所有县城将被联合起来，无论代码是如何离开块的，尽管如此，try-catch块是令人讨厌的，并且你有复制代码，你将加入正常的控制流以及捕获块的线程中，复制代码不是一个好事情，因为这意味着需要改变更多的地方，我们在一个对象的析构函数中检查它，毕竟，这是C++中被惯用的清除资源的方法，下面是你的类

class join_threads
{
    std::vector<std::thread>& threads;
public:
    explicit join_threads(std::vector<std::thread>& threads_) : threads(threads_) {}
    ~join_threads()
    {
        for (unsigned long i = 0; i < threads.size(); ++i)
        {
            if (threads[i].joinable())
                threads[i].join();
        }
    }
};

//这与清单2.3中的thread_guard类是相似的，除了它扩展为适合所有线程，你可以如清单8.4所示简化代码

//清单8.4 std::accumulate的异常安全并行版本

template<typename Iterator, typename T>
T parallel_accumulate(Iterator first, Iterator last, T init)
{
    unsigned long const length = std::distance(first, last);
    
    if (!length)
        return init;
    
    unsigned long const min_per_thread = 25;
    unsigned long const max_threads = (length + min_per_thread - 1) / min_per_thread;
    unsigned long const hardware_threads = std::thread::hardware_concurrency();
    unsigned long const num_threads = std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);
    unsigned long const block_size = length / num_threads;
    
    std::vector<std::future<T>> futures(num_threads-1);
    std::vector<std::thread> threads(num_threads-1);
    join_threads joiner(threads);   //①
    
    Iterator block_start = first;
    for (unsigned long i = 0; i < (num_threads - 1); ++i)
    {
        Iterator block_end = block_start;
        std::advance(block_end, block_size);
        std::packaged_task<T(Iterator,Iterator)>  task(accumulate_block<Iterator, T>());
        futures[i] = task.get_future();
        
        threads[i] = std::thread(std::move(task), block_start, block_end);
        block_start = block_end;
    }
    
    T last_result = accumulate_block()(block_start,last);
    T result = init;
    for (unsigned long i = 0; i < (num_threads - 1); ++i)
    {
        result += futures[i].get();   //②
    }
    result += last_result;
    return result;
}

//一旦你创建了你的线程容器，也就创建了一个新类的实例①来加入所有在退出的线程，你可以去除你的联合循环，只要你知道无论函数是否退出，这些县城都将被联合起来，注意调用futures[i].get()②将被阻塞直到结果出来，因此在这一点并不需要明确地与线程融合起来，这与清单8.2中的圆形不一样，在清单8.2中你必须与线程联合起来确保正确复制了结果向量，你不仅得到了异常安全的代码，而且你的函数也更短了，因为将联合代码提取到你的新(可再用的)类中了

#pragma clang diagnostic pop
