//
//  4_7.cpp
//  123
//
//  Created by chenyanan on 2017/4/28.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <iostream>
#include <thread>
#include <mutex>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//4.4.2 具有消息传递的同步操作
//CSP的思想很简单，如果没有共享数据，则每一个线程可以完全独立地推理得到，只需基于它对所接收到消息时，它会根据初始状态进行操作，并以某种范式更新其状态，并可能像其他线程发送一个或多个消息，编写这种词按成的一种方式，是将其形式化并实现一个有限状态机模型，但这并不是唯一的方式，状态机在应用程序结构中是隐式的，在任意给定的情况下，究竟哪种方法佳，取决于具体形式的行为需求和编程团队的专长，但是选择实现每一个线程，则将其分割成独立的进程可能会从共享数据并发中移除很多复杂性，因而使得变成更加容易，降低了错误率

//真正的通讯序列进程并不共享数据，所有的通信都通过消息队列，但由于C++线程共享一个地址空间，因此不可能强制执行这一需求，这就是准则介入的地方，作为应用程序或类库的作者，我们的职责是确保我们不在线程间共享数据，当然，为了线程之间的通信，线程队列必须是共享的，但是其细节可以封装在类库内

//想象一下，你正在实现ATM的代码，此代码需要处理人试图取钱的交互和与相关银行的交互，还要控制物理机器接受此人的卡片，显示恰当的信息，处理按键，出钞并退回用户的卡片

//处理这一切事情的其中一种方法，是将代码分成三个独立的线程，一个处理物理机器，一个处理ATM逻辑，还有一个与银行进行通信，这些线程可以单纯的通过传递消息而非共享数据进行通信，例如，当有人在及其旁边将卡插入或按下按钮时，处理机器的线程可以发送消息至逻辑线程，同时逻辑线程将发送消息至机器线程，指示要发多少钱等等

//对ATM逻辑进行建模的一种范式，是将其视为一个状态机，在每种状态中，线程等待可接受的消息，然后对其进行处理，这样可能会转换到一个新的状态，并且循环继续，一个简单实现中所涉及的状态如图4.3所示，在这个简化了的实现中，系统等待卡片插入，一旦卡片插入，便等待用户输入其密码，每次一个数字，他们可以删除最后输入的数字，一单输入的数字足够多，就验证密码，如果密码错误，则结束，将卡片退回给用户，并继续等待有人插入卡片，如果密码正确，则等待用户取消或选择取出的金额，如果用户取消，则结束，并推出银行卡，如果用户选择了一个金额，则等待银行确认后发现金并退出其卡片，或者显示"资金不足"的消息并退出其卡片，显然，真正的ATM比这复杂得多，但这已经足以阐述整个想法

//图4.3ATM的简单状态机模型

//有了ATM逻辑的状态机设计之后，就可以使用一个具有标识每一个状态的成员函数的类来实现之，每一个成员函数都可以等待指定的一组传入消息，并在它们到达后进行处理，其中可能触发切换到另一个状态，每个不同的消息类型可以由一个独立的struct来表示，清单4.15展示了这样一个系统中ATM逻辑的部分简单实现，包括主循环和第一个状态的实现，即等待卡片插入

//如你所见，所有消息传递锁必须的同步完全隐藏于消息传递库内部(其基本实现以及本事例的完成代码见附录C)

//清单4.15 ATM逻辑类的简单实现

struct card_inserted
{
    std::string account;
};

class atm
{
    messaging::receiver incoming;
    messaging::sender bank;
    messaging::sender interface_hardware;
    void (atm::*state)();
    
    std::string account;
    std::string pin;
    
    void waiting_for_card()   //①
    {
        interface_hardware.send(display_enter_card());   //②
        incoming.wait()   //③
        .handle<card_insterted>([&](const card_inserted& msg) {   //④
            account = msg.account;
            pin = "";
            interface_hardware.send(display_enter_pin());
            state = &atm::getting_pin;
        });
    }
    
    void getting_pin();
public:
    void run()   //⑤
    {
        state = &atm::waiting_for_card();   //⑥
        try {
            for (;;)
            {
                (this->*state)();   //⑦
            }
        }
        catch (const messaging::close_queue &)
        {
            
        }
    }
};

