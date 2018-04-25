//
//  5_2.cpp
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

//5.2.2 std::atomic_flat上的操作
//std::atomic_flat是最简单的标准原子类型，它代表一个布尔标准，这一类型的对象可以是两种状态之一，设置或清除，这是故意为之的基础，仅仅是为了用作构造块，基于此，除非在极特殊情况下，否则我都不希望看到使用它，即便如此，它将作为讨论其他原子类型的起点，因为他展示了一些应用于原子类型的通用策略

//类型为std::atomic_flat的对象必须用ATOMIC_FLAG_INIT初始化，这会将该标志初始化为清除状态，在这里没有其他的选择，此标志总是以清除开始

std::atomic_flag fl = ATOMIC_FLAG_INIT;

//这适用于所有对象被声明的地方，且无论其具有什么作用域，这是唯一需要针对初始化进行特殊处理的原子类型，但同时也是唯一保证无锁的类型，如果std::atomic_flat对象具有状态存储持续时间，那么就保证了静态初始化，这意味着不存在初始化顺序问题，它总是在该标识上的首次操作进行初始化

//一旦标识对象初始化完成，你只能对它做三件事:销毁，清除，设置并查询其先前的值，这些分别对应于析构函数,clear()成员函数以及test_and_st()成员函数,clear()和test_and_set()成员函数都可以指定一个内存顺序,clear()是一个存储操作，因此不能有memory_order_acquire或memory_order_acq_rel语义，但是test_and_set()是一个读-修改-写操作，并以此能够适用任意的内存顺序标签，至于诶个原子操作，其默认值都是memory_order_seq_cst，例如

static void f()
{
    std::atomic_flag fl = ATOMIC_FLAG_INIT;
    fl.clear(std::memory_order_release);   //①
    bool x = fl.test_and_set();   //②
    printf("%d\n", x);
}

//此处，对clear()的调用①明确要求适用释放语义清除该标志，而对test_and_set()的调用②使用默认内存顺序来设置标志和获取旧的值

//你不能从一个std::atomic_flag对象拷贝构造另一个对象，也不能将一个std::atomic_flag赋值给另外一个，这并不是std::atomic_flag特有的，而是所有原子类型共有的，在原子类型上的操作全都定义为原子的，在拷贝构造函数或拷贝赋值的情况下，其值必须先从一个对象读取，再写入另外一个，这是在两个独立对象上的独立操作，其组合不可能是原子的，因此，这些操作是禁止的

//有限的特性集使得std::atomic_flag理想地适合于用作自旋锁互斥元，最开始，该标志为清除，互斥元时解锁的，为了锁定互斥元，循环执行test_and_set()直至旧值为false，指示这个县城将值设为true，解锁此互斥元就是简单的清除标志，这个实现展示在清单5.1中

//清单5.1 使用std::atoic_flag的自旋锁互斥实现

class spinlock_mutex
{
    std::atomic_flag flag;
public:
    spinlock_mutex() : flag(ATOMIC_FLAG_INIT) {}
    void lock()
    {
        while (flag.test_and_set(std::memory_order_acquire));
    }
    void unlock()
    {
        flag.clear(std::memory_order_release);
    }
};

//这样一个互斥元是非常基本的，但它足以与std::lock_guard<>一同使用(参见第3章)，就其本质而言，它在lock()中执行了一个忙等待，因此如果希望有任何程度的竞争，这就是个糟糕的选择，但它足以确保互斥，当我们观察内存顺序语义时，将看到这是如何保证与互斥元锁相关的必要的强制顺序，这个例子将在5.3.6节中阐述

//std::atomic_flag由于限制性甚至不能用做一个通用的布尔标识，因为它不具有简单的无修改查询操作，为此，你最好还是使用std::atomic<bool>接下来我们将介绍之

//5.2.3 基于std::atomic<bool>的操作

//最基本的原子整数类型是std::atomic<bool>如你所期望那样，这是一个比std::atomic_flag功能更全的布尔标志，虽然它仍然是不可拷贝构造和构造复制IDE，但可以从一个非原子的bool来构造它，所以它可以被初始化为true和false，同时也可以从一个非原子的bool值来对std::atomic<bool>的实例赋值

