//
//  3_2.cpp
//  123
//
//  Created by chenyanan on 2017/4/20.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <mutex>
#include <string>
#include <algorithm>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//3.2.2 精心组织代码来保护共享数据

//使用互斥量来保护数据，并不是仅仅在每一个成员函数中都加入一个std::lock_guard对象那么简单， 一个迷失的指针或引用，将会让这种保护形同虚设，不过，检查迷失指针或引用是很容易的，只要没有成员函数通过返回值或者输出参数的形式向其调用者返回指向受保护数据的指针或引用，数据就是安全的，如果你还想往祖坟上刨，就没这么简单了，在确保成员函数不会传出指针或引用的同时，检查成员函数是否通过指针或引用的方式来调用也是很重要的(尤其是这个操作不在你的控制下时)，函数可能没在互斥量保护的区域内，存储着指针或者引用，这样就很危险，更危险的是，将保护数据作为一个运行时参数，如同下面清单中所示那样

//清单3.2 无意中传递了保护数据的引用

class some_data
{
    int a;
    std::string b;
public:
    void do_something();
};

class data_wrapper
{
private:
    some_data data;
    std::mutex m;
public:
    template<typename Function>
    void process_data(Function func)
    {
        std::lock_guard<std::mutex> l(m);
        func(data);                           //①传递"受保护的"数据到用户提供的函数
    }
};

some_data* unprotected;

static void malicious_function(some_data& protected_data)
{
    unprotected = &protected_data;
}

data_wrapper x;

static void f()
{
    x.process_data(malicious_function);      //②传入一个恶意函数
    unprotected->do_something();             //③对受保护的数据进行未受保护的访问
}

//例子中process_data看起来没有任何问题，std::lock_guard对数据做了很好的保护，但调用用户提供的函数func①，就意味着foo能够绕过保护机制将函数malicious_function传递进去②，在没有锁定互斥量的情况下调用do_something()

//这段代码的问题自傲与根本没有保护，指示将所有可访问的数据结构代码标记为互斥，函数foo()中调用unprotected->do_something()的代码威能被标记为互斥，这种情况下，C++线程库无法提供任何帮助，只能有程序员来使用正确的互斥锁来保护数据，从乐观的角度上看，还是有方法可循，切勿将受保护数据的指针或引用传递到互斥锁作用域之外，无论是函数返回值，还是存储在外部可见内存，亦或是以参数形式传递到用户提供的函数中去

//虽然这是在使用互斥量保护共享数据时常犯的错误，但绝不仅仅是一个潜在的陷阱而已，下一节中，你将会看到，即便是使用了互斥量对数据进行了保护，条件竞争依旧可能存在

#pragma clang diagnostic pop


















