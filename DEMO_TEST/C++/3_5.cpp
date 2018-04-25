//
//  3_5.cpp
//  123
//
//  Created by chenyanan on 2017/4/21.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <exception>
#include <memory>
#include <string>
#include <mutex>
#include <stack>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//5. 一个线程安全堆栈的示范意义

//清单3.4展示了在接口中没有竞争条件的栈的类定义，实现了选项1和3，pop()有来个两个重载，一个接受存储该值的位置的引用，另一个返回std::shared_ptr<>，它具有一个简单的接口，只有两个函数，push()和pop()

//通过削减接口，你考虑了最大的安全性，甚至对整个堆栈的操作都受到限制，栈本身不能被赋值，因为赋值运算符被删除①(参见附录A中A.2节)，而且也没有swap()函数，然而，它可以被复制，假设栈的元素可以被复制，如果栈是空的，pop()函数引发一个empty_stack异常，所以即使在调用empty()后栈被修改，一切仍然将正常工作，正如选项3的描述中提到的，如果需要，std::shared_ptr的使用允许栈来处理内存分配的问题，同时避免对new和delete的过多调用，五个堆栈操作现在变成三个，push(),pop(),empty()，甚至empty()都是多余的，接口的简化可以更好的控制数据，你可以确保互斥元为了操作的整体而被锁定，清单3.5展示了一个简单的实现，一个围绕std::stack<>的封装器

//清单3.5 一个线程安全的栈的详细类定义

struct empty_stack : std::exception
{
    const char* what() const throw();
};

template<typename T>
class threadsafe_stack
{
private:
    std::stack<T> data;
    mutable std::mutex m;
public:
    threadsafe_stack() {}
    threadsafe_stack(const threadsafe_stack& other)
    {
        std::lock_guard<std::mutex> lock(other.m);
        data = other.data;   //①在构造函数中执行复制
    }
    threadsafe_stack& operator=(const threadsafe_stack&) = delete;
    
    void push(T new_value)
    {
        std::lock_guard<std::mutex> lock(m);
        data.push(new_value);
    }
    
    std::shared_ptr<T> pop()
    {
        std::lock_guard<std::mutex> lock(m);
        if (data.empty())
            throw empty_stack();   //在试着出栈值的时候检查是否为空
        const std::shared_ptr<T> res(std::make_shared<T>(data.top()));   //在修改栈之前分配返回值
        data.pop();
        return res;
    }
    
    void pop(T& value)
    {
        std::lock_guard<std::mutex> lock(m);
        if (data.empty())
            throw empty_stack();
        value = data.top();
        data.pop();
    }
    
    bool empty() const
    {
        std::lock_guard<std::mutex> lock(m);
        return data.empty();
    }
};

//这个栈的实现实际上是可复制的，源对象中的拷贝构造函数锁定互斥元，然后复制内部栈，你在构造函数体中进行复制①而不是成员初始化列表，以确保互斥元被整个副本持有

//top()和pop()的讨论表明，接口中有问题的竞争条件基本上因为锁定的粒度过小而引起，保护没有覆盖期望操作的整体，互斥元的问题也可以由锁定的粒度过大因此，极端情况是单个的全局互斥元保护所有共享的数据，在一个有大量共享数据的系统中，这可能会消除并发的所有性能又是，因为线程被限制为每次只能运行一个，即便是在他们访问数据的不同部分的时候，被设计为处理多处理器系统的Linux内核的第一个版本，使用了单个全局内核锁，虽然这也能工作，但却意味着一个双处理器系统通常比两个单处理器系统的性能更差，四个处理器系统的性能远远没有四个单核处理器系统的性能好，有太多对内核的竞争，因此在更多处理器上运行的线程无法进行有效的工作，Linux内核的后续版本已经转移到一个更细粒度的锁定方案，因而四个处理器的系统性能更接近理想的单处理器系统的四倍，因为竞争少得多

//细粒度锁定方案的一个问题，就是有时为了保护操作中的所有数据，需要不止一个互斥元，如前所述，有时要做的正确的事情是增加被互斥元所覆盖的数据粒度，以使得只需要一个互斥元被锁定，然而，这有时是不可取的，例如互斥元保护着一个类的各个势力，在这种情况下，在下个级别进行锁定，将意味着要么将锁丢给用户，要么就让单个互斥元保护该类的所有实例，这些都不理想

//如果对于一个给定的操作你最终需要锁定两个或更多的互斥元，还有另一个潜在的问题潜伏在侧:死锁，这几乎是竞争条件的反面，两个线程不是在竞争称为第一，而是每一个都在等待另外一个，因而都不会有任何进展

#pragma clang diagnostic pop

