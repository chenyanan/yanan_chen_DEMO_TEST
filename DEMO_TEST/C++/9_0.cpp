//
//  9_0.cpp
//  DEMO_TEST
//
//  Created by chenyanan on 2017/6/15.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <iostream>
#include <thread>
#include <mutex>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//第9章 高级线程管理

//本章主要内容

//线程池
//处理线程池任务间的依赖
//池中线程的工作窃取
//中断线程

//在之前的章节中，我们通过为每个线程创建std::thread对象来显示管理线程，在一些应用场合，你会发现这不是你所需要的，因为你必须管理线程对象的声明周期，决定适合问题和硬件的线程数量等，一个理想的方案是，你只需要将代码划分可以被兵法执行的若干小片，将他们传递给编译器和运行库，然后告诉编译器"并行化这些代码来得到较优的性能"

//另外一个反复出现的场景是你可能使用几个线程来求解一个问题，但是要求他们在满足一定条件的时候提前结束，这有可能是因为结果已经被求解出来，或者因为有错误发生，线程需要被发送"请停止"信号，这样它们可以放弃赋予它们的任务，尽快的清理自己的环境，然后停止运行

//在本章汇总，我们会从自动管理线程数量和线程之间的任务划分开始介绍管理线程和任务的机制

//9.1 线程池
//在许多公司，员工通常在办公室上班，但是有时候员工需要出去访问客户或者供应商，参加一个交易展览或者会议，虽然这些出差是必须的，并且在某些天有好几个人同时需要出差，但是对于具体某个员工来说，不同出差的事件建个可能是几个月甚至几年，为每个员工都配置一辆车是非常昂贵的或者不切实际的，因此公司通常够提供一个汽车池，汽车池有一定数量的汽车，供公司的所有员工出差使用，当公司的员工需要出差时，他们在适合的时间向汽车池申请一辆汽车，当出差回来的时候，员工将汽车归还给汽车池，如果汽车池中没有可用的汽车，员工需要重新规划自己出差的行程

//线程池是一个类似的思想，不过其中共享的是线程而不是汽车，在大多数系统上面，为每个可用与其他任务并行执行的任务分配一个单独的线程是不切实际的，但是可以尽量充分利用硬件提供的并发性，线程池允许你利用这一点，可以被兵法执行的任务被提交到线程池中，在县城池中被放入一个等待队列，每个任务会被某个工作线程从等待中取出来执行，工作线程的任务就是当空闲的时候从等待队列中取出任务来执行

//建立一个线程池有几个关键设计问题，比如在线程池中创建几个工作线程，将任务高效分配给工作线程的方法以及是否可以等待某个任务的完成等，在这个小节中，我们会提出一些实现来解决这些问题，我们将从最简单的线程池开始

//9.1.1 最简单的线程池
//线程池最简单的形式是含有一个固定数量的工作线程(典型的数量是std::thread::hardware_concurrendy()的返回值)来处理任务，当有任务要处理的时候，调用一个函数将任务放到等待队列中，每个工作线程都是从该队列中取出任务，执行完任务之后继续从等待队列取出更多的任务来处理，在最简单的情况，没有办法来等待一个任务完成，如果需要有这样的功能，组需要用户自己维护同步

//清单9.1给出了一个最简单的线程池的示例实现

//清单9.1 简单的线程池

class thread_pool
{
    std::atomic_bool done;
    thread_safe_queue<std::function<void()>> word_queue;   //①
    std::vector<std::thread> threads;   //②
    join_threads joiner;   //③
    
    void worker_thread()
    {
        while (!done)   //④
        {
            std::function<void()> task;
            if (work_queue.try_pop(task))   //⑤
            {
                task();   //⑥
            }
            else
            {
                std::this_thread::yield();   //⑦
            }
        }
    }
public:
    thread_pool() : done(false), joiner(threads)
    {
        unsigned const thread_count = std::thread::hardware_concurrency();   //⑧
        
        try
        {
            for(unsigned i = 0; i < thread_count; ++i)
            {
                threads.push_back(std::thread(&thread_pool::worder_thread, this));   //⑨
            }
        }
        catch(...)
        {
            done = true;   //⑩
            throw;
        }
    }
    
