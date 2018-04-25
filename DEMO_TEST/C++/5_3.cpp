//
//  5_3.cpp
//  123
//
//  Created by chenyanan on 2017/5/4.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <iostream>
#include <thread>
#include <mutex>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//5.2.5 标准原子整型的操作

//除了一组通常的操作(load(),store(),exchange(),compare_exchange_weak()和compare_exchange_strong())之外，像std::atomic<int>或者std::atomic<unsigned long long>这样的原子整型还有相当广泛的一组操作可用:fetch_add(),fetch_sub(),fetch_and(),fetch_or(),fetch_xor()，这些运算符的符合形势(+=,-=,&=,|=,^=)，前缀/后缀自减(++x,x++,--x,x--)，这并不是很完整的一组可用在普通的整型上进行的符合赋值运算，但是一足够接近了，只有除法，乘法，位移运算符是缺失的，因为原子整型值通常作为计数器或者位掩码来使用，这不是一个特别明显的损失，如果需要的话，额外的操作可以通过在一个循环中使用compare_exchange_weak()来实现

//这些语义和std::atomic<T*>的fetch_add()和fetch_sub()的语义密切地匹配，命名函数原子级执行它们的操作，并返回旧值，而符合赋值运算符返回新值，前缀与后缀自增自减也跟平常一样工作，++x增加变量值并返回新值，而x++增加变量并返回旧值，正如你到目前所期待的那样，在这两中情况下，结果都是一个相关联的整型值

//现在，我们已经了解了所有的基本原子类型，剩下的就是泛型std::atomic<>初级模板而不是这些特化，那么让我们接着看下面的内容

//5.2.6 std::atomic<>初级类模板

//除了标准的原子类型，初级模板的存在允许用户创建一个用户定义的类型的原子变种，然而，你不能只是将用户定义类型用于std::atomic<>该类型必须满足一定的准则，为了对用户定义类型UDT使用std::atomic<UDT>这种类型必须有一个平凡的(trivial)拷贝赋值运算符，这意味着该类型不得拥有任何虚函数或虚基类，并且必须使用编译器生成的拷贝赋值运算符，不仅如此，一个用户定义类型拷贝赋值运算符，这实质上允许编译器将memcpy()或一个等价的操作用于赋值操作，因为没有用户编写的代码要运行

//最后，该类型必须是按位相等可比较的，这伴随着赋值要求，你不仅要能够使用memcpy()复制UDT类型的对象，而且还必须能够使用memcmp()比较实例是否相等，为了使比较/交换操作能够工作，这个保证是必须的

//这些限制的原因可以追溯到第3章中的一个准则，不要在锁定范围之外，向受保护的数据通过将其作为参数传递给用户所提供的函数的方式，传递指针和引用，一般情况下，编译器无法为std::atomic<UDT>生成无锁代码，所以它必须对所有的操作使用一个内部锁，如果用户提供的拷贝赋值或比较运算符是被允许的，这将需要传递一个受保护数据的引用作为一个用户提供的函数的参数，因而违背准则，同样地，类库完全自由地为所有需要单个锁定的原子操作使用单个锁定，并允许用户提供的函数被调用，虽然认为因为一个比较操作花了很长时间，该锁可能会导致死锁或造成其他线程阻塞，最后，这些限制增加了编译器能够直接为std::atomic<UDT>利用原子指令的机会(并因此是一个特定的实例无锁)，因为它可以把用户定义的类型作为一组原始字节来处理

//需要注意的是，虽然你可以使用std::atomic<float>或std::atomic<double>因为内置的浮点类型确实满足与memcpy和memcmp一同使用的准则，在compare_exchange_strong情况下这种行为可能会令人惊讶，如果存储的值具有不同的表示，即使旧的存储值与被比较的值得数值相等，该操作可能会失败，请注意，浮点值没有原子的算术操作，如果你使用一个具有定义了相等比较运算符的用户定义类型的std::atomic<>你会得到和compare_exchange_strong类似的情况，而且该操作符会与使用memcmp进行比较不同，该操作可能因另一相等值具有不同的表示而失败

//如果你的UDT和一个int或一个void*大小相同(或更小)，大部分常见的平台能够为std::atomic<UDT>使用原子指令，一些平台也能够使用大小是int或void*两倍的用户定义类型的原子指令，这些平台通常支持一个与compare_exchange_xxx函数相对应的所谓的双字比较和交换(double-word-compare-and-swap,DWCAS)指令，正如你将在第7章中看到的，这种支持可帮助写无锁代码

//这些意味着，例如，你不能创建一个std::atomic<std::vector<int>>但你可以把它与包含计数器、标识符、指针、甚至简单的数据元素的数组的类一起使用，这不是特别的问题，越是复杂的数据结构，你就越有可能像在它之上进行简单的赋值和比较之外的操作，如果情况是这样，你最好还是使用std::mutex来确保所需的操作得到适当的保护，正如在第3章中所描述的

