//
//  5_12.cpp
//  123
//
//  Created by chenyanan on 2017/5/5.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <iostream>
#include <thread>
#include <mutex>
#include "assert.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//7. 使用获取-释放顺序和MEMORY_ORDER_CONSUME的数据依赖
//在本节的绪论中我提到了memory_order_consume是获取-释放顺序模型的一部分，但前面的描述中它很明显的缺失了，这是因为memory_order_consume是很特别的，它全是数据依赖，它引入了5.3.2节中提到的线程间happens-before关系有着细微差别的数据依赖

//有两个数据依赖的新的关系:依赖顺序在其之前(dependency-ordered-before)和带有对其的依赖(carries-a-dependency-to)，与sequenced-before相似，carries-a-dependency-to严格适用于单个线程之内，是操作数据依赖的基本类型，如果操作A的结果被用作操作B的操作数，那么A带有对B的依赖，如果操作A的结果是类似int的标量类型的值，那么如果A的结果存储在一个变量中，并且该变量随后被用作操作B的操作数，此关系也是适用的，这种操作也具有传递性，所以如果A带有对B的依赖且B带有对C的依赖，那么A带有对C的依赖

//另一方面，dependency-ordered-before的关系可以适用于线程之间，它是通过使用标记了memory_order_consume的原子载入操作引入的，这是memory_order_acquire的一种特例，它限制了对直接依赖的数据同步，标记为memory_order_release,memory_order_acq_rel或memory_order_seq_cst的存储操作A的依赖顺序在标记为memory_order_consume的载入操作之前，如果此消耗读取所存储的值的话，如果载入使用memory_order_acquire,那么这与synchronizes-with关系所得到的是相反的，如果操作B带有操作C的某种依赖，那么A也是依赖顺序在C之前

//如果这对线程间happens-before关系没有影响，那么在同步目的上就无法为你带来任何好处，但它的确实现了，如果A依赖顺序在B之前，则A也是线程间发生于B之前

//这种内存顺序的一个重要用途，是在原子操作载入指向某数据的指针的场合，通过在载入上使用memory_order_consume以及在之前的存储上使用memory_order_release，你可以确保锁指向的数据得到正确的同步，无需再其他非依赖的数据上强加任何同步需求，清单5.10系那是了这种情况的例子

//清单5.10 使用std::memory_order_consume同步数据

struct X
{
    int i;
    std::string s;
};

std::atomic<X*> p;
std::atomic<int> a;

static void create_x()
{
    X* x = new X;
    x->i = 42;
    x->s = "hello";
    a.store(99, std::memory_order_relaxed);   //①
    p.store(x, std::memory_order_relaxed);   //②
}

static void use_x()
{
    X* x;
    while (!(x = p.load(std::memory_order_consume)))   //③
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    assert(x->i == 42);   //④
    assert(x->s == "hello");   //⑤
    assert(a.load(std::memory_order_relaxed) == 99);   //⑥
}

int main()
{
    std::thread t1(create_x);
    std::thread t2(use_x);
    t1.join();
    t2.join();
}

//即使对a的存储①的顺序在对p的存储②之前，并且对p的存储被标记为memory_order_release，p的载入③被标记memory_order_consume，这意味着，对p的存储只发生在依赖p的载入值得表达式之前，这还意味着，在X结构的数据成员④⑤上的断言保证不会被触发，因为p的载入滴啊有对那些通过变量x的表达式的依赖，另一方面，在a的值上的断言或许会触发，或许不会被触发，此操作并不依赖于从p载入的值，因而对读到的值就没有保证，如你所见，因为它被标记为memory_order_relaxed, 所以特别的显著

//有时候，你并不希望四处带着依赖所造成的开销，你想让编译器能够在寄存器中缓存值，并且对操作进行重新排序以优化代码，而不用担心依赖，在这些情形下，你可以使用std::kill_dependency()显示地打破依赖链条，std::kill_dependency()是一个简单的函数模板，它将所提供的参数复制到返回值，但这样做会打破依赖链条，例如，如果你有一个全局的只读数组，但这样会打破依赖链条，例如，如果你有一个全局肚饿只读数组，你在从另外的线程中获取该数组的索引时使用了std::memory_order_consume你可以用std::kill_denpendency()让编译器直到它无需重新读取数组项的内容，就像下面的这个例子

static void do_something_with(int i) {}

int global_data[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19};
std::atomic<int> Index;

static void f()
{
    int i = Index.load(std::memory_order_consume);
    do_something_with(global_data[std::kill_dependency(i)]);
}

//当然这样一个简单的场景里你一般根本不会使用std::memory_order_consume，但是你可能会在具有更复杂代码的相似情形下调用std::kill_dependendy()你必须记住这是一项优化，所以只应小心的使用，用在优化器表明需要使用的地方

//现在，我已经涵盖了内存顺序的基础，是时候看看synchronizes-with关系中更复杂的部分，即体现为释放序列(releasesequence)的形式

#pragma clang diagnostic pop