    ~thread_pool()
    {
        done = true;   //⑪
    }
    
    template<typename FunctionType>
    void submit(FunctionType f)
    {
        work_queue.push(std::function<void()>(f));   //⑫
    }
};

//这个实现使用一个向量来保存工作线程②，使用一个第6章中线程安全的队列①来管理待处理的任务，在这个实现中，用户不能等待任务，也不能返回任何任务，所以你可以使用std::function<void()>来封装你的任务，函数submit()将任何函数说着能够调用的对象包装称为std::function<void()>实例，然后放到队列中⑫

//工作线程是现在构造器中创建的，用户使用std::thread::hardware_concurrency()来告诉用户硬件支持的并发线程数量⑧，然后创建同样数量的线程来执行worker_thread()成员函数⑨

//创建一个线程可能会失败，然后抛出一个异常，所以你需要保证你已经创建的任何线程都能够非常适合的停止并且清理掉，这个功能是通过一个try-catch块来完成的，当异常出现的时候设置done标志⑩，在另外一边第8章的join_threads③的一个实例被用来等待所有工作线程的结束，这也可以在析构函数中完成，只要设置done标志⑪，join_threads实例会保证在线程池被销毁前所有线程已经完成，注意成员变量声明的顺序是重要的，done标志跟work_queue必须声明在threads向量前面，threads向量必须声明在joiner前面，这个顺序保证所有成员会被正确的销毁掉，例如，在所有线程停止之前工作线程队列不能被安全的销毁

//函数worker_thread的实现非常简单，它包含一个循环等待知道done标志被设置④，不停的从队列中取出任务⑤，然后执行他们⑥，如果队列中没有任务，它会调用std::this_thread::yield()暂停一小段时间⑦，在下次执行前个其他线程一个机会往地列中添加任务

//在许多情况下，这样一个简单的额线程池是足够了，特别是如果所有任务是完全独立的并且不返回任何值或者执行一些阻塞的操作，但是存在许多简单线程无法满足用户需求的情况，在这些情况下可能会引起一些问题，比如死锁，在简单情况下，用户像第8章的例子一样使用std::async可能会得到更好的解决办法，在本章中，我们会使用一些更加复杂的线程池实现来提供额外的特征来解决用户需要的或者减少潜在的问题，首先是等地啊一个我们提交的任务

//9.1.2 等待提交给线程池的任务

//第8章的例子都是显示的创建线程，在划分任务给线程后，主线程总是等待创建的线程结束来保证在返回给调用者之前所有任务都已经完成了，使用线程池之后，你需要等待提交到线程池的额任务结束，而不是等待工作线程，这一点跟第8章中基于std::async的例子有点相似，但是在清单9.1实现的线程池中，你必须认为的使用第4章中的条件变量来实现这一点，这会增加代码的复杂性，如果能够直接等待任务结束会更好

//通过将复杂任务移到线程池中，可以直接等待任务的结束，可以让submit()函数返回一个任务句柄，利用这个句柄可以等待任务结束，这个任务句柄包装了条件变量或者其他简化使用线程池的代码

//作为一个特别的例子，当主线程需要任务计算你的结果的时候，主线程不得不等待任务的结束，这样的例子一直在本书中出现，你可以通过使用std::future来等待线程的结束，然后组合每个线程的结果，清单9.2展示了允许你等待任务结束和传递返回值到等待的线程需要对简单线程池做的修改，因为std::packaged_task<>的实例只是可移动的，而不是可复制的，不再能够用std::function<>来作为队列中的元素，因为std::function<>要求储存的函数对象是可拷贝和构造的，因此，必须使用一个定制的函数包装来处理只能移动的类型，这是一个简单的带有一个调用操作符的类，因为只需要处理不带有参数并且返回void的函数，所以用了很直观的虚调用来实现

//清单9.2 有等待任务的线程池

