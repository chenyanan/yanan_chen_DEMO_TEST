//
//  4_6.cpp
//  123
//
//  Created by chenyanan on 2017/4/28.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <iostream>
#include <thread>
#include <mutex>
#include <list>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//4.4 使用操作同步来简化代码
//使用截至目前在本章中描述的同步工具作为构建模块，允许你着重关注需要同步的操作而非机制，一种可以简化代码的方式，是采用一种更减函数式的(functional,在函数式编程意义上)的方法来编写并发程序，并非直接在线程之间共享数据，而是每个任务都可以提供它所需要的数据，并通过使用future将结果传播至需要它的线程

//4.4.1 带有future的函数式编程
//函数式编程指的是一种编程风格，函数调用的结果金单纯依赖于该函数的参数而不依赖与任何外部状态，这与函数的数学概念相关，同时也意味着如果yoga同一个参数执行一个函数两次，结果是完全一样的，这是许多C++标准中数学函数，如sin，cos，sqrt，以及基本类型简单操作如3+3，6*9，1.3/4.7的特性，春函数也不修改任何外部状态，函数的影响完全局限在返回值上

//这使得事情变得易与思考，尤其当涉及并发时，因为第3章中讨论的许多与共享内存相关的问题不复存在，如果没有修改共享数据，那么就不会有竞争条件，因此也就没有必要使用互斥元来保护共享数据，这是一个如此强大的简化，使得注入Haskell这样的编程语言，在默认情况下其所有函数都是纯函数，开始在编写并发系统中变得更为流行，因为大多数东西都是纯的，实际上的确修改共享状态的非纯函数就显得鹤立鸡群，因而也更易于理解它们是如何纳入应用程序整体结构的

//然而函数式编程的好处不仅仅局限在那些将其作为默认范型的语言，C++是一种多范型语言，它完全可以用FP风格编写程序，随着lambda函数(参见附录A中A.6节)的到来，从Boost到TR1的std::bind合并，和自动变量类型推断的引入(参见附录A中A.7节),C++11比C++98更为容易实现函数式编程，future是使得C++中FP风格的并发其实可行的最后一块拼图，一个future可以在线程间来回传递，使得一个线程的计算结果依赖于另一个的结果，而无需任何对共享数据的显示访问

//1. FP风格快速排序

//为了说明在FP风格并发中future的使用，让我们来看一个简单的快速排序算法的实现，算法的基本思想很简单，给定一列值，取一个元素作为中轴，然后将列表分为两组，比中轴小的为一组，大于等于中轴的为一组，列表的已排序副本，可以通过对这两组进行排序，并按照先是比中轴小的值已排序列表，接着是中轴，再后返回大于等于中轴的值已排序列表的顺序进行返回来获取，图4.2展示了10个整数的列表是如何根据此步骤进行排序的，FP风格的顺序实现在随后的代码中展示，它通过值得形式接受并返回列表，而不是像std::sort()那样就地排序

template <typename T>
std::list<T> sequential_quick_sort(std::list<T> input)
{
    if (input.emtpy())
        return input;
    
    std::list<T> result;
    result.splice(result.begin(), input, input.begin());   //①
    const T& pivot = *result.begin();   //②
    
    auto divide_point = std::partition(input.begin(), input.end(), [&](const T& t) { return t < pivot; });   //③
    
    std::list<T> lower_part;
    lower_part.splice(lower_part.end(), input, input.begin(), divide_point);   //④
    
    auto new_lower(sequential_quick_sort(std::move(lower_part)));   //⑤
    auto new_higher(sequential_quick_sort(std::move(input)));   //⑥
    
    result.splice(result.end(), new_higher);   //⑦
    result.splice(result.begin(), new_lower);   //⑧
    return result;
}

//尽管接口是FP风格的，如果你自始至终使用FP风格，就会制作很多副本，即你在内部使用了"标准"其实风格，取出第一个元素作为中轴，方法是用splice()①将其从列表前端切下，虽然这样可能导致一个次优排序(考虑到比较和交换的次数)，由于列表遍历的缘故，用std::list做任何事情都可能增加很多的时间，你已知要在结果中得到它，因此可以直接将其拼接至将要使用的列表中，现在，你还会想要将其作比较，因此让我们对它进行引用，以避免复制②，接着可以使用std::partition将序列划分成小于中轴的值和不小于中轴的值③，指定划分依据的最简单方式是使用一个lambda函数，使用引用补货以避免复制pivot的值(参见附录中A中A.5节更多地了解lambda函数)

//std::partition()就地重新排列列表，并返回一个迭代器，它标记着第一个不小于中轴值得元素，迭代器的完整类型可能相当冗长，因此可以使用auto类型说明符，使得编译器帮你解决(参见附录A中A.7节)

