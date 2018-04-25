//
//  5_16.cpp
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

//5.4 小结
//本章介绍了C++11内存模型的底层细节，以及在线程间提供同步基础的原子操作，这包括了有std::atomic<>类模板的特化提供的基本原子类型，由std::atomic<>主模板提供的泛型原子接口，在这些类型上的操作，以及各种内存顺序选项的复杂细节

//我们还看了屏障，以及他们如何通过原子类型上的操作配对，以强制顺序，最后，我们回到开头，看了看原子操作是如何用来在独立线程上的非原子操作之间强制顺序的

//在接下来的章节中，我们将学习在原子操作之外使用高级同步功能，来为并发访问设计高效的容器，而且我们会编写并行处理数据的算法

#pragma clang diagnostic pop
