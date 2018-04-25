//
//  7_3.cpp
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

//7.2.1 编写不用锁的线程安全栈

//栈的基本假设是相当简单的，按照添加结点的逆序来检索结点，后进先出(LIFO)，因此，确保一次只添加一个值到栈中是很重要的，另一个线程可以立刻检索结点，并且确保只有一个线程返回指针指向第一个结点(这个界结点被第一个检索)，并且每个节点都按顺序指向下一个结点

//在这种原则下，添加一个节点相对比较简单

//1. 创建一个新节点
//2. 将它的next指针指向当前的head结点
//3. 将head结点指向此新节点

//在单线程环境中，这种方法是可以的，但是如果别的线程也在修改栈，那么这种方法就不行了，最重要的是，如果两个线程同时添加结点，那么在第2步和第3步间就会存在竞争条件，当你的线程在第2步读取头结点和第3步更新头结点之间，第二个线程可能会修改head的值，这就会导致另一个线程所做的修改无效或者有更坏的影响，在我们考虑解决这一竞争条件之前，请注意一旦head被更新指向你新创建的结点，别的线程就可以读取该节点，因此，在head指向新节点之前将新节点完全准备好也是至关重要的，以后就无法修改此结点了

//那么，我们能够如何处理此竞争条件呢？答案就在第3步中使用一个原子比较/交换操作来保证从你在第2步中读取它开始，head就未被修改过，如果head被修改了，那么可以循环和再尝试，清单7.2给出了如何实现不使用锁的线程安全push()

//清单7.2 实现不使用锁的线程安全push()

template<typename T>
class lock_free_stack
{
private:
    struct node
    {
        T data;
        node* next;
        node(const T& data_) : data(data_) {}   //①
    };
    
    std::atomic<node*> head;
public:
    void push(const T& data)
    {
        const node* new_node = new node(data);   //②
        new_node-> next = head.load();   //③
        while (!head.compare_exchange_weak(new_node->next, new_node));   //④
    }
};

//这段代码恰好符合了上面提到的三点计划:创建一个新节点②，将新结点的next指针指向当前的head③，将head指向这个新节点④，通过在node构造函数①中填充node结构体的数据，这就可以保证node一被构造好就可以被使用，因此这个简单的问题就被解决了，然后使用compare_exchange_weak()来确保head指针的值与new_node->next③的值是一样的，如果这两个值是一样的，那么将head指向new_node，这段代码中使用了比较/交换函数的一部分，如果它返回false则表明此次比较没有成功(例如，因为另一个线程修改了head)，此时，第一个参数(new_node->next)的将被更新为head当前的值，因此，通过此次循环，就不需要你每次重载head，因为编译器已经做了此项操作，同样，因为失败时只需要直接循环，因此可以使用compare_exchange_weak，在某些构架下，它比compare_exchange_strong能够产生更优化的代码(如第5张所示)

//因此，在么没有pop()操作的情况下，先按照准则来快速检查一下push()，唯一能引发异常的地方就是构造新节点的时候①，但是它之后会清除这些，并且链表没有被修改，因此这是非常安全的，因为你建造的数据将被存储为node的一部分，并且可以使用compare_exchange_weak()来更新head指针，因此这里没有成问题的竞争条件，一旦比较/交换函数成功了，结点就被插入链表并可以被使用了，这里没有使用锁，因此不会产生死锁，并且push()函数出色地实现了功能

//当然，闲杂已经有了在栈中增加数据的方法，还需要在栈中移出数据的方法，从表面上看，这要简单一些

//1. 读取head当前的zhi
//2. 读取head->next
//3. 将head->next设置为head
//4. 返回检索到的节点的值
//5. 删除检索到的结点

//然而，在存在多个线程的情况下，这个问题就不这么简单了，如果同时有两个线程从栈中移出元素，他们可能在第1步中同时读取了相同的head值，如果一个线程在其他线程执行第2步前顺利执行到第5步，那么第二个线程将被解引用悬挂指针，这是写无锁代码中的最大的问题之一，因此从现在开始，先忽略第5步并且先不删除结点

