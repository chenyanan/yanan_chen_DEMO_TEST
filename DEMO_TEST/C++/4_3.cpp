//
//  4_3.cpp
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

//4.2 使用future等待一次性事件
//假设你要乘飞机去国外度假，在到达机场并且完成了各种值机手续后，你仍然需要等待航班准备等级的通知，也需要几个小时，当然，你可以找到一些方法来消磨时间，比如看书，上网，或者在昂贵的机场咖啡厅吃东西，但是从更笨上说，你只是在等待一件事情，登记时间到了的信号，不仅如此，一个给定的航班只进行一次，你下次去度假的时候，就会等待不同的航班

//C++标准库使用future为这列一次性时间建模，如果一个线程需要等待特定的一次性事件，那么它就会获取一个future来代表这一事件，然后，该线程可以周期性地在这个future上等待一小段时间以检查事件是否发生(检查发出告示板)，而在检查间隙执行其他的任务(在高价咖啡厅吃东西)，另外，它还可以去做另外一个任务，直到其所需的时间已发生才继续进行，随后就等待future变为就绪(ready)，future可能会有与之相关的数据(比如你的航班在哪个登机口登机)，或可能没有，一旦时间已经发生(即future已变为就绪)，future无法复位

//C++标准库中有两类future，是由<future>库的头文件中声明的两个类模板实现的:唯一期望和共享期望(std::future<>和std::shared_future<>)，这两个类模板是参照std::unique_ptr和std::shared_ptr建立的，std::future的实例是仅有的一个指向其关联时间的实例，而多个std::shared_future的实例则可以指向同一个事件，对后者而言，所有实例将同时变为就绪，并且它们都可以访问所有与该事件相关联的数据，这些关联的数据，就是这两种future称为模板的原因，像std::unique_ptr和std::shared_ptr一样，末班参数就是关联数据的类型，std::future<void>，std::shared_future<void>模板特化应该用于无关联数据的场合，虽然future被用于线程间通讯，但是future对象本身却并不提供同步访问，如果多个线程需要访问同一个future对象，他们必须通过互斥元或其他同步机制来保护访问，如第3章所述，然而，正如你将在4.2.5节中看到的，多个线程可以分别访问自己的std::shared_future<>副本而无需进一步的同步，即使它们都指向同一个异步结果

//最基本的一次性事件是在后台运行着的计算结果，早在第2章中你就看到过std::thread并没有提供一种简单的从这一任务中返回值得方法，我保证这将在第4章中通过使用future加以解决，现在是时候去看看如何做了

//4.2.1 从后台任务中返回值
//假设你有一个长期运行的计算，预期最终将得到一个有用的结果，但是现在，你还不需要这个值，也许你已经找到一种方法来确定生命，宇宙，万物的答案，这是从Douglas Adams那里偷来的一个例子，你可以启动一个新的线程来执行该计算，但这也意味着你必须注意将结果传回来，因为std::thread并没有提供直接的机制来这样做，这就是std::async函数模板(同样声明于<future>头文件中)的由来

//在不需要立刻得到结果的时候，你可以使用std::async来启动一个异步任务，std::async返回一个std::future对象，而不是给你一个std::thread对象让你在上面等待，std::future对象最终将持有函数的返回值，当你需要这个值时，只要在future上调用get()，线程就会阻塞直到future就绪，然后返回该值，清单4.6展示了一个简单的例子

//清单4.6 使用std::future获取异步任务的返回值

#include <future>
#include <iostream>

int find_the_answer_to_ltuae();
void do_other_stuff();

int main()
{
    std::future<int> the_answer = std::async(find_the_answer_to_ltuae);
    do_other_stuff();
    std::cout << "The answer is " << the_answer.get() << std::endl;
}

//std::asyne允许你通过将额外的参数添加到调用中，来讲附加参数传递给函数，这与std::thread是同样的方式，如果第一个参数是指向成员函数的指针，第二个参数则提供了用来应用该成员函数的对象(直接地，或通过指针，或封装在std::ref中)，其余的参数则作为参数传递给该成员函数，否则，第二个及后续的参数将作为参数，传递给第一个参数所指定的函数或可调用对象，和std::thread一样，如果参数是右值，则通过移动原来的参数来创建副本，这就允许使用只可移动的类型同时作为函数和参数，请看清单4.7