static void f1()
{
    std::atomic<bool> b(true);
    b = false;
}

//从非原子的bool进行复制操作还要注意的一件事是它与通常的惯例不同，并非像其复制的对象返回一个引用，它返回的是具有所赋值的bool，这对于原子雷兴是另一种常见的模式，他们所支持的赋值操作符返回值(属于相应的非原子类型)而不是引用，如果返回的是原子变量的引用，所有依赖于赋值结果的代码将显示地载入该值，可能会获取被另一线程修改的结果，通过一非原子值得形式返回赋值结果，可以避免这种额外的载入，你会知道所获取的值是已存储的实际值

//与使用std::atomic_flag的受限的clear()函数不同，写操作(无论是true还是false)是通过调用store()来完成的，类似地，test_and_set()被更通用的exchange()成员函数所替代，它可以让你用索选泽的新值来代替已存储的值，同时获取原值，std::atomic<bool>支持对值得普通无修改查询，通过隐式转换成普通的bool，或显示调用load()，正如你所期望的,store()是一个存储操作，而load()是载入操作,exchange()是读-修改-写操作

#include <atomic>

static void f2()
{
    std::atomic<bool> b;
    bool x = b.load(std::memory_order_acquire);
    b.store(true);
    x = b.exchange(false, std::memory_order_acq_rel);
}

//exchage并非std::atomic<bool>支持的唯一的读-修改-写操作，它还引入了一个操作，用于在当前值与期望值相等时，存储新的值

//根据当前值存储一个新值(或者否)

//这个新的操作被称为比较/交换，它以compare_exchange_weak()和compare_exchange_strong()成员函数形式出现，比较/交换操作是使用原子类型编程的基石，它比较原子变量值和所提供的期望值，如果两者相等，则存储提供的期望值，如果两者不等，则期望值更新为源自变量的实际值，比较/交换函数的返回类型为bool，如果执行了存储即为true，反之则为false

//对于compare_exchange_weak()，即使原始值等于期望值也可能出现存储不成功，在这种情况下变量的值是不变的，compare_exchange_weak()的返回值为false，这最有可能发生在缺少单个的比较并交换指令的机器上，此时处理器无法保证该操作被完成，可能因为执行操作的线程在必需的指令序列中间被切换出来，同时在线程多余处理器数量的操作系统中，它被另一个计划中的线程代替，这就是所谓的伪失败，因为失败的原因是时间的函数而不是变量的值

//由于comepare_exchange_weak()可能会伪失败，它通常必须用在循环中

static void f3()
{
    bool expected = false;
    extern std::atomic<bool> b;   //在别处设置
    while (!b.compare_exchange_weak(expected, true) && !expected);
}

//在这种情况下，只要expected仍为false，表明compare_exchanged_weak()调用伪失败，就保持循环

//另一方面，仅当实际值不等于expected值时compare_exchange_strong()才能返回false，这样可以消除对循环的需求，正如你希望知道它所显示的那样，是否成功改变了一个变量，或者是否有另一个线程抢先抵达

//如果你想要改变变量，无论其初始值是多少(也许是一个依赖于当前值的更新值)，expected的更新就变得很有用，每次经过循环时，expected被重新载入，因此如果没有其他线程在此期间修改其值，那么compare_exchange_weak()或compare_exchange_strong()的调用在下一次循环中应该是成功地的，如果计算待存储的值很简单，为了避免在compare_exchange_weak()可能会伪失败的平台上的双重循环，(于是comapre_exchange_strong()包含一个循环)，则使用compare_exchange_weak()可能是有好处的，另一方面，如果计算待存储的值本身是耗时的，当expected值没有变化时，使用compare_exchange_strong()来避免被迫重新计算待存储的值可能是没有意义的，对于std::atomic<bool>而言这并不重要，毕竟只有两个可能的值，但对于较大的原子类型这会有所不同

//比较/交换函数还有一点非同寻常，它们可以接受两个内存顺序参数，这就允许内存顺序的语义在成功和失败的情况下有所区别，对于一次陈宫调用具有memory_order_acq_rel语义而一次失败的调用有着memory_order_relaxed语义，这想必是极好的，一次失败的比较/交换并不进行存储，因此它无法具有memory_order_release或memory_order_acq_rel语义，因此在失败时，禁止提供这些值作为顺序，你也不应为失败提供比成功更严格的内存顺序，如果你希望memory_order_acquire或者memory_order_seq_cst作为失败语义，你也必须为成功指定这些语义

