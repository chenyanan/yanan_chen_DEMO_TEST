//
//  5_13.cpp
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

//5.3.4 释放序列和synchronizes-with

//早在5.3.1节，我提到过你可以在对原子变量的载入，和来自另一个线程的对该原子变量的载入之间，建立一个synchronizes-with关系，即便是在存储和载入之间有一个读-修改-写顺序的操作存在的情况下，前提是所有的操作都做了适当的标记，现在既然我已经介绍了可能的内存顺序"标签"，我就可以详细解释这一点，如果存储被标记为memory_order_release,memory_order_acq_rel,memory_order_seq_cst，载入被标记为memory_order_consume,memory_order_acquire,memory_seq_cst，并且链条中的每个操作都载入由之前操作写入的值，那么，该操作链条就构成了一个释放序列，并且最初的存储与最终的载入是synchronizes-with的(例如memory_order_acquire或memory_order_seq_cst)或者是dependency-ordered-before的(例如memory_order_consume)，该链条中的所有原子的读-修改-写操作都可以具有任意的内存顺序(甚至是memory_order_relaxed)

//要了解这是什么意思以及它为什么很重要，考虑atomic<int>作为在一个共享的队列中的项目数量的count，如清单5.11所示

//清单5.11 使用原子操作从队列中读取值

#include <vector>
#include <atomic>
#include <thread>

std::vector<int> queue_data;
std::atomic<int> count;

static void wait_for_more_items() {}
static void process(int i) {}

static void populate_queue()
{
    const unsigned number_of_items = 20;
    queue_data.clear();
    for (unsigned i = 0; i < number_of_items; ++i)
        queue_data.push_back(i);
    count.store(number_of_items, std::memory_order_release);   //①最初的存储
}

static void consume_queue_items()
{
    while (true)
    {
        int item_index;
        if ((item_index = count.fetch_sub(1, std::memory_order_acquire)) <= 0)   //②一个读-修改-写操作
        {
            wait_for_more_items();   //③等待更多项目
            continue;
        }
        process(queue_data[item_index - 1]);   //读取queue_data是安全的
    }
}

int main()
{
    std::thread a(populate_queue);
    std::thread b(consume_queue_items);
    std::thread c(consume_queue_items);
    a.join();
    b.join();
    c.join();
}

//处理事情的方法之一是让产生数据的线程将项目存储在一个共享的缓冲区中，然后执行count.store(number_of_items,memory_order_release)①让其他线程直到数据可用，消耗队列项目的线程接着可能会执行count.fetch_sub(1, memory_order_acquire)②队列中索取一个项目，在实际读取共享缓冲区④之前，一旦count变为零，有没有更多的项目，线程就必须等待③

//如果只有一个消费者线程，这是良好的，fetch_sub()是一个具有memory_order_acquire语义的读取，并且存储具有memory_order_release语义，所以存储与载入同步，并且该线程可以从缓冲区里读取项目，如果又两个线程在读，第二个fetch_sub()将会看到由第一个写下的值，而非由store写下的值，若没有该释放序列的规则，第二个线程不会具有与第一个线程的happens-before关系，并且读取共享缓冲区也不是安全的，除非第一个fetch_sub()也具有memory_order_release语义，这会带来两个消费者线程之间不必要的同步，如果在fetch_sub操作上没有释放序列规则或是memory_order_release，就没有什么能要求对queue_data的存储对第二个消费者可见，你就会遇到数据竞争，幸运的是，第一个fetch_sub()的确参与了释放序列，并因此store()与第二个fetch_sub()同步，这两个消费者线程之间依然没有synchronizes-with关系，这如图5.7所示，图5.7中的虚线标识的是释放序列，实现表示的是happens-before关系

//在这一链条中，可以由任意数量的链接，但前提是它们都是类似fetch_sub()这样的读-修改-写操作，store()仍然与每一个句有memory_order_acquire标记的操作同步，在这个例子里，所有的链接都是一样的，都是获取操作，但它们可以由具有不同的内存顺序语义的不同操作组合而成

//尽管大多数的同步关系来自于应用到原子变量操作的内存顺序语义，但通过屏障(fence)来引入额外的顺序约束时可能的

// queue_data 填充
// count.store() 释放
//                      count.fetch_sub() 获取
//                      queue_data 处理
//                                                count.fetch_sub() 获取
//                                                queue_data 处理
// populate_queue       consume_queue_items       consume_queue_items

//图5.7 清单5.11中队列操作的释放序列

#pragma clang diagnostic pop
