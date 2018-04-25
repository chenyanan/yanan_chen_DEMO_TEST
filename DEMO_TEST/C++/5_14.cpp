//
//  5_14.cpp
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

//5.3.5 屏障
//没有一套屏障的原子操作库是不完整的，这些操作可以强制内存顺序约束，而无需修改任何数据，并且与使用memory_order_relaxed顺序约束的原子操作组合起来使用，屏障是全局的操作，能在执行该屏障的线程里影响其他原子操作的顺序，屏障一般也被称为内存障碍，它们之所以这样命名，是因为他们在代码中放置了一行代码，使得特定的操作无法穿越，也许你还记得5.3.3节中，在独立变量上的松散操作通常可以自由地被编译器或硬件重新排序，屏障限制了这一自由，并且在之前并不存在的地方引入happens-before和synchronizes-with关系

//让我么你从在清单5.5中的每个线程上的两耳原子操作之间添加屏障开始，如清单5.12所示

//清单5.12 松散操作可以使用屏障来排序

#include <atomic>
#include <thread>
#include "assert.h"

std::atomic<bool> x,y;
std::atomic<int> z;

static void write_x_then_y()
{
    x.store(true, std::memory_order_relaxed);   //①
    std::atomic_thread_fence(std::memory_order_release);   //②
    y.store(true, std::memory_order_relaxed);   //③
}

static void read_y_then_x()
{
    while (!y.load(std::memory_order_relaxed));   //④
    std::atomic_thread_fence(std::memory_order_acquire);   //⑤
    if (x.load(std::memory_order_relaxed))   //⑥
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
    assert(z.load() != 0);   //⑦
}

//释放屏障②与获取屏障⑤同步，因为从y的载入在读取存储在③的值，这意味着对x的存储①发生在从x的载入⑥之前，所以读取的值必然是true，并且在⑦处的断言不会触发，这与原来没有屏障的情况下，对x的存储和从x的载入没有顺序，是相反的，所以断言可以触发，注意，这两个屏障都是必要的，你需要在一个线程中有一个释放，在另一个线程中有一个获取，从而实现synchronizes-with关系

//在这种情况下，释放屏障②的效果，与对y的存储③被表姐为memory_order_release而不是memory_order_relaxed是相似的，同样，获取屏障⑤令其余从y的载入④被标记为memory_order_acquire相似，这是屏蔽的总体思路:如果获取操作看到了释放屏障后发生的存储的结果，该屏障与获取操作同步，如果在获取屏障之前发生的载入看到释放操作的结果，该释放操作与获取屏障同步，当然，你可以在两边都设置屏障，就像在这里的例子一样，在这种情况下，如果在获取屏障之前发生的载入看见了释放屏障之后发生的存储所写下的值，该释放屏障与获取屏障同步

//尽管屏障同步依赖于在屏障之前或之后的操作所读取或写入的值，注意同步点就是屏障本身是很重要的，如果你从清单5.12中将write_x_then_y拿走，将对x的写入移到下面的屏障之后，断言中的条件就不再保证是真的，即便对x的写入在对y的写入之前

static void another_write_x_then_y()
{
    std::atomic_thread_fence(std::memory_order_release);
    x.store(true, std::memory_order_relaxed);
    y.store(true, std::memory_order_relaxed);
}

//这两个操作不再被屏蔽分割，所以也不再有序，只有当屏障出现在对x的存储和对y的存储之间时，才能施加顺序，当然，屏障的存在与否并不影响现存的，有其他原子操作带来的happens-before关系所强制的顺序

//这个例子中，以及本章中目前为止几乎所有其他的例子，完全是从具有原子类型的变量建立起来，然而，使用原子操作来强制顺序的真正优点，是它们可以在非原子操作上强制顺序，并且因此避免了数据竞争的未定义行为，就像之前你在清单5.2中所看到的那样

#pragma clang diagnostic pop