//正如已经提到的，这里所描述的实现是从ATM所需的真正逻辑中粗略简化而来的，但这确实给你一种消息传递的编程风格的感受，没有必要去考虑同步和并发的问题，只要考虑在任意一个给定的店，可能会接受到哪些消息，以及该发送哪些消息，ATM逻辑的状态机在任意一个给定的店，可能会接收到哪些消息，以及该发送哪些消息，ATM逻辑的状态机在单线程上运行，而系统的其他部分，比如阴阳的接口和终端的借口，则在独立的线程上运行，这种程序设计风格被称作行为角色模型，系统中有多个离散的角色(均运行在独立线程上)，用来互相发送消息以完成手头任务，除了直接通过消息传递的状态外，没有任何共享状态

//执行是run()成员函数开始的⑤，设置初始状态为waiting_for_card⑥并重复执行代表当前状态(不管它是什么)的成员函数⑦，状态函数就是atm类的简单成员函数，waiting_for_card状态函数①也很简单，发送一则消息至界面以显示"等待卡片"的消息②，接着等待要处理的消息，这里能处理的唯一消息类型是card_inserted消息③，可以通过lambda函数进行处理④，你可以将任意函数或函数对象传递给handle函数，但是像这种简单的情况，用lambda是最容易的，注意handle()函数调用是链接至wait()函数的，如果接收到的消息不匹配指定的类型，他将被丢弃，线程将继续等待直至接收到匹配的消息

//lambda函数本身只是将卡片中的账户号码缓存至一个成员变量中，清除当前密码，发送消息给接口硬件以显示要求用户输入其密码，改为"获取密码"状态，一旦消息处理程序完成，状态函数即返回，同时主循环调用新的状态函数⑦

//getting_pin状态函数略有些复杂，它可以处理三种不同类型的消息，如图4.3所示，有清单4.16加以展示

//清单4.16 简单ATM实现的getting_pin状态函数

void atm::getting_pin()
{
    incoming.wait().handle<digit_pressed>([&](const digit_pressed& msg) {   //①
        const unsigned pin_length = 4;
        pin += msg.digit;
        if (pin.length() == pin_length)
        {
            bank.send(verify_pin(account, pin, incoming));
            state = &atm::verigying_pin;
        }
        
    }).handle<clear_last_pressed>([&](const clear_last_pressed& msg) {   //②
        if (!pin.empty())
            pin.resize(pin.length() - 1);
            
    }).handle<cancel_pressed>([](const cancel_pressed& msg) {   //③
        state = &atm::done_processing;
    });
}

//这时，有三种能够处理的消息类型，所以wait()函数有是三个handle()调用链接至结尾①、②、③，每一次对handle()的调用将消息类型指定为模板参数，然后传至接受该特定消息类型作为参数的lambda函数，由于调用通过这种方式连接起来，wait()的实现知道它正在等待一个digit_pressed消息，clear_last_pressed消息或是cancel_pressed消息，任何其他类型的消息将再次被丢弃

//这时，在获取消息之后你并不一定非要改变状态，例如，如果你得到了一个digit_pressed消息，仅需要将它添加至pin，除非它为最后的数字，清单4.15中的主循环⑦将再次调用gettting_pin()来等待下一个数字(或是清除或取消)

//这对于图4.3所示的行为，每一个状态框通过不同的成员函数来实现，他们等待相关的消息并适当地更新状态

//如你所见，这种编程风格可以大大简化涉及并发系统的任务，因为每个线程可以完全独立地对待，这就是使用多线程来划分关注点的一个例子，并且需要你明确决定如何在线程间划分任务

//4.5 小结
//线程间的同步操作，是编写一个使用并发的应用程序的重要组成部分，如果没有同步，那么线程本质上是独立的，就可以被写作独立的应用程序，由于它们之间存在相关活动而作为群组运行，在本章中，我介绍了同步操作的各种方式，从最基本条件变量，到future，promise以及打包任务，我还讨论了解决同步问题的方式，函数式编程，其中每个任务产生的结果完全依赖于它的输入而不是外部环境，以及消息传递，其中线程间的通信是通过一个扮演中介角色的消息子系统发送异步消息来实现的

//我们已经讨论了许多在C++中可用的高阶工具，现在是时候来看看令这一切得以运转的底层工具了:C++内存模型和原子操作

