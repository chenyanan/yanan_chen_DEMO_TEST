//
//  9_1.cpp
//  DEMO_TEST
//
//  Created by chenyanan on 2017/6/18.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <iostream>
#include <thread>
#include <mutex>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//9.2 中断线程

//在许多场景中，向一个长时间运行的线程发出一个信号告诉线程是停止执行的时候是一个很渴望的行为，这有可能是因为线程是一个线程池的工作线程而线程池正在被销毁，或者是因为其他一些原因，不管是何种原因，基本思想是一样的，你需要从一个线程发送一个信号告诉另外要给线程应该停止运行而不是一直执行到线程自然结束，你同样需要让线程适当的结束而不是简单的退出而造成线程池不一致的状态

//你当然可以为每种情况都设计一种单独的机制，但是这种做法没有通用性，引入许多重复工作，一种共有的机制不但可以让为后面的场景写代码变得容易，而且允许你写出能够被中断，不用担心在什么地方被使用的代码，C++11标准并没有提供这样的机制，但是建立这样的机制是相对直观的，让我们先看看怎样可以完成这个机制，从启动和中断一个线程的接口的角度开始

//9.2.1 启动和中断另一个线程

//首先让我们从可中断线程的接口开始，一个可中断线程的接口需要有那些呢？在最基本的层次上，所有你需要的是跟std::thread一样的接口，外加一个interrupt()函数

class interruptible_thread
{
public:
    template<typename FunctionType>
    interruptible_thread(FunctionType f);
    void join();
    void detach();
    void joinable() const;
    void interrupt();
};
    
//在内部，你可以使用std::thread来管理线程本身，然后使用一些定制的数据结构来处理中断，从线程本身的角度来看中断是什么呢？在最基本的层面上你也许会说"我可以在这里被中断"，你想要一个中断点，为了让这个能够不用传递额外的参数就能使用，需要一个简单的不带任何参数的函数:interruption_point()，这意味着中断相关的数据结构需要使用一个thread_local的变量来访问，这个私有变量是在线程启动的时候设置好的，这样当一个线程调用你的interruption_point()函数的时候，他会检查当前运行的线程的数据结构，我们随后会给出interruption_point()的实现

//这个thread_local标志是你不能简单使用的std::thread来管理线程的主要原因，他需要使用特殊的分配方法，使得interruptible_thread实例可以访问，并且新启动的线程也能够访问，你可以通过在传递给std::thread实际启动一个线程的之前包装一下，如清单9.9所示

//清单9.9 interruptible_thread的基本实现

class interrupt_flag
{
public:
    void set();
    bool is_set() const;
};

thread_local interrupt_flag this_thread_interrupt_flag;   //①

class interruptible_thread
{
    std::thread internal_thread;
    interrupt_flat* flag;
public:
    template<typename FunctionType>
    interruptible_thread(FunctionType f)
    {
        std::promise<interruptible_flag*> p;   //②
        internal_thread = std::thread([f,&p]{   //③
            p.set_value(&this_thread_interrupt_flag);
            f();   //④
        });
        flag = p.get_future().get();   //⑤
    }
    
    void interrupt()
    {
        if (flag)
            flag->set();   //⑥
    }
};

//用户给定的函数f被包装在一个lambda函数中③，在此函数中，保存了一份f的拷贝以及一个局部promise的引用p②，lambda函数在调用用户提供的函数之前④为新的线程将promise设置为this_thread_interrupt_flag(这个变量被声明为thread_local①)的地址，被调用的线程染回会等待跟promise相关联的future变得可用，然后存储在flag成员变量中⑤，注意到即使lambda函数试运行在新线程上面，并且持有一对局部变量p的应用，这是没有问题的，因为interruptible_thread的构造函数会一直等待知道p不再被新的线程引用，注意这个实现不负责处理等待线程结束或者分离线程，你需要保证当线程存在的时候或者被分离了，flag标志被清理掉来避免危险的指针

//9.2.2 检测一个线程是否被中断

