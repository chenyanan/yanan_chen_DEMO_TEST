//
//  6_8.cpp
//  123
//
//  Created by chenyanan on 2017/5/10.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <iostream>
#include <thread>
#include <mutex>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//6.3.2 编写一个使用锁的线程安全链表

//链表是一种最基本的数据结构，因此它应该能被直接写成线程安全的，不是吗？那么，这取决于你追求什么样的功能，并且需要提供迭代器支持，这是我一直避免将其加入到你的基础图中的东西，因为它太复杂了，STL风格的迭代器支持，指的是迭代器必须持有某种对容器内部数据结构的引用，如果容器可以被另一个线程修改，这个引用必须仍然有效，这就从根本上要求迭代器在部分数据结构上持有锁，考虑到STL风格的迭代器的生存期是完全不受容器控制的，这就不是个好主意

//另一种方式是提供类似于for_each这样的迭代函数作为容器本身的一部分，这就让容容器完全负责迭代器和锁，但是这与第3章中提到的避免死锁原则是冲突的，为了使得for_each做一些有用的操作，它就必须在持有内部锁的时候调用用户提供的代码，不仅如此，为了使用户提供的代码能够作用于数据项，它必须将对每个数据项的引用传递给用户提供的代码，你可以通过向用户体用的代码传递每个数据项的副本来避免这一点，但是当数据项很大时，这种南方是就很耗费资源

//留给你的问题是，要为链表提供哪些操作，如果回顾一下清单6.11以及6.12，就可以知道需要下列操作

//向链表添加新项目
//从链表中删除满足一定条件的项目
//在链表中查找满足一定条件的项目
//更新满足一定条件的项目
//复制链表中每个项目到另一个容器中

//为了令其成为良好的通用链表容器，添加进一步的操作例如在指定位置插入是很有用的，但是对于查找表示不需要的，因此我将留给读者作为练习

//在链表中使用细粒度锁的基本思想是每个节点使用一个互斥元，如果链表很大，就会有很多互斥元!其好处就是在链表不同部分的操作是真正并发的，每个操作仅在其真正关注的结点上持有锁，并且当它移动到下一个结点时，会解锁每个节点，清单6.13给出了正是这样一个链表的实现

//清单6.13 支持迭代的线程安全链表

template<typename T>
class threadsafe_list
{
    struct node   //①
    {
        std::mutex m;
        std::shared_ptr<T> data;
        std::unique_ptr<node> next;
        
        node() : next() {};   //②
        node(const T& value) : data(std::make_shared<T>(value)) {}   //③
    };
    
    node head;
public:
    threadsafe_list() {}
    ~threadsafe_list()
    {
        remove_if([] (const node&) { return true; });
    }
    
    threadsafe_list(const threadsafe_list& other) = delete;
    threadsafe_list& operator=(const threadsafe_list& other) = delete;
    
    void push_fornt(const T& value)
    {
        std::unique_ptr<node> new_node(new node(value));   //④
        std::lock_guard<std::mutex> lk(head.m);
        new_node->next = std::move(head.next);   //⑤
        head.next = std::move(new_node);   //⑥
    }
    
    template<typename Fucntion>
    void for_each(Fucntion f)   //⑦
    {
        node* current = &head;
        std::unique_lock<std::mutex> lk(head.m);   //⑧
        while (const node* next = current->next.get())   //⑨
        {
            std::unique_lock<std::mutex> next_lk(next->m);   //⑩
            lk.unlock();   //⑪
            f(*next->data);   //⑫
            current = next;
            lk = std::move(next_lk);   //⑬
        }
    }
    
    template<typename Predicate>
    std::shared_ptr<T> find_first_if(Predicate p)   //⑭
    {
        node* current = &head;
        std::unique_lock<std::mutex> lk(head.m);
        while (const node* next = current->next.get())
        {
            std::unique_lock<std::mutex> next_lk(next->m);
            lk.unlock();
            if (p(*next->data))   //⑮
                return next->data;   //⑯
            current = next;
            lk = std::move(next_lk);
        }
        return std::shared_ptr<T>();
    }
    
