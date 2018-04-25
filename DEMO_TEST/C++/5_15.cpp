//
//  5_15.cpp
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

//5.3.6 用原子操作排序非原子操作

//如果你将清单5.12中的x替换为一个普通的非原子bool值(如清单5.13所示)，该行为是确保相同的

//清单5.13 在非原子操作上强制顺序

#include <atomic>
#include <thread>
#include "assert.h"

bool x = false;   //x现在是一个普通的非原子变量
std::atomic<bool> y;
std::atomic<int> z;

static void write_x_then_y()
{
    x = true;   //①在屏障前存储x
    std::atomic_thread_fence(std::memory_order_release);
    y.store(true, std::memory_order_relaxed);   //②在屏障后存储y
}

static void read_y_then_x()
{
    while(!y.load(std::memory_order_relaxed));   //③等待到你看见来自#2的写入
    std::atomic_thread_fence(std::memory_order_acquire);
    if (x)   //④将读取#1写入的值
        ++z;
}

int main()
{
    x = false;
    y = false;
    z = 0;
    std::thread a(write_x_then_y);
    std::thread b(read_y_then_x);
    a.join();
    b.join();
    assert(z.load() != 0);   //⑤此断言不会触发
}

//屏障仍然为对x的存储①，对y的存储②，从y的载入③和从x的载入④提供了强制顺序，并且在对x的存储和从x的载入之间仍然有happens-before关系，所以断言⑤还是不会触发，存放②和载入y③仍然必须是原子的，否则，在y上就会有数据竞争，但是屏障在x的操作上强制了顺序，一旦读线程看见了已存储的y值，这个强制顺序意味着x上没有数据竞争，即使它被一个线程修改并且被另一个线程读取

//并不是只有屏障才能排序非原子操作，你之前在清单5.10中看到的用memory_order_release,memory_order_consume对偶来排序对动态分配对象的访问，以及本章中的许多例子，都可以通过将其中一些memory_order_relaxed操作替换为普通的非原子操作来进行重写

//通过使用原子操作来排序非原子操作，是hanppens-before中的sequenced-before部分变得如此重要的所在，如果一个非原子操作的顺序在一个原子操作之前，同时该原子操作发生于另一个线程中的操作之前，这个非原子操作同样发生在另一个线程的该操作之前，这就是清单5.13中x上的操作顺序的来历，也是清单5.2中的示例可以工作的原因，这也是C++标准库中更高级别的同步功能的基础，比如互斥元和条件变量，想看一看这是如何工作的，考虑一下清单5.1中的简单的自旋锁互斥元

//lock()操作是在flat.test_and_set()上的循环，使用的是std::memory_order_acquire顺序，而unlock()是对flag.clear()的调用，用的是std::memory_order_release顺序，当第一个线程调用lock()时，标志被初始化为清除，所以第一次调用test_and_set()将设置标志并返回false，表明该线程现在拥有了锁，并且终止循环，线程接下来就可以自由地修改被互斥元保护的任何数据，此时所有其他调用lock()的线程会发现标志已经被设置，而且会被阻塞在test_and_set()循环中

//当持有锁的线程已完成修改受保护的数据时，它会调用unlock()，继而调用具有std::memory_order_release语义的flag.clear()，然后他会与后续的来自另一个线程上lock()的调用中的对flag.test_and_set()的调用同步，因为该调用具有std::memory_order_acquire语义，由于对受波阿虎数据的修改必然顺序在unlock()之前，于是也就发生在后续的来自于第二个线程的lock()之前(因为unlick()和lock()之间的synchronizes-with关系),并且发生在来自于第二个线程的对数据的任意访问之前，一旦第二个线程获取了锁

//虽然其他的互斥元的实现会具有不同的内部操作，但其基本原理是相同的，lock()是在一个内部内存地址上的获取操作，unlock()是在相同内存地址上的释放操作

#pragma clang diagnostic pop