//清单4.7 使用std::async来将参数传递给函数

#include <string>
#include <future>

struct X
{
    void foo(int, const std::string&);
    std::string bar(const std::string&);
};

X x;
auto f1 = std::async(&X::foo, &x, 42, "hello");   //调用p->foo(42,"hello"),其中p是&x
auto f2 = std::async(&X::bar, x, "goodbye");      //调用tmpx.bar("goodbye"),其中tmpx是x的副本

struct Y
{
    double operator() (double);
};

Y y;
auto f3 = std::async(Y(), 3.141);                 //调用tmpy(3.141),其中tmpy是从Y()移动构造的
auto f4 = std::async(std::ref(y), 2.718);         //调用y(2.718)

X baz(X& x) { return X(); }
auto ff = std::async(baz, std::ref(x));           //调用baz(x)

class move_only
{
public:
    move_only();
    move_only(move_only&&);
    move_only(const move_only&) = delete;
    move_only& operator=(move_only&&);
    move_only& operator=(const move_only&) = delete;
    
    void operator()() {};
};
auto f5 = std::async(move_only());                //调用tmp(),其中tmp()是从std::move(move_only())构造的

//默认情况下，std::async是否启动一个新线程，或者在等待future时任务是否同步运行都取决于具体实现方式，在大多数情况下这就是你所想要的，但你可以在函数调用之前使用给一个额外的参数来指定酒精使用何种方式，这个参数为std::launch类型，可以使std::lanuch：：defered以表明该函数调用将会延迟，直到在future上调用wait()或get()为止，或者是std::launch::async以表明该函数必须运行在他自己的线程上，又或者是std::lanuch::deferred | std::lanuch::async以表明可以由具体实现来选择，最后一个选项是默认的，如果函数调用被延迟，它有可能永远都不会实际运行，例如，

void f()
{
    auto f6 = std::async(std::launch::async, Y(), 1.2);   //在新线程中运行
    auto f7 = std::async(std::launch::deferred, baz, std::ref(x));   //在wait()或get()中运行
    auto f8 = std::async(std::launch::deferred | std::launch::async, baz, std::ref(x));   //由具体实现来选择
    auto f9 = std::async(baz, std::ref(x));   //由具体实现来选择
    f7.wait();   //调用了延迟的函数
};
//f7.wait();   //调用延迟了的函数

//正如你稍后将在本章看到，并将在第8章中再次看到的那样，使用std::async能够轻易地将算法转化成可以并行运行的任务，然而，这并不是将std::future与任务相关联的唯一方式，你还可以通过将任务封装在std::packaged_task<>类模板的一个实例中，或者通过编写代码，用std::promise<>类模板显示设置值等方式来实现，std::packaged_task是比std::promise更高层次的抽象，所以我将从它开始

//4.2.2 将任务与future相关联
//std::packaged_task<>将一个future绑定到一个函数或可调用对象上，当std::packaged_taskM<>对象被调用时，它就调用相关联的函数或可调用对象，并且让future就绪，将返回值作为关联数据存储，这可以被用作线程池的构建，或者其他任务管理模式，例如在每个任务自己的线程上运行，或在一个特定的后台线程上按顺序运行所有任务，如果一个大型操作可以分成许多自包含的子任务，其中每一个都可以被封装在一个std::packaged_task<>实例中，然后将该实例传给任务调度器或线程池，这样就抽象出了任务的详细信息，调度程序仅需处理std::packaged_task<>实例，而非各个函数

//std::packaged_task<>类模板的模板参数为函数签名，比如void()标识无参数无返回值的函数，或是像int(std::string&, double*)标识接受std::string的非const引用和指向double的指针，并返回int的函数，当你构造std::packaged_task实例的时候，你必须传入一个函数或可调用对象，它可以接受指定的参数并且返回指定的返回类型，类型无需严格屁屁额，你可以用一个接受int并返回float的函数构造std::packaged_task<double(double)>，因为这些类型是可以隐式转换的

