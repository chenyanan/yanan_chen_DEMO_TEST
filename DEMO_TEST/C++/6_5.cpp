//
//  6_5.cpp
//  123
//
//  Created by chenyanan on 2017/5/9.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <iostream>
#include <thread>
#include <mutex>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//2.等待一个数据项pop
//清单6.6提供了一个使用细粒度锁的线程安全队列，但它只支持try_pop()(并且只有一种重载)，前面清单6.2中的有用的wait_and_pop()函数呢？能否用细粒度锁实现相同的接口呢？

//当然，回答是肯定的，但真正的问题是，怎么做？修改push()是很简单的，只需要在函数的尾部添加data_cond.notify_one()调用即可，就如同在清单6.2中一样，实际上这并非那么简单，使用细粒度锁是为了实现并发量的最大化，如果在对notify_one()的调用中保留互斥元被锁定(如同清单6.2)，那么如果被通知的线程在互斥元解锁之前被唤醒，它就得等待互斥元，另一个方面，假设在调用notify_one()之前就解锁互斥元，那么在当等待中的线程被唤醒时，此互斥元就可以被它使用(假设没有别的线程先锁定它)，这是一个小小的改进，但某些情况下可能是很重要的

//wait_and_pop()更复杂一些，因为得决定在哪里等待，断言是什么，以及需要锁定哪个互斥元，你所等待的条件是"队列非空"，它是用head != tail表示，如上所示，这有可能要求head_mutex和tail_mutex都被锁定，但是在清单6.6中，你已经决定只需要在读取tail的时候锁住tail_mutex, 比较本身是不需要的，因此可以将相同的逻辑应用到此处，如果将断言设定为head! = get_tail()，就只需要持有head_mutex，因此在调用data_cond.wait()时可以使用head_mutex上的锁，一旦增加了等待逻辑，这种实现就跟try_pop()一样了

//try_pop()的第二个重载以及相应的wait_and_pop()重载就需要仔细思考一下，如果只是将从old_head获取到的std::shared_ptr<>替换为向value参数拷贝赋值，就可能存在异常安全问题，此刻，数据项已经从队列中删除，且互斥元已解锁，剩下的就是向调用者范围数据，然而，如果拷贝赋值引发异常(这是很有可能发生的)，数据项就会丢失，因为无法将其返回到队列中同样的位置

//如果模板参数中使用的实际类型T具有不引发异常的移动复制运算符，或是不引发异常的交换操作，就可以使用之，但是我们实际上更想要一个适用于所有类型T的通用解决方案，在这种情况下，就需要在结点从链表中删除前，将可能的异常依法移动到锁的范围内，这就意味着，需要另外的pop_head()重载，在修改链表之前获取存储的值

//相比之下，empty()就很平常了，只需要锁定head_mutex并检查head == get_tail()(如清单6.10所示)，队列最终的代码如清单6.7，清单6.8，清单6.9和清单6.10所示

//清单6.7 使用锁的等待的线程安全队列:内部与接口

template<typename T>
class threadsafe_queue
{
private:
    struct node
    {
        std::shared_ptr<T> data;
        std::unique_ptr<node> next;
    };
    
    std::mutex head_mutex;
    std::unique_ptr<node> head;
    std::mutex tail_mutex;
    node* tail;
    std::condition_variable data_cond;
    
public:
    threadsafe_queue() : head(new node), tail(head.get()) {}
    threadsafe_queue& operator=(const threadsafe_queue& other) = delete;
    
    std::shared_ptr<T> try_pop();
    bool try_pop(T& value);
    std::shared_ptr<T> wait_and_pop();
    void wait_and_pop(T& value);
    void push(T new_value);
    void empty();
};

//像队列中入队新节点是很直观的，其实现(如清单6.8所示)与之前展示的非常接近

template<typename T>
void threadsafe_queue<T>::push(T new_value)
{
    std::shared_ptr<T> new_data(std::make_shared<T>(std::move(new_value)));
    std::unique_ptr<node> p(new node);
    {
        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        tail->data = new_data;
        node* const new_tail = p.get();
        tail->next = std::move(p);
        tail = new_tail;
    }
    data_cond.notify_one();
}

//清单6.9 使用锁和等待的线程安全队列:wait_and_pop()

template<typename T>
class threadsafe_queue_A
{
private:
    struct node
    {
        std::shared_ptr<T> data;
        std::unique_ptr<node> next;
    };
    std::mutex head_mutex;
    std::unique_ptr<node> head;
    std::mutex tail_mutex;
    node* tail;
    std::condition_variable data_cond;
    
