//
//  4_5.cpp
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

//4.3.1 时钟

//就C++标准库所关注的而言，时钟是时间信息的来源，具体来说，时钟是提供以下四个不同部分信息的类

//现在(now)时间
//用来表示从时钟获取到的时间值的类型
//时钟的节拍周期
//时钟是否以均匀的速率进行计时，决定其是否为匀速时钟

//时钟的当前时间可以通过调用该时钟类的静态成员函数now()来获取，例如std::chrono::sysem_clock::now()返回系统时钟的当前时间，对于具体某个时钟的时间点类型，是通过time_poing成员的typedef来指定的，因此some_clock::now()的返回类型是some_clock::time_point

//时钟的街拍周期是由分数秒指定的，它由时钟的period成员typedef给出，每秒走25拍的时钟，就具有std::ratio<1,25>的period，而每2.5秒走一拍的时钟则具有std::ratio<5,2>的period，如果时钟的节拍周期直到运行时方可知晓，或者可能所给的应用程序运行期间可变，则period可以指定为平均的节拍周期，最小可能的节拍周期，或是编写类库的人认为合适的一个值，在所给的一次程序的执行中，无法保证观察到的节拍周期与该时钟所指定的period相符

//如果一个时钟以均匀速率计时(无论该时速是否匹配period)且不能被调整，则该时钟被称为匀速时钟，如果时钟是均匀的，则时钟类的is_steady静态数据成员为true，反之为false，通常情况下，std::chrono::syste_clock是不匀速的，因为始终可以调整，考虑到本地时钟漂移，这种调整甚至是自动执行的，这样的调整可能会引起调用now()所返回的值，比之前调用now()锁返回的值更早，这违背了均匀计时速率的要求，如你马上要看到的那样，匀速始终对于计算超时来说非常重要，因此C++标准库提供形式为std::chrono::steady_clock的均匀时钟，由C++标准库提供的其他时钟包括std::chrono::system_clock(上文已提到)，它代表系统的"真实时间"时钟，并为时间点和time_t值之间的相互转换提供函数，还有std::chrono::high_resolution_clock，它提供所有类库时钟中最小可能的节拍周期(和可能的最高精度)，它实际上可能是其他时钟之一的typedef，这些时钟与其他时间工具都定义在<chrono>类库头文件中

//我们马上要看看如何表示时间点，但在此之前，先来看看时间段是如何表示的

//4.3.2 时间段
//时间段是时间支持中的最简单部分，他们是由std::chrono::duration<>类模板(线程库使用的所有C++时间处理工具均位于std::chrono的命名空间中)进行处理的，第一个模板参数为所代表类型(如int,long,double)，第二个参数是个分数，指定每个时间段单位表示多少秒，例如，以short类型存储的几分钟的数目表示为std::chrono::duration<short,std::ration<60,1>>，因为1分钟有60秒，另一方面，以double类型存储的毫秒数则表示为std::chrono::duration<double, std::ration<1,1000>>，因为1毫秒为1/1000秒

//标准库在std::chrono命名空间中为各种事件段提供了一组预定义的typedef，nanoseconds,microseconds,milliseconds,seconds,minutes和hours，它们均使用一个足够大的整数类型以供表示，以至于如果你希望的话，可以使用合适的单位来比欧式超过500年的时间段，还有针对所有国际单位比例的typedef可供指定自定义时间段时使用，从std::atto(10的负18次方)至std::exa(10的18次方)(还有更大的，若你的平台具有128位整数类型)，例如std::duration<double,std::cent>是以double类型表示的1/100秒的计时

//在无需截断值的场合，时间段之间的转换时隐式的(因此将小时转换成秒是可以的，但将秒转换成小时则不然)，显示转换可以通过std::chrono::duration_cast<>实现

std::chrono::milliseconds ms(54802);
std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(ms);

//结果是阶段而非四舍五入，因此在此例中s值为54

//时间段支持算数运算，因此可以加减时间段来得到新的时间段，或者可以乘除一个底层表示类型(第一个模板参数)的常数，因此5*seconds(1)和seconds(5)或minutes(1)-seconds(55)是相同的，时间段中单位数量的技术可以通过count()成员函数获取，因此std::chrono::milliseconds(1234).count()为1234

//基于时间段的等待是通过std::chrono::duration<>实例完成的，例如，可以等待future就绪最多35秒

#include <future>
#include <chrono>

static int do_some_stuff() { return 1; }
static void do_something_with(int i) {}

void f()
{
    std::future<int> f = std::async(do_some_stuff);   //do_some_stuff的返回值放在std::future中
    if (f.wait_for(std::chrono::milliseconds(35)) == std::future_status::ready)
        do_something_with(f.get());   //get()拿到std::future的结果
}

//等待函数都会返回一个状态以表示等待是否超时，或者所等待的时间是否发生，在这种情况下，你在等待一个future，若等待超时，函数返回std::future_status::timeout若返回future就绪，则返回std::future_status::ready或者如果future任务推迟，则返回std::future_status::deferred，基于时间段的等待使用类库内部的匀速时钟来衡量时间，因此35毫秒意味着35毫秒的逝去时间，即便系统时钟在等待期间经行了调整(向前或向后)，当然，系统调度的多变和OS时钟的不同进度意味着线程之间发出调用并返回的时间时间可能远远长于35毫秒