//现在你可以设置中断标志了，但是如果线程不去检查它自身是否被中断的话不会给你带来任何好处，在最简单的情况下，你可以使用interruption_point()函数来检查线程自身是否被中断，你可以当线程可以被安全的中断的时候调用这个函数，如果中断标志被设置的话它会抛出一个thread_interrupted异常

void interruption_point()
{
    if (this_thread_interrupt_flag.is_set())
        throw thread_interrupted();
}

//你可以砸某个方便的点上调用这个函数

void foo()
{
    while(!done)
    {
        interruption_point();
        process_next_iterm();
    }
}

//虽然这可以工作，但是它不完美，在一些中断线程最好的地方是它备足赛住了，正在等待某个信号，这意味着线程为了调用interruption_point()没有在运行，在这里你需要的是一个通过使用中断的方式来等待某个事情

//9.2.3 中断等待条件变量

//现在你可以通过在精心选定的位置上显示调用interruption_point()来检测中断，但是当你想要一个阻塞等待的事假你，如等待一个条件变量被通知，这不能给你多少帮助，你需要一个新的函数interruptible_wait()，这个函数你可以为你想要等待的不同的事情重载，你可以知道怎样中断一个等待工作，我已经提到一个你可能想要等待的是条件变量，所以让我们从条件变量开始，为了能够中断一个条件变量的等待，你需要这样做呢？最简单的事情是当你设置中断标志的时候通知条件便令，然后再等待的后面立即加上一个中断点，但是为了让这种方式可以工作，你不得不通知所有等待该条件变量的线程来保证你感兴趣的线程被唤醒，等待者们需要处理假的唤醒，使得其他线程能够将这个事件当成一个假的换行，interrupt_flag结构需要能够存储一个指向条件变量的指针，这样它可以在有调用set()的时候被通知，interruptible_wait()的一个关于条件变量的实现如下面清单9.10中的代码所示

//清单9.10 因std::condition_variable而遭到破坏的interruptible_wait函数实现

void interruptible_wait(std::condition_variable& cv, std::unique_lock<std::mutex>& lk)
{
    interruption_point();
    this_thread_interrupt_flag.set_condition_variable(cv);
    cv.wait(lk);
    this_thread_interrupt_flag.clear_condition_variable();
    interruption_point();
}

//假定设置和清楚一个带着中断标志的条件变量的函数存在，这个代码是优质而且简单的，它先检查中断，然后为当前线程关联一个带interrupt_flag的条件变量①，等待条件变量②，清楚关联的条件变量③，然后再一次检查中断，如果线程是在等待条件变量的时候中断，调用中断的线程会广播条件变量将你唤醒，所以你可以检查中断，不幸的是，这份代码是不能工作的，它存在两个问题，第一个问题相对比较明显，因为std::condition_variable::wait()可能会抛出异常，所以你肯可能没有删除中断标志和条件变量的关联就退出了，这可以通过使用一个结构的析构函数来删除关联性来修复它

//第二个问题不是那么明显，这份代码中存在一个竞争条件，如果线程是在调用interruption_point()后面被中断，那么条件变量是否跟中断标志关联已经不要紧了，因为线程不是在等待所以不能被条件变量唤醒，你需要在保证线程在上一次检查中断和调用wait()之间不能被通知，在不改变std::condition_variable内部结构的情况下，你只有一种方法来做这个，使用lk持有的互斥锁来保持这块区域，这要求将其传递给调用set_condition_variable，不幸的是，这又会产生一个问题，你需要传递一个声明周期未知的互斥锁给另外一个线程，那个线程在不知道它是否已经锁住互斥锁的情况下试图锁住，这有潜在的死锁可能，以及试图锁住一个已经被销毁的互斥锁，如果不能可靠地中断一个条件变量的等待，限制会非常大，你可以在没有特殊的interruptible_wait()做得几乎一样好，那么你还有其他什么可选的方法呢？一个选项是在等待中放入一个等待的最大时间，给wait_for()传递一个很小的事件间隔(加1毫秒)而不是使用wait()，这给线程在看到中断前等待的时间设置了一个上限，如果你这样做，等待的线程会看到由定时器到期带来的大量的假唤醒，但是他不能轻易地带啦帮助，清单9.11是这样的一个实现，还有对应的interrupt_flag的实现