//当使用一个用户定义的类型T进行实例化时，std::atomic<T>的接口被限制为一组操作，对于std::atomic<bool>:load(),store(),exchange(),compare_exchange_weak(),compare_exchange_strong()，以及赋值自和类型转换类型T的实例，是可用的，

//表5.3展示了在每个原子类型上的可用操作

//                              atomic_flag   atomic<bool>   atomic<T*>   atomic<integral type>   atomic<other type>
//test_and_st                        Y
//clear                              Y
//is_lock_free                                     Y             Y                 Y                       Y
//load                                             Y             Y                 Y                       Y
//store                                            Y             Y                 Y                       Y
//exchange                                         Y             Y                 Y                       Y
//compare_exchange_weak,                           Y             Y                 Y                       Y
//compare_exchange_strong                          Y             Y                 Y                       Y
//fetch_add,+=                                                   Y                 Y
//fetch_sub,-=                                                   Y                 Y
//fetch_or,|=                                                                      Y
//fetch_and,&=                                                                     Y
//fetch_xor,^=                                                                     Y
//++,--                                                          Y                 Y


//5.2.7 子操作的自由函数
//到现在为止，我一直限制自己去描述原子类型的操作的成员函数形式，然而，各种原子类型上的所有操作也都有等效的非成员函数，对于大部分非成员函数来说，都是以相应的成员函数来命名的，只是带有一个atomic_前缀(例如std::atomic_load())，这些桉树也为每个原子类型进行了重载，在有机会指定内存顺序标签的地方，它们有两个变种，一个没有标签，一个带有_explicit后缀和额外的参数作为内存顺序的标签(例如，std::atomic_store(&atomic_var, new_value)与std::atomic_store_explicit(&atomic_var, new_value, std::memory_order_release)相对)，被成员函数引用的原子对象是隐式的，然而所有的自由函数接受一个指向原子对象的指针作为第一个参数

//例如，std::atomic_is_lock_free()只有一个变种(尽管为每个类型重载)，而std::atomic_is_lock_free(&a)对于原子类型a的对象，与a.is_lock_free()返回相同的值，同样的，std::atomic_load(&a)和a.load()是一样的，但是a.load(std::memory_order_acquire)等同于std::atomic_load_explicit(&a,std::memory_order_acquire)

//自由函数被设计为可兼容C语言，所以它们在所有情况下使用指针而不是引用，例如，compare_exchange_weak()和compare_exchange_strong()成员函数(期望值)的第一参数是引用，而std::atomic_compare_exchange_weak()(第一个参数是对象指针)的第二个参数是指针，std::atomic_compare_exchange_weak_explicit()同时也需要指定成功与失败的内存顺序，而比较/交换成员函数具有一个单内存顺序形式(默认为std::memory_order_seq_cst)和一个分别接受成功与失败内存顺序的重载

//std::atomic_flag上的操作违反这一潮流，原因在于他们在名字中拼出"flag"的部分:std::atomic_flat_test_and_set()，std::atomic_flat_clear()尽管指定内存顺序的其他变种具有_explicit后缀，std::atomic_flat_test_and_set_explicit()和std::atomic_flag_clear_explicit()

//C++标准库还提供了为了以原子为访问std::shared_ptr<>实例的自由函数，这打破了只有原子类型支持原子操作的原则，因为std::shared_ptr<>很肯定地不是原子类型，然而，C++标准委员会认为提供这些额外的函数是足够重要的，可用的原子操作有:载入(load),存储(store),交换(exchange)和比较/交换(compare/exchange)，这些操作以标准原子类型上的相同操作的重载形式被提供，接受std::shared_ptr<>作为第一个参数

struct my_data {};
static void process_data(std::shared_ptr<my_data> theData) {}

std::shared_ptr<my_data> p;

static void process_global_data()
{
    std::shared_ptr<my_data> local = std::atomic_load(&p);
    process_data(local);
}

static void update_golbal_data()
{
    std::shared_ptr<my_data> local(new my_data);
    std::atomic_store(&p, local);
}

//至于其他类型上的原子操作，_explicit变量也被提供并允许你指定所需的内存顺序，std::atomic_is_lock_free()函数可以用于检查是否实现了使用锁以确保原子性

//正如在引言中所提到的，标注你的原子类型能做的不仅仅是避免与数据竞争香瓜你的未定义行为，它们允许用户在线程间强制操作顺序，这个强制的顺序是为保护数据和注入std::mutex和std::future<>这样同步操作的工具的基础，考虑到这一点，让我们进入到本章真正的重点，内存模型的并发方面的细节，以及原子操作如何用来同步数据和强制顺序

#pragma clang diagnostic pop
