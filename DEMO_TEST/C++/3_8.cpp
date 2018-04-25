//
//  3_8.cpp
//  123
//
//  Created by chenyanan on 2017/4/21.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <stdio.h>
#include <thread>
#include <mutex>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//3.2.6 用std::unqiue_lock灵活锁定
//通过松弛不变量，std::unique_lock比std::lock_guard提供了更多的灵活性，一个std::unique_lock实例并不总是拥有与之相关联的互斥元，首先就像你可以把std::adopt_lock作为第二参数传递给构造函数，以便让锁对象来管理互斥元上的锁那样，你也可以把std::unique_lock对象本身传递给std::lock()来获取，使用std::unique_lock和std::defer_lock，而不死std::lock_guard和std::adopt_lock能够很容易地将清单3.6写成清单3.9中所示的那样，这段代码具有相同的行数，并且本质上是等效的，除了一个小问题，std::unique_lock占用更多空间并且使用起来比std::lock_guard略慢，允许std::unique_lock实例不拥有互斥元的灵活性室友代价的，这条信息必须被存储，并且必须被更新

//清单3.9 在交换操作中使用std::lock()和std::unique_lock

class some_big_object {};

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
        using std::swap;
        if (&lhs == &rhs)
            return;
        std::unique_lock<std::mutex> lock_a(lhs.m, std::defer_lock);   //①
        std::unique_lock<std::mutex> lock_b(lhs.m, std::defer_lock);   //①std::defer_lock保留互斥元为未锁定
        std::lock(lock_a, lock_b);   //②
        swap(lhs.some_detail, rhs.some_detail);
    }
};

//在清单3.9中，std::unique_lock对象能够被传递给std::lock()②，因为std::unique_lock提供了lock(),try_lock(),try_lock()和unlock()三个成员函数，他们会转发给底层互斥元上同名的成员函数去做实际的工作，并且只是更新在std::unique_lock实例内部的一个标识，来标识该实例当前是否拥有此互斥元，为了确保unlock()在析构函数中被正确调用，这个标识是必须的，如果该实例确已拥有此互斥元，则析构函数必须调用unlock(),并且如果该实例并未拥有此互斥元，则析构函数决不能调用unlock()，可以通过调用owns_lock()成员函数来查询这个标识

//如你所想，这个标识必须被存储在某个地方，因此std::unique_lock对象的大小通常大于std::lock_guard对象，并且相比于std::lock_guard使用std::unique_lock的时候，会有些许性能损失，因为需要对标识进行相应的更新或检查，如果std::lock_guard足以满足要求，我会建议优先使用它，也就是说，还有一些使用std::unique_lock更适合于手头任务的情况，因为你需要利用额外的灵活性，一个例子就是延迟锁定，正如你已经看到的，另一种情况是锁的所有权需要从一个作用域转移到另一个作用域

#pragma clang diagnostic pop
