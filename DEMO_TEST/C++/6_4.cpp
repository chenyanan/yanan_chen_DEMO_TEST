//
//  6_4.cpp
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

//1.通过分离数据允许并发

//可以通过预先分配一个不存储数据的傀儡结点，以保证列队列中总是至少会有一个结点，将在头尾的两个访问分开，来解决这个问题，对于一个空队列，head和tail都指向这个傀儡结点，而不是NULL，这样就没问题了，因为如果当队列为空，try_pop()不会访问head->next，当你将一个节点加入队列(于是就有一个真正的结点)，head和tail就指向不同的节点，因此在head->next和tail->next就不存在竞争，缺点是，必须添加一个额外的间接层，通过指针存储数据，以便允许假节点，清单6.5展示该实现现在的样子

//清单6.5 使用傀儡结点的简单队列

template<typename T>
class puppet_queue
{
private:
    struct node
    {
        std::shared_ptr<T> data;   //①
        std::unique_ptr<node> next;
    };
    
    std::unique_ptr<node> head;
    node* tail;
    
public:
    puppet_queue() : head(new node), tail(head.get()) {}   //②
    puppet_queue(const puppet_queue& other) = delete;
    puppet_queue& operator=(const puppet_queue& other) = delete;
    
    std::shared_ptr<T> try_pop()
    {
        if (head.get() == tail)   //③
            return std::shared_ptr<T>();
        const std::shared_ptr<T> res(head->data);   //④
        std::unique_ptr<node> old_head = std::move(head);
        head = std::move(old_head->next);   //⑤
        return res;   //⑥
    }
    
    void push(T new_value)
    {
        std::shared_ptr<T> new_data(std::make_shared<T>(std::move(new_value)));   //⑦
        std::unique_ptr<node> p(new node);   //⑧
        tail->data = new_data;   //⑨
        const node* new_tail = p.get();
        tail->next = std::move(p);
        tail = new_tail;
    }
};

//对try_pop()的改变时很小的，首先，是比较head与tail，俄入世检查其是否为NULL，因为傀儡结点意味着head不可能是NULL，因为head是一个std::unique_ptr<node>你需要调用head.get()来进行比较，其次，因为node现在是通过指针来存储数据的①所以可以直接获取指针④而不必构造T的一个新实例，最大改变是在push()中，必须在堆上创建T的一个新实例，并且在std::shared_ptr<>⑦中取得其所有权(使用std::make_shared是为了避免第二次为引用计数分配内存的开销)，你创建的新节点将会作为新的傀儡结点，因此无需向构造函数提供new_value⑧，取而代之的是，将旧的傀儡结点上的数据设置为新分配的new_value的副本⑨，最后，为了得到一个傀儡结点，你必须在构造函数中创建它②

//到目前为止，我敢肯定你想知道这些变化为你带来了什么，并如何让队列变得线程安全，好吧，push()现在只访问tail，不访问head，这是一个改进，try_pop()既访问head又访问tail，但只要在最初的比较重需要tail，因此这个锁是很短暂的，最大的收获就是，傀儡结点意味着try_pop()和push()不会再同一个结点上进行操作，因此不再需要一个包含一切的互斥元，因此，你可以为head和tail各设置一个互斥元，那锁应该放在哪呢？

//我么你的目标是实现最大程度的并发，因此希望持有锁的时间尽可能的短，push()比较简单，互斥元在访问tail的全称都需要被锁定，这意味着应该在新节点被分配好之后⑧以及当前的尾结点分配数据前⑨，锁定互斥元，该锁定需要一直持有到函数结束

//try_pop()就没有那么简单了，首先，你需要锁定head上的互斥元，并持有它直到head使用完毕，本质上，正是这个互斥元你决定了哪个线程进行pop操作，一旦head改变⑤，你就可以解锁该互斥元，在返回结果的时候⑥，无需锁定它，剩下对tail的访问，需要锁定尾互斥元，因为只需要访问tail一次，所以可以只在进行读取的时候获取该互斥元，最好在将其封装在函数内部来实现，世界上，因为需要锁定head互斥元的代码只是该成员的子集，所以将其封装在函数里会更清晰，最终的代码如清单6.6所示

//清单6.6 使用细粒度锁的线程安全队列

template<typename T>
class threadsafe_fineness_queue
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
    
    node* get_tail()
    {
        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        return tail;
    }
    
    std::unique_ptr<node> pop_head()
    {
        std::lock_guard<std::mutex> head_lock(head_mutex);
        
        if (head.get() == get_tail())
            return nullptr;
        
        std::unique_ptr<node> old_head = std::move(head);
        head = std::move(old_head->next);
        return old_head;
    }
    
public:
    threadsafe_fineness_queue() : head(new node), tail(head.get()) {}
    threadsafe_fineness_queue(const threadsafe_fineness_queue& other) = delete;
    threadsafe_fineness_queue& operator=(const threadsafe_fineness_queue& other) = delete;
    
    std::shared_ptr<T> try_pop()
    {
        std::unique_ptr<node> old_head = pop_head();
        return old_head ? old_head->data : std::shared_ptr<T>();
    }
    
    void push(T new_value)
    {
        std::shared_ptr<T> new_data(std::make_shared<T>(std::move(new_value)));
        std::unique_ptr<node> p(new node);
        const node* new_tail = p.get();
        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        tail->data = new_data;
        tail->next = std::move(p);
        tail = new_tail;
    }
};

//让我们用批判的眼光来分析这段代码，思考6.1.1节中列出的准则，在寻找被破坏的不变量前，你应该确定它们到底是什么