//如果你没有为失败指定一个顺序，就会嘉定它与成功是相同的，除了顺序的release部分被除去，memory_order_release变成memory_order_relaxed,memory_order_acq_rel变成memory_order_acquire，如果你都没有指定，它们通常默认为memory_order_seq_cst,这位成功和失败都提供了完成的序列顺序，以下对compare_exchange_weak()的两个调用时等价的

static void f4()
{
    std::atomic<bool> b;
    bool expected;
    b.compare_exchange_weak(expected, true, std::memory_order_acq_rel, std::memory_order_acquire);
    b.compare_exchange_weak(expected, true, std::memory_order_acq_rel);
}

//我将把选择内存顺序的后果保留在5.3节

//std::atomic<bool>和std::atomic_flag之间的另外一个区别是std::atomic<bool>可能不是无锁的，为了保证操作的原子性，实现可能需要内部地获得一个互斥锁，对于这种罕见的情况，当它重要时，你可以使用is_lock_free()成员函数检查是否对std::atomic<bool>的啊哦做是无锁的，除了std::atomic_flag，对于所有原子类型，这是另一个特征共同

//原子类型中其次简单的是原子指针特化std::atomic<T*>那么接下来我们看看这些

//5.2.4 std::atomic<T*>上的操作:指针算术运算

//对于某种类型T的指针的原子形式是std::atomic<T*>正如bool的原子形式是std::atomic<bool>一样，接口基本上是相同的，只不过它对相应的指针类型的值进行操作而非bool值，与std::atomic<bool>一样，它既不能拷贝构造，也不能拷贝赋值，虽然它可以从适合的指针中构造和赋值，和必须的is_lock_free()成员函数一样，std::atomic<T*>也有load(),store(),exchanged(),compare_exchange_weak()和compare_exchange_strong()成员函数，具有和std::atomic<bool>相似的语义，也是接受和返回T*而不是bool

//std::atomic<T*>提供的新操作是指针运算符，这种基本的操作是由fetch_add()和fetch_sub()成员函数提供的，可以在所存储的地址上进行原子加法和减法，+=,-=,++,--的前缀与后缀的自增自减等运算符，都提供了方便的封装，运算符的工作方式，与你从内置类型中所期待的一样，如果x是std::atomic<Foo*>指向Foo对象数组的第一项，那么x+=3将它改为指向第四项，并返回一个普通的指向第四项的Foo*，fetch_add()和fetch_sub()有席位的区别，它们返回的是原值(所以x.fetch_add(3)将x更新为指向的第四个值，但是返回一个指向数组中的第一个值的指针)，该操作也称为交换与添加，它是一个原子的读-修改-写操作，就像exchange()和compare_exchange_weak()/compare_exchange_strong()，与其他的操作一样，返回值是一个普通的T*值而不是对std::atomic<T*>对象的引用，因此，调用代码可以基于之前的值执行操作

#include "assert.h"

class Foo {};

static void f5()
{
    Foo some_array[5];
    std::atomic<Foo*> p(some_array);
    Foo* x = p.fetch_add(2);   //将p加2并放回原来的值
    assert(x == some_array);
    assert(p.load() == &some_array[2]);
    x = (p -= 1);   //将p减1并返回新的值
    assert(x == &some_array[1]);
    assert(p.load() == &some_array[1]);
}

//该函数形式也允许内存顺序语义作为一个额外的函数调用参数而被指定

//p.fetch_add(3, std::memory_order_release);

//因为fetch_add()和fetch_sub()都是读-修改-写操作，所以它们可以具有任意的内存顺序标签，也可以参与到释放列表中，对于运算形式，指定顺序语义是不可能的，因为没有办法提供信息，因此这些形式总是具有memory_order_seq_cst语义

//其余的基本原子类型本质上都是一样的，它们都是原子整型，因此彼此都具有相同的接口，区别是相关联的内置类型是不同的，我们将它们视为一个群组

#pragma clang diagnostic pop
