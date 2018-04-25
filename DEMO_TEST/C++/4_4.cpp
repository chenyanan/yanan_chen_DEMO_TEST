//
//  4_4.cpp
//  123
//
//  Created by chenyanan on 2017/4/27.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <iostream>
#include <thread>
#include <mutex>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//4.2.3 生成(std::)promise
//当有一个需要处理大量网路连接的应用程序时，通常倾向于在独立的线程上分别处理每个链接，因为这能是的网络通讯更易于理解也更易于编程，这对于较低的连接数(因为线程数也较低)效果很好，然而，随着连接数的增加，这就变得不那么合适了，大量的县城就会消耗大量操作系统资源，并可能导致大量的上下文切换(当线程数超过了可用的硬件并发)，进而影响性能，在极端情况下，操作系统可能会在其网络连接能力用尽之前，就为运行新的线程而耗尽资源，在具有超大量网络连接的应用中，通常用少量线程(可能仅有一个)来处理链接，每个线程一次处理多个链接

//考虑其中一个处理这种链接的线程，数据包将以基本上随机的顺序来自与待处理的各个连接，同样地，数据包将以随机顺序进行排队发送，在多数情况下，应用程序的其他部分将通过特定的网络连接，等待着数据被成功地发送或是新一批数据被成功地接收

//std::promise<T>提供一种设置值(类型T)方式，它可以在这之后通过相关联的std::future<T>对象进行读取，一对std::promise/std::future为这一设施提供了一个可能的机制，等待中的线程可以阻塞future，同时提供数据的线程可以使用配对中的promise项，来设置相关的值并使future就绪

//你可以通过调用get_future()成员函数来获取与给定的std::promise相关的std::future对象，比如std::packaged_task，当设置完promise的值(使用set_value()成员函数)，future会变为就绪，并且可以用来获取所存储的数值，如果销毁std::promise时未设置值，则将存入一个异常，4.2.4节描述了异常时如何跨越线程转移的

//清单4.10展示了处理如前所述的链接的示例代码，在这个例子中，使用一队std::promise<bool>/std::future<bool>对来标识一块传出数据的成功传输，与future关联的值就是一个简单的成功/失败标志，对于传入的数据包，与future关联的数据为数据包的负载

//清单4.10 使用promise在单个线程中处理多个链接

#include <future>

struct data_package
{
    bool id;
    bool payload;
};

struct outgoing_package
{
    int payload;
    std::promise<bool> promise;
};

typedef bool payload_type;

struct connection_set
{
    struct connection_iterator
    {
        void operator++(int) {}
        connection_set operator++() { return connection_set(); }
        bool operator!=(const connection_iterator& rhs) { return this->data != rhs.data; }
        connection_set* operator->() { return data; }
        
        connection_set* data;
    };
    
    std::promise<bool> promise;
    
    connection_iterator begin() { return connection_iterator(); }
    connection_iterator end()   { return connection_iterator(); }
    bool has_incoming_data() { return true; }
    bool has_outgoing_data() { return true; }
    data_package incoming() { return data_package(); }
    outgoing_package top_of_outgoing_queue() { return outgoing_package(); }
    void send(int payload) {}
    std::promise<bool>& get_promise(bool id) { return promise; }
};

bool done(const connection_set& conn) { return true; }

void process_connections(connection_set& connections)
{
    while (!done(connections))   //①
    {
        for (connection_set::connection_iterator connection = connections.begin(), end = connections.end(); connection != end; ++connection)   //②
        {
            if (connection->has_incoming_data())   //③
            {
                data_package data = connection->incoming();
                std::promise<payload_type>& p = connection->get_promise(data.id);   //④
                p.set_value(data.payload);
            }
            if (connection->has_outgoing_data())   //⑤
            {
                outgoing_package data = connection->top_of_outgoing_queue();
                connection->send(data.payload);
                data.promise.set_value(true);   //⑥
            }
        }
    }
}

//函数process_connections()一直循环到done()返回ture①，每次循环中，轮流检查每个连接②，在有传入数据时获取之③或是发搜狗队列中的传出数据⑤，此处假定一个输入数据包具有ID和包含实际数据在内的负载，此ID被映射至std::promise(可能是通过在关联容器中进行查找)④，并且该值被设为数据包的负载，对于传出的数据包，数据包取自传出队列，并实际上通过此连接发送，一旦发送完毕，渔船出数据关联的promise被设为true以表示传输成功⑥，此映射对于实际网络协议是否完好，取决于协议本身，这种promise/future风格的结构可能并不适用于某特定情况，尽管它确实与某些操作系统支持的异步I/O具有相似的结构