//清单9.11 在为std::condition_variable的interruptible_wait中使用超时

class interrupt_flag
{
    std::atomic<bool> flag;
    std::condition_variable* thread_cond;
    std::mutex set_clear_mutex;
public:
    interrupt_flag() : thread_cond(0) {}
    
    void set()
    {
        flag.store(true, std::memory_order_relaxed);
        std::lock_guard<std::mutex> lk(set_clear_mutex);
        if (thread_cond)
            thread_cond->notify_all();
    }
    
    bool is_set() const
    {
        return flag.load(std::memory_order_relaxed);
    }
    
    void set_condition_variable(std::condition_variable& cv)
    {
        std::lock_guard<std::mutex> lk(set_clear_mutex);
        thread_cond = &cv;
    }
    
    struct clear_cv_on_destruct()
    {
        ~clear_cv_on_destruct()
        {
            this_thread_interrupt_flag.clear_condition_variable();
        }
    };
};

void interruptible_wait(std::condition_variable& cv, std::unique_lock<std::mutex>& lk)
{
    interruption_point();
    this_thread_interrupt_flag.set_condition_variable(cv);
    interrupt_flag::clear_cv_on_destruct guard;
    interruption_point();
    cv.wait_for(lk, std::chrono::milliseconds(1));
    interruption_point();
}

//如果你有一个要等待的断言，那么1毫秒的超时会被完全隐藏咋断言的环境中

template<typename Predicate>
void interruptible_wait(std::condition_variable& cv, std::unique_lock<std::mutex>& lk, Predicate pred)
{
    interruption_point();
    this_thread_interrupt_flag.set_condition_variable(cv);
    interrupt_flag::clear_cv_on_destruct guard;
    while (!this_thread_interrupt_flag.is_set() && !pred())
    {
        cv.wait_for(lk, std::chrono::milliseconds(1));
    }
    interruption_point();
}

//这会导致等待的条件被检查很多次，但是非常容易地用于代替wait()函数调用，带定时器的变形也非常容易实现，只要等待一个给定的事件，如1毫秒，或者其他短时间，现在std::condition_variable等待已久可以处理的，怎样等待一个std::condition_variable_any变量呢？是与此相同，或者你能做得更好？

//9.2.4 中断在std::condition_variable_any上的等待

//std::condition_variable_any变量跟std::condition_variable不同的是它可以跟任何所类型配合工作而不是只能跟std::unique_lock<std::mutex>配合，结果是这会让事情变得简单，你可以更好地处理std::condition_variable_any因为它可以跟任何锁配合，你可以建立自己的所类型用来加锁/解锁interrupt_flag的内部函数set_clear_mutex以及提供给等待调用的锁，如下面清单9.12所示

//清单9.12 为std::condition_variable_any而设的interruptible_wait

class interrupt_flag
{
    std::atomic<bool> flag;
    std::condition_variable* thread_cond;
    std::condition_variable_any* thread_cond_any;
    std::mutex set_clear_mutex;
public:
    interrupt_flag() : thread_cond(0), thread_cond_and(0) {}
    
    void set()
    {
        flag.store(true, std::memory_order_relaxed);
        std::lock_guard<std::mutex> lk(set_clear_mutex);
        if (thread_cond)
            thread_cond->notify_all();
        else if (thread_read_any)
            thread_cond_any->notify_all();
    }
    
