//
//  3_11.cpp
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

//3.3 用于共享数据保护的替代工具
//虽然互斥元时最通用的机制，但提到保护共享数据时，他们并不是唯一的选择，还有别的替代品，可以在特定情况下提供更恰当的保护

//一个特别极端(但却相当常见)的情况，就是共享数据只在初始化才需要并发访问的保护，但在那之后却不需要显示同步，这可能使因为数据是已经创建就是只读的，所以就不存在可能的同步问题，或者是因为必要的保护作为数据上操作的一部分被隐式地执行，在任何一种情况中，在数据被初始化之后锁定互斥元，纯粹是为了保护初始化，这是不必要的，并且对性能会产生的不必要的打击，为了这个原因，C++标准提供了一种机制，出翠微了在初始化过冲中保护共享数据

//3.3.1 在初始化时保护共享数据
//袈裟你有一个构造起来非常昂贵的共享资源，只有当实际需要时你才会要这样做，也许，它会打开一个数据库连接或分配大量的内存，像这样的延迟初始化在单线程中是很常见的，每个请求资源的操作首先检查它是否已经初始化，如果没有就在使用之前初始化之

class some_resource
{
public:
    void do_something() {}
};

std::shared_ptr<some_resource> resource_ptr;

static void foo_one()
{
    if (!resource_ptr)
        resource_ptr.reset(new some_resource);
    resource_ptr->do_something();   //①
}

//如果共享资源本身对于并发访问是安全的，当将其转换为多线程代码时唯一需要保护的部分就是初始化①，但是像清单3.11中这样的朴素的转换，会引起使用该资源的线程产生不必要的序列化，这是因为每个线程都必须等待互斥元，以检查资源是否已经被初始化

//清单3.11 使用互斥元进行线程安全的延迟初始化

std::mutex resource_mutex;

static void foo_two()
{
    std::unique_lock<std::mutex> lk(resource_mutex);   //所有的线程在这里被序列化
    if (!resource_ptr)
        resource_ptr.reset(new some_resource);   //只有初始化需要被保护
    lk.unlock();
    resource_ptr->do_something();
}

//这段代码是很常见的，不必要的序列化问题已足够大，以至于许多人都试图想出一个更好的方法来实现，包括臭名昭著的二次检查锁定模式，在不获取锁①(在下面的代码中)的情况下首次读取指针，并仅当次指针为NULL时获得该锁，一旦已经获取了锁，改制真要被再次检查②(这就是二次检查的部分)，以防止在首次检查和这个县城获取锁之间，另一个线程就已经完成了初始化

static void undefined_behaviour_with_double_checked_locking()
{
    if (!resource_ptr)   //①
    {
        std::lock_guard<std::mutex> lk(resource_mutex);
        if (!resource_ptr)   //②
            resource_ptr.reset(new some_resource);   //③
        resource_ptr->do_something();   //④
    }
}

//不幸的是，这种模式因某个原因而臭名昭著，它有可能产生恶劣的竞争条件，因为在锁外部的读取①与锁内部由另一线程完成的写入不同步③，这就因此创建了一个竞争条件，不仅涵盖了指针本身，还涵盖了指向的对象，就算一个线程看到另一个线程写入的指针，它也可能无法看到新创建的some_resource实例，从而导致do_something()④的调用在不正确的值上运行，这死一个竞争条件的例子，该类型的竞争条件被C++标准定义为数据竞争，因此被定义为未定义行为，因此这是肯定要避免的，内存模型的消息讨论参见第五章，包括了什么构成数据竞争

//C++标准委员会也发现这是一个重要的场景，所以C++标准库提供了std::once_flag和std::call_once来处理这种情况，与其锁定互斥元并显示地检查指针，还不如每个线程都可以使用std::call_one，到std::call_once返回时，指针将会被某个线程初始化(以完全同步的方式)，这样就安全了，使用std::call_once比现实使用互斥元通常会有更低的开销，特别是初始化已经完成的时候，所以在std::call_once符合所要求的功能时应优先使用之，下面的例子展示了与清单3.11相同的操作，改写为使用std::call_once，在这种情况下，通过调用函数来完成初始化，但是通过一个担忧函数调用操作符的类实例也可以很容易地完成初始化，与标准库中接受函数或者断言作为参数的大部分函数类似，std::call_once可以与任意函数或可调用对象合作

//std::shared_ptr<some_resource> resource_ptr;
std::once_flag resource_flag;   //①

void init_resource()
{
    resource_ptr.reset(new some_resource);
}

static void f()
{
    std::call_once(resource_flag, init_resource);
    resource_ptr->do_something();
}

//在这个例子中，std::once_flag①和被初始化的数据都是命名空间作用域的对象，但是std::call_once()可以很容易地用于类成员的延迟初始化，如清单3.12所示

//清单3.12 使用std::call_once的线程安全的类成员延迟初始化

struct data_packet {};
struct connection_info {};
struct connection_handle
{
    void send_data(const data_packet& packet) {}
    data_packet receive_data() { return data_packet(); }
};
struct connection_manager
{
    static connection_handle open(const connection_info& connection_info) { return connection_handle(); }
};

class X
{
private:
    connection_info connection_details;
    connection_handle connection;
    std::once_flag connection_init_flag;
    
    void open_connection()
    {
        connection = connection_manager::open(connection_details);
    }
public:
    X(const connection_info& connection_details_) : connection_details(connection_details_) {}
    void send_data(const data_packet& data)   //①
    {
        std::call_once(connection_init_flag, &X::open_connection, this);   //②
        connection.send_data(data);
    }
    data_packet receive_data()   //③
    {
        std::call_once(connection_init_flag, &X::open_connection, this);   //②
        return connection.receive_data();
    }
};

//在这个例子中，初始化由首次调用send_data()①或是由首次调用receive_data()③来完成，使用成员函数open_connection()来初始化数据，同样需要将this这孩子很传入函数，和标准库中其他接受可调用对象的函数一样，比如std::thread和std::bind()的构造函数，这是通过传递一个额外的参数给std::call_once()来完成的②

//值得注意的是，像std::mutex std::once_flag的实例是不能被复制或移动的，所以如果想要这样吧它们作为类成员来使用，就必须显示定义这些你所需要的特殊成员函数

//一个在初始化过程中可能会有竞争条件的场景，是将局部变量声明为static的，这种变量的初始化，被定义为在时间控制首次经过其声明时发生，对于多个调用该函数的线程，这意味着可能会有针对定义"首次"的竞争条件，在许多C++11之前的编译器上，这个竞争条件在实践中是有问题的，因为多个线程可能都认为他们是第一个，并试图去初始化该变量，又或者线程可能都认为它们是第一个，并试图去初始化该变量，又或者线程可能会在初始化已在另一个线程上启动但尚未完成之时试图使用它，在C++11中，这个问题得到了解决，初始化被定义为只发生在一个线程上，并且其他线程不可以继续直到初始化完成，所以竞争条件仅仅在于哪个线程会执行初始化，而不会有更多别的问题，对于需要单一全局实例的场合，这可以用作std::call_once的替代品

class my_class {};

my_class& get_my_class_instance()
{
    static my_class instance;   //①初始化保证线程是安全的
    return instance;
}

//多个线程可以继续安全地调用get_my_class_instance()①，而不必担心初始化时的竞争条件

//保护金用于初始化的数据是更普遍的场景下的一个特例，那些很少更新的数据结构，对于大多数时间而言，这样的数据结构是只读的，因而可以毫无顾忌地被多个线程同时读取，但是数据结构偶尔可能需要更新，这里我们所需要的是一种承认这一事实的保护机制



#pragma clang diagnostic pop
