//
//  7_7.cpp
//  123
//
//  Created by chenyanan on 2017/5/12.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <iostream>
#include <thread>
#include <mutex>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//7.2.5 将内存模型应用至无锁栈

//在改变内存顺序前，你需要检查操作以及他们之间的关系，然后就可以寻找提供这些关系的最小内存顺序，我了实现这一点，就必须在不同场景下从线程角度考虑情况，最简单的场景就是一个线程入栈一个数据项，并且稍后另一个线程将那个数据项出栈，我们先考虑这种情况

//在这种简单情况加，涉及数据的三个重要部分，第一部分是用来传输head数据的counted_node_ptr，第二部分是head引用的结点数据结构，第三部分是结点指向的数据项

//线程push()的时候首先构造数据项和结点，然后设置head，线程pop()的时候首先加载head的值，然后基于head做一个比较/交换循环来增加引用计数，最后读取结点数据结构来得到next的值，在这里可以看出这样一种关系，next的值是一个普通的非原子性对象，因此为了安全读取它，必须存在存储(入栈线程执行的操作)发生在加载(出栈线程执行的操作)之前这样一种关系，因为push()中唯一的原子操作是compare_exchange_weak()，所以需要一个释放操作来实现在线程间实现这样一种先后顺序关系，而且comapre_exchange_weak()必须是std::memory_order_release或者更强的，如果compare_exchange_weak()失败了，那么就继续循环并且不做任何改变，因此在这种情况下需要使用std::memory_order_relaxed

void push(const T& data)
{
    counted_node_ptr new_node;
    new_node.ptr = new node(data);
    new_node.external_count = 1;
    new_node.ptr->next = head.load(std::memory_order_relaxed);
    while(!head.comapre_exchange_weak(new_node.ptr->next, new_node, std::memory_order_release, std::memory_order_relaxed));
}

//那么pop()的代码怎么样呢？为了实现这种先后顺序关系，你必须在读取next之前有一个std::memory_order_acquire或者更强的操作，接引用指针读取的next值是increase_head_count()中的compare_exchange_strong()读取的旧值，因此如果成功的话就需要有先后顺序，正如在push()中一样，如果交换失败的话，只需要继续循环，因此失败时可以使用任意的顺序

void increase_head_count(counted_node_ptr& old_counter)
{
    counted_node_ptr new_counter;
    
    do
    {
        new_counter = old_counter;
        ++new_counter.external_count;
    } while(!head.compare_exchange_strong(old_counter, new_counter, std::memory_order_acquire, std::memory_order_relaxed));
    
    old_counter.external_count = new_counter.external_count;
}

//如果compare_exchange_strong()调用成功，那么读取的结点的ptr值设置为old_counter中存储的值，因为push()中的存储是一个释放操作，并且compare_exchange_strong()调用成功，那么读取的结点的ptr值设置为old_counter中存储的值，因为push()中的存储是一个释放操作，并且compare_exchange_strong()是一个释放操作，因此存储与加载同步而且存在先后发生顺序的关系，所以push()中存储ptr发生在pop()中读取ptr->next之后，因此是安全的

//注意，在最初的head.load()中内存顺序并不是很重要，因此可以安全使用std::memory_order_relaxed

//下一步，compare_exchange_strong()将head的值设为old_head.ptr->next，是否需要操作来确保线程的数据完整性？如果交换成功就读取ptr->data的操作发生在线程加载它之前，尽管如此，你赢得到如下保证，increase_head_count()中的获取操作保证了push()线程中存储和比较/交换操作存在同步关系，因为push()线程中存储data发生在存储head之前，调用increase_head_count()发生在加载ptr->data之前，所以就存在一种先后顺序关系，并且即使pop()中的比较/交换使用std::memory_order_relaxed这种先后顺序关系也是存在的，ptr->data改变的唯一的地方就是调用swap()，并且没有别的线程可以在同一个结点上进行操作，这就是整个比较/交换

//如果comapre_exchange_strong()失败了，直到下一次循环的时候才会访问old_head的新值，并且你决定了increased_head_count()中的std::memory_order_acquire是足够的，因此std::memory_order_relaxed也是足够的