//消息传递框架与完整的ATM示例

//在第4章中，我展示了一个在线程之间使用消息传递框架来传递消息的例子，其中简单实现了ATM中的代码，下面给出该示例的完整代码，其中包含了消息传递框架

//清单C.1展示了消息队列，其中以指向基类的指针存放了一列消息，特定的消息类别使用从该基类派生的类模板来处理，压入一个条目，即是构造一个包装类的相应实例并且存储一个指向它的指针，弹出一个条目，即是返回该指针，由于message_base类没有任何成员函数，弹出线程在能够访问存储的消息之前，需要将此指针转换为一个合适的wrapped_message<T>指针

//清单C.1 简单的消息队列

#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>

namespace messaging
{
    struct message_base   //我们的队列项目的基类
    {
        virtual ~message_base() {}
    };
    
    template<typename Msg>
    struct wrapped_message : message_base   //每个消息类型有特殊定义
    {
        Msg contents;
        explicit wrapped_message(const Msg& contents_) : contents(contents_) {}
    };
    
    class queue   //我们的消息队列
    {
        std::mutex m;
        std::condition_variable c;
        std::queue<std::shared_ptr<message_base>> q;   //实际的队列存储message_base的指针
    public:
        template<typename T>
        void push(const T& msg)
        {
            std::lock_guard<std::mutex> lk(m);
            q.push(std::make_shared<wrapped_message<T>>(msg));   //将发出的消息封装并且存储指针
            c.notify_all();
        }
        std::shared_ptr<message_base> wait_and_pop()
        {
            std::unique_lock<std::mutex> lk(m);
            c.wait(lk, [&]{ return !q.empty(); });   //阻塞直到队列非空
            auto res=q.front();
            q.pop();
            return res;
        }
    };
}

//发送消息是通过清单C.2所示的sender类实例来处理的，它仅仅是对消息队列的简单包装，只允许消息被压入，复制sender的实例仅仅复制指向队列的指针，而非队列本身

//清单C.2 sender类

namespace messaging
{
    class sender
    {
        queue* q;   //sender就是封装了队列指针
    public:
        sender(): q(nullptr) {}   //默认构造的sender没有队列
        explicit sender(queue*q_): q(q_) {}   //允许从指向队列的指针进行构造
        template<typename Message>
        void send(const Message& msg)
        {
            if(q)
                q->push(msg);
        }
    };
}

//接受消息要稍微复杂一点，你不仅要等待队列中的消息，还需要检查其类型是否符合所等待的消息类型，并且调用相应的处理函数，这些都始于是否符合所等待的消息类型，并且调用相应的处理函数，这些都始于清单C.3中展示的receiver类

//清单C.4 dispatcher类

namespace messaging
{
    class close_queue {};   //用来关闭队列的消息
    
//清单C.5 TemplateDispatcher类模板

    template<typename PreviousDispatcher,typename Msg,typename Func>
    class TemplateDispatcher
    {
        queue* q;
        PreviousDispatcher* prev;
        Func f;
        bool chained;
        
        TemplateDispatcher(const TemplateDispatcher&) = delete;
        TemplateDispatcher& operator=(const TemplateDispatcher&) = delete;
        
        template<typename Dispatcher,typename OtherMsg,typename OtherFunc>
        friend class TemplateDispatcher;   //TemplateDispatcher实例之间互为友元
        
        void wait_and_dispatch()
        {
            for(;;)
            {
                auto msg=q->wait_and_pop();
                if(dispatch(msg))   //①如果我们处理了消息就跳出循环
                    break;
            }
        }
        
        bool dispatch(std::shared_ptr<message_base> const& msg)
        {
            if(wrapped_message<Msg>* wrapper = dynamic_cast<wrapped_message<Msg>*>(msg.get()))   //②检查消息类型并调用函数
            {
                f(wrapper->contents);
                return true;
            }
            else
                return prev->dispatch(msg);   //③链至前一个调度器
        }
    public:
        TemplateDispatcher(TemplateDispatcher&& other) : q(other.q), prev(other.prev), f(std::move(other.f)), chained(other.chained)
        {
            other.chained = true;
        }
        
