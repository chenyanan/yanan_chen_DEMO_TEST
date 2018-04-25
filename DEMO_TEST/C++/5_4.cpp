//
//  5_4.cpp
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

//5.3 同步操作和强制顺序
//假设有两个线程，其中一个是要填充由第二个线程所读取的数据结构，为了避免有问题的竞争条件，第一个线程设置一个标志来只是数据已经就绪，在标志设置之前第二个线程不读取数据，清单5.2展示了这样的场景

//清单5.2 从不同的线程中读取和写入变量

#include <vector>
#include <atomic>
#include <thread>
#include <chrono>
#include <iostream>

std::vector<int> data;
std::atomic<bool> data_ready(false);

void reader_thread()
{
    while (!data_ready.load())   //①
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::cout << "The anser=" << data[0] << "\n";   //②
}

static void writer_thread()
{
    data.push_back(42);   //③
    data_ready = true;   //④
}

//撇开等待数据准备的循环的低效率①，你确实需要这样的工作，因为不这样的话在线程间共享数据就变得不可行，每一项数据都强制成为原子的，你已经直到在没有强制顺序的情况下，非原子的读②和写③同一个数据是未定义的行为，所以为了使其工作，某个地方必须有一个强制的顺序

//所要求的强制顺序来自于std::atomic<bool>变量data_ready上的操作，它们通过happens-before(发生于之前)和syschronizes-with(与之同步)内存耐磨性的关系的优点来提供必要的顺序，数据写入③发生于写入data_ready标志④之前，标志的读取①发生于数据的读取②之前，当从data_ready①读取的值为true时，写与读同步，创建一个happens-before的关系，因为happens-before是可传递的，数据的写入③发生于标志的写入④之前，发生于从标准读取true值①之前，又发生于数据的读取之前，你就有了一个强制顺序:数据的写入发生于数据的读取之前，一切都好了，图5.2表明了两个线程间happens-before关系的重要性，我从度线程添加了while循环的几次迭代

//data.push_back(42)   data_ready.load()(返回false)
//data_ready = true    data_ready.load()(返回false)
//                     data_ready.load()(返回true)
//                     data[0](返回42)
//写线程                读线程

//所有这一切都似乎相当直观，写入一个值当然发生在读取这个值之前!使用默认的原子操作，这的确是真的(这就是为什么它是默认的)，但它需要阐明:原子操作对于顺序要求也有其他的选项，这一点我马上回提到

//现在，你已经在实战中看到了happens-before和sysnchronizes-with，现在是时候来看看它们到底是什么意思了，我将从synchronizes-with开始

//5.3.1 synchronizes-with关系
//synchronizes-with关系是你只能在原子类型上的操作之间得到的东西，如果一个数据结构包含原子类型，并且在该数据结构上的操作会在内部执行适当的原子操作，该数据结构上的操作(如锁定互斥元)可能会提供这种关系，但是从根本上说synchronizes-with关系只出资原子类型上的操作

//基本的思想是这样的:在一个变量x上的一个被适当标记的原子写操作W，与在x上的一个被适当标记的，通过写入(w)，或是由与执行最初的写操作w相同的线程在x上的后续原子写操作，或是由任意线程在x上一系列的原子的读-修改-写操作(如fetch_add())或compare_exchange_weak())来读取所存储的值得原子读操作同步，其中随后通过第一个线程读取的值是通过w写入的值(参见5.3.4节)

//现在讲"适当标记"部分放在一边，因为原子类型的所有的操作是默认适当标记的，这基本上和你像的一样，如果线程A存储一个值而线程B读取该值，那么线程A中的存储和线程B中的载入之间存在一种synchronizes-with关系，如清单5.2中一样

//我敢肯定你已经猜到，微妙之处尽在"适当标记"的部分，C++内存模型允许各种顺序约束应用于原子类型上的操作，这就是我所指的标记，内存顺序的各种选项以及他们如何涉及synchronizes-with关系包含在5.3.3节中，让我么你退一步来看看happens-before关系

//5.3.2 happens-before关系
//happens-before(发生于之前)关系是程序中顺序的基本构建，它指定了哪些操作看到其他操作的结果，对于单个线程，它是直观的，如果一个操作排在另一个操作之前，那么该操作就发生于另一个操作之前，这就意味着，在源代码中，如果一个操作(A)发生于另一个操作(B)之前的语句里，那么A就发生于B之前，你可以在清单5.2中看到:data的写入操作③发生于data_ready的读取操作④之前，如果操作发生在同一条语句中，一般他们之间没有happens-before关系，因为他们是无序的，这只是顺序未指定的另一种说法，你知道清单5.3中的代码程序将输出"1,2"或"2,1"，但是并未指定究竟是哪个，因为对get_num()的两次调用的顺序未指定

#include <iostream>

static void foo(int a, int b)
{
    std::cout << a << "," << b << std::endl;
}

int get_num()
{
    static int i = 0;
    return ++i;
}

int main()
{
    foo(get_num(), get_num());   //对get_num()的调用是无序的
}

//有时候，单条语句中的操作是有顺序的，例如使用内置的都好操作符或者使用一个表达式的结果作为另一个表达式的参数，但是一般来说，单条语句中的操作是非顺序的，而且也没有sequenced-before(因此也没有happens-before)，当然，一条语句中的所有操作在下一句的所有操作之前发生

//这确实只是你习惯的单线程顺序规则的一个重述，那么新的呢？新的部分是线程之间的交互，如果线程间的一个线程上的操作A发生与另一个线程上的操作B之前，那么A发生与B之前，这并没有真的起到多大帮助，你只是增加了一种内心的关系(线程间happens-before)，但这在你编写多线程代码时是一个重要的关系

//在基础水平上，线程间happens-before相对简单，并依赖于5.3.1节中介绍的synchronizes-with关系，如果一个线程中的操作A与另一个线程中的操作B同步，那么A线程间发生于B之前，这也是一个可传递关系，如果A线程间发生于B之前，B线程间发生于C之前，那么A线程间发生与C之前，你也可以在清单5.2中看到

//线程间happens-before还可与sequenced-before关系结合，如果操作A的顺序在操作B之前，操作B线程间发生与C之前，那么A线程间发生与C之前，类似地，如果A与B同步，B的顺序在操作C之前，那么A线程间发生于C之前，这两个在一起意味着，如果你对单个线程中的数据做了一系列的改变，对于执行C的线程上的后续线程课件的数据，你只需要一个synchronizes-with关系

//这些事在线程间强制操作顺序的关键规则，使得清单5.2中的所有东西得以运行，数据依赖性有一些额外的细微差别，你很快就会看到，为了让你理解这一点，我需要介绍用于原子操作的内存顺序标签，以及他们如何与synchronizes-with关系相关联

#pragma clang diagnostic pop