    template<typename Lockable>
    void wait(std::condition_variable_any& cv, Lockable& lk)
    {
        struct custom_lock
        {
            interrupt_flag* self;
            Lockable& lk;
            
            custom_lock(interrupt_flag* self_, std::condition_variable_any& cond, Lockable& lk_) : self(self_), lk(lk_) {
                self->set_clear_mutex.lock();   //①
                self->thread_cond_any = &cond;   //②
            }
            
            void unlock()   //③
            {
                lk.unlock();
                self->set_clear_mutex.unlock();
            }
            
            void lock()
            {
                std;:lock(self->set_clear_mutex, lk);   //④
            }
            
            ~custom_lock()
            {
                self->thread_cond_any = 0;   //⑤
                self->set_clear_mutex.unlock();
            }
        };
        
        custom_lock cl(this, cv, lk);
        interruption_point();
        dv.wati(cl);
        interruption_point();
    }
    //rest as before
};

template<typename Lockable>
void interruptible_wait(std::condition_variable_any& cv, Lockable& lk)
{
    this_thread_interrupt_flag.wait(cv,lk);
}

//你的自定义锁类型在其构造函数①中获得内部set_clear_mutex锁然后设置指针thread_cond_any指向传递给构造函数②的std::condition_variable_any变量，Lockable引用保存起来为以后使用，这必须是已经被锁住，现在你可以不用担心竞争来检查中断了，如果在这个点上中断标志被设置，它是在你获得set_clear_mutex锁之前被设置的，当条件变量调用你的unlock()时，你释放Lockable对象以及内部的set_clear_mutex③，此时允许试图中断你的线程在wait()函数内部去获得set_clear_muetx锁和检查thread_cond_any指针，但是之前的却不能，一旦wait()函数结束等待，他会调用你的lock()函数，这个函数再次去获得内部的set_clear_mutex和Lockable对象的锁④，现在你可以再次在你的custom_lock的析构函数清理thread_cond_any指针之前再次调用wait()函数去检查中断⑤，在custom_lock的析构函数中你也会释放set_clear_mutex锁

//9.2.5 中断其他阻塞调用

//到现在为止我们已经讨论了关于条件变量等待的情况，但是想互斥锁，future以及其他类似的阻塞源语的等待该怎样做呢？一般情况下，你不得不使用一个用在std::condition变脸的定时器选项，因为在不访问互斥元或者future的内部结构的前提下，没有办法来中断一个短时间的等待，但是使用定时器选项你知道你要等待什么，所以你可以在interruptible_wait()函数中循环检查，作为一个例子，下面代码是针对std::future的interruptible_wait()重载版本

template<typename T>
void interruptible_wait(std::future<T>& uf)
{
    while(!this_thread_interrupt_flag.is_set())
    {
        if (uf.wait_for(lk,std::chrono::milliseconds(1)==std::future_status::ready))
            break;
    }
    interruption_point();
}

//这会一直等到要么中断标志被设置，要么future已经准备好了，但是每次在future上执行阻塞等待1ms，这意味着，假定使用高精度的时钟，在中断请求被通知之前平均每次大搞需要等待0.5ms，wait_for函数经常会等待一整个时钟滴答，所以如果你的时钟滴答的间隔是15ms，你会每次至少等待15ms而不是1ms，取决于应用场景，这有可能会变得无可坚守，你总是可以降低定时器到期的事件间隔，但是其坏处是线程被换行更多次来检查中断标志，这会增加县城切换的额外开销

//到现在为孩子我们已经讨论了你应该怎样工作使用interruption_point()和interruptible_wait()函数检测中断你，但是你应该怎样处理中断呢？

//9.2.6 处理中断

//从被中断的线程的角度来看，一个中断只是一个thread_interrupted异常，这可以像其他异常一样进行处理，典型的操作是你可以使用一个标准的catch块来捕获它

try
{
    do_something();
}
catch(thread_interrupted&)
{
    handle_interruption();
}

//这意味着你可以捕获中断，用某些方法来处理它，然后继续执行，如果你这样做，另外一个线程再次被中断，你可能想这样做如果你的线程是在执行一些列独立的任务的话，中断一个任务会导致那个任务被放弃，线程会继续执行列表中的下一个任务