        TemplateDispatcher(queue* q_, PreviousDispatcher* prev_, Func&& f_) : q(q_), prev(prev_), f(std::forward<Func>(f_)), chained(false)
        {
            prev_->chained = true;
        }
        
        template<typename OtherMsg, typename OtherFunc>
        TemplateDispatcher<TemplateDispatcher, OtherMsg, OtherFunc>
        handle(OtherFunc&& of)   //④可以链接附加的处理函数
        {
            return TemplateDispatcher<TemplateDispatcher, OtherMsg, OtherFunc>(q, this, std::forward<OtherFunc>(of));
        }
        
        ~TemplateDispatcher() noexcept(false)   //⑤析构函数又是noexcept(false)的
        {
            if(!chained)
                wait_and_dispatch();
        }
    };

//TemplateDispatcher<>类模板是基于dispatcher类构建的，并且二者几乎雷同，尤其是析构函数仍然调用了wait_and_dispatch()来等待一个消息

//如果你处理了消息，那么久不会抛出异常，所以你需要在消息循环①中检查你是否真的处理了消息，当你成功处理了消息时，消息处理即行停止，你就可以等待下一时刻的另一组消息，如果你恰好得到了一条匹配的消息类型，那么所提供的函数就会被调用②，而不是抛出异常(尽管处理函数自身可能会抛出异常)，如果没有得到匹配的消息，那么久连接至前一个dispatcher③，在首个实例中，它就是diapatcher，但如果你将调用链接至handle()函数④，以允许多种类型的消息被处理，这可能会先于TemplateDispatcher<>初始化，如果消息不匹配的话，这将会翻过来连接到前一个句柄上，因为所有句柄都可能引发异常(包括dispatcher为close_queue消息的默认句柄)，析构函数必须再次声明为noexcept(false)⑤

//这个简单的框架允许你将任意类型的消息压入队列中，然后在接收端有选择地匹配你能够处理的消息，它同时允许你为了压入消息而绕过对队列的引用，同时保持接收端的私密性
    
    class dispatcher
    {
        queue* q;
        bool chained;
        
        dispatcher(const dispatcher&) = delete;   //不能复制的调度器实例
        dispatcher& operator=(const dispatcher&) = delete;
        
        template<typename Dispatcher, typename Msg, typename Func>   //允许TemplateDispatcher实例访问内部成员
        friend class TemplateDispatcher;
        
        void wait_and_dispatch()
        {
            for(;;)   //①循环、等待和调度消息
            {
                auto msg = q->wait_and_pop();
                dispatch(msg);
            }
        }
        
        bool dispatch(std::shared_ptr<message_base> const& msg)   //②dispatch()检查close_queue消息，并抛出
        {
            if(dynamic_cast<wrapped_message<close_queue>*>(msg.get()))
                throw close_queue();
            return false;
        }
    public:
        dispatcher(dispatcher&& other) : q(other.q),chained(other.chained)   //调度器实例可以被移动
        {
            other.chained = true;   //来源不得等待消息
        }
        
        explicit dispatcher(queue* q_) : q(q_),chained(false) {}
        
        template<typename Message, typename Func>
        TemplateDispatcher<dispatcher, Message, Func>
        handle(Func&& f)   //③使用TemplateDispatcher处理特定类型的消息
        {
            return TemplateDispatcher<dispatcher, Message, Func>(q, this, std::forward<Func>(f));
        }
        
        ~dispatcher() noexcept(false)   //④析构函数可能抛出异常
        {
            if(!chained)
                wait_and_dispatch();
        }
    };
}

//给dispatch()的循环，dispatch()本身非常简单，它检查消息是否为一个close_queue消息，如果是，就抛出一个异常，否则，它返回false来指示该消息未被处理，close_queue异常正式析构函数被标记为noexcept(false)的原因，如果没有这个注解，析构函数的默认异常规定将会是noexcept(true)，表明没有异常能够被抛出，那么close_queue异常就会终止程序

//然而你并非经常去主动调用wait()，大部分时间你会希望去处理一个消息，这就是handle()成员函数③的用武之地，它是一个模板，并且消息类型是无法推断的，所以你必须指定待处理的消息类型，并且传入一个函数(或者可调用的对象)来处理它，handle()自身将队列，当前的dispatcher对象和处理函数传递给一个新的TemplateDispatcher类模板的实例，来处理指定类型的消息，这些展示在清单C.5的例子中，问什么要在等待消息之前就在析构函数里测试chained的值呢？因为这样不仅可以防止移入的对象等待消息，而且允许你讲等待消息的重任交给新的TemplateDispatcher实例

