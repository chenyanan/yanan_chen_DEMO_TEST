//
//  7_1.cpp
//  123
//
//  Created by chenyanan on 2017/5/10.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <iostream>
#include <thread>
#include <mutex>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//7.1 定义和结果

//使用互斥元，条件变量以及future来同步数据的算法和数据结构被称为阻塞(blocking)的算法和数据结构，调用库函数的应用会终端一个线程的执行，直到另一个线程执行一个动作，这种库函数调用被称为阻塞调用，因为直到阻塞被释放时线程才能继续执行下去，通常，操作系统会完全阻塞一个线程(并且将这个线程的时间片分配给另一个线程)直到另一个线程执行了适当的动作将其解锁，可以是解锁互斥元，通知条件或者使得future就绪

//不使用阻塞库函数的数据结构和算法被称为非阻塞(nonblocking)的，但是，并不是所有这样的数据结构都是无锁(lock-free)的，因此我们来看一看非阻塞数据结构的各种类型

#pragma clang diagnostic pop