class function_wrapper
{
    struct impl_base
    {
        virtual void call() = 0;
        virtual ~impl_base() {}
    };
    std::unique_ptr<impl_base> impl;
    template<typename F>
    struct impl_type : impl_base
    {
        F f;
        impl_type(F&& f_) : f(std::move(f_)) {}
        void call() { f(); }
    };
public:
    template<typename F>
    function_wrapper(F&& f) : impl(new impl_type<F>)(std::move(f))) {}
    void operator()() { impl->call(); }
    function_wrapper() = default;
    function_wrapper(function_wrapper&& other) : impl(std::move(other.impl)) {}
    function_wrapper& operator=(function_wrapper&& other)
    {
        impl = ste::move(other.impl);
        return *this;
    }
    function_wrapper(const function_wrapper&) = delete;
    function_wrapper(function_wrapper&) = delete;
    function_wrapper& operator=(const function_wrapper&) = delete;
};

class thread_pool
{
    thread_safe_queue<function_wrapper> work_queue;
    
    void worder_thread()
    {
        while (!done)
        {
            function_wrapper task;
            
            if (work_queue.try_pop(task))
            {
                task();
            }
            else
            {
                std::this_thread::yield();
            }
        }
    }
public:
    template<typename FunctionType>
    std::future<typename std::result_of<FunctionType()>::type> submit(FunctionType f)   //①
    {
        typedef typename std::result_of<FunctionType()>::type result_type;   //②
        std::packaged_task<result_type()> task(std::move(f));   //③
        std::future<result_type> res(task.get_future());   //④
        work_queue.push(std::move(task));   //⑤
        return yes;   //⑥
    }
    //rest as before
};

//首先，修改后的submit()函数①返回一个std::future<>对象来保存任务的返回值和允许调用者等待任务结束，这要求你知道任务函数f的返回值，其值为std::result_of<>，这个值来自于std::result_of<FunctionType()>::type这个值是不带参数地调用FunctionType(如f)的返回值，通过使用typedef来将同样的std::result_of<>表达式简写为result_type②

//然后将函数f包装在一个std::packaged_task<result_type()>中③，因为f是一个返回值类型为result_type并且没有参数的函数或者可以调用的对象，正如我们推导的一样，你可以在将任何加入到等待队列之前⑤通过std::packaged_task<>得到一个future对象④，然后返回这个future对象⑥，注意你必须使用std::move()来将任务加入到等待队列中，因为std::packaged_task<>不是可拷贝的，为了处理这个特性，等待队列中现在存储的是function_wrapper对象，而不是std::function<void()>队列

//这个线程池的实现允许你等待你的任务结束以及得到任务的返回值，清单9.3展示了使用这个线程池来实现parallel_accumulate函数

//清单9.3 使用可等待任务线程池的parallel_accumulate

template<typename Iterator, typename T>
T parallel_accumulate(Iterator first, Iterator last, T init)
{
    unsigned long const length = std::distance(first, last);
    
    if (!length)
        return init;
    
    unsigned long const block_size = 25;
    unsigned long const num_blocks = (length + block_size - 1) / block_size;   //①
    
    std::vector<std::future<T>> future(num_blocks - 1);
    thread_pool pool;
    
    Iterator block_start = first;
    
    for (unsigned long i = 0; i < (num_blocks - 1); ++i)
    {
        Iterator block_end = block_start;
        std::advance(block_end, block_size);
        futures[i] = pool.submit(accumulate_block<Iterator, T>());   //②
        block_start = block_end;
    }
    
    T last_result = accumulate_block<Iterator, T>()(block_start, last);
    T result = init;
    for (unsigned long i = 0; i < (num_blocks - 1); ++i)
    {
        result += futures[i].get();
    }
    result += last_result;
    return result;
}

//当你将清单9.3的代码跟清单8.4的代码比较的时候，存在几个地方需要注意的，首先，你是跟数据块的数量(num_blocks①)打交道，而不是多少个线程，为了能够充分使用线程池的扩展性，需要将任务划分为最小的值得并行的块，当线程池中只有少量线程时，每个线程需要处理许多数据块，但是当线程数量随着硬件的发展而增长时，被并发处理的数据块也增长