//清单C.3 receiver类

namespace messaging
{
    class receiver
    {
        queue q;   //一个receiver拥有此队列
    public:
        operator sender()   //允许隐式转换到引用队列的sender
        {
            return sender(&q);
        }
        dispatcher wait()   //等待队列创建调度
        {
            return dispatcher(&q);
        }
    };
}

//与sender仅仅医用一个消息队列不同，receiver拥有它，你可以使用隐式转换来获得一个引用该队列的sender类，进行消息调度的复杂性始于对wait()的调用，这将穿件一个dispatcher对象，它从receiver中引用该队列，dispatcher类展示在下一个清单中，正如你所见，这项工作是在析构函数中完成的，在清单C.4的例子中，此工作是由等待消息和调度消息组成的

//为了完成第4章的实例，在清单C.6中给出了消息，清单C.7，清单C.8和清单C.9中给出几种状态机，驱动代码在清单C.10中给出

//清单C.6 ATM消息

struct withdraw
{
    std::string account;
    unsigned amount;
    mutable messaging::sender atm_queue;
    withdraw(const std::string account_, unsigned amount_, messaging::sender atm_queue_) : account(account_), amount(amount_), atm_queue(atm_queue_) {}
};

struct withdraw_ok {};

struct withdraw_denied {};

struct cancel_withdrawal
{
    std::string account;
    unsigned amount;
    
    cancel_withdrawal(const std::string& account_, unsigned amount_) : account(account_), amount(amount_) {}
};

struct withdrawal_processed
{
    std::string account;
    unsigned amount;
    
    withdrawal_processed(const std::string& account_, unsigned amount_) : account(account_), amount(amount_) {}
};

struct card_inserted
{
    std::string account;
    explicit card_inserted(const std::string& account_) : account(account_) {}
};

struct digit_pressed
{
    char digit;
    
    explicit digit_pressed(char digit_) : digit(digit_) {}
};

struct clear_last_pressed {};

struct eject_card {};

struct withdraw_pressed
{
    unsigned amount;
    
    explicit withdraw_pressed(unsigned amount_) : amount(amount_) {}
};

struct cancel_pressed {};

struct issue_money
{
    unsigned amount;
    issue_money(unsigned amount_) : amount(amount_) {}
};

struct verify_pin
{
    std::string account;
    std::string pin;
    mutable messaging::sender atm_queue;
    
    verify_pin(const std::string& account_, const std::string& pin_, messaging::sender atm_queue_) : account(account_), pin(pin_), atm_queue(atm_queue_) {}
};

struct pin_verified {};
struct pin_incorrect {};
struct display_enter_pin {};
struct display_enter_card {};
struct display_insufficient_funds {};
struct display_withdrawal_cancelled {};
struct display_pin_incorrect_message {};
struct display_withdrawal_options {};

struct get_balance
{
    std::string account;
    mutable messaging::sender atm_queue;
    
    get_balance(const std::string& account_, messaging::sender atm_queue_) : account(account_), atm_queue(atm_queue_) {}
};

struct balance
{
    unsigned amount;
    explicit balance(unsigned amount_) : amount(amount_) {}
};

struct display_balance
{
    unsigned amount;
    explicit display_balance(unsigned amount_) : amount(amount_) {}
};

struct balance_pressed {};

//清单C.7 ATM状态机

class atm
{
    messaging::receiver incoming;
    messaging::sender bank;
    messaging::sender interface_hardware;
    void (atm::*state)();
    std::string account;
    unsigned withdrawal_amount;
    std::string pin;
    
    void process_withdrawal()
    {
        incoming.wait()
        .handle<withdraw_ok>([&](const withdraw_ok& msg)
        {
            interface_hardware.send(issue_money(withdrawal_amount));
            bank.send(withdrawal_processed(account,withdrawal_amount));
            state = &atm::done_processing;
        })
        .handle<withdraw_denied>([&](const withdraw_denied& msg)
        {
            interface_hardware.send(display_insufficient_funds());
            state = &atm::done_processing;
        })
        .handle<cancel_pressed>([&](const cancel_pressed& msg)
        {
            bank.send(cancel_withdrawal(account,withdrawal_amount));
            interface_hardware.send(display_withdrawal_cancelled());
            state = &atm::done_processing;
        });
    }
    
