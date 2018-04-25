//
//  6_3.cpp
//  123
//
//  Created by chenyanan on 2017/5/5.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <iostream>
#include <thread>
#include <mutex>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//6.2.3 使用细粒度锁和条件变量的线程安全队列

//在清单6.2和清单6.3中，有一个被保护的数据项(data_queue)和一个互斥元，为了使用细粒度锁，你需要瞧一瞧队列的组成部分，并且讲一个互斥元与每个不同的数据项联系起来

//实现队列最贱的数据结构为单链表，如图6.1所示，队列包含一个头指针，指向链表的第一项，并且每一项都指向下一项，将数据项从队列中移走时，首先将头指针指向下一个书记徐昂，然后返回以前头指针所指向的数据项

//数据项被添加到队列的另一端，为了实现这一点，队列还包含一个尾指针，指向链表的最后一项，通过将最后一项的next指针指向新结点，然后将为指针更新为指向新的一项，可以实现添加结点，当链表为空时，头指针和为指针均为NULL

//清单6.4展示了这种队列的简单实现，基于清单6.2中队列接口的缩减版本，因此队列只支持单线程使用，因此只需要一个try_pop()函数，而没有wait_and_pop()

//清单6.4 一种简单的单线程队列实现

template<typename T>
class queue
{
private:
    struct node
    {
        T data;
        std::unique_ptr<node> next;
        node(T data_) : data(std::move(data_)) {}
    };
    
    std::unique_ptr<node> head;   //①
    node* tail;   //②
    
public:
    queue() {}
    queue(const queue& other) = delete;
    
    std::shared_ptr<T> try_pop()
    {
        if (!head)
            return std::shared_ptr<T>();
        const std::shared_ptr<T> res(std::make_shared<T>(std::move(head->data)));
        std::unique_ptr<node> const old_head = std::move(head);
        head = std::move(old_head->next);   //③
        return res;
    }
    
    void push(T new_value)
    {
        std::unique_ptr<node> p(new node(std::move(new_value)));
        node* const new_tail = p.get();
        
        if (tail)
            tail->next = std::move(p);   //④
        else
            head = std::move(p);   //⑤
        tail = new_tail;   //⑥
    }
};

//首先，要注意清单6.4使用了std::unique_ptr<node>来管理结点，因为这儿可以保证当结点不再需要时，它们(以及它们指向的数据)能被删除，而无需显示地编写delete，所有者链表从head开始管理，指向最后一个节点的tail是个裸指针

//尽管在单线程环境中这种实现方法可以良好地工作，但是如果在多线程环境下试图使用细粒度锁，就会带来wanting，假定有两个数据项(head①和tail②)，原则上可以使用两个互斥元，一个用来保护head，另一个用来保护tail，但是这样会存在一些问题

//最明显的问题就是，push()可以修改head⑤和tail⑥，因此它就需要锁住这两个互斥元，虽然倒霉，但也不是什么大问题，因为锁住两个互斥元时可能的，关键的问题是，push()和pop()都要访问某个节点的next指针，push()更新tail->next④，try_pop()读取head->next③，如果队列中只有一个节点，那么head==tail，即head->next和tail->next是同一个的对象，因而需要保护，因为还没读取head和tail时，你无法分辨它们是否为同一个对象，你就必须在push()和try_pop()中锁定同一个互斥元，所以没有比以前做得更好，有没有办法摆脱这一困境呢?

#pragma clang diagnostic pop