//迄今为止的所有代码完全忽略了异常，虽然想想一个万物都始终运行完好的世界是美好的，但却不切实际，有时候磁盘装满了，有时候你要找的东西恰好不在那里，有时候网络故障，有时数据库损坏，如果你正在需要结果的县城中执行操作，代码可能只使用异常报告了一个错误，因此仅仅因为你想用std::packaged_task或std::promise就限制性地要求所有事情都正常工作是不必要的，因此C++标准库提供一个简便的方式，来处理则重情况下的异常，并允许他们作为相关结果的一部分而保存

//4.2.4 为future保存异常

//考虑下面剪短的代码片段，如果将-1传入square_root()函数，会引发一个异常，同时将被调用者所看到

#include "math.h"

double square_root(double x)
{
    if (x < 0)
        throw std::out_of_range("x < 0");
    return sqrt(x);
}

//现在假设不是仅从当前线程调用square_root():
//double y = square_root(-1);

//而是以异步调用的形式运行调用:
//std::future<double> f = std::async(square_root, -1);
//double y = f.get();

//两者行为完全一致自然是最理想的，与y得到函数调用的任意一种结果完全一样，如果调用f.get()的线程能像在单线程情况下一样，能够看到里面的异常，那是极好的，

//实际情况则是，如果作为std::asyne一部分的函数调用引发了异常，该异常会被存储在future中，代替所存储的值，future变为就绪，并且对get()的调用会重新引发所存储的异常(注:重新引发的是原始异常对象抑或其副本，C++标准并没有指定，不同的编译器和库在此问题上做出了不同的选择)，这同样发生在将函数封装如std::packaged_task的时候，当任务被调用时，如果封装的函数引发异常，该异常代替结果存入future，准备在调用get()时引发

//顺理成章，std::promise在显示地函数调用下提供相同的功能，如果期望存储一个异常而不是一个值，则调用set_exception()成员函数而不是set_value()，这通常是在引发异常作为所发的一部分时用在catch块中，将该异常填入promise

#include <exception>

double calculate_value() { return 0.0; }

extern std::promise<double> some_promise;

void f()
{
    try
    {
        some_promise.set_value(calculate_value());
    }
    catch(...)
    {
        some_promise.set_exception(std::current_exception());
    }
}

//这里使用std::current_exception()来获取已引发的异常，作为替代，可以使用std::copy_exception()直接存储新的异常而不引发

void ff()
{
    some_promise.set_exception(std::make_exception_ptr(std::logic_error("foo ")));
}

//在异常的类型已知时，这比使用try/catch块更为简洁，并且应该优先使用，这不仅仅简化了代码，也为编译器提供了更多的优化代码的机会

//另一种将异常存储至future的方式，是销毁与future关联的std::promise或std::packaged_task而无需在promise上调用设置函数或是调用打包任务，在任何一种情况下，如果future尚未就绪，std::promise或std::packaged_task的析构函数会将具有std::future_error异常存储在相关联的状态中，通过创建future，你承诺提供一个值或异常，而通过销毁该值或异常的来源，你违背了承诺，在这种情况下如果编译器没有将任何东西存进future，等待中的线程可能会永远等下去

//到目前为止，所有的例子都是用了std::future，然而，std::future有其局限性，最起码，只有一个线程能等待结果，如果需要多于一个的线程等待同一事件，则需要使用std::shared_future来代替

//4.2.5 等待自多个线程
//尽管std::future能处理从一个线程向另一个线程转移数据所需的全部必要的同步，但是调用某个特定的std::future实例的成员函数却并没有相互同步，如果从多个线程访问单个std::future对象而不进行额外的同步，就会出现数据竞争和未定义行为，这是有意为之的，std::future模型同意了异步结果的所有权，同时get()的单发性质使得这样的并发访问没有意义，只有一个线程可以获取值，因为在首次调用get()后，就没有任何可获取的值留下了

//如果你的并发代码的绝妙设计要求多个线程能够等待同一个事件，目前还无需失去信心，std::shared_future完全能够实现这一点，鉴于std::future是仅可移动的，所以所有权可以在实例间转移，但是一次只有一个实例指向特定的异步结果，std::shared_future实例是可复制的，因此可以有多个对象引用同一个相关状态

