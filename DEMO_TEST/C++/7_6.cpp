//
//  7_6.cpp
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

//7.2.4 使用引用计数检测结点

//回顾7.2.2节，删除结点的问题就在于检测哪些结点正在被别的线程读取，如果可以精确识别出哪些结点正在被引用以及何时没有线程读取这些结点，那么就可以删除此结点，风险指针通过存储读取每个节点的线程数来处理此问题，引用计数通过存储一定数量的线程读取结点来处理这个问题

//这种方法看上去更好更直接，但是在实际中很难处理，首先，你可能认为std::shared_ptr<>可以处理这种问题，毕竟，这是要给引用计数指针，不幸的是，尽管std::shared_ptr<>中的一些操作是原子的，但是它们不能保证是无锁的，尽管这与原子类型上的任何操作并没有不同，但是在许多情况下std::shared_ptr<>被使用，并且使得原子操作是无锁的会导致使用这个类有花费，如果你的平台提供这样一个实现，即当std::atomic_is_lock_free(&some_shared_ptr)返回true，所有的内存回收时间都离开，如清单7.9所示，其中只使用std::shared_ptr<node>

template<typename T>
class lock_free_stack
{
private:
    struct node
    {
        std::shared_ptr<T> data;
        std::shared_ptr<node> next;
        
        node(const T& data_) : data(std::make_shared<T>(data_)) {}
    };
    
    std::shared_ptr<node> head;
public:
    void push(const T& data)
    {
        const std::shared_ptr<node> new_node = std::make_shared<node>(data);
        new_node->next = head.load();
        while(!std::atomic_compare_exchange_weak(&head, &new_node->next, new_node));
    }
    
    std::shared_ptr<T> pop()
    {
        std::shared_ptr<node> old_head = std::atomic_load(&head);
        while (old_head && !std::atomic_compare_exchange_weak(&head, &old_head, old_head->next));
        return old_head ? old_head->data : std::shared_ptr<T>();
    }
};

//在可能的情况下，std::shared_ptr<>实现不是无锁的，需要搜共处理引用计数

//一种可能的技术设计为每个节点使用不止一个而是两个引用计数，一个内存计数和一个外部计数，这两个计数值之和是结点总的引用书，外部计数始终与结点指针在一起，并且每次读取指针的时候外部计数增一，当读取结点结束时，内部计数减一，读取指针这样一个简单操作会导致外部计数增一，并且在此操作结束时内部计数减一

//当内部计数/指针对不在需要时(即多个线程不再访问结点时)，内部计数增加外部计数的值减一，并废除外部计数，一旦内部计数的值为零，就没有引用结点，此时可删除此结点，使用原子操作来更新共享数据也是很重要的，现在我们来看一个使用这种计数来确保结点只会被安全收回的无栈锁的实现

//更好的内部数据结构和push()的实现如清单7.10所示

//清单7.10 在使用两个引用计数的无锁栈中入栈结点

template<typename T>
class lock_free_stack_one
{
private:
    struct node;
    
    struct counted_node_ptr   //①
    {
        int external_count;
        node* ptr;
    };
    
    struct node
    {
        std::shared_ptr<T> data;
        std::atomic<int> internal_count;   //②
        counted_node_ptr next;   //③
        
        node(const T& data_) : data(std::make_shared<T>(data_)), internal_count(0) {}
    };
    
    std::atomic<counted_node_ptr> head;   //④
public:
    ~lock_free_stack_one()
    {
        while(pop());
    }
    
    void push(const T& data)   //⑤
    {
        counted_node_ptr new_node;
        new_node.ptr = new node(data);
        new_node.external_count = 1;
        new_node.ptr->next = head.load();
        while (!head.compare_exchange_weak(new_node.ptr->next, new_node));
    }
    
    void pop()
    {
    
    }
};

//首先，在counted_node_ptr结构中包含了外部变量与结点的指针①，在node结构体③中将使用counted_node_ptr类型的next指针以及内部变量②，因为counted_node_ptr是一种简单的结构，因此在std::atomic<>模板中使用它作为列表的头结点④

