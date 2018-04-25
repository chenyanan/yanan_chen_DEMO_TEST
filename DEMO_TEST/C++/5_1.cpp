//
//  5_1.cpp
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

//5.2 C++中的原子操作及类型

//原子操作是一个不可分割的操作，从系统中的任何一个线程中，你都无法观察到一个完成到一半的这种操作，他要么做完了，要么就没做完，如果读取对象值得载入操作是原子的，并且所有对该对象的修改也都是原子的，那么这个载入操作所获取到的要么是对象的初始值，要么是被某个修改者存储后的值

//其对里面，是一个废园子操作可能被另一个线程视为半完成，如果这是一次存储操作，那么被另一个线程观察到的值可能既不是存储前的值也不是已存储的值，而是其他的东西，如果执行非原子的载入操作，那么它可能获取对象的一部分，由另一个线程修改了值，然后再获取其余的对象，这样获取到的既不是第一个值也不是第二个值，而是两者的某种组合，这就是一个简单的有问题的竞争条件，正如第3张所述的那样，但是在这个级别，它可能构成一个数据竞争(参见5.1节)并因而导致未定义行为

//在C++中，大多数情况下需要通过原子类型来得到原子操作，让我们看一看

//5.2.1 标准原子类型

//标准原子类型可以在<atomic>头文件中找到，在这种类型上的所有操作都是原子的，在语言定义的意义上说，只有在这些类型上的操作是原子的，虽然你可以利用互斥元使得其他操作看起来像原子的，事实上，标准原子类型本身就可以进行这样的模拟，它们(几乎)都具有一个is_lock_free()成员函数，让用户决定在给定类型上的操作是否直接用原子指令完成(x.is_lock_free()返回true)，或者通过使用编译器和类库内部的锁来完成(x.is_lock_free()返回false)

//唯一不提供is_lock_free()成员函数的类型是std::atomic_flag该类型是一个非常简单的布尔标识，并且在这个类型上的操作要求是无锁的，一旦你有了一个简单的无锁布尔标志，就可以用它来实现一个简单的锁，从而以此为基础实现所有其他的原子类型，当我说非常简单时，指的是:类型为std::atomic_flag的对象被初始化为清除，接下来，他们可以被查询和设置(通过test_and_set()成员函数)或者清除(通过clear成员函数)，就是这样:没有赋值，没有拷贝构造函数，没有测试和清除，也诶有任何其他的操作

//其余的原子类型全都是通过std::atomic<>类模板的特化来访问的，并且有一点更加的全功能，但可能不是无锁的(如前所述)，在大多数流行的平台上，我们认为内置类型的原子变种(例如std::atomic<int>和std::atomic<void*>)确实是无锁的，但却并非是要求的，你马上会看到，每个特化的接口反映了类型的属性，例如，像&=这样的未操作没有为普通指针做定义，因此他们也没有为原子指针做定义

//除了可以直接使用std::atomic<>，你还可以使用表5.1所示的一组名称，来体积已经提供实现的原子类型，鉴于原子类型如何被添加到C++标准的历史，这些替代类型名称可能指的是相应的std::atomic<>特化，也可能是该特化的基类，在同一个程序中混用这些替代名称和std::atomic<>特化的直接命名，可能会因此导致不可移植的代码

//表5.1 标准原子类型的替代名称和它们所对应的std::atomic<>特化

//(原子类型 对应的特化)
//atomic_bool std::atomic<bool>
//atomic_char std::atomic<char>
//atomic_schar std::atomic<signed char>
//atomic_uchar std::atomic<unsigned char>
//atomic_int std::atomic<int>
//atomic_uint std::atomic<unsigned>
//atomic_short std::atomic<short>
//atomic_ushort std::atomic<unsigned short>
//atomic_long std::atomic<long>
//atomic_ulong std::atomic<unsigned long>
//atomic_llong std::atomic<long long>
//atomic_ullong std::atomic<unsinged long long>
//atomic_char16_t std::atomic<char16_t>
//atomic_char32_t std::atomic<char32_t>
//atomic_wchar_t std::atomic<wchar_t>