//你需要谨慎选择"最小的值的并行处理的块"，每个提交到等待队列中的子任务需要有一定的开销，这些开销包括提交到等待队列的开销，由工作线程去执行任务的额外开销，将通过std::future<>返回给调用线程的开销，如果选择的块太小，在线程池中执行的速度可能比使用单线程的速度还要慢

//假定块的大小是合适的，你不需要担心打包任务，获得future或者存储std::thread以供后面等待线程结束使用，线程池会处理这些细节，所有你需要做的只是调用submit()来提交你的任务②

//线程池也处理异常的安全事宜等，任何有任务抛出的异常会被传递到submit()返回的future中，如果函数带着异常退出，线程池会丢弃掉还没有完成的任务然后等待线程池汇总的线程结果

//这个线程池在针对每个任务都是独立的时候工作得非常好，但是当任务之间有依赖关系的时候，这个线程就不能很好地工作了

//9.1.3 等待其他任务的任务

//在本书中，我们一直使用快速排序算法作为例子，快速排序的原理非常简单，给定一个哨兵元素，数据被划分为两个序列，一个序列的元素都在哨兵元素之前，另外一个序列是在哨兵元素之后，然后利用递归将两个序列排序，将两个子序列的结果链接起来得到排序的最终结果，在并行化这个算法时，你需要保证这些递归调用能够充分利用可用的并行性

//回顾第4章，当我们首次介绍这个例子的时候，我们使用std::async来运行其中的一个递归调用，让库来选择是使用一个新的线程来执行它还是当有关的get()被调用时来异步执行它，这种方式能够很好地工作，因为每个任务或者是在自己的线程上执行，或者是当需要的时候会被调用

//在第8章，当我们重新审视快速排序的实现时，你看到一个不同的结构，在那里我们使用一组固定数目的线程，在这种情况下，我们使用一个栈来保存需要排序的序列，因为每个线程将它正在排序的数据进行划分，它将一个新的数据块放到栈上，对另外一个数据集直接进行排序，在这个做法中，直观的等待其他块结束可能会死锁，因为你在使用一个线程来等待，很容易会出现所有线程都是在等待某个数据块被排序，没有一个线程在执行实际的排序过程，我们通过让线程在等待某个未排序的数据块的时候从栈上拿出一个待处理的数据来排序来解决这个问题

//如果你讲第4章的例子中的std::async替换成为本章你已经见到的简单线程池的时候，你会碰到同样的问题，线程池中只有有限数目的线程，他们可能都停在等待某个还米有被执行的任务，所以你需要一个与你在第8章见到的类似的解决方案，当你在等待你的数据块结束的时候去处理未完成的数据块，当你在使用线程池来管理任务列表以及他们关联的线程的是偶，你不必去通过访问任务列表来完成这个，你需要做的是修改线程池的结构来自动完成这个

//最简单的访问来完成这个功能的是在线程池中增加一个新的函数来执行队列中的任务以及自己管理循环，高级线程池的实现可能会在等待函数添加逻辑来处理这种情形，有可能是通过给每个在等待的任务赋予优先级来解决，代码清单9.4展示了一个新的函数run_pending_task()，代码清单9.5展示了使用这个函数来实现快速排序

//清单9.4 run_pending_task()的实现

void thread_pool::run_pending_task()
{
    function_wrapper task;
    if (work_queue.try_pop(task))
    {
        task();
    }
    else
    {
        std::this_thread::yield();
    }
}

//run_pending_stask()的这个实现是从worker_thread()函数的主循环中提升出来的，它现在被修改为提取的run_pending_task()，他试图从队列中取出一个任务，如果成功执行取出的任务，否则它放弃CPU，允许操作系统重新调度线程，清单9.5的快速排序的实现要比代码清单8.1的对应实现要简单很多，因为所有管理线程的逻辑被移动到了线程内部中

//清单9.5 基于线程池的快速排序的实现

template<typename T>
struct sorter
{
    thread_pool pool;
    