    template<typename Predicate>
    void remove_if(Predicate p)   //⑰
    {
        node* current = &head;
        std::unique_lock<std::mutex> lk(head.m);
        while (const node* next = current->next.get())
        {
            std::unique_lock<std::mutex> next_lk(next->m);
            if (p(*next->data))   //⑱
            {
                std::unique_ptr<node> old_next = std::move(current->next);
                current->next = std::move(next->next);   //⑲
                next_lk.unlock();
            }   //⑳
            else
            {
                lk.unlock();   //㉑
                current = next;
                lk = std::move(next_lk);
            }
        }
    }
};

//清单6.13中的threadsafe_list<>是一个单链表，每个入口是一个node结构体①，默认构造的node是链表的head，开始时它的next指针为NULL②，新节点是通过push_front()函数增加的，首先构造一个新节点④，在堆上分配存储的数据③，保留next指针为NULL，然后你需要为head节点获取互斥元上的锁来得到正确的next值，并且通过将head.next设置为指向新节点来得到正确的next值⑤，并且通过将head.next设置为指向新节点来实现将这个节点插入链表前方⑥，到目前为止，一切都还不错，你只需要锁住一个互斥元来将新项目插入链表，因此没有死锁的风险，同样，缓慢的内存分配发生在锁之外，因此锁只保护几个指针值得更新并不会失败，下面是迭代函数

//首先，我们来看看for_each()⑦，这个操作用接受某种类型的Function，所用于表中的每一个元素，通常在大多数标准库中，它会通过值形式接受此函数，并且可以与真正的函数或者是具有函数调用操作符的某种类型的对象一起运作，在这种情况下，函数必须接受类型为T的值作为唯一的参数，这就是你进行交替锁定的地方，刚开始，你锁定head节点上的互斥元⑧，然后就可以安全地获得指向next节点的指针(使用get()因为你并不打算获取该指针的所有权)，如果该指针不为NULL⑨，就锁定那个节点上的互斥元⑩来处理数据，一旦你锁定那个节点，就可以释放之前节点的锁⑪，并且调用指定的函数⑫，一旦函数完成，就可以更新current指针指向你刚刚处理的结点，并且将锁的拥有权从next_lk移动(move)到lk⑬，因为for_each将每个数据项直接传递给提供的Function，如果需要的话，你可以使用它更新数据项或者将它们复制到另一个容器中，或其他任何事情，如果函数表现良好这就是安全的，因为拥有数据项的结点在整个调用中都持有互斥元

//find_first_if()⑭与for_each()是类似的，最主要的而不同指出在于提供的Predicate必须返回true来表明找到了匹配项或者false来表明没找到匹配项⑮，一旦你找到匹配项，你就返回找到的数据⑯而不是继续查找，你可以用for_each()来实现它，但是一旦找到匹配项就不需要继续处理链表中剩下的部分了

//remove_if()⑰有些不同，因为这个函数必须真正地更新链表，你不能用for_each()来实现它，如果Predicate返回true⑱，你就可以通过更新current->next⑲将这个节点从链表中删除，一旦完成，就可以释放next节点互斥元所斥候的锁，当你讲它移入的std::unique_ptr<node>超出范围时，该节点就被删除了⑳，在这种情况下，你不用更新current，因为需要检查新的next结点，如果Predicate返回false，你就像以前一样处理//㉑

//那么，这些互斥元上存在着死锁或者竞争条件么？答案毫无疑问是否定的，前提是所提供的断言和函数是表现良好的，迭代总是按照同一方式，通常从head结点开始，并且总是在释放当前互斥元前就锁住下一个互斥元，因此不可能在不同的线程间有不同的锁顺序，唯一可能产生竞争条件的，就是在remove_if()中对待删除结点的删除，因为你会在解锁互斥元之后才这么做(销毁一个锁定的互斥元时未定义的行为)，然而，稍加思考就会知道这确实是安全的，因为你仍然持有之前结点(current)上的互斥元，因此没有新线程可以试图获取你要删除的结点上的锁

//并发的机会又如何呢？这种细粒度锁的关键是在单个互斥元上提高并发的可能性，那么实现这一点了么？使得，已经实现了，不同的线程可以同时在链表的不同结点上工作，无论它们正在用for_each()处理每个出具想，还用find_first_if()搜索，还是用remove_if()来删除项目⑳，但是因为每个节点的互斥元轮流被锁定，线程不能互相超越，如果一个线程花了很长时间处理一个特定的结点，当别的线程到达此特定结点时就必须等待

#pragma clang diagnostic pop