//现在，你已经选择了FP风格接口，因此如果打算使用递归来排序这两"半"，则需要创建两个列表，可以通过再次使用splice()将从input到divide_point的值移动至一个新的列表:lower_part④，这使得input中只仅留下剩余的值，你可以接着用递归调用对这两个列表进行排序⑤、⑥， 通过使用std::move()传入列表，也可以在此处避免复制，但是结果也将被隐式地移动出来，最后，你可以再次使用splice()将result以正确的顺序连接起来，new_higher值在中轴之后直到末尾⑦，而new_lower值从开始起，自导中轴之前⑧

//2. FP风格并行快速排序

//由于已经使用了函数式风格，通过future将其转换成并行版本就很容易了，如清单4.13所示，这组操作与之前相同，区别在于其中一部分现在并行地运行，此版本时候用future和函数式风格实现快速排序算法

//清单4.13 使用future的并行快速排序

#include <future>

template <typename T>
std::list<T> parallel_quick_sort(std::list<T> input)
{
    if (input.empty())
        return input;
    
    std::list<T> result;
    result.splice(result.begin(), input, input.begin());
    const T& pivot = *result.begin();
    
    auto divide_point = std::partition(input.begin(), input.end(), [&](const T& t) { return t < pivot; });
    
    std::list<T> lower_part;
    lower_part.splice(lower_part.end(), input, input, input.begin(), divide_point);
    
    std::future<std::list<T>> new_lower(std::async(&parallel_quick_sort<T>, std::move(lower_part)));   //①
    auto new_higher(paraller_quick_sort(std::move(input)));   //②
    
    result.splice(result.end(), new_higher);   //③
    result.splice(result.begin(), new_lower.get());   //④
    return result;
}

//这里最大的变化是，没有在当前线程中对较小的部分进行排序，而是在另一个线程中使用std::async()对其进行排序①，列表的上部分跟之前一样直接递归②进行排序，通过递归调用paraller_quick_sort()，你可以充分利用现有的硬件并发能力，如果std::async()每次启动一个新的线程，那么如果你想下递归三次，就会有8个线程在运行，如果想下递归10次(对大约1000个元素)，如果硬件可以处理的话，就将有1024个线程在运行，如果类库认定产生了过多的任务(也许是因为任务的数量超过了可用的硬件并发能力)，则可能改为同步地产生新任务，他们会在调用get()的线程中运行，而不是在新的线程中，从而避免在不利于性能的情况下把任务传递到另一个线程的开销，值得注意的是，对于std::async的实现来说，只有显示制定了std::launch::deferred为每一个任务开启一个新线程(甚至在面对大量的过度订阅时)，或是只有显示指定了std::launch::async再同步运行所有任务，才是完全符合的，如果依靠类库惊醒自动判定，建议你查阅一下这一实现的文档，看一看他究竟表现出什么样的行为

//与其使用std::asyne()不如自行编写spawn_task()函数作为std::packaged_task()函数作为std::packaged_task和std::thread的简单封装，如清单4.14所示，为函数调用结果创建了一个std::packaged_task从中获取future，在线程中运行至并返回future，其本身并不会带来多少优点(实际上可能导致大量的过度订阅)，但它为迁移到一个更复杂的实现做好准备，它通过一个工作线程池，将任务添加到一个即将运行的队列里，我们会在第9章研究线程池，相比于使用std::async之后在你确实知道将要做什么，并且希望想要通过线程池建立的方式进行完全掌控和执行任务的时候，才值得首选这种方法

//总之，回到parallel_quick_sort，因为只是使用直接递归获取new_higher，你可以将其拼接到之前的位置③，但是现在now_lower是std::future<std::list<T>>而不仅是列表，因此需要在调用splice()之前调用get()来获取值④，这就会等待后台任务完成，并将结果移动至splice()调用，get()返回一个引用了所包含结果的右值，所以它可以被移除(参见附录A中A.1.1节更多地了解右值引用和移动语义)

//即使假设std::async()对可用的硬件并发能力进行最优化的使用，者仍不是快速排序的理想并行实现，原因之一是，std::partition完成了很多工作，且仍为一个连续调用，但对于目前应足够好了，如果你对最快可能的并行实现由兴趣，请查阅学术文献，

//清单4.14 一个简单spawn_task的实现

template <typename F, typename A>
std::future<typename std::result_of<F(A&&)>::type> spawn_task(F&& f, A&& a)
{
    typedef typename std::result_of<F(A&&)>::type result_type;
    std::packaged_task<result_type(A&&)> task(std::move(f));
    std::future<result_type> res(task.get_future());
    std::thread t(std::move(task), std::move(a));
    t.detach();
    return res;
}

//函数式编程便不是唯一的避开共享可变数据的并发编程范式，另一种范式为CSP(Communicating Sequential Process, 通讯顺序处理)，这种范式下线程在概念航完全独立，没有共享数据，但是具有允许消息在他们之间进行传递的通信通道，这是被编程语言Erlang所采用的范式，也通常被MPI(Message Passing Interface，消息传递接口)环境用于C和C++中的高性能计算，我可以肯定到目前为止，你不会对这在C++中可也可以在一些准则下得到支持而感到意外，接下来的一届将讨论实现这一点的一种方式

#pragma clang diagnostic pop