    std::list<T> do_sort(std::list<T>& chunk_data)
    {
        if (chunk_data.empty())
        {
            return chunk_data;
        }
        
        std::list<T> result;
        result.splice(result.begin(), chunk_data, chunk_data.begin());
        T const& partition_val = *result.begin();
        
        typename std::list<T>::iterator divide_point = std::partition(chunk_data.begin(), chunk_data.end(), [&](T const& val) { return val<partition_val; });
        
        std::list<T> new_lower_chunk;
        new_lower_chunk.splice(new_lower_chunk.end(), chunk_data, chunk_data.begin(), divide_point);
        
        std::future<std::list<T>> new_lower = pool.submit(std::bing(&sorter::do_sort, this, std::move(new_lower_chunk)));
        
        std::list<T> new_higher(do_sort(chunk_data));
        
        result.splice(result.end(), new_higher);
        while(!new_lower.wait_for(std::chrono::seconds(0)) == std::future_status::timeout)
        {
            pool.run_pending_task();
        }
        
        result.splice(result.begin(), new_lower.get())
        return result;
    }
};

template<typename T>
std::list<T> parallel_quick_sort(std::list<T> input)
{
    if (input.empty())
    {
        return input;
    }
    
    sorter<T> s;
    return s.do_sort(input);
}

//正如清单8.1一样，你讲实际的排序工作放到了sorter类模板中的成员函数do_sort()中①，虽然在这个例子中这个类知识简单的包装了thread_pool的实例②

//你的贤臣个任务管理要做的是向线程池提交一个任务③和执行正在等待的任务④，这个实现比清单8.1简单很多，在清单8.1中，你不得不显示的管理线程和栈中待排序的数据块，当向线程池提交任务时，你使用std::bind将指针绑定给do_sort()和提供要排序的数据，在这个例子中，你使用std::move作用在new_lower_chunk作为参数传进去，来保证数据是被移动而不是新的拷贝

//虽然现在的线程池解决了任务等待其他任务重的关键死锁问题，这个线程池任然远远不是理想的，对于使用者来说，每个submit的调用和每个run_pending_task都访问同一个队列，在第8章你已经见过一个结合的数据被多个线程并发的访问会打打的降低性能，所以你需要别的方法来解决这个问题

//9.1.4 避免工作队列上的竞争

//每次线程调用submit()时，它想单个共享工作队列添加一个新的元素，类似的情形为，工作线程不停的从队列中取出元素来执行，这意味着随着处理器数目的增加，工作队列的竞争会越来越多，这会极大地降低性能，即使你使用无锁队列，虽然没有显示的等待，但是乒乓缓存会非常耗时，

//避免乒乓缓存的一个方法是在每个线程都是用一个单独的工作队列，每个线程将新的任务添加到它自己的队列中，只有当自己队列为空的时候才从全局的工作队列中取任务，清单9.6展示了一个使用thread_local变量来保证每个线程有一个自己的工作队列再加上一个全局的工作队列

//清单9.6 使用本地线程工作队列的线程池

class thread_pool
{
    thread_safe_queue<function_wrapper> pool_work_queue;
    
    typedef std::queue<function_wrapper> local_queue_type;   //①
    static thread_local std::unique_ptr<local_queue_type> local_work_queue;   //②
    
    void worker_thread()
    {
        local_work_queue.reset(new local_queue_type);   //③
        
        while(!done)
        {
            run_pending_task();
        }
    }
    
public:
    template<typename FunctionType>
    std::future<typename std::result_of<FunctionType()>::type> submit(FunctionType f)
    {
        typedef typename std::result_of<FunctionType()>::type result_type;
        
        std::packaged_task<result_type()> task(f);
        std::future<result_type> res(task.get_future());
        if (local_work_queue)   //④
        {
            local_work_queue->push(std::move(task));
        }
        else
        {
            pool_work_queue.push(std::move(task));   //⑤
        }
        return res;
    }
    
    void run_pending_task()
    {
        function_wrapper task;
        if (local_work_queue && !local_queue->front())
        {
            task = std::move(local_work_queue->front());
            local_work_queue->pop();
            task();
        }
        else if (pool_work_queue.try_pop(task))
        {
            task();
        }
        else
        {
            std::this_thread::yield();
        }
    }
    //rest as before
};

