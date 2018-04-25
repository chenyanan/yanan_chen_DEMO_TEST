//
//  5_11.cpp
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

//6. 获取-释放顺序传递性同步
//为了考虑传递性顺序，你需要至少三个线程，第一个线程修改一些共享变量，并对其中一个进行存储-释放，第二个线程接着对应于存储-释放，使用载入-获取来读取该变量，并且在第二个共享变量执行存储-释放，最后，第三个线程在第二个共享变量上进行载入-获取，在载入-获取操作看到存储-释放操作所写入的值以确保synchronizes-with关系的前提下，第三个线程可以读取由第一个线程存储的其他变量的值，即使中间线程诶呦动其中的任何一个，该情景展示于清单5.9中

//清单5.9 使用获取和释放顺序的传递性同步

#include "assert.h"

std::atomic<int> data[5];
std::atomic<bool> sync1(false), sync2(false);

static void thread_1()
{
    data[0].store(42, std::memory_order_relaxed);
    data[1].store(97, std::memory_order_relaxed);
    data[2].store(17, std::memory_order_relaxed);
    data[3].store(-141, std::memory_order_relaxed);
    data[4].store(2003, std::memory_order_release);
    sync1.store(true, std::memory_order_release);   //①
}

static void thread_2()
{
    while (!sync1.load(std::memory_order_acquire));   //②
    sync2.store(true, std::memory_order_release);   //③
}

static void thread_3()
{
    while (!sync2.load(std::memory_order_acquire));   //④
    assert(data[0].load(std::memory_order_relaxed) == 42);
    assert(data[1].load(std::memory_order_relaxed) == 97);
    assert(data[2].load(std::memory_order_relaxed) == 17);
    assert(data[3].load(std::memory_order_relaxed) == -141);
    assert(data[4].load(std::memory_order_relaxed) == 2003);
}

//即使thread_2只动了变量sync1和sync2，也足以同步thread_1②和thread_3③以确保assert不会触发，首先从thread_1存储data发生于对sync1的存储①之前，因为他们在同一个线程里是sequenced-before关系，因为sync1的载入①在while循环中，它最终会看到thread_1中存储的值，并且因此形成释放-获取对的另一半，因此，对sync1的存储发生于sync1在while循环中的最后载入之前，该载入的顺序在sync2③之前(因而也是happens-before的)，与thread_3中while循环④构成了一对释放-获取，对sync2的存储③也就因而发生在载入④之前，又发生在从data载入之前，因为happens-before的传递性质，你可以将它们串起来:对data的存储发生在对sync1的存储①之前，又发生在从sync1的载入②之前，又发生在对sync2的存储③之前，又发生在从sync2的载入④之前，又发生在data的载入之前，因此thread_1中对data的存储发生在thread_3中从data载入之前，并且asserts不会触发

//在这个例子中，你通过使用thread_2中个一个具有memory_order_acq_rel的读-修改-写操作，将sync1和sync2合并为一个变量，有一个选项是使用compare_exchange_strong()，以确保该值只有当thread_1中的存储值可见时才能被更新

std::atomic<int> sync(0);
static void another_thread_1()
{
    //...
    sync.store(1, std::memory_order_release);
}

static void another_thread_2()
{
    int expected = 1;
    while (!sync.compare_exchange_strong(expected, 2, std::memory_order_acq_rel))
        expected = 1;
}

static void another_thread_3()
{
    while (sync.load(std::memory_order_acquire) < 2);
    //...
}

//如果你使用的读-修改-写操作，挑选你所希望的语义是很重要的，在这种情况下，你同事需要获取和释放语义，因此memory_order_acq_rel是合适的，但是你也可以使用其他的顺序，基友memory_order_acquire语义的fetch_sub操作不与任何东西同步，即便是它存储一个值，因为它并不是一个释放操作，同样地，一次存储无法与具有memory_order_release语义的fetch_or操作同步，因为fetch_or的读取部分并不是一个获取操作，具有memory_order_acq_rel语义的读-修改-写操作表现得既像获取又像释放，所以之前的一次存储可以与这样的操作同步，并且它也可以与之后的而一次载入同步，就像这个例子中的情况一样

//如果你将获取-释放操作一致混合起来，顺序一致载入表现的像是获取语义的载入，并且顺序一致存储表现的像是释放语义的存储，顺序一致的的读-修改-写操作表现得既像获取又像释放操作，松散操作仍然hi松散的，但是会受到额外的synchronizes-with和随之而来的happens-before关系的约束，这是由于获取-释放语义的使用而引入的

//暂且不管可能存在的不直观的后果，任何使用锁的人都不得不面对相同的顺序问题，锁定互斥元时一个获取操作，而解锁互斥元是一个释放操作，对于互斥元，你了解到必须确保当你读取一个值得时候，锁定与你写它时曾锁定的相同的互斥元，你的获取和释放操作必须是对同一个变量以确保顺序，如果数据收到互斥元的保护，锁的独占特性意味着结果与令它的锁定与解锁称为顺序一致操作所得到结果没有区别，类似地，如果你在原子变量上使用获取与释放顺序建立一个简单的锁，那么从使用该锁的代码的角度来看，其行为会表现为顺序一致，即便内部的操作并非这样

//如果你对你的原子操作不需要顺序一致顺序的严格性，获取-释放顺序的配对同步可能会比顺序一致性操作所要求的全局顺序有着低得多的同步成本，这里需要权衡的是确保该顺序能够正确工作，以及线程间不直观的行为不会出问题所需求的脑力成本

#pragma clang diagnostic pop





