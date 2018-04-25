//
//  5_6.cpp
//  123
//
//  Created by chenyanan on 2017/5/4.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <iostream>
#include <thread>
#include <mutex>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//1. 顺序一致式顺序
//默认的顺序被命名为顺序一致(sequentiallyconsistent)，因为这意味着程序的行为与一个简单的顺序的世界观是一致的，如果所有原子类型实例上的操作是顺序一致的，多线程程序的行为，就好像是所有这些操作由单个线程以某种特定的顺序进行执行一样，这是迄今为止最容易理解的内存顺序，这也是他作为默认值得原因，所有线程够必须看到操作的相同顺序，这使得推断用原子变量编写的带啊的行为变得容易，你可以写下不同线程的所有可能的操作顺序，消除那些不一致的，并验证你的代码在其他程序里是否和预期的行为一样，这也意味着，操作不能被重排，如果你的代码在一个线程中有一个操作在另一个之前，其顺序必须对多有其他的线程可见

//从同步的观点来看，顺序一致的存储于读取该存储值得同一个变量的顺序一致载入是同步的，者提供了一种两个(或多个)线程操作的顺序约束，但顺序一致比它更加强大，在使用顺序一致原子操作的系统中，所有在载入后完成的顺序一致原子操作，也必须出现在其他线程的存储之后，清单5.4中的示例实际演示了这个顺序约束，该约束并不会推进使用具有松散内存顺序的原子操作，它们仍然可以看到操作处于不同的顺序，所以你必须在所有的线程上使用顺序一致的操作以获利

//然而，易于季节就产生了代价，在一个带有许多处理器的弱顺序机器上，它可能导致显著的性能惩罚，因为操作的这题顺序必须与处理器保持一致，可能需要处理器之间进行秘籍(且昂贵)的同步操作，这就是说，有些处理器构架(如常见的X86和X86-64构架)提供相对低廉的顺序一致性，因此如果你担心下使用顺序一致性对性能的影响，检查你的目标价爱处理器构架的文档

//清单5.4实际这哪是了顺序一致性，x和y的载入和存储是memory_order_seq_cst显示标记的，尽管此标记在这种情况下可以省略，因为他是默认的

//清单5.4 顺序一致隐含着总体顺序

#include <atomic>
#include <thread>
#include "assert.h"

std::atomic<bool> x,y;
std::atomic<int> z;

static void write_x()
{
    x.store(true, std::memory_order_seq_cst);   //①
}

static void write_y()
{
    y.store(true, std::memory_order_seq_cst);   //②
}

static void read_x_then_y()
{
    while(!x.load(std::memory_order_seq_cst));
    if (y.load(std::memory_order_seq_cst))   //③
        ++z;
}

static void read_y_then_x()
{
    while (!y.load(std::memory_order_seq_cst));
    if (x.load(std::memory_order_seq_cst))   //④
        ++z;
}

int main()
{
    x = false;
    y = false;
    z = 0;
    std::thread a(write_x);
    std::thread b(write_y);
    std::thread c(read_x_then_y);
    std::thread d(read_y_then_x);
    a.join();
    b.join();
    c.join();
    c.join();
    assert(z.load() != 0);   //⑤
}

//assert⑤可能永远不触发，因为对x的存储①或者对y的存储②必须先发生，尽管没有特别指定，如果read_x_then_y中载入y③返回false，x的存储必须发生于y的存储之前，这时read_y_then_x中载入x④必须返回true，因为while循环在这一点上确保y是true，由于memory_order_seq_cst的语义需要在所有标记memory_order_seq_cst的操作系统上有着单个总体顺序，返回false的载入y③和对x的存储①之间有一个隐含的顺序关系，为了有单一的总体顺序，如果一个线程看到x==true，随后看到y==false，这意味着在这个总体顺序上，x的存储发生在y的存储之前

//当然，因为一切是堆成的,它也可能反方向发生，x的载入④返回false，迫使y的载入③返回true，在这两种情况下，z都等于1，两个载入都可能返回true，导致z等于2，但是无论如何z都不可能等于0

//对于read_x_then_y看到z为true且y为false的情况，其造作和happens-before关系如图5.3所示，从read_x_then_y中的y载入到write_y中y的存储的虚线，展示了所需要的隐含的顺序关系以保持顺序一致，在memory_order_seq_cst操作的全局顺序中，载入必须发生在存储之前，已达到这里所给的结果

//起始 x = false, y = false
//x.store(true)                                           y.store(true)
//                x.load()返回true     y.load()返回true
//                y.load()返回false    x.load()返回true
//write_x         read_x_then_y       read_y_then_x       write_y

//顺序一致是最直观和直觉的排序，但也是最昂贵的内存顺序，因为它要求所有线程之间的全局同步，在多处理器系统中，这可能需要处理器之间相当秘籍和耗时的通信

//为了避免这种同步成本，你需要跨出顺序一致的世界，并来绿使用其他内存顺序

#pragma clang diagnostic pop