//在看过时间段后，接下来可以继续看看时间点

//4.3.3时间点

//你还可以从另一个共享同一个时钟的时间点减去一个时间点，结果为指定两个时间点之间长度的时间段，这对于代码块的计时非常有用，例如

static void do_something() {}

void ff()
{
    auto start = std::chrono::high_resolution_clock::now();
    do_something();
    auto stop = std::chrono::high_resolution_clock::now();
    std::cout << "do_something() took " << std::chrono::duration<double, std::chrono::seconds>(stop - start).count() << " seconds" << std::endl;
}

//然而std::chrono::time_point<>实例的时钟参数能做的不仅仅是指定纪元，当你将时间点传给到接受绝对超时的等待函数时，时间点的始终参数可以用来测量时间，当时钟改变时会产生一个重要的影响，因为之二以等待会跟踪时钟的改变，并且在时钟的now()函数返回一个晚于指定超时的值之前都不会返回，如果时钟向前调整，将减少等待的总长度(按照均匀时钟计量)，繁殖如果向后调整，就可能增加等待的总长度

//如你所料，时间点和等待函数的_until变种共同使用，典型用例是在用作从程序中一个固定点的某个时钟::now()开始的偏移量，尽管与系统时钟相关联的时间点可以通过在对用户可见的时间，用std::chrono:;system_clock::to_time_point()静态成员函数从time_t转换而来，例如，如果又一个最大值为500毫秒的时间，来等待一个与条件变量相关的事件，你可以按清单4.11所示来做

//等待一个具有超时的条件变量

#include <condition_variable>
#include <mutex>
#include <chrono>

std::condition_variable cv;
bool done;
std::mutex m;

bool wait_loop()
{
    const auto timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    std::unique_lock<std::mutex> lk(m);
    while (!done)
        if (cv.wait_until(lk, timeout) == std::cv_status::timeout)
            break;
    return done;
}

//如果没有向等待传递断言，那么这是在有时间限制下等待条件变量的推荐方式，这种方式下，循环的总长度是有限的，如4.1.1节所示，当不传入断言时，需要通过循环来使用条件变量，一边处理伪唤醒，如果在循环中使用wait_for()，可能在伪唤醒前，就已结束等待几乎全部时长，并且在经过下一次等待开始后再来一次，这可能会重复任意次，使得总的等待时间无穷无尽

//在看过了指定超时的基础知识后，让我们来看看能够使用超时的函数

//4.3.4 接受超时的函数

//超时的最简单用法，是将延迟添加到特定线程的处理过程中，以便在它无所事事的时候避免占用其他线程的处理时间，在4.1节你曾见过这样的例子，在循环中轮询一个"完成"标记，处理他的两个函数是std::this_thread::sleep_for()和std::this_thread::sleep_until()，它们像一个基本闹钟一样工作，在指定时间段(使用sleep_for())或直至指定的时间点(使用sleep_until())，线程进入睡眠状态，sleep_for()对于那些类似于4.1节中的例子是有意义的,其中一些事情必须周期性地进行，并且逝去的时间是重要的，另一方面，sleep_until()允许安排线程在特定时间点唤醒，这可以用来触发半夜里的备份，或在早上6:00打印工资条，或在做视频回放时暂停线程直至下一帧的刷新

//当然，睡眠斌并不是唯一的接受超时的工具，你已经看到了可以将超时与条件变量和future一起使用，如果互斥元支持的话，甚至可以试图在互斥元获得锁时使用超时，普通的std::mutex和std::recursive_mutex并不支持锁定上的超时，但是std::timed_mutex和std::recursive_timed_mutex支持，这两种类型均支持try_lock_for()和try_lock_until()成员函数，它们可以在指定时间段内或在指定时间点之前尝试获取锁，表4.1展示了C++标准库中可以接受超时的函数及其参数和返回值，列作时间段的参数必须为std::duration<>的实例，而那些列作time_point的必须为std::time_point<>的实例

//表4.1接受超时的函数

std::this_thread                  sleep_for(duration) sleep_until(time_point)
std::condition_variable           wait_for(lock,duration) wait_until(lock, time_point)
std::condition_variable_any
                                  wait_for(lock,duration,predicate) wait_until(lock,time_point,predicate)
std::timed_mutex
std::recursive_timed_mutex        try_lock_for(duration) try_lock_until(time_point)
std::unique_lock<TimedLockable>   unique_lock(lockable,duration) unique_lock(lockable,time_point)
                                  try_lock_for(duration) try_lock_until(time_point)
std::future<ValueType>
std::shared_future<ValueType>     wait_for(duration) wait_until(time_point)

//目前，我已经介绍了条件变量，future，promise和打包任务的机制，接下来是时候看一看更广的图景，以及如何利用它们来简化线程间操作的同步

#pragma clang diagnostic pop