//同基本原子类型一样，C++标准库也为原子类型提供了一组typedef，对应于各种像std::size_t这样的废园子的标注库typedef，如表5.2所示

//表5.2 标准库原子类型定义以及其对应的内置类型定义

//(原子typedef 对应的标准库typedef)
//atomic_int_least8_t int_least8_t
//atomic_uint_least8_t uing_least8_t
//atomic_int_least16_t int_least16_t
//atomic_uint_least16_t uint_least16_t
//atomic_int_least32_t int_least32_t
//atomic_uint_least32_t uint_least32_t
//atomic_int_least64_t int_least64_t
//atomic_uint_least64_t uint_least64_t
//atomic_int_fast8_t int_fast8_t
//atomic_uint_fast8_t uint_fast8_t
//atomic_int_fast16_t int_fast16_t
//atomic_uint_fast16_t uint_fast16_t
//atomic_int_fast32_t int_fast32_t
//atomic_uint_fast32_t uint_fast32_t
//atomic_int_fast64_t int_fast64_t
//atomic_uint_fast64_t uint_fast64_t
//atomic_intptr_t intptr_t
//atomic_unitptr_t uintptr_t
//atomic_size_t size_t
//atomic_ptrdiff_t ptrdiff_t
//atomic_intmax_t intmax_t
//atomic_uintmax_t uintmax_t

//好多的类型啊，有一个很简单的模式，对于标注你的typedef T，其对应的原子类型与之同名并带有atomic_前缀:atomic_T，这同样适用于内置类型，却别是signed缩写为s,unsgined缩写为U，long long缩写为llong，一般来说，无论你要使用的是什么T，都只是简单地说std::atomic<T>而不是使用替代名称

//传统意义上，标准原子类型是不可复制且不可赋值的，因为它们没有拷贝构造函数和拷贝赋值运算符，但是它们确实支持从相应的内置类型的赋值进行隐式转换并赋值，与直接load()和store()成员函数,exchange(),compare_exchange_weak()以及compage_exchange_strong()一样，它们在适合的地方还支持符合复制运算符:+=,-=,*=,|=，同时整形和针对指针的std::atomic<>特化还支持++和--，这些运算符也拥有相应命名的具有相同功能的成员函数:fetch_add(),fetch_or()等，赋值运算符和成员函数的返回值既可以是存储的值(在赋值运算符的情况下)或运算之前的值(在明明函数的情况下)，这避免了可能出现的问题，这些问题源于这种赋值运算符通常会放回一个将要赋值的对象的引用，为了从这种医用中获取存储的值，代码就得指向独立的读操作，这就允许另一个线程在赋值运算和读操作之间修改其值，并未竞争条件敞开大门

//然而，std::atomic<>类模板并不仅仅是一组特化，它具有一个主模板，可以用于创建一个用户定义类型的原子变种，由于它是一个泛型类模板，操作只限为load(),store()(和与用户定义类型间的相互赋值),exchange(),compare_exchange_weak()和compare_exchange_strong()

//在原子类型上的每一个操作均具有一个可选的内存顺序参数，它可以用来指定所需的内存顺序语义，内存顺序选项的确切语义参见5.3节，就目前而言，了解运算分为三种类型就足够了

//存储(store)操作，可以包括memory_order_releaxed, memory_order_release, memoty_order_seq_cst顺序
//载入(load)操作，可以包括memory_order_relaxed, mem_ory_order_consue, memory_order_acquire, memory_order_seq_cst顺序
//读-修改-写操作，可以包括memory_order_releaxed, memory_order_consume, memory_order_qcquire, memory_order_release, memory_order_acq_rel或memory_order_seq_cst顺序
//所有操作的默认顺序为memory_order_seq_cst
//现在让我们来看看能够在标准原子类型上进行的操作，从std::atomic_flat开始

#pragma clang diagnostic pop
