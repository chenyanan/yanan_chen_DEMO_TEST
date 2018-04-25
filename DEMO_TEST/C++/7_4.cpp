//
//  7_4.cpp
//  123
//
//  Created by chenyanan on 2017/5/11.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <iostream>
#include <thread>
#include <mutex>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//7.2.2 停止恼人的泄漏:在无锁数据结构中管理内存

//我们首先观察pop()，当一个线程删除结点，而另一个线程仍然持有指向此节点的指针时，我们选择泄漏结点来避免竞争条件，那么久只能解引用了，尽管如此，在任何合理的C++程序中，泄漏内存都是不可接受的，因此，我们必须做一些事，现在该考虑这个问题并且找出解决办法了

//最基本的问题及时，你想释放一个结点，但是直到你确保没有别的线程持有指向此结点的指针的时候，你才能释放此结点，如果只有一个线程曾经在一个特定的栈实例上调用pop()，那么就不会再操作此结点了，因此调用pop()的线程就一定是唯一一个操作此结点的线程，并且可以安全地删除此结点

//另一方面，如果需要处理多个线程在同一个栈实例上调用pop()的情况，那么就需要一些方法来追踪何时可以安全地删除结点，这就从根本上意味着你需要为节点写一个专用的垃圾回收器，现在，这可能听上去很可怕，尽管它确实很讨厌，但是也不是太糟糕，只需要检查结点，尽管它确实很讨厌，但是也不是太糟糕，只需要检查结点，并且只检查在pop()中存取的结点，不需要担心在push()中存取的结点，因为他们只能被一个线程存取，直到他们在栈上位置，然而，多个线程可能在pop()中存取同一个结点

//如果没有线程调用pop()，那么可以删除目前等待删除的所有结点，因此，当你获得数据时，如果将此结点添加到"将被删除"的列表中，那么当没有线程调用pop()时就可以删除它，如何知道有没有别的线程在调用pop()呢？有个简单的方法，数清数目，如果在进入的时候计数器加一，在离开的时候计数器减一，那么当计数器为零的时候，就可以安全地删除"将被删除"列表中的结点，当然，此计数器必须为原子计数器，从而可以安全地被多个线程存取，清单7.4给出了修改后的pop()函数，并且清单7.5列出了此实现的支撑函数

//清单7.4 当pop()中更没有线程时回收结点

template<typename T>
class lock_free_stack_thr
{
private:
    struct node
    {
        std::shared_ptr<T> data;
        node* next;
        
        node(const T& data_) : data(std::make_shared<T>(data_)) {}
    };
    std::atomic<node*> head;
    
    std::atomic<unsigned> threads_in_pop;   //①原子变量
    void try_reclaim(node* old_head);
public:
    std::shared_ptr<T> pop()
    {
        ++threads_in_pop;   //②在做任何其他事情前增加计数
        node* old_head = head.load();
        while (old_head && !head.compare_exchange_weak(old_head, old_head->next));
        std::shared_ptr<T> res;
        if (old_head)
        {
            res.swap(old_head->data);   //③如果可能，回收删除的几点
        }
        try_reclaim(old_head);   //④从结点中提取数据，而不是复制数据
        return res;
    }
};

//原子变量threads_in_pop①被用作计数目前有多少线程试图从占中移出数据项，在pop()开始的地方②增加计数器，在try_reclaim()中减少计数器，而一旦结点被移出的时候就会调用此函数④，因为可能将延迟删除结点，因此可以使用swap()来讲数据从结点中删除③，而不仅仅复制指针，因此放不再需要时可以自动地删除此数据，而不会因为存在对未删除结点的引用而一直保持它，清单7.5给出了try_reclaim()的内部实现

//清单7.5 引用计数的回收机制

template<typename T>
class lock_free_stack_for
{
private:
    struct node
    {
        std::shared_ptr<T> data;
        node* next;
        
        node(const T& data_) : data(std::make_shared<T>(data_)) {}
    };
    std::atomic<node*> head;
    std::atomic<unsigned> threads_in_pop;
    
    std::atomic<node*> to_be_deleted;
    
    static void delete_nodes(node* nodes)
    {
        while (nodes)
        {
            node* next = nodes->next;
            delete nodes;
            nodes = next;
        }
    }
    
    void try_reclaim(node* old_head)
    {
        if (threads_in_pop == 1)   //①
        {
            node* nodes_to_delete = to_be_deleted.exchange(nullptr);   //②将要被删除的结点清单
            if (!--threads_in_pop)   //③是pop()中唯一的线索吗?
            {
                delete_noes(nodes_to_delete);
            }
            else if (nodes_to_delete)   //⑤
            {
                chain_pending_nodes(nodes_to_delete);   //⑥
            }
            delete old_head;   //⑦
        }
        else
        {
            chain_pending_node(old_head);   //⑧
            --threads_in_pop();
        }
    }
    