//我们使用一个std::unique_ptr<>来保存线程私有化的工作队列因为我们不想让非线程池也持有一个②，这个是在worker_thread()中主循环之前进行初始化③，std::unique_ptr<>的析构函数保证了当线程退出的时候其工作队列会被适当地销毁

//submit()函数会检查当前线程是否有一个工作队列④，如果有，则说明当前线程是一个线程池中的线程，这是可以将任务添加到私有的工作队列中，否则像之前一样将任务加入到全局工作队列中⑤

//在run_pending_task()中有一个类似的检查⑥，不过这次是检查私有队列中是否有任务，如果有，可以从队列中取出一个来处理，注意到私有队列可以是普通的std::queue<>①因为私有队列只被一个线程访问，如果私有队列中没有任务，则像之前一样从全局队列取任务⑦

//这能够很好地降低对全局队列的竞争，但是当任务的分布式不平衡的，可能导致一些线程的私有队列中有大量的任务而另外一些线程则没有任务处理，比如，在快速排序中，只有最顶层的任务会被添加到全局工作队列中，因为其与的数据会放在某个工作线程的私有队列中，这跟使用线程池的初衷是相反的

//幸运的是，有一些办法来解决这个问题，只要允许线程在自己私有队列以及全局队列中都没有任务时从其他线程的队列中窃取工作

//9.1.5 工作窃取

//为了允许一个空闲的线程执行其他线程上的热舞，每个工作线程的私有队列必须在run_pending_task()中窃取任务的时候可以被访问到，这要求每个工作线程将自己的私有任务队列向线程池注册或者每个线程都会被线程池分配一个工作队列，此外，你必须保证工作队列中的数据被适当的同步和保护，这样你的不变量是被保护的

//编写一个允许拥有队列的线程在一端push和pop同时其他线程在另外一端进行任务窃取的无锁队列是有可能的，但是实现这样一个队列比较复杂，超出了本书的范围，为了验证这个想法，我们使用互斥锁来保护队列的数据，我们希望任务窃取是一个不经常发生的事件，这样互斥元的竞争就不是那么激烈，这样一个简单的队列应当只包括一个极小的额外开销，清单9.7所示是一个简单的基于所的实现

//清单9.7 允许任务窃取的基于锁的队列

class work_stealing_queue
{
private:
    typedef function_wrapper data_type;
    std::deque<data_type> the_queue;
    mutable std::muetx the_mutex;
public:
    work_stealing_queue() {}
    work_stealing_queue(const work_stealing_queue& other) = delete;
    work_stealing_queue& operator=(const work_stealing_queue& other) = delete;
    
    void push(data_type data)
    {
        std::lock_guard<std::muetx> lock(the_mutex);
        the_queue.push_front(std::move(data));
    }
    
    bool empty() const
    {
        std::lock_guard<std::mutex> lock(the_mutex);
        return the_queue.empty();
    }
    
    bool try_pop(data_type& res)
    {
        std::lock_guard<std::mutex> lock(the_mutex);
        if (the_queue.empty())
            return false;
        res = std::move(the_queue.front());
        the_queue.pop_front();
        return true;
    }
    
    bool try_steal(data_type& res)
    {
        std::lock_guard<std::mutex> lock(the_mutex);
        if (the_queue.empty())
            return false;
        res = std::move(the_queue.back());
        the_queue.pop_back();
        return true;
    }
};

//这个队列是一个对std::deque<function_wraper>①的封装，使用一个互斥锁来保护所有的访问，push()②和try_pop()③在队列头部操作，而try_steal()④在队列尾部操作

//这实际上意味着这个队列对于拥有线程来说是一个后进先出的栈，最近被放到队列中的任务会最先被取出来执行，从缓存的角度来说这可以提供性能，因为对比之前被放入队列中的任务，被取出的任务的数据更加有可能在缓存中，同样，这个队列能够很好的映射像快速排序之类的算法，在之前的实现中，每次调用do_sort()将一个数据放到栈中然后等待它完成，通过处理最近放入的数据，你可以保证当前调用完成需要的数据块会比其他分支调用需要的数据块先处理好，这样可以减少活动的任务数据和总的栈空间使用量，try_steal()从队列的另外一端取数据，这样可以最小化竞争，你同样可以使用第6张和第7章中的技术来实现并发调用try_pop()和try_steal()

