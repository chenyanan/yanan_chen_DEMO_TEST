//
//  3.9cpp
//  123
//
//  Created by chenyanan on 2017/4/24.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <iostream>
#include <thread>
#include <mutex>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//3.2.7 在作用域之间转移锁的所有权
//因为std::unique_lock实例并没有拥有与其相关的互斥元，所以通过四处移动实例，互斥元的所有权可以在实例之间进行转移，在某些情况下这种转移是自动的，比如从函数中返回一个实例，而在其他情况下，你必须通过调用std::move()来显示实现，从根本上说，这取决于源是否为左值，实变量或对实变量的引用，或者是右值，某种临时变量，如果源为右值，则左右全转移是自动的，而对于左值，多有权转移必须显示的完成，以避免从变量中意外地转移了所有权，std::unique_lock就是可移动但不可复制的类型的例子，关于移动语义的详情，可参阅附录A中A.1.1节

//一种可能的用法，是允许函数锁定一个互斥元，并将此锁的所有权转移给调用者，于是调用者接下来可以在同一个锁的保护下执行额外的操作，下面的代码片段展示了这样的例子:函数get_lock()锁定了互斥元，然后在将锁返回给调用者之前准备数据

static void prepare_data() {}
static void do_something() {}

static std::unique_lock<std::mutex> get_lock()
{
    extern std::mutex some_mutex;
    std::unique_lock<std::mutex> lk(some_mutex);
    prepare_data();
    return lk;   //①
}

static void process_data()
{
    std::unique_lock<std::mutex> lk(get_lock());   //②
    do_something();
}

//因为lk是在函数内声明的自动变量，它可以被直接返回①而无需调用std::move()，编译器负责调用移动构造函数，process_data()函数可以直接将所有权转移到它自己的std::unique_lock实例②，并且对do_something()的调用能够依赖被正确准备了的数据，而无需另一个线程再次期间去修改数据

//通常使用这种模式，是在待锁定的互斥元依赖于程序的当前状态，或者依赖于传递给返回std::unqiue_lock对象的函数的参数的地方，这种用法之一，unique_lock对象的函数的参数的方法，这种用法之一，就是并不直接放回锁，但是使用一个网关类的数据成员，以确保正确锁定了对受保护的数据的访问，这种情况下，所有对该数据的访问都通过这个网关类，当你想要访问数据时，就获取这个王关类的实例(通过调用类似于前面例子中的get_lock()函数)，它会获取锁，然后，你可以通过网关对象的成员函数来访问数据，在完成之后，销毁网关桂香的成员函数来访问数据，在完成后，销毁网关对象，从而释放锁，并允许其他线程访问受保护的数据，这样的网关对象很肯能是可移动的(因此它可以从函数返回)，在这种情况下，锁对象的数据成员也需要是可移动的

//std::unique_lock的灵活性同样允许实例在被销毁之前撤回他们的锁，你可以使用unlock()尘缘函数来实现，就像对于互斥元那样，std::unique_lock支持与互斥元一样的用来锁定和解锁的基本成员函数集合，这是为了让它可以用于通用函数，比如std::lock在std::unique_lock实例被销毁之前释放锁的能力，意味着你可以有选择的在特定的代码分支释放锁，如果很显然不再需要这个锁，这对于应用程序的性能可能很重要，持有锁的时间比所需时间更长，会导致性能下降，因为其他等待该锁的线程，被阻止运行超过了所需的时间

#pragma clang diagnostic pop