//因为thread_interrupted是一个异常，当调用能产生中断的代码断的时候所有异常安全的预防措施必须北至新年给来保证资源没有被泄露以及数据结构保持在一个一致的状态，经常发生的一种情况是让中断来结束线程，这种情况下你只要让异常抛出就可以了，但是如果你让异常传播的范围超过了std::thread的构造器的话，std::terminate将会被调用，整个程序都会被终止，为了避免被迫记得在每个你传递到interruptible_thread的函数中放一个catch(thread_interrupted)句柄，作为替代，你可以京此catch块放到你用来初始化interrupt_flag的包装器中，这样做会让允许中断异常未被处理而传播变得安全，因为接下来仅仅会中值单独一条线程，在interruptible_thread构造函数中的线程初始化闲杂看起来是这个样子

internal_thread=std::thread([f,&p]{
    p.set_value(&this_thread_interrupt_flag);
    
    try
    {
        f();
    }
    catch(thread_interrupted const&)
    {
        
    }
});

//现在让我们看一个中断很有用的完整例子

//9.2.7 在应用中退出时中断后台任务

//现在考虑桌面走索应用，次应用也与用户进行互动，他需要见识文件系统的状态，识别所有的改变并且更新它的索引，为了避免影响GUI的响应性，这种处理就留给基础线程来完成，这个基础线程需要在应用的生命期时钟运行，在应用初始化的时候就启用它，然后一直运行知道引用结束，对于这样一个应用通常只有在机器被关机的时候在会发生，因为此医用需要时钟运行来保持最新的索引，在任何情况下，当应用结束的时候，就需要按顺序关闭基础线程，实现它的一种方法就是中断它

//清单9.13展示了这样一个系统的线程管理部分的一个简单实现

//清单9.13 在后台监视文件系统

std::mutex config_mutex;
std::vector<interruptible_thread> background_threads;

void backaground_thread(int disk_id)
{
    while(true)
    {
        interruption_point();   //①
        fs_change fsc = get_fs_changes(disk_id);   //②
        if (fsc.has_changes())
        {
            update_index(fsc);   //③
        }
    }
}

void start_background_processing()
{
    background_threads.push_back(interrupible_thread(background_thread,disk_1));
    background_threads.push_back(interruptible_thread(background_thread,disk_2));
}

int main()
{
    start_background_processing();   //④
    process_gui_until_exit();   //⑤
    std::unique_lock<std::mutex> lk(config_mutex);
    for (unsigned i = 0; i < background_threads.size(); ++i)
    {
        background_threads[i].interrupt();   //⑥
    }
    for (unsigned i = 0; i < background_threads.size(); ++i)
    {
        background_threads[i].join();   //⑦
    }
}

//启动的时候，开始运行基础线程④，然后主线程将基础线程与处理GUI一起处理⑤，当用户要求应用退出的时候，中断这些基础程序⑥，然后主线程等待每个基础线程在推出前完成⑦，基础线程在一个循环里聚集，检查磁盘变化②并且更新索引③，每次循环它们通过调用interruption_point()检查中断①

//为什么你在等待前要中断所有线程？为什么不逐个中断然后再移动到下一个前进行等待？答案就是并发性，当线程被中断，它们闭会立即结束，因为它们在退出前必须前进到下一个中断点然后运行析构函数调用和异常处理代码，通过立即联合所有线程，你就可以使中断线程等待，即使它仍然可以做它能做的有用的工作，中断别的线程，你等待知道不在有任何工作的时候(所有线程都被中断)，这才允许所有线程被中断来并行地处理他们的中断并且更快结束

//这种中断的方法可以简单扩展为增加进一步中断调用或者通过一个具体代码块来禁止中断，但是浙江留待读者考虑

//9.3 总结

//本章，我们考虑了许多"高级的"线程管理方法:线程池和中断线程，你已经看到使用贝蒂工作队列如何减少同步管理以及潜在提供线程池的吞吐量，并且看到当等待子任务完成时如何运行队列中别的任务来减少发生死锁的可能性

//我们也考虑了许多方法来允许一个线程中断另一个线程的处理，例如使用特殊终端店和如何将原本会被中断阻塞的函数变得可以被中断

//

#pragma clang diagnostic pop