    node* get_tail()
    {
        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        return tail;
    }
    
    std::unique_ptr<node> pop_head()   //①
    {
        std::unique_ptr<node> old_head = std::move(head);
        head = std::move(old_head->next);
        return old_head;
    }
    
    std::unique_lock<std::mutex> wait_for_data()   //②
    {
        std::unique_lock<std::mutex> head_lock(head_mutex);
        data_cond.wait(head_lock, [&] { return head.get() != get_tail(); });
        return std::move(head_lock);   //③
    }
    
    std::unique_ptr<node> wait_pop_head()
    {
        std::unique_lock<std::mutex> head_lock(wait_for_data());   //④
        return pop_head();
    }
    
    std::unique_ptr<node> wait_pop_head(T& value)
    {
        std::unique_lock<std::mutex> head_lock(wait_for_data());   //⑤
        value = std::move(*head->data);
        return pop_head();
    }
public:
    std::shared_ptr<T> wait_and_pop()
    {
        const std::unique_ptr<node> old_head = wait_pop_head();
        return old_head->data;
    }
    
    void wait_and_pop(T& value)
    {
        const std::unique_ptr<node> old_head = wait_pop_head(value);
    }
};

//清单6.9所示的pop端实现具有一些辅助函数来简化代码和去重，例如pop_head()①与wait_for_data()②，相对应地，它们修改链表以删除首项，并且等待队列中有数据要pop，wait_for_data()特别值得一提，因为它不仅使用一个lambda函数作为断言来等待条件变量，而且它向调用者返回锁的实例③，这是为了确保当数据被相关的wait_pop_head()重载④，⑤修改时，持有相同的锁，在清单6.10中列出的try_pop()代码中也复用了pop_head()

//清单6.10 使用锁和等待的线程安全队列:try_pop()和empty()

template<typename T>
class threadsafe_queue_B
{
private:
    struct node
    {
        std::shared_ptr<T> data;
        std::unique_ptr<node> next;
    };
    
    std::mutex head_mutex;
    std::unique_ptr<node> head;
    std::mutex tail_mutex;
    node* tail;
    std::condition_variable data_cond;
    
    node* get_tail()
    {
        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        return tail;
    }
    
    std::unique_ptr<node> pop_head()
    {
        std::unique_ptr<node> old_head = std::move(head);
        head = std::move(old_head->next);
        return old_head;
    }
    
    std::unique_ptr<node> try_pop_head()   /* new */
    {
        std::lock_guard<std::mutex> head_lock(head_mutex);
        if (head.get() == get_tail())
            return std::unique_ptr<node>();
        return pop_head();
    }
    
    std::unique_ptr<node> try_pop_head(T& value)   /* new */
    {
        std::lock_guard<std::mutex> head_lock(head_mutex);
        if (head.get() == get_tail())
            return std::unique_ptr<node> ();
        value = std::move(*head->data);
        return pop_head();
    }
public:
    std::shared_ptr<T> try_pop()   /* new */
    {
        std::unique_ptr<node> old_head = try_pop_head();
        return old_head ? old_head->data : std::shared_ptr<T>();
    }
    
    bool try_pop(T& value)   /* new */
    {
        const std::unique_ptr<node> old_head = try_pop_head(value);
        return old_head;
    }
    
    void empty()   /* new */
    {
        std::lock_guard<std::mutex> head_lock(head_mutex);
        return (head.get() == get_tail());
    }
};

//这种队列的实现将会作为第7章中提到的无锁队列的基础，这是一个无界队列，只要还有可用的内存，即使没有值被删除，线程也可以一直往队列中push新的值，与无界队列相对的是有界队列，当队列被创建的时候，它的最大长度也已经定下来了，一旦一个有界队列已满，再试图往队列中push更多的元素就会失败或者阻塞，直到有元素从队列中pop出来，以腾出空间，有界队列对那些基于执行的任务在线程中划分工作，要确保均匀铺开工作的时候，是很有用的(参见第8章)，这可以阻止线程过快填充队列，以至于远远超过从队列中读取数据的线程

//这里展示的无界队列的实现可以通过在push()中等待条件变量，很容易地扩展为限制长度的队列，不同于等待队列中有数据项(例如在pop()中所做的)，你需要等待队列中的项目数量低于最大值，对有界队列的进一步讨论超出了本书的范围，现在，让我们越过队列，来看一看更复杂的数据结构

#pragma clang diagnostic pop
