//
//  5_8.cpp
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

//3. 松散顺序
//以松散顺序执行的原子类型上的操作不参与synchronizes-with关系，单线程中的同一个变量的操作仍然服从happens-before关系，但相对与其他线程的顺序几乎没有任何要求，唯一的要求是，从同一个线程对单个原子变量的访问不能被重排，一旦给定的线程已经看到了原子变量的特定值，该线程之后的读取就不能获取该变量更早的值，在没有任何额外的同步的情况下，每个变量的修改顺序是使用memory_order_relaxed的线程之间唯一共享的东西

//为了展示松散顺序操作到底能多松散，你只需要两个线程，如清单5.5所示

//清单5.5 放松操作由极少数的排序要求
 
 #include <atomic>
 #include <thread>
 #include "assert.h"
 
 std::atomic<bool> x,y;
 std::atomic<int> z;
 
 static void write_x_then_y()
 {
     x.store(true, std::memory_order_relaxed);   //①
     y.store(true, std::memory_order_relaxed);   //②
 }
 
 static void read_y_then_x()
 {
     while(!y.load(std::memory_order_relaxed));   //③
     if (x.load(std::memory_order_relaxed))   //④
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
     assert(z.load() != 0);   //⑤
 }

//这次assert⑤可以触发，因为x的载入④能够读到false，即便是y的载入③读到了true并且x的存储①发生于y存储②之前，x和y是不同的变量，所以关于每个操作所产生的值得可见性没有顺序保证

//不同变量的松散操作可以被自由地重排，前提是它们服从所有约束下的happens-before关系(例如，在同一个线程中)，它们并不引入synchronizes-with关系，清单5.5中的happens-before关系如图5.4所示，伴随一个可能的结果，即便在存储操作之间和载入操作之间存在happens-before关系，但任一存储和任一载入之间却不存在，所以载入可以在顺序之外看到存储

//起始x = false, y = false
//x.store(true, relaxed)
//y.store(true, relaxed)   y.load(relaxed)返回true
//                         x.load(relaxed)返回false
//write_x_then_y           read_y_then_x

//图5.4 松散的原子核happens-before

//让我们在清单5.6中看一看带有三个变量和五个线程的稍微复杂的例子

//清单5.6 多线程的松散操作

#include <thread>
#include <atomic>
#include <iostream>

std::atomic<int> x1(0), y1(0), z1(0);   //①
std::atomic<bool> go(false);   //②

unsigned const loop_count = 10;

struct read_values
{
    int x, y, z;
};

read_values values1[loop_count];
read_values values2[loop_count];
read_values values3[loop_count];
read_values values4[loop_count];
read_values values5[loop_count];

static void increment(std::atomic<int>* var_to_int, read_values* values)
{
    while (!go)   //③旋转，等待信号
        std::this_thread::yield();
    for (unsigned i = 0; i < loop_count; ++i)
    {
        values[i].x = x1.load(std::memory_order_relaxed);
        values[i].y = y1.load(std::memory_order_relaxed);
        values[i].z = z1.load(std::memory_order_relaxed);
        var_to_int->store(i + 1, std::memory_order_relaxed);   //④
        std::this_thread::yield();
    }
}

static void read_vals(read_values* values)
{
    while(!go)
        std::this_thread::yield();   //⑤旋转，等待信号
    for (unsigned i = 0; i < loop_count; ++i)
    {
        values[i].x = x1.load(std::memory_order_relaxed);
        values[i].y = y1.load(std::memory_order_relaxed);
        values[i].z = z1.load(std::memory_order_relaxed);
        std::this_thread::yield();
    }
}

static void print(read_values* v)
{
    for (unsigned i = 0; i < loop_count; ++i)
    {
        if (i)
            std::cout << ",";
        std::cout << "(" << v[i].x << "," << v[i].y << "," << v[i].z << ")";
    }
    std::cout << std::endl;
}

int aMain()
{
    std::thread t1(increment, &x1, values1);
    std::thread t2(increment, &y1, values2);
    std::thread t3(increment, &z1, values3);
    std::thread t4(read_vals, values4);
    std::thread t5(read_vals, values5);
    
    go = true;  //⑥开始自行主循环的信号
    
    t5.join();
    t4.join();
    t3.join();
    t2.join();
    t1.join();
    
    print(values1);   //⑦打印最终的值
    print(values2);
    print(values3);
    print(values4);
    print(values5);
    
    return 0;
}

//本质上这是一个非常简单的程序，你有三个共享的全局原子变量①和五个线程，每个线程循环10次，用memory_order_relaxed读取三个原子变量的值，并将其存储在数组中，这三个线程中的每个线程每次通过循环④更新其中一个原子变量，而其他两个线程只是读取，一旦所有的线程都被链接，你就打印由每个线程存储在数组中的值⑦

//原子变量go②用来确保所有的线程尽可能靠近相同的时间喀什循环，启动一个线程是一个昂贵的操作，若没有明确的延迟，第一个线程可能会在最后一个贤臣开始前就结束了，每个线程在进入主循环之前等待go变成true③、⑤，仅当所有的线程都已经开始后⑥，go才被设置成true

//这个程序的一个可能的输出如下所示

//(0,0,0),(1,0,0),(2,0,0),(3,0,0),(4,0,0),(5,7,0),(6,7,8),(7,9,8),(8,9,8),(9,9,10)
//(0,0,0),(0,1,0),(0,2,0),(1,3,5),(8,4,5),(8,5,5),(8,6,6),(8,7,9),(10,8,9),(10,9,10)
//(0,0,0),(0,0,1),(0,0,2),(0,0,3),(0,0,4),(0,0,5),(0,0,6),(0,0,7),(0,0,8),(0,0,9)
//(1,3,0),(2,3,0),(2,4,1),(3,6,4),(3,9,5),(5,10,6),(5,10,8),(5,10,10),(9,10,10),(10,10,10)
//(0,0,0),(0,0,0),(0,0,0),(6,3,7),(6,5,7),(7,7,7),(7,8,7),(8,8,7),(8,8,9),(8,8,9)

//前三行是线程在进行更新，最后两行是线程仅进行读取，每个三元组是一组变量x，y，z以这样的顺序经历循环，这个输出中有几点要注意

//第一系列值显示了x在每个三元组里依次增加1，第二系列y依次增加1，以及第三系列z依次增加1

//每个三元组的x元素仅在给定系列中自增，y和z元素也一样，但增量不是平均的，而且在所有西安城之间的相对顺序也不同

//线程3并没有看到任何的x和y的更新，它只能看到它对z的更新，然而这并不能阻止其他线程看到对z的更新混合在对z的更新混合在对x和y的更新中

//这对于松散操作是一个有效的结果，但并非唯一有效的结果，任何由三个变量组成，每个变量轮流保存从0到10的值，同时使得线程递增给定的变量，并打印出该变量从0到9的值得一系列值，都是有效的

#pragma clang diagnostic pop