//这些支持双字比较和交换操作的平台上，这个结构足够小，使得std::atomic<counted_node_ptr>是无锁的，如果不是在你的平台上，那么最好使用清单7.9中提到的std::shared_ptr<>，因为当类型太大使得平台的原子指令不能实现时，std::atomic<>将使用一个互斥元来保证原子性(因此最后使得你的"无锁"算法变成基于锁的算法)，或者，如果你想限制计数的位数，并且你知道你的平台中指针有空闲位(例如，地址空间只有48位但是指针有64位)，你可以在单个字中计数存储在指针的空闲位中，这种方法需要与平台相关的知识，这就超出了本书的范围了

//push()相对简单一些⑤，构造一个指向新分配界定的counted_node_ptr，并将它的next赋值为当前的head，之后用compare_exchange_weak()来个head赋值，就像之前的清单所示，设置计数器时，将内部计数设为零，外部计数设为一，因为这是新创建的结点，只有一个外部引用(即head本身)

//同理，pop()的实现也复杂了一些，如清单7.11所示

//清单7.11 使用两个引用计数从无锁栈中出栈一个结点

template<typename T>
class lock_free_stack_two
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
        } while(!head.compare_exchange_strong(old_counter, new_counter));   //①
    
        old_counter.external_count = new_counter.external_count;
    }
    
public:
    std::shared_ptr<T> pop()
    {
        counted_node_ptr old_head = head.load();
        for (;;)
        {
            increase_head_count(old_head);
            node* const ptr = old_head.ptr;   //②
            if (!ptr)
            {
                return std::shared_ptr<T>();
            }
            
            if (head.compare_exchange_strong(old_head, ptr->next))   //③
            {
                std::shared_ptr<T> res;
                res.swap(ptr->data);   //④
                
                int const count_increase = old_head.external_count - 2;   //⑤
                
                if (ptr->internal_count.fetch_add(count_increase) == -count_increase)   //⑥
                {
                    delete ptr;
                }
                
                return res;   //⑦
            }
            else if (ptr->internal_count.fetch_sub(1) == 1)
            {
                delete ptr;   //⑧
            }
        }
    }
};

//这里，一旦你载入了head的值，就必须将引用此head结点的外部计数的值增一，用以表明你引用了此结点并且确保解引用是安全的，如果在增加引用计数前解引用此指针，那么另一个线程就可以在你读取这个节点前释放该节点，因此使得它变成悬挂指针，这是使用两个分开的引用计数的主要原因，通过增加外部引用计数，就可以保证直到你访问时指针仍然是有效的，在compare_exchange_strong()循环内部增加计数的值①，确保了没有别的线程在此时改变它

//一旦增加了计数，为了访问它指向的结点，可以安全解引用从head载入的ptr的值②，如果指针不为空，就可以通过在head上调用comapre_exchange_strong()来移动结点③

//如果comapre_exchange_strong()成功了，就可以拥有该节点，并且交换出data以备以后返回它④，这就确保了就算别的线程读取栈的时候一直持有指向此结点的指针，data也不需要一直保持，然后就可以使用原子操作fetch_add将结点外部计数的值加到内部计数上⑥，如果当前引用计数的值为零，那么先前你增加的值(即fetch_add的返回值)就是负数，此时就可以删除这个结点，请注意你增加的值比外部计数的值减少2⑤，你已经从列表中移出了结点，因此计数减一，无论是否删除此结点，程序都结束了，因此可以返回data⑦

//如果比较/交换③失败了，则表明在此之前另一个线程移动了该节点，或者另一个线程入栈了一个新的结点，不管怎么样，你都需要用比较/交换返回的head新值重新开始，但是首先你必须减少你试图移动的结点的引用计数，该线程不会再读取它了，如果这是持有引用的最后一个线程(因为另一个线程将它从栈中移出)，那么内部引用计数的值为一，因此减少一将使得值变为零，在这种情况下，可以在循环之前删除此结点⑧

//迄今为止，所有原子操作使用了默认的std::memory_order_seq_cst内存顺序，在大多数系统中，这种方法比别的内存顺序消耗更多的执行时间以及同步开销，现在你有决定数据结构逻辑的权利，就可以考虑放松一些内存顺序要求，可以减少使用栈的不必要开销，因此，先不考虑栈，考虑一下无锁队列的设计，检查栈操作并问问自己，对于一些操作是否可以使用更简单的内存顺序并且获得同样的安全性

#pragma clang diagnostic pop