    void process_balance()
    {
        incoming.wait()
        .handle<balance>([&](const balance& msg)
        {
            interface_hardware.send(display_balance(msg.amount));
            state = &atm::wait_for_action;
        })
        .handle<cancel_pressed>([&](const cancel_pressed& msg)
        {
            state = &atm::done_processing;
        });
    }
    
    void wait_for_action()
    {
        interface_hardware.send(display_withdrawal_options());
        incoming.wait()
        .handle<withdraw_pressed>([&](const withdraw_pressed& msg)
        {
            withdrawal_amount = msg.amount;
            bank.send(withdraw(account,msg.amount,incoming));
            state = &atm::process_withdrawal;
        })
        .handle<balance_pressed>([&](const balance_pressed& msg)
        {
            bank.send(get_balance(account,incoming));
            state = &atm::process_balance;
        })
        .handle<cancel_pressed>([&](const cancel_pressed& msg)
        {
            state = &atm::done_processing;
        });
    }
    void verifying_pin()
    {
        incoming.wait()
        .handle<pin_verified>([&](const pin_verified& msg)
        {
            state = &atm::wait_for_action;
        })
        .handle<pin_incorrect>([&](const pin_incorrect& msg)
        {
            interface_hardware.send(display_pin_incorrect_message());
            state = &atm::done_processing;
        })
        .handle<cancel_pressed>([&](const cancel_pressed& msg)
        {
            state = &atm::done_processing;
        });
    }
    void getting_pin()
    {
        incoming.wait()
        .handle<digit_pressed>([&](const digit_pressed& msg)
        {
            unsigned const pin_length = 4;
            pin += msg.digit;
            if(pin.length() == pin_length)
            {
                bank.send(verify_pin(account,pin,incoming));
                state = &atm::verifying_pin;
            }
        })
        .handle<clear_last_pressed>([&](const clear_last_pressed& msg)
        {
            if(!pin.empty())
                pin.pop_back();
        })
        .handle<cancel_pressed>([&](const cancel_pressed& msg)
        {
            state = &atm::done_processing;
        });
    }
    void waiting_for_card()
    {
        interface_hardware.send(display_enter_card());
        incoming.wait()
        .handle<card_inserted>([&](const card_inserted& msg)
        {
            account = msg.account;
            pin = "";
            interface_hardware.send(display_enter_pin());
            state = &atm::getting_pin;
        });
    }
    void done_processing()
    {
        interface_hardware.send(eject_card());
        state = &atm::waiting_for_card;
    }
    atm(atm const&) = delete;
    atm& operator=(atm const&) = delete;
public:
    atm(messaging::sender bank_, messaging::sender interface_hardware_) : bank(bank_), interface_hardware(interface_hardware_) {}
    
    void done()
    {
        get_sender().send(messaging::close_queue());
    }
    
    void run()
    {
        state = &atm::waiting_for_card;
        try
        {
            for(;;)
                (this->*state)();
        }
        catch(messaging::close_queue const&)
        {
            
        }
    }
    messaging::sender get_sender()
    {
        return incoming;
    }
};

//清单C.8

class bank_machine
{
    messaging::receiver incoming;
    unsigned balance;
public:
    bank_machine() : balance(199) {}
    void done()
    {
        get_sender().send(messaging::close_queue());
    }
    void run()
    {
        try
        {
            for(;;)
            {
                incoming.wait()
                .handle<verify_pin>([&](verify_pin const& msg)
                {
                    if(msg.pin=="1937")
                        msg.atm_queue.send(pin_verified());
                    else
                        msg.atm_queue.send(pin_incorrect());
                    
                })
                .handle<withdraw>([&](withdraw const& msg)
                {
                    if(balance>=msg.amount)
                    {
                        msg.atm_queue.send(withdraw_ok());
                        balance-=msg.amount;
                    }
                    else
                        msg.atm_queue.send(withdraw_denied());
                })
                .handle<get_balance>([&](get_balance const& msg)
                {
                    msg.atm_queue.send(::balance(balance));
                })
                .handle<withdrawal_processed>([&](withdrawal_processed const& msg)
                {
                  
                })
                .handle<cancel_withdrawal>([&](cancel_withdrawal const& msg)
                {
                   
                });
            }
        }
        catch(messaging::close_queue const&)
        {
            
        }
    }
    messaging::sender get_sender()
    {
        return incoming;
    }
};

