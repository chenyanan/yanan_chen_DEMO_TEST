//
//  3_3.cpp
//  123
//
//  Created by chenyanan on 2017/4/21.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <exception>
#include <memory>

#include <stack>
#include <queue>
#include <string>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//3.2.3 发现接口内在的竞争条件

//仅仅因为使用了互斥元或其他几只来保护共享数据，未必会免于竞争条件，你仍然需要确定保护了适当的数据，再次考虑双向链表的例子，为了让线程安全地删除节点，你需要确保已组织对三个节点的并发访问，要删除的节点及其两边的节点，如果你分别保护访问每个节点的指针，就不会比未使用互斥元的代码更好，因为竞争条件仍会发生，需要保护的不是个别步骤中的个别节点，而是整个删除操作中的整个数据结构，这种情况下最简单的解决办法，就是用单个互斥元保护整个列表，如清单3.1中所示

//仅仅因为在列表上的个别操作是安全的，你还没有摆脱困境，你仍然会遇到竞争条件，即便是一个非常简单的接口，考虑像std::stack容器适配器这样的堆栈数据结构，如清单3.3中所示，除了构造函数和swap()，对std::stack你只有五件事情可以做，可以push()一个新元素入栈，pop()一个元素出栈，读top()元素，检查它是否empty()以及读取元素数量，堆栈的size()，如果更改top()使得它返回一个副本，而不是引用(这样你就遵照了3.2.2节的标准)，同时用互斥元保护内部数据，该接口依然固有地受制于竞争条件，这个问题对基于互斥元的实现并不是独一无二的，他是一个接口问题，因此对于无锁实现仍然会发生竞争条件

//清单3.3 std::stack容器的实现

template<typename T, typename Container = std::deque<T>>
class stack {
public:
    explicit stack(const Container&);
    explicit stack(Container&& = Container());
    template <class Alloc> explicit stack(const Alloc&);
    template <class Alloc> stack(const Container&, const Alloc&);
    template <class Alloc> stack(Container&&, const Alloc&);
    template <class Alloc> stack(stack&&, const Alloc&);
    
    bool empty() const;
    size_t size() const;
    T& top();
    const T& top() const;
    void push(const T&);
    void push(T&&);
    void pop();
    void swap(stack&&);
};

//这里的问题是empty()的结果和size()不可靠，虽然他们可能在被调用时是正确的，一旦他们返回，在调用了empty()或size()的线程可以使用该信息之前，其他线程可以自由地访问堆栈，并且可能push()新元素入栈或pop()已有的元素出栈

//特别的，如果该stack实例是非共享的，如果栈非空，检查empty()并调用top()访问顶部元素是安全的，如下所示

static void do_something(int value) {}

static void f()
{
    stack<int> s;
    if (!s.empty())   //①
    {
        const int value = s.top();   //②
        s.pop();   //③
        do_something(value);
    }
}

//它不仅在单线程代码中是安全的，预计为:在空堆栈上调用top()是未定义的行为，对于共享的stack对象，这个调用序列不再安全，因为在调用empty()①和top()②之间可能有来自另一个线程的pop()调用，删除最后一个元素，因此，这是一个典型的竞争条件，为了保护栈的内容而在内部使用互斥元，却并未能将其阻止，这就是接口的影响

//怎么解决呢？发生这个问题是接口设计的后果，所以解决办法就是改变接口，然而，这仍然回避了问题，要做出什么样的改变，在最简单的情况下，你只要声明top()在调用时如果栈中没有元素则引发异常，虽然这直接解决了问题，但它是编程变得更麻烦，因为现在你得能捕捉异常，即使对empty()的调用返回false，这基本上使得empty()的调用变得纯粹多余

//如果你仔细看看前面的代码片段，还有另一个可能的竞争条件，但这一次是在调用top()②和调用pop()③之间，考虑运行着前面代码片段的两个线程，他们都引用着同一个stack对象s,这并非罕见的情形，当为了性能而使用线程时，有数个线程在不同的数据上运行相同的代码是很常见的，并且一个共享的stack对象非常适合用来在它们之间分隔工作，假设一开始栈有两个元素，那么你不用担心在任一线程上的empty()和top()之间的竞争，只需要考虑可能的执行模式

/*
 
if(!s.empty())
                            if(!s.empty())
const int value = s.top()
                            const int value = s.top()
s.pop()
do_something(value)         s.pop()
                            do_something(value);
 */

//如你所见，如果这些是仅有的在运行的线程，在两次调用top()修改该栈之间没有任何东西，所以这两个线程将看到相同的值，不仅如此，在pop()的两次调用之间没有对top()调用，因此，栈上的两个值其中一个还没有读取就被丢弃了，而另一个被处理了两次，这是另一种竞争条件，远比empty()/top()竞争的未定义行为更为糟糕，从来没有任何明显的错误发生，同时错误造成的后果可能和诱因差距甚远，尽管他们明显取决于do_something()到底做什么

//这要求对接口进行更加激进的改变，在互斥元的保护下结合对top()和pop()两者的调用，Tom Cargill指出，如果栈上对象的拷贝构造函数能够引发异常，结合调用可能会导致问题，从Herb Sutter的异常安全的观点来看，这个问题被处理得较为全面，但潜在的竞争条件为这一结合带来了新的东西

//对于那些尚未意识到这个问题的人，考虑一下stack<vector<int>>，现在，vector是一个动态大小的容器，所以当你复制vector时，为了复制其内容，库就必须从堆中分配更多的内存，如果系统负载过重，或有明显的资源约束，此次内存分配就可能失败，于是vector的拷贝构造函数可能引发std::bad_alloc异常，如果vector中含有大量的元素的话则尤其可能，如果pop()函数被定义为返回出栈值，并且从栈中删除它，就会有潜在的问题，仅在栈被修改后，出栈值才返回给调用者，但复制数据以返回给调用者的过程可能会引发异常，如果发生这种情况，刚从栈中出栈的数据会丢失，它已经从栈中被删除了，但该复制却没有成功，std::stack接口的设计者笼统地将操作一分为二，获取顶部的元素top(),然后将其从栈中删除pop()，以至于你无法安全的复制数据，它将留在栈上，如果问题是堆内存不足，也许用用程序可以释放一些内存，然后再试一次

//不幸的是，这种划分正式你在消除竞争条件中试图去避免的！值得庆幸的是，还有替代方案，但他们并非无代价的

#pragma clang diagnostic pop