//tail->next == nullptr
//tail->data == nullptr
//head == tail表明这是一个空链表
//只有一个元素的链表必须满足head->next == tail
//对于链表中的每一个结点x，当x != tail时，x->data指向T的一个实例，并且x->next指向链表中的下一个结点，x->next == tail表明x是链表中的最后一个节点
//从head开始，沿着next结点会最终迭代到tail

//就其本身而言，push()是直观的:对数据结构所做的唯一更改收到tail_mutex的保护，并且它们维持了不变量，因为新的尾结点是一个空结点，并且data和next已经正确设置给旧的尾结点了，而旧的尾结点成为了链表中最后一个真正的结点

//try_pop()这一部分很有趣，结果证明了不仅tail_mutex上的锁对于保护tail本身的读取时必要的，而且确保从头结点读取数据不会产生数据竞争也是很必要的，如果没有这个互斥元，就有可能出现一个线程调用try_pop()的同时，另一个线程调用push()，而且它们的操作并没有确定的顺序，尽管每个成员函数在互斥元上都持有一个锁，但它们锁定的是不同的互斥元，并且可能访问相同的数据，毕竟，队列中的所有数据都源于对push()的调用，因为线程都可能会访问同一个数据而没有确定的顺序，就有可能导致数据竞争和未定义的行为，正如你在第5章中看到的那样，辛亏调用get_tail()中对tail_mutex的锁定解决了一切问题，因为调用get_tail()与调用push()锁定了相同的互斥元，在这两次调用之间就有了确定的顺序，要么调用get_tail()发生在调用push()之前，这种情况下get_tail()看到的是tail的旧值，要么调用get_tail()发生在调用push()之后，这种情况下get_tail()看到的是tail的新值，并且在新的数据依附在了tail之前的值上

//在锁定head_mutex的情况下调用get_tail()也是很重要的，如果不这么做，对pop_head()的调用可能会被阻塞在调用get_tail()和锁定head_mutex之间，因为可能有别的线程调用try_pop()(接下来就是pop_head())，并且先获得了锁，从而阻止刚开始的线程继续执行下去

/*

 std::unique_ptr<node> pop_head()
 {
     node* const old_tail = get_tail();
     std::lock_guard<std::mutex> head_lock(head_mutex);
     
     if (head.get() == old_tail)
     return nullptr;
     
     std::unique_ptr<node> old_head = std::move(head);
     head = std::move(old_head->next);
     return old_head;
 }
 
*/

//在这种损坏的情况下，对get_tail()的调用是在锁的范围之外做出的，你可能会发现在你的初始线程能够获取head_mutex上的锁的时候，head和tail都已经发生了变化，并且不仅返回的尾结点不仅仅不再是尾部，甚至都不再是链表的一部分了，这有可能意味着即使head真的是最后一个节点，head和old_tail的比较也会失败，因此，当更新head的时候，你可能将head移动到tail之前，超多链表的结尾，破坏数据结构，在清单6.6的正确视线中，始终保持在head_mutex()上的锁的范围内调用get_tail()，这就确保没有别的线程可以改变head，并且tail只会移动得更远(随着调用push()加入新的结点)，因此是百分之百安全的，head永远都不会越过get_tail()返回的值，因而保持了不变量

//一旦pop_head()通过更新head，将结点从队列中删除，互斥元就解锁了，并且try_pop()就可以提取数据并在有节点的时候将其删除(如果没有，就返回一个std::shared_ptr<>的NULL实例)，按理说它就是能够访问该节点的唯一线程

//接下来，外部接口是清单6.2中的程序的一个子集，因此同样可以分析得到，接口中不存在固有的竞争条件

//异常就更有趣了，因为改变了数据分配模式，因此很多地方都可能产生异常，try_pop()的操作中只有锁定互斥元才能引发异常，并且直到它获取锁之后才会修改数据，因此try_pop()是异常安全的，另一方面，push()在堆上分配一个T的实例以及一个新的结点实例，而这两者都可能会引发异常，不管怎么样，这两个新分配的对象都复制给智能指针，因此当引发异常时他们就会被释放，一旦获取了锁，push()中的其他操作都不会引发异常，所以你再次稳操胜券，push()也是异常安全的

//因为没有改变接口，所以没有新的死锁的外部机会，同样也没有内部机会，只有在pop_head()中才会获取两次锁，它总司先获取head_mutex，然后再获取tail_mutex，所以也不会死锁

//剩下的问题就是关于并发的实际可能性，这种数据结构实际上具有比清单6.2中数据结构相当多的并发范围，因为这些锁是更细粒度的，并且在锁外部完成的更多，例如，push()中，分配新节点和新数据项的是无需持有锁的，这就意味着多个线程可以并发地分配新节点和新数据项而不会出现问题，每次只有一个线程可以在链表中插入新节点，并且此操作仅仅是通过一些简单的指针赋值来实现的，所以与所有内存分配操作都在std::queue<>内部的，基于std::queue<>的实现相比，它持有锁的时间根本就不算多

//同样的，try_pop()持有tail_mutex的时间也很短，以保护tail的读取，因此，几乎整个队try_pop()的调用可以与push()的调用同时发生，同样，当持有head_mutex的时候所执行的操作是很少的，昂贵的delete操作(在结点指针的析构函数中)在锁的外面，这就增加了同时发生的调用try_pop()的数量，一次只有一个节点可以调用pop_head()，但是多个线程可以删除就结点并且安全地返回数据

#pragma clang diagnostic pop
