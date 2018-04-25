//
//  5_10.cpp
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

//5. 获取-释放顺序

//获取-释放顺序是松散顺序的进步，操作任然没有总的顺序，但的确引入了一些同步，在这种顺序模型下，原子载入是获取(acquire)操作(memory_order_acquire)，原子存储是释放(release)操作(memory_order_release)，原子的读-修改-写操作(例如fetch_add()或exchange())是获取、释放或两者兼备(memory_order_acq_rel)，同步在进行释放的线程和进行获取的线程之间是对偶的，释放操作与读取写入值的获取操作同步，这意味着，不同的线程仍然可以看到不同的排序，但这些顺序是收到限制的，清单5.7采用获取-释放语义而不死顺序一致顺序，对清单5.4进行了改写

//清单5.7 获取-释放并不意味着总体排序

#include <atomic>
#include <thread>
#include "assert.h"

std::atomic<bool> x,y;
std::atomic<int> z;

static void write_x()
{
    x.store(true, std::memory_order_release);
}

static void write_y()
{
    y.store(true, std::memory_order_release);
}

static void read_x_then_y()
{
    while(!y.load(std::memory_order_acquire));
    if (x.load(std::memory_order_acquire))   //①
        ++z;
}

static void read_y_then_x()
{
    while (!y.load(std::memory_order_acquire));
    if (x.load(std::memory_order_acquire))   //②
        ++z;
}

int main()
{
    x = false;
    y = false;
    std::thread a(write_x);
    std::thread b(write_y);
    std::thread c(read_x_then_y);
    std::thread d(read_y_then_x);
    a.join();
    b.join();
    c.join();
    c.join();
    assert(z.load() != 0);   //③
}

//在这个例子里，断言③是可以触发(就像在松散顺序中的例子)，因为对x的载入②和对y的载入①都读取false是可能的，x和y由不同的线程写入，所以每种情况下从释放到获取的顺序对于另一个线程中的操作是没有影响的

//图5.6展示了清单5.7中的happens-before关系，伴随一种可能的结果，即两个读线程看到了世界的不同方面，这可能是因为没有像前面提到的happens-before关系来强制顺序

//x = false, y = false
//x.store(true, release)                                                        y.store(true, release)
//                          x.load(acquire)返回true   y.load(acquire)返回true
//                          y.load(acquire)返回false  x.load(acquire)返回false
//write_x                   read_x_then_y            read_y_then_x              write_y
//图5.6 获取-释放和happens-before

//为看到获取-释放顺序的优点，你需要考虑类似清单5.5中来自同一个线程中的两个存储，如果你将对y的存储更改为使用memory_order_release，并且让对y的载入使用类似于清单5.8中的memory_order_acquire，那么你实际上就对x上的操作欧施加了一个顺序

//清单5.8 获取-释放操作可以在松散操作中施加顺序

#include <atomic>
#include <thread>
#include "assert.h"

std::atomic<bool> x1,y1;
std::atomic<int> z1;

static void another_write_x_then_y()
{
    x1.store(true, std::memory_order_relaxed);   //①旋转，等待y被设为true
    y1.store(true, std::memory_order_release);   //②
}

static void another_read_y_then_x()
{
    while (!y1.load(std::memory_order_acquire));   //③
    if (x1.load(std::memory_order_relaxed))   //④
        ++z1;
}

int aMain()
{
    x1 = false;
    y1 = false;
    z1 = 0;
    std::thread a(another_write_x_then_y);
    std::thread b(another_read_y_then_x);
    a.join();
    b.join();
    assert(z1.load() != 0);   //⑤
    return 0;
}

//最终，对y的加载③将会看到由存储写下的true②，因为存储使用memory_order_release并且载入使用memory_order_acquire，存储与载入同步，对x的存储①发生在对y的存储②之前，因为他们在同一个线程里，因为对y的存储与从y的载入同步，对x的存储同样发生于从y的载入之前，并且推而广之发生于从x的载入④之前，于是，从x的载入必然读到true，而且断言⑤不会触发，如果从y的载入不在while循环里，就不一定是这个情况，从y的载入可能读到false，在这种情况下，对从x读取到的值就没有要求了，为了提供同步，获取和释放操作必须配对，由释放操作存储的值必须被获取操作看到，以便两者中的任意一个生效，如果②出的存储或③出的载入有一个是松散操作，对x的访问就没有顺序，因此就不能保证④出的载入读取true，且assert会被触发

//你仍然可以用带着笔记本在他们小隔间的人们的形式来考虑获取-释放顺序，但是你必须对模型添加更多，首先，假定每个已完成的而存储都是一批更新的一部分，因此当你呼叫一个人让他写下一个数字时，你也要告诉他这次更新是哪一批的一部分:"请写下99，作为第423批的一部分"，对于一批的最后一次存储，你也可以这样告诉他:"请写下147，作为第423批的最后一次的存储"，小隔间中的人会及时地写下这一信息，以及谁给了他这些值，这模拟了一个存储-释放操作，下一次，你告诉某人写下一个值，你应增加批次号码:"请写下41，作为第424批的一部分"

//当你询问一个值时，会有一个选择:你可以仅仅询问一个值(这是个松散载入)，再次情况下这个人只会给你一个数字，或者你可以询问一个值以及它是不死这一批中最后一个数的信息(模拟载入-获取)，如果你询问批次信息，并且该值不是这一批中的最后一个，他会告诉你类似于"这个数字是987，仅仅是一个'普通'的值"，然而如果它曾经是这一批中的最后一个，他会告诉你类似于"这个数字是987，是来自Anne的地956批的最后一个数"，现在，获取-释放语义闪耀登场:如果当你询问一个值时告诉他你所摘掉的所有批次，他会从他的列表中向下查找，找到你知道的任意一个批次的最后一个值，并且要么给你那个数字，要么列表更下方的一个数字

//这是如何获取-释放语义的呢？让我恩看一看这个例子，最开始，线程a运行write_x_then_y并对小间隔x内的人说，"请写下true作为线程a第一批的一部分"，然后他即使地写下了，线程a随后对小间隔y内的人说，"请写下true作为线程a第一批的最后一笔"，他也及时地写下了，于此同时，线程b运行着read_y_then_x，线程b持续地向小间隔y里的人索取带着批次信息的值，直到他说"true"，他可能要询问很多次，但是最终这个人总会说"true"，大师砸小间隔y里的人不能仅仅说"true"，他还要说"这是线程a第一批的最后一笔"

//现在，线程b继续向合资x里的人询问值，但是这一次他说:"请让我拥有一个值，并且通过这一方式让我知晓线程a的第一批"，所以现在，小间隔x中的人必须查看他的列表，找到线程a中第一批的最后一次提及，他所具有的唯一一次提及是true值，同时也是列表上的最有一个值，所以他必须读取该值，窦泽，它会破坏游戏规则

//如果你回到5.3.2节查看线程间happens-before的定义，一个重要的内容就是它的传递性:如果A线程发生于B之前，并且B线程间发生于C之前，则A线程间放生于C之前，这意味着获取-释放顺序可以用来在多个线程之间同步数据，甚至在"中间线程"并没有实际接触数据的场合

#pragma clang diagnostic pop