//现在，即便有了std::shared_future各个对象的成员函数仍然是不同步的，所以为了避免从多个线程访问单个对象时出现数据竞争，必须使用锁来保护访问，首选的使用方式，是用一个对象的副本来代替，并且让每个线程访问自己的副本，从多个线程访问共享的异步状态，如果每个线程都是通过自己的std::shared_future对象去访问该状态，那么就是安全的，见图4.1(2064/10923)

//std::shared_future的一个潜在用处，是实现类似复杂电子表格的并行执行，每个单元都有一个单独的终值，可以被多个其他单元格中的公式使用，用来计算各个单元格结果的公式可以使用std::shared_future来引用第一个单元，如果所有独立单元格的公式被并行执行，那些可以继续完成的任务可以进行，而那些依赖于其他单元格的公式将阻塞，直到其依赖关系准备就绪，这就使得系统能最大限度地利用可用的硬件并发

//引用了异步状态的std::shared_future实例可以通过引用这些状态的std::future实例来构造，由于std::future对象不和其他对象共享异步状态的所有权，因此该所有权必须通过std::move转移到std::shared_future将std::future留在空状态，就像它被默认构造了一样

#include "assert.h"

void fff()
{
    std::promise<int> p;
    std::future<int> f(p.get_future());
    assert(f.valid());   //①future f是有效的
    std::shared_future<int> sf(std::move(f));
    assert(!f.valid());   //②f不再有效
    assert(sf.valid());   //③sf现在有效
}

//此处，future f刚开始是有效的①，因为它引用了promise p的同步状态，但是将状态转移至sf后，f不再有效②，而sf有效③

//正如其他可移动的对象那样，所有权的转移对于右值是隐式，因此可以从std::promise对象的get_future()成员函数的返回值直接构造一个std::shared_future例如

std::promise<std::string> p;
std::shared_future<std::string> sf(p.get_future());   //①所有权的隐式转移

//此处，所有权的转移是隐式的，std::shared_future<>根据std::future<std::string>类型的右值进行构造①

//std::future具有一个额外特性，即从变量的初始化自动推断变量类型的功能，使得std::shared_future的使用更加方便(参见附录A中A.6节)，std::future具有一个share()成员函数，可以构造一个新的std::shared_future并且直接将所有权转移给它，这可以节省大量的录入，也是代码更易于更改

#include <map>

/*

 std::promise<std::map<SomeIndexType, SomeDataType, SomeComparator, SomeAllocator>::iterator> p;
 auto sf = p.get_future().share();
 
 */

//这种情况下，sf的类型被推断为相当拗口的std::shared_future<std::map<SomeIndexType, SomeDataType, SomeComparator, SomeAllocator>::interator>，如果比较器或分配器改变了，仅需改变promise的类型，future的类型将自动更新以匹配

//有些时候，你会想要限制等待时间的时长，无论是因为某段代码能够占用的时间有着硬性的限制，还是因为如果时间不会很快发生，线程就可以去做其他有用的工作，为了处理这个功能，许多等待函数具有能够制定超时的变量

//4.3 有时间限制的等待

//前面介绍的所有阻塞调用都会阻塞一个不确定的时间段，挂起线程直至等待的时间发生，在许多情况下这是没问题的，但在某些情况下你会希望给等待时间加一个限制，这就使得能够发送某种形式的"我还活着"的消息给交互用户或者另一个进程，或是在用户已经放弃等待并且按下取消键时，干脆泛起等待

//有两类可供指定的超时，一为基于时间段的超时，即等待一个指定的时间长度(例如30ms)，或是绝对超时，即等到一个指定的时间点(例如世界标准时间2011年11月30日17:30:15.045987023)，大多数等待函数提供处理这两种形式超时的变量，处理基于时间段超时的变量具有_for后缀，而处理绝对超时的变量具有_until后缀

//例如，std::conditioin_variable具有两个重载版本的wait_for()成员函数和两个重载版本的wait_until()成员函数，对应于两个重载版本的wait()，一个重载只是等待到收到信号，或超时，或发生伪唤醒，另一个重载在唤醒时检测所给的断言，并只在所给的断言为true(以及条件变量已收到信号)或超时的情况下才返回

//在细看使用超时的函数之前，让我们从时钟开始，看一看在C++是如何指定时间的

#pragma clang diagnostic pop