//但是，这也没有解决掉所有的问题，这里存在着另一个问题，如果两个线程读取同一个head值，那么它们将会返回同一个结点，这就违背了栈数据结构的目的，因此必须避免发生这种情况，你可以用push()中使用的方法来解决竞争，使用比较/交换来更新head，如果比较/交换失败了，要么是因为在栈中插入了一个新节点，要么是因为另一个线程从栈中移出了你打算移出的结点，无论是哪一种情况，都需要返回到第1步(尽管比较/交换调用可以重新读取head)

//一旦比较/交换调用成功了，那么这就是唯一的从栈中移出指定结点的线程，因此可以安全的执行第4步，pop()如下所示

template<typename T>
class another_lock_free_stack
{
    struct node
    {
        T data;
        node* next;
        node(const T& data_) : data(data_) {}   //①
    };
    
    std::atomic<node*> head;
public:
    void pop(T& result)
    {
        node* old_head = head.lod();
        while (!head.compare_exchange_weak(old_head, old_head->next));
        result = old_head->data;
    }
};

//尽管这种方法很好很简明，但是除了未删除的结点外还有一些别的问题，首先，当链表为空时它就行不通了，如果head是空指针，那么当它试图读取next指针时就会导致未定义的行为，这也很容易通过在while循环中检查空指针来解决，可以同时在空栈上引发异常或者返回一个bool来表明成功或失败

//第二个问题就是异常安全问题，当我们在第3章中首次介绍线程安全栈时，你可以看出只通过返回值返回对象会留下一个异常安全问题，当复制返回值的时候，如果引发了异常，那么此值就会被丢失，在这种情况下，传递对值的引用是一种解决方案，因为如果抛出异常的话，这可以确保栈不会被修改，可惜，这里不能使用这种方法，一旦你知道这是唯一一个返回结点的线程，你就可以安全地复制数据了，这就意味着这个节点已经被移出队列了，因此，通过引用传递返回值的对象就不再是一个优势了，当然也可能只是返回值，如果想安全地返回值，就必须使用第3章中提到的另一个方法，返回一个指向数据值的(智能)指针

//如果返回只能指针，那么久可以只返回空指针来表明没有返回值，但是这就要求数据在堆上被分配，如果将堆分配作为pop()的一部分你，你仍然没有做得更好，因为堆分配也可能引发异常，反而，当吧数据push()进栈时，可以为此数据分配内存，反正都要为节点分配内存，返回std::shared_ptr<>不会引发异常，因此pop()是安全的，将这些总结起来得到清单7.3所示的代码

//清单7.3 缺少节点的无锁栈

template<typename T>
class lock_free_stack_two
{
private:
    struct node
    {
        std::shared_ptr<T> data;   //①data现在由指针持有
        node* next;
        
        node(const T& data_) : data(std::make_shared<T>(data_)) {}   //②为新分配的T创建std::shared_ptr
    };
    
    std::atomic<node*> head;
public:
   void push(const T& data)
    {
        const node* new_node = new node(data);
        new_node->next = head.load();
        while (!head.compare_exchange_weak(new_node->next, new_node));
    }
    
    std::shared_ptr<T> pop()
    {
        node* old_head = head.load();
        while (old_head && !head.compare_exchange_weak(old_head, old_head->next));   //③在解引用之前检查old_head不是一个空指针
        return old_head ? old_head->data : std::shared_ptr<T>();   //④
    }
};

//数据现在由指针持有①，因此需要在结点构造函数中在堆上分配数据②，在compare_exchange_weak()循环引用中解引用old_head前，必须检查空指针，最后，如果存在于结点相关的值，那么久必须返回该值，否则就返回一个空指针，注意，尽管这是无锁的，但是它不是无等待的，因为在pus()和pop()的while循环中，如果compare_exchange_weak()一直失败的话，理论上可以一直循环下去

//如果有垃圾回收器在你后面打点(比如在C#或Java这样的托管语言中),那么此工作就已经完成了，一旦没有线程存取此结点，那么旧的结点被收集并被再次利用，然而，没有多少C++编译器有垃圾回收器，因此通常需要自己整理

#pragma clang diagnostic pop

