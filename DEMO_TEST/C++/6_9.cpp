//
//  6_9.cpp
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

//6.4 小结

//本章开头考虑了为并发设计一个数据结构意味着什么，并且提供了一些准则来实现，然后我们完成一些通用的数据结构(栈，队列，哈希映射以及链表)，考虑了如何在设计并发存取的时候应用这些准则来实现他们，使用锁来保护数据结构并阻止数据竞争，现在你可以考虑设计你自己的数据结构，观察哪里有并发的机会以及哪里可能会存在竞争条件

//在第7章，我们将看一看完全避免锁的方法，使用底层原子操作来提供必要的顺序限制，并且遵循同样的准则

#pragma clang diagnostic pop
