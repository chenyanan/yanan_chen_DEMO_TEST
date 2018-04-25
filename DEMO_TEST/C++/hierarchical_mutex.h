//
//  hierarchical_mutex.cpp
//  123
//
//  Created by chenyanan on 2017/4/21.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <thread>
#include <mutex>

class hierarchical_mutex
{
    std::mutex internal_mutex;
    const unsigned long hierarchy_value;
    unsigned long previous_hierarchy_value;
    static thread_local unsigned long this_thread_hierarchy_value;
    
    void check_for_hierarchy_violatio()
    {
        if (this_thread_hierarchy_value <= hierarchy_value)
            throw std::logic_error("mutex hierarchy violated");
    }
    
    void update_hierarchy_value()
    {
        previous_hierarchy_value = this_thread_hierarchy_value;
        this_thread_hierarchy_value = hierarchy_value;
    }
    
public:
    explicit hierarchical_mutex(unsigned long value) : hierarchy_value(value), previous_hierarchy_value(0) {}
    
    void lock()
    {
        check_for_hierarchy_violatio();
        internal_mutex.lock();
        update_hierarchy_value();
    }
    
    void unlock()
    {
        this_thread_hierarchy_value = previous_hierarchy_value;
        internal_mutex.unlock();
    }
    
    bool try_lock()
    {
        check_for_hierarchy_violatio();
        if (!internal_mutex.try_lock())
            return false;
        update_hierarchy_value();
        return true;
    }
};

thread_local unsigned long hierarchical_mutex::this_thread_hierarchy_value(ULONG_MAX);

//这里的关键是使用thread_local的值来表示当前线程的层次值，this_thread_hierarchy_value,它被初始化为最大值，所以在刚开始的时候任意互斥元都可以被锁定，由于它被声明为thread_local,每个线程都有属于自己的副本，所以在一个线程中该变量的状态，完全独立于从另一个线程中读取的该变量状态，参阅附录A，A.8节可以获得关于threa_local的更多信息

//因此，当线程第一次锁定hierarchical_mutex的实例时，this_thread_hierarchy_value的值为ULONG_MAX，就其本质而言，ULONG_MAX比其他任意值都大，所以通过了check_for_hierarchy_violation()中的检查，在检查通过之后，lock()代理内部的互斥元用以实际锁定，一旦锁定成功，就可以更新层次值

//现在如果在持有第一个hierarchical_mutex上的锁的同时，锁定另一个hierarchical_mutex上的锁的同时，锁定另一个hierarchical_mutex，则this_thread_hierarchy_value的值反映的是第一个互斥元的层次值，为了通过检查，第二个互斥元的层次值必须小于已经持有的互斥元的层次值

//现在保存当前县城之前的层次值是很重要的，这样才能在unlock()中恢复它，否则你就无法再次所赐你个一个具有更高层次值得互斥元，即便该线程并没有持有任何锁，因为只有当你持有internal_mutex时才能保存之前的层次值，并在解锁该内部互斥元之前释放它，你可以安全地将其存储在hierarchical_mutex自身中，因为它被内部互斥元上的锁安全地保护

//try_lock()和lock()工作原理相同，只是如果在internal_mutex上调用try_lock()失败，那么你就无法拥有这个锁，所以不能更新层次值，并且返回false而不是true

//虽然检测是在运行时检查，但它至少不依赖于时间，你不必去等待能够导致死锁出现的罕见情况发生，此外，需要以这种方式划分应用程序和互斥元的设计流程，可以在写入代码之前帮助取消许多可能导致死锁的原因，即使你还没有到达实际编写运行时检测的那一步，进行设计联系任然是值得的