    void chain_pending_nodes(node* nodes)
    {
        node* last = nodes;
        while(const node* next = last->next)   //⑨跟随下一个指针，链至末尾
        {
            last = next;
        }
        chain_pending_ndoes(nodes, last);
    }
    
    void chain_pending_nodes(node* first, node* last)
    {
        last->next = to_be_deleted;   //⑩
        while(!to_be_deleted.compare_exchange_weak(last->next, first));   //⑪循环以保证last->next正确
    }
    
    void chain_pending_node(node* n)
    {
        chain_pending_nodes(n,n);   //⑫
    }
};

//当回收结点时，如果threads_in_pop的值为1①，那么在pop()中这就是唯一的线程，这就意味着可以安全删除刚移除出来的结点⑦，并且可能安全地删除等待的结点，如果计数器的值不为1，那么删除任何结点都不安全，因此将刺激诶单加入到等待的列表中⑧

//现在假设threads_in_pop的值为1，此时需要回收等待的结点，如果不回收，那么僵这些结点将一直等待直到栈被销毁，回收结点时，首先用原子操作exchange来查找列表②，然后将threads_in_pop的技术减一③，如果计数减一后值为零，就可以得知没有别的线程存取此等待结点列表，可能会有新的等待结点，但是只要回收列表是安全的，就不需要操心这些新的等待结点，然后，调用delete_nodes来迭代此列表，并且删除结点④

//如果计数减一后值不为零，那么回收结点就不安全了，因此如果此时后等待的结点⑤，那么就将此节点插入到等待删除结点列表的尾部⑥，当多个线程同时存取数据结构时就有可能发生这种情况，别的线程可能在第一次去threads_in_pop值①和查找列表②之间调用pop()，这就可能在列表中增加新的结点，并且此结点被一个或多个线程存取，在图7.1中，线程C增加结点Y至to_be_deleted列表中，即使线程B仍然引用结点Y作为old_head，并且会读取此结点的next指针，因此线程A在删除结点的时候不可避免地会造成线程B未定义的行为

//          head->X->Y->Z->
// to_be_deleted->A->
//threads_in_pop == 0

//线程A调用pop()并且在首次读取threads_in_pop之后被try_reclaim()抢占
//          head->Y->Z->
// to_be_deleted->A->
//threads_in_pop == 1(线程A)

//线程B调用pop()并且在首次读取head之后被抢占
//          head->Y->Z->
//      old_head->Y->Z->
// to_be_deleted->A->
//threads_in_pop == 2(线程A和B)

//线程C调用pop()并且一直运行到pop()返回
//          head->Z->
// to_be_deleted->Y->A->
//threads_in_pop == 2(线程A和B、C已完成)

//线程A恢复并且仅在执行了to_be_deleted.exchange(nullptr)之后才被抢占
//            head->Z->
//nodes->to_delete->Y->A->
//如果我们不再次测试threads_in_pop结点Y和A会被删除
//  threads_in_pop == 2
//   to_be_deleted->nullptr

//线程B恢复并且为调用compare_exchange_strong()而读取old_head->next
//          head->Z->
//      old_head->Y(叉，结点Y在A的to_be_deleted列表上)->Z->
//threads_in_pop == 2

//图7.1 三个线程并发调用pop()，在回收try_reclaim()中删除的结点之后必须检查threads_in_pop

//为了将等待删除的结点链接到等待列表中，需要重新使用结点的next指针来讲它们链接起来，对于重新链接一个已存在链表到列表尾部，则需要遍历链表来找到尾部⑨，用当前的to_be_deleted指针来替代最后一个结点的next指针⑩，并且存储链表中的第一个结点作为新的to_be_deleted指针⑪，必须在循环中使用compare_exchange_weak来确保没有遗漏其他线程添加的结点，这种做法的好处是当链表发生变化时，从链尾更新next指针，在链表中添加一个结点是一种特殊情况，即链表中添加的第一个结点与最后一个节点是相同的⑫

//在低负载的情况下，即当没有进程在调用pop()这样一种合适的静态点的时候，这种方法是很有效的，尽管如此，在回收结点和删除刚移出的结点之前⑦都需要检查threads_in_pop计数器是否减少为零③，这是因为这种状态是很短暂的，删除结点是一种会消耗一定时间的操作，并且别的线程修改列表的窗口越小越好，在线程第一次发现threads_in_pop的值为1与试图删除结点之间的时间越长，别的线程调用pop()以及threads_in_pop的值不再为1的可能性就越大，因此就阻止了此结点被真正的删除

//在高负载的情况下，因为在其他线程调用pop()结束之前就会有别的线程调用pop()，因此基本上不可能有这种静止状态，在这种情况下，to_be_deleted列表很容易就越界了，并且再次内存泄漏，如果没有任何静止状态，那么就需要用别的方法来回收结点，关键点就是识别没有别的线程将访问某个特定的结点，那么久可以回收此结点了，迄今为止，最简单的方法就是使用风险指针(hazardpointers)

#pragma clang diagnostic pop

