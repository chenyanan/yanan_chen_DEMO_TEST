//
//  3_6.cpp
//  123
//
//  Created by chenyanan on 2017/4/21.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <iostream>
#include <thread>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//3.2.4 死锁:问题和解决方案

//试想一下，你有一个由两个部分组成的玩具，并且你需要两个部分一起玩，例如玩具鼓和鼓棒，现在假设你有两个小孩，他们两人都喜欢玩它，如果其中一人同时得到鼓和鼓棒，那这个孩子就可以高兴的玩鼓，知道厌烦，如果另一个孩子想要玩，就得等，不管这让她多不爽，现在想象一下，鼓和鼓棒被分别埋在玩具箱里，你的孩子同时都决定玩他们，于是他们去翻玩具箱，其中一个发现了鼓，而另外一个发现了鼓棒，现在他们被困住了，除非一个人让另一个人玩，不然每个人都回来着他已有的东西，并要求另一个人将另一部分给自己，否则就都玩不成

//现在想象一下，你没有抢玩具的孩子，但却有争夺互斥元的线程，一对线程中的每一个都需要同时锁定两个互斥元来执行一些操作，并且每个线程都拥有了一个互斥元，同时等待另外一个，线程都无法继续，应为每个线程都在等待另一个释放其互斥元，这种情景称为死锁，它是在需要锁定两个或更多互斥元以执行操作时的最大问题

//DEATH LOCK SAMPLE
std::mutex M1;
std::mutex M2;

static void t1() {
    M1.lock();
    std::this_thread::sleep_for(std::chrono::seconds(10));
    M2.lock();
    M2.unlock();
    M1.unlock();
}

static void t2() {
    M2.lock();
    std::this_thread::sleep_for(std::chrono::seconds(10));
    M1.lock();
    M1.unlock();
    M2.unlock();
}

static void f() {
    std::thread T1(t1);
    T1.detach();
    
    std::thread T2(t2);
    T2.detach();
}
//T1 LOCK M1，
//T2 LOCK M2 IN 10 SEC
//T1 WAIT M2
//T2 WAIT M1
//DEATH LOCK

//为了避免死锁，常见的建议是始终使用相同的顺序锁定这两个互斥元，如果你总是在互斥元B之前锁定互斥元A，那么你永远不会死锁，有时候这是很直观的，因为互斥元服务于不同的目的，但其他时候却并不那么简单，比如当互斥元分别保护相同类的各个实例时，例如，考虑同一个类的两个实例之间的数据交换操作，为了确保数据被正确的交换，而不受并发修改的影响，两个实例上的互斥元都必须被锁定，然而，如果选择了一个固定的顺序(例如，作为第一个参数提供的实例的互斥元，然后是作为第二个参数所提供的实例的互斥元)，可能适得其反，这意味着两个线程尝试通过交换参数，而在相同的两个实例之间交换数据，你将产生死锁

//幸运的是，C++标准库中的std::lock可以解决这一问题，std::lock函数可以同时锁定两个或更多的互斥元，而没有死锁的风险，以下的代码展示了如何使用它来完成简单的交换操作

struct some_big_object {};

static void swap(some_big_object& lhs, some_big_object& rhs) {}

class X
{
private:
    some_big_object some_detail;
    std::mutex m;
public:
    X(const some_big_object& sd) : some_detail(sd) {}
    friend void swap(X& lhs, X& rhs)
    {
        if (&lhs == &rhs)
            return;
        std::lock(lhs.m, rhs.m);   //①
        std::lock_guard<std::mutex> lock_a(lhs.m, std::adopt_lock);   //②
        std::lock_guard<std::mutex> lock_b(rhs.m, std::adopt_lock);   //③
        swap(lhs.some_detail, rhs.some_detail);
    }
};

//首先，检查参数以确保他们是不同的实例，因为试图在你已经锁定了的std::mutex上再次锁定，是未定义的行为(允许同一线程多重锁定的互斥元类型为std::recursive_mutex)，然后调用std::lock()锁定这两个互斥元①，同时构造两个std::lock_guard的实例②③，每个实例对应一个互斥元，额外提供一个参数std::adopt_lock给互斥元，告知std::lock_guard对象该互斥元已被锁定，并且他们只应沿用互斥元上已有锁的所有权，而不是试图在构造函数中锁定互斥元

//这就确保了通常在受保护的操作可能引发异常的情况下，函数退出时正确的解锁互斥元，这也考虑到了简单返回，此外值得一提的是，在对std::lock的调用中锁定lhs.m抑或是rhs.m都可能引发异常，在这种情况下，该异常被传播出std::lock如果std::lock已经成功地在一个互斥元上施加了锁，当它试图在另一个互斥元上获取锁的时候，就会引发异常，前一个互斥元将会自动释放，std::lock提供了关于锁定给定的互斥元的全或无的语义

//尽管std::lock能够帮助你在需要同时获得两个或更多锁的情况下避免死锁，但是如果要分别获取锁，就没有用了，在这种情况下，你必须依靠你作为开发人员的戒律，以确保不会得到死锁，这谈何容易，死锁是在编写多线程代码时遇到的最令人头痛的问题之一，而且往往无法预测，大部分时间内一切都工作正常，然而，有一些相对简单的规则可以帮助你写出无死锁的代码

#pragma clang diagnostic pop
