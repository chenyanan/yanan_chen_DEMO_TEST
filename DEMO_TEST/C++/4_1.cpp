//
//  4_1.cpp
//  123
//
//  Created by chenyanan on 2017/4/26.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//4.1.1 用条件变量等待条件
//标准C++库提供了两个条件变量的实现,std::condition_variable和std::condition_variable_any，这两个实现都在<condition_variable>库的头文件中声明，两者都需要和互斥元一起工作，以便提供恰当的同步，前者仅限于和std::mutex一起工作，而后者则可以与符合称为类似互斥元的最低标准的任何东西一起工作，因此以any为后缀，因为std::condition_variable_any更加普遍，所以会有大小,性能后者操作系统资源方面的形式的额外代价的可能，因此应该首选std::condition_variable，除非需要额外的灵活性

//那么，如何使用std::condition_variable去处理引言中的例子，怎么让正在等待工作的线程休眠，直到有数据要处理呢，清单4.1展示了一种方法，你可以用条件变量来实现这一点

//清单4.1 使用std::condition_variable等待数据

struct data_chunk {};

static void process(const data_chunk& data) {}
static bool more_data_to_prepare() { return true; }
static bool is_last_chunk(const data_chunk& data) { return true; }
static data_chunk prepare_data() { return data_chunk(); }

static std::mutex mut;
static std::queue<data_chunk> data_queue;   //①
static std::condition_variable data_cond;

static void data_preparation_thread()
{
    while (more_data_to_prepare())
    {
        const data_chunk data = prepare_data();
        std::lock_guard<std::mutex> lk(mut);
        data_queue.push(data);   //②
        data_cond.notify_one();   //③
    }
}

static void data_processing_thread()
{
    while (true)
    {
        std::unique_lock<std::mutex> lk(mut);   //④
        
        data_cond.wait(lk, []{ return !data_queue.empty(); });   //⑤
        data_chunk data = data_queue.front();
        data_queue.pop();
        lk.unlock();   //⑥
        process(data);
        if (is_last_chunk(data))
            break;
    }
}

//首先，你拥有一个用来在两个线程之间传递数据的队列①，当数据就绪时，准备数据的线程使用std::lock_guard去锁定保护队列的互斥元，并且将数据压入队列中②，然后，它在std::condition_variable的实例上调用notify_one()成员函数，已通知等待中的线程(如果有的话)③

//在另外一侧，你还有处理线程，该线程首先锁定互斥元，但是这次使用的是std::unique_lock而不是std::lock_guard④，你很快就会明白为什么，该线程接下来在std::condition_variable上调用wait()，传入锁对象以及标识正在等待的条件的lambda函数⑤，lambda函数是C++中的一个新功能，它允许你编写一个匿名函数作为另一个表达式的一部分，它们非常适用于为类似于wait()这样的标准库函数指定断言，在这个例子中，简单的lambda函数[]{ return !data_queue.empty(); }检查data_queue是否不为empty(),即队列中已有数据准备处理，附录A，A.5节更加详细的描述了lambda函数

//wait()的实现接下来检查条件(通过调用所提供的lambda函数),并在满足时返回(lambda函数返回true),如果条件不满足(lambda函数返回false)，wait()解锁互斥元，并将该线程置于阻塞或等待状态，当来自数据准备线程中对notify_one()的调用通知条件变量时，线程从睡眠状态中苏醒(接触其阻塞)，重新获得互斥元上的锁，并再次检查条件，如果条件已经满足，就从wait()返回值，互斥元仍被锁定，如果条件不满足，该线程解锁互斥元，并恢复等待，这就是为什么需要std::unique_lock而不是std::lock_guard，等待中的线程在等待期间必须解锁互斥元，并在这之后重新将其锁定，而std::lock_guard没有提供这样的灵活性，如果互斥元在线程休眠期间始终被锁定，数据准备线程将无法锁定该互斥元，以便将项目添加至队列，并且等待中的线程将永远复发看到其条件得到满足

//清单4.1为等待使用了一个简单的lambda函数⑤，该函数检查队列是否非空，但是任何函数或可调用对象都可以传入，如果你已经有一个函数来检查条件(也许因为它比这样一个简单的实验更加复杂)，哪儿这个函数就可以直接传入，没有必要将其封装在lambda中，在对wait()的调用中，条件变量可能会对所提供的条件检查任意多次，然而，它总是在互斥元被锁定的情况下这样做，并且当(且仅当）用来测试条件的函数返回true,它机会立即返回，当等待线程重新获取互斥元并检查条件时，如果它并非直接影响另一个线程的通知，这就是所谓的伪唤醒，由于所有的这种伪唤醒的次数和平路根据定义是不确定的，如果你这样做，就必须左好多此产生副作用的准备

//解锁std::unique_lock的灵活性不仅适用于wait()的调用，它还可用于你有待处理但仍未处理的数据⑥，处理数据可能是一个耗时的操作，并且如你在第三章中看到的，在互斥元上持有锁超过所需的时间就是个不好的情况

//清单4.1所示的使用队列在线程之间传输数据，是很常见的场景，做得好的话，同步可以被限制在队列本省，大大减少了同步问题和竞争条件大概的数量，鉴于此，现在让我们从清单4.1中提取一个泛型的线程安全队列



#pragma clang diagnostic pop