//清单C.9

std::mutex iom;

class interface_machine
{
    messaging::receiver incoming;
public:
    void done()
    {
        get_sender().send(messaging::close_queue());
    }
    void run()
    {
        try
        {
            for(;;)
            {
                incoming.wait()
                .handle<issue_money>([&](issue_money const& msg)
                {
                    {
                        std::lock_guard<std::mutex> lk(iom);
                        std::cout<<"Issuing " <<msg.amount<<std::endl;
                    }
                })
                .handle<display_insufficient_funds>([&](display_insufficient_funds const& msg)
                {
                    {
                        std::lock_guard<std::mutex> lk(iom);
                        std::cout<<"Insufficient funds"<<std::endl;
                    }
                })
                .handle<display_enter_pin>([&](display_enter_pin const& msg)
                {
                    {
                        std::lock_guard<std::mutex> lk(iom);
                        std::cout <<"Please enter your PIN (0-9)" <<std::endl;
                    }
                })
                .handle<display_enter_card>([&](display_enter_card const& msg)
                {
                    {
                        std::lock_guard<std::mutex> lk(iom);
                        std::cout <<"Please enter your card (I)" <<std::endl;
                    }
                })
                .handle<display_balance>([&](display_balance const& msg)
                {
                    {
                        std::lock_guard<std::mutex> lk(iom);
                        std::cout <<"The balance of your account is " << msg.amount<<std::endl;
                    }
                })
                .handle<display_withdrawal_options>([&](display_withdrawal_options const& msg)
                {
                    {
                        std::lock_guard<std::mutex> lk(iom);
                        std::cout <<"Withdraw 50? (w)" << std::endl;
                        std::cout <<"Display Balance? (b)" << std::endl;
                        std::cout <<"Cancel? (c)" << std::endl;
                    }
                })
                .handle<display_withdrawal_cancelled>([&](display_withdrawal_cancelled const& msg)
                {
                    {
                        std::lock_guard<std::mutex> lk(iom);
                        std::cout << "Withdrawal cancelled" <<std::endl;
                    }
                })
                .handle<display_pin_incorrect_message>([&](display_pin_incorrect_message const& msg)
                {
                    {
                        std::lock_guard<std::mutex> lk(iom);
                        std::cout << "PIN incorrect" << std::endl;
                    }
                })
                .handle<eject_card>([&](eject_card const& msg)
                {
                    {
                        std::lock_guard<std::mutex> lk(iom);
                        std::cout << "Ejecting card" << std::endl;
                    }
                });
            }
        }
        catch(messaging::close_queue&)
        {
            
        }
    }
    messaging::sender get_sender()
    {
        return incoming;
    }
};

//清单C.10

int main()
{
    bank_machine bank;
    interface_machine interface_hardware;
    atm machine(bank.get_sender(),interface_hardware.get_sender());
    std::thread bank_thread(&bank_machine::run,&bank);
    std::thread if_thread(&interface_machine::run,&interface_hardware);
    std::thread atm_thread(&atm::run,&machine);
    messaging::sender atmqueue(machine.get_sender());
    bool quit_pressed=false;
    while(!quit_pressed)
    {
        char c=getchar();
        switch(c)
        {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                atmqueue.send(digit_pressed(c));
                break;
            case 'b':
                atmqueue.send(balance_pressed());
                break;
            case 'w':
                atmqueue.send(withdraw_pressed(50));
                break;
            case 'c':
                atmqueue.send(cancel_pressed());
                break;
            case 'q':
                quit_pressed=true;
                break;
            case 'i':
                atmqueue.send(card_inserted("acc1234"));
                break;
        }
    }
    bank.done();
    machine.done();
    interface_hardware.done();
    atm_thread.join();
    bank_thread.join();
    if_thread.join();
}



#pragma clang diagnostic pop
