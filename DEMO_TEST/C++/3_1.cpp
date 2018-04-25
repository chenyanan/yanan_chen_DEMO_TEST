//
//  3_1.cpp
//  123
//
//  Created by chenyanan on 2017/4/21.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <list>
#include <mutex>
#include <thread>
#include <algorithm>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//清单3.1使用互斥量保护列表

std::list<int> some_list;   //①
std::mutex some_mutex;   //②

static void add_to_list(int new_value)
{
    std::lock_guard<std::mutex> guard(some_mutex);   //③
    some_list.push_back(new_value);
}

static bool list_contains(int value_to_find)
{
    std::lock_guard<std::mutex> guard(some_mutex);   //④
    return std::find(some_list.begin(), some_list.end(), value_to_find) != some_list.end();
}

//清单3.1中有一个全局变量①，这个全局变量被一个全局的互斥量保护②，add_to_list()③和list_contains()④函数中使用std::lock_guard<std::mutex>，使得这两个函数中对数据的访问是互斥的:list_contains()不可能看到正在被add_to_list()修改的列表

//虽然某些情况下，使用全局变量没问题，但在大多数情况下，互斥量通常会与保护的数据放在同一个类中，而不是定义成全局变量，这是面向对象设计的准则，将其放在一个类中，就可让他们联系在一起，也可以对类的功能进行封装，并进行数据保护，在这种情况下，函数add_to_list和list_contains可以作为这个类的成员函数，互斥量和要保护的数据，在类中都需要定义为private成员，这会让访问数据的代码变的清晰，并且容易看出在什么时候对互斥量上锁，当所有成员函数都会在调用时对数据上锁，结束时对数据解锁，那么就保证了数据访问时不变量不会被破坏

//当然，也不总是那么理想，聪明的你一定注意到了，当其中一个成员函数返回的是保护数据的指针或引用，会破坏对数据的保护，具有访问能力的指针或引用可以访问(并可能修改)被保护的数据，而不会被互斥锁限制，互斥量保护的数据需要对接口的设计相当严谨，要确保互斥量能锁住任何对保护数据的访问，并且不留后门

#pragma clang diagnostic pop
