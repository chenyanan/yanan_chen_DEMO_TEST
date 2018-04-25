//
//  3_12.cpp
//  123
//
//  Created by chenyanan on 2017/4/24.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <iostream>
#include <thread>
#include <mutex>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//3.3.2 保护很少更新的数据结构
//假设有一个用于村粗DNS条目缓存的表，它用来将域名解析为相应的IP地址，通常，一个给定的DNS条目将在很长一段时间里保持不变，在许多情况下，DNS条目会保持数年不变，虽然随着用户访问不同的网站，新的条目可能会被不时地添加到表中，但这一数据却将在其整个生命中基本保持不变，定期检查缓存条目的有效性是很重要的，但是只有在细节已有时间改变的时候才会需要更新

//虽然更新是罕见的，但他们仍然会繁发生，并且如果这个缓存可以从很多个线程访问，他就需要在更新过程中进行适当的保护，以确保所有县城在读取缓存时都不会看到损坏的数据结构

//在缺乏完全预期用法并且为并发更新与读取专门设计(例如在第六章和第七章的那些)的专用数据结构的情况下，这种更新要求线程在进行更新时独占访问数据结构，直到它完成了操作，一旦更新完成，该数据结构对于多线程并发访问又是安全的了，使用std::mutex来保护数据结构就因而显得过于悲观，因为这会在数据结构没有进行修改时消除并发读取数据结构的可能，我们需要的是另一种互斥元，这种新的互斥元通常称为读写互斥元，因为它考虑到了两种不同的用法，由单个"写线程"独占访问或共享，由多个"读"线程并发访问

//新的C++标准库并没有直接提供这样的互斥元，尽管已向标准委员会提议，由于这个建议违背接纳，本节中的例子使用由Boost库提供的实现，它是基于这个建议的，在第8章中你会看到，使用这样的互斥元并不是万能药，性能依赖于处理器的数量以及读线程和更新线程的相对工作负载，因此，分析代码在目标系统上的性能是很重要的，以确保额外的复杂度会有实际的收益

//你可以使用boost::shared_mute的实例来实现同步，而不是使用std::mutex的实例，对于更新操作，std::lock_guard<boost::shared_mutex>和std::unique_lock<boost::shared_mutex>可用于锁定，以取代相应的std::mutex特化，这确保了独占访问，就像std::mutex那样，那些不需要更新数据结构的线程能够转而使用boost::shared_lock<boost::shared_mutex>来获得共享访问，这与std::unique_lock用起来是相同的，除了多个线程在同一时间，同一boost::share_mutex上可能会具有共享锁，唯一限制是，如果任意一个线程拥有一个共享锁，唯一的限制是，如果任意一个线程拥有一个共享锁，试图获取独占锁的线程会被阻塞，直到其他线程全都撤回它们的锁，同样地，如果任意一个县城具有独占锁，其他线程都不能获取共享锁或独占锁，直到第一个线程撤回了它的锁

//清单3.13展示了一个简单的如前面所描述的DNS缓存，使用std::map来保存缓存数据，用boost::share_mutex进行保护

#include <map>
#include <string>
#include <mutex>
#include <boost/thread/shared_mutex.hpp>

class dns_entry;

class dns_cache
{
    std::map<std::string, dns_entry> entries;
    mutable boost::shared_mutex entry_mutex;
public:
    dns_entry find_entry(const std::string& domain) const
    {
        boost::shared_lock<boost::shared_mutex> lk(entry_mutex);   //①
        const std::map<std::string,dns_entry>::const_iterator it = entries.find(domain);
        return (it == entries.end()) ? dns_entry() : it->second;
    }
    
    void update_or_add_entry(const std::string& domain, const dns_entry& dns_details)
    {
        std::lock_guard<boost::shared_mutex> lk(entry_mutex);   //②
        entries[domain] = dns_details;
    }
}

//在清单3.13中，find_entry()使用一个boost::shared_lock<>实例来保护它，以供共享，只读的访问①，多个线程因而可以毫无问题地同时调用find_entry()，另一方面，update_or_add_entry()使用一个std::lock_guard<>实例，在表被更新时提供独占访问②，不仅在调用update_or_add_entry()中其他线程被阻止进行更新，调用find_entry()的线程也会被阻塞

#pragma clang diagnostic pop