//那么其他线程呢？是否需要更强的方式来保证别的线程是安全的？答案是否定的，因为只有比较/交换操作会改变head，因为这些是"读-修改-写"操作，它们通过push()中的比较/交换形成了部分释放顺序，因此push()中的compare_exchange_weak()与调用increase_head_count()中的compare_exchanage_strong()同步，它读取存储的值，即使别的线程同时在修改head

//因此，你基本上完成了，只需要处理修改引用计数的fetch_add()的操作，返回这个节点数据的线程可以继续，因为没有别的线程会修改这个节点数据，尽管如此，任何没有成功取值的线程知道别的线程的确修改了节点数据，它使用swap()获得引用的数据项，因此，为了避免数据竞争，你需要确保swap()发生在delete之前，实现它的一个简单方式就是在成功返回分支的fetch_add()中使用std::memory_order_release，并且在再次循环分支的fetch_add()中使用std::memroy_order_acquire还可以进一步简化设计，只有一个线程进行删除操作(将技术设置为零的线程)，也只有一个线程进行删除操作，因为fetch_add()是一个"读-修改-写"操作，它组成了释放顺序的一部分，因此可以使用额外的load()，如果再次循环分支将引用计数减为零，那么为了保证同步关系可以使用std::memory_order_acquire来重载引用计数，并且fetch_add()可以使用std::memory_order_relaxed清单7.12所示就是使用新的pop()的栈的最终实现

//清单7.12 使用引用计数和放松原子操作的无锁栈

template<typename T>
class lock_free_stack
{
private:
    struct node;
    
    struct counted_node_ptr
    {
        int external_count;
        node* ptr;
    };
    
    struct node
    {
        std::shared_ptr<T> data;
        std::atomic<int> internal_count;
        counted_node_ptr next;
        
        node(const T& data_) : data(std::make_shared<T>(data_)), internal_count(0) {}
    };
    
    std::atomic<counted_node_ptr> head;
    
    void increase_head_count(counted_node_ptr& old_counter)
    {
        counted_node_ptr new_counter;
        
        do
        {
            new_counter = old_counter;
            ++new_counter.external_count;
        } while(!head.compare_exchange_strong(old_counter, new_counter,
                                              std::memory_order_acquire,
                                              std::memory_order_relaxed));
        
        old_counter.external_count = new_counter.external_count;
    }
public:
    ~lock_free_stack()
    {
        while (pop());
    }
    
    void push(const T& data)
    {
        counted_node_ptr new_node;
        new_node.ptr = new node(data);
        new_node.external_count = 1;
        new_node.ptr->next = head.load(std::memory_order_relaxed);
        while (!head.compare_exchange_weak(new_node.ptr->next, new_node,
                                           std::memory_order_release,
                                           std::memory_order_relaxed));
    }
    
    std::shared_ptr<T> pop()
    {
        counted_node_ptr old_head = head.load(std::memory_order_relaxed);
        for (;;)
        {
            increase_head_count(old_head);
            node* const ptr = old_head.ptr;
            if (!ptr)
            {
                return std::shared_ptr<T>();
            }
            if (head.compare_exchange_strong(old_head, ptr->next, std::memory_order_relaxed))
            {
                std::shared_ptr<T> res;
                res.swap(ptr->data);
                
                int const count_increase = old_head.external_count - 2;
                
                if (ptr->internal_count.fetch_add(count_increase, std::memory_order_release) == - count_increase)
                {
                    delete ptr;
                }
                
                return res;
            }
            else if (ptr->internal_count.fetch_add(-1, std::memory_order_relaxed) == 1)
            {
                ptr->internal_count.load(std::memory_order_acquire);
                delete ptr;
            }
        }
    }
};

//这是一个实验，但是最后成功了，通过使用更放松的操作，在没有影响正确性的情况下提升了性能，正如你所见，这里的pop()实现有37行代码，在清单6.1基于锁的栈中pop()有8行代码，在清单7.2不使用内存管理的无锁栈中pop()有7行代码，现在我们考虑写一个无锁队列，你可以看到一个相似的模式，无锁代码中的很多复杂性都来自于内存管理 

#pragma clang diagnostic pop