//指定的函数签名的返回类型确定了从get_future()成员函数返回的std::future<>的类型，而函数签名的参数列表用来指定封装任务的函数调用运算符的签名，例如，std::packaged_task<std::string(std::vector<char>*,int)>的部分类定义如清单4.8所示

//清单4.8 std::packaged_task<>特化的部分类定义

template<>
class std::packaged_task<std::string(std::vector<char>*,int)>
{
public:
    template <typename Callable>
    explicit packaged_task(Callable&& f);
    std::future<std::string> get_future();
    void operator()(std::vector<char>*, int);
};

//该std::packaged_task对象是一个可调用对象，它可以被封装入一个std::function对象，作为线程函数传给std::thread或传给需要可调用对象的另一个函数，或者干脆直接调用，当std::packaged_task作为函数对象被调用时，提供给函数调用运算符的参数被传给所包含的函数，并且将返回值作为异步结果，存储在由get_future()获取的std::future中，因此你可以将任务封装在std::packaged_task中，并且在把std::packaged_task对象传到别的地方进行适当调用之前获取future，当你需要结果时，你可以等待future变为就绪，清单4.9的例子实际展示了这一点

//线程之间传递任务
//许多GUI框架要求从特定的线程来完成GUI的更新，所以如果另一个线程需要更新GUI,它必须向正确的线程发送消息来实现这一点，std::packaged_task提供了一种更新GUI的方法，该方法无需为每个与GUI相关的活动获取自定义消息

//清单4.9 使用std::packaged_task在GUI线程上运行代码

#include <deque>
#include <mutex>
#include <future>
#include <thread>
#include <utility>

std::mutex m;
std::deque<std::packaged_task<void()>> tasks;

bool gui_shutdown_message_received();
void get_and_process_gui_message();

void gui_thread()   //①
{
    while(!gui_shutdown_message_received())   //②
    {
        get_and_process_gui_message();   //③
        std::packaged_task<void()> task;
        {
            std::lock_guard<std::mutex> lk(m);
            if (tasks.empty())   //④
                continue;
            task = std::move(tasks.front());   //⑤
            tasks.pop_front();
        }
        task();   //⑥
    }
}

std::thread gui_bg_thread(gui_thread);

template <typename Func>
std::future<void> post_task_for_gui_thread(Func f)
{
    std::packaged_task<void()> task(f);   //⑦
    std::future<void> res = task.get_future();   //⑧
    std::lock_guard<std::mutex> lk(m);
    tasks.push_back(std::move(task));   //⑨
    return res;   //⑩
}

//此代码非常简单，GUI线程①循环直到收到通知GUI停止的消息②，反复轮询待处理的GUI消息③，比如用户点击，以及任务队列中的任务，如果队列中没有任务④，则再次循环，否则，从任务队列中提取任务⑤，接触队列中的锁，并运行任务⑥，当任务完成时，与该任务相关联的future被设定为就绪

//在队列上发布任务也同样简单那，利用所提供的函数创建一个新任务包⑦，通过调用get_future()成员函数从任务中获取future⑧，同时在返回future到调用处之前⑨将任务至于列表之上⑩，发出消息给GUI线程的带啊如果需要知道任务已完成，则可以等待该future，若无需知道，则可以丢弃该future

//本示例中的任务使用std:packaged_task<void()>它封装了一个接受零参数且返回void的函数或可调用对象(如果他返回了别的东西，则返回值被丢弃)，这是最简单的任务，但如你在前面所看到的，std:packaged_task也可以用于更复杂的情况，通过指定一个不同的函数签名作为模板参数，你可以改变返回类型(以及在future的关联状态中存储的数据类型)核函数调用运算符的参数类型，这个示例可以进行简单的扩展，让那些在GUI线程上运行的任务接受参数，并且返回std::future中的值，而不是仅一个完成指示符

//那些无法用一个简单函数调用表达的任务和那些结果可能来自不止一个地方的任务又当如何？这些情况都可以通过第三种创建future的方式来处理:使用std::promise来显示地设置值

#pragma clang diagnostic pop