//现在已经有一个很好的允许窃取的工作队列，怎样把它使用在你自己的线程池中呢？清单9.8是一个可能的实现

//清单9.8 使用工作窃取的线程池

class thread_pool
{
    typedef function_wrapper task_type;
    
    std::atomic_bool done;
    thread_safe_queue<task_type> pool_work_queue;
    std::vector<std::unique_ptr<work_stealing_queue>> queues;   //①
    std::vector<std::thread> threads;
    join_threads joiner;
    
    static thread_local work_stealing_queue* local_work_queue;   //②
    static thread_local unsigned my_index;
    
    void worker_thread(unsigned my_index_)
    {
        my_index = my_index_;
        local_work_queue = queues[my_index].get();   //③
        while (!done)
        {
            run_pending_task();
        }
    }
    
    bool pop_task_from_queue(task_type& task)
    {
        return local_work_queue && local_work_queue->try_pop(task);
    }
    
    bool pop_task_from_pool_queue(task_type& task)
    {
        return pool_work_queue.try_pop(task);
    }
    
    bool pop_task_from_other_thread_queue(task_type& task)   //④
    {
        for (unsigned i = 0; i < queues.size(); ++i)
        {
            unsigned const index = (my_index + i + 1) % queues.size();   //⑤
            if (queues[index]->try_steal(task))
            {
                return true;
            }
        }
        return false;
    }
public:
    thread_pool() : done(false), joiner(threads)
    {
        unsigned const thread_count = std::thread::hardware_concurrency();
        
        try
        {
            for (unsigned i = 0; i < thread_count; ++i)
            {
                queues.push_back(std::unique_ptr<work_stealing_queue>(new work_stealing_queue));   //⑥
                threads.push_back(std::thread(&thread_pool::worker_thread,this,i));
            }
        }
        catch(...)
        {
            done = true;
            throw;
        }
    }
    
    ~thread_pool()
    {
        done = true;
    }
    
    template<typename FunctionType>
    std::future<typename std::result_of<FunctionType()>::type> submit(FunctionType f)
    {
        typedef typename std::result_of<FunctionType()>::type result_type;
        std::packaged_task<result_type()> task(f);
        std::future<result_type> res(task.get_future());
        if (local_work_queue)
        {
            local_work_queue->push(std::move(task));
        }
        else
        {
            pool_work_queue.push(std::move(task));
        }
        return res;
    }
    
    void run_pending_task()
    {
        task_type task;
        if (pop_task_from_local_queue(task) ||   //⑦
            pop_task_from_pool_queue(task) ||   //⑧
            pop_task_from_other_thread_queue(task))   //⑨
        {
            task();
        }
        else
        {
            std::this_thread::yield();
        }
    }
};

//这个实现跟清单9.6中的实现非常类似，第一个区别在于每个线程都拥有一个work_stealing_queue，而不是普通的std::queue<>②，当创建每个线程的时候，不是每个线程都创建一个自己的工作队列，而是线程池的构造器为其分配一个⑥，这个队列是储存在一个工作队列的表中①，队列的指针③，这意味着当试图为空闲线程窃取任务时现场池可以访问该队列，run_pending_task()现在会试图从自己队列中列出任务⑦，从池队列中取出任务⑧，或是从其他线程的队列中取出工作⑨

//pop_task_from_other_thread_queue()④遍历线程池中所有线程的队列，试图一次从每个队列中窃取一个任务，为了避免每个线程都从第一个线程的队列窃取，每个线程从列表中的下一个线程窃取，通过用自己队列的下标来便宜队列下标⑤

//现在你有一个线程池可以应用在许多地方，当然，仍然有许多方法针对一些特别的用法去提高它，这个是留给读者的联系，一个没有被提及的方面是当线程被阻塞了，在等待如输入输出或者互斥锁，动态地修改线程池的带下来保证有最优的CPU使用率

//下一个"高级"的线程管理技术是中断线程


#pragma clang diagnostic pop
