//
//  6_7.cpp
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

//设计一个细粒度锁的MAP数据结构
//如同在6.2.3中提到的队列一样，为了允许细粒度锁，你需要仔细考虑数据结构的细节，而不是仅仅封装一个类似与std::map<>这样已存在容器，这里通常有三种常见的方法来实现一个类似于查找表的关联容器

//二叉树，例如红黑树
//已排序数组
//哈希表

//二叉树不能为扩大并发机会提供太多的空间，每次查找或修改必须从访问根节点开始，因此根节点必须被锁定，尽管当线程沿着树往下移动的时候回释放这个锁，但是这也不比锁定整个数据结构好多少

//已排序数组就更糟了，因为无法事先得知一个给定的数据在数组的哪个位置，所有就需要一次锁定整个数组

//只剩下哈希表了，假设有一定数量的捅，一个键属于哪个捅完全取决于这个键及其哈子函数的特性，这就意味着可以安全地在每个捅上有一个独立的锁，如果再次使用支持多重读或单一写的互斥元，你就将并发机会增加了N重，这里的N是捅的数量，其缺点是为键找一个好的哈希函数，C++标准库提供了std::hash<>模板，可以用来实现这一目的，它已经为int等基本类型以及std::string这样的常见库类型进行了特化，而且使用者也可以轻易地为别的键类型将其进行特化，如果效仿标准的无序容器，以及在进行哈希计算时将函数对象类型作为模板参数，那么使用者可以选择是否为键类型特化std::hash<>，或是提供一个单独的哈希函数

//那么，让我们来看清单6.11中的代码，一个线程安全查找表的实现将会是怎样的呢？这里列出了一种实现可能

//清单6.11 线程安全查找表

#include <list>
#include <map>

template<typename Key, typename Value, typename Hash = std::hash<Key>>
class threadsafe_lookup_table
{
private:
    class bucket_type
    {
    private:
        typedef std::pair<Key, Value> bucket_value;
        typedef std::list<bucket_type> bucket_data;
        typedef typename bucket_data::iterator bucket_iterator;
        
        bucket_data data;
        mutable boost::shared_mutex mutex;   //①
        
        bucket_iterator find_entry_for(const Key& key) const   //②
        {
            return std::find(data.begin(), data.end(), [&](const bucket_value& item) { return item.first == key; });
        }
    public:
        Value value_for(const Key& key, const Value& default_value) const
        {
            boost::shared_lock<boost::shared_mutex> lock(mutex);   //③
            const bucket_iterator found_entry = find_entry_for(key);
            return (found_entry == data.end()) ? default_value : found_entry->second;
        }
        
        void add_or_update_mapping(const Key& key, const Value& value)
        {
            std::unique_lock<boost::shared_mutex> lock(mutex);   //④
            const bucket_iterator found_entry = find_entry_for(key);
            if (found_entry == data.end())
            {
                data.push_back(bucket_value(key,value));
            }
            else
            {
                found_enrty->second = value;
            }
        }
        
        void remove_mapping(const Key& key)
        {
            std::unique_lock<boost::shared_mutex> lock(mutex);   //⑤
            const bucket_iterator found_entry = find_entry_for(key);
            if (found_entry != data.end())
            {
                data.erase(found_entry);
            }
        }
    };
    
    std::vector<std::unique_ptr<bucket_type>> buckets;   //⑥
    Hash hasher;
    
    bucket_type& get_bucket(const Key& key) const   //⑦
    {
        const std::size_t bucket_index = hasher(key) % buckets.size();
        return *buckets[bucket_index];
    }
    
public:
    typedef Key key_type;
    typedef Value mapped_type;
    typedef Hash hash_type;
    
    threadsafe_lookup_table(unsigned num_buckets = 19, const Hash& hasher_ = hash()) : buckets(num_buckets), hasher(hasher_)
    {
        for (unsigned i = 0; i < num_buckets; ++i)
        {
            buckets[i].reset(new bucket_type);
        }
    }
    
    threadsafe_lookup_table(const threadsafe_lookup_table& other) = delete;
    threadsafe_lookup_table& operator=(const threadsafe_lookup_table& other) = delete;
    
    Value value_for(const Key& key, const Key& default_value = Value()) const
    {
        return get_bucket(key).value_for(key, default_value);   //⑧
    }
    
    void add_or_update_mapping(const Key& key, const Value& value)
    {
        get_bucket(key).add_or_update_mapping(key,value);   //⑨
    }
    
    void remove_mapping(const Key& key)
    {
        get_bucket(key).remove_mapping(key);   //⑩
    }
};

//该实现使用std::vector<std::unique_ptr<bucket_type>>⑥来持有捅，允许在构造函数中指定捅的数量，默认值是19，这是一个任意选择的质数，哈希表与质数数量的捅合作得最好，每个捅都被一个boost::shared_mutex的实例保护①，使得每个捅都可以允许很多个并发读或者单个调用其中一个修改函数

//因为捅的数量是固定的，所以在调用get_bucket()函数⑦时无需锁定⑧，⑨，⑩，而且桶互斥元随后可以被锁定为共享(只读)所有权③，或是独占(读/写)所有权④，⑤，对每个函数都是适用的

//这三个函数都使用桶上的find_entry_for()成员函数②来确定在桶上是否有入口，每个桶包含一个键/值的std::list<>，因此添加和删除项目是很简单的

//我还关注了并发的角度，所有的东西都被互斥元锁合适地保护着，那么异常安全呢？value_for并不就该任何东西，所以没关系，如果它引发异常，也不会影响数据结构，remove_mapping调用erase的时候会修改列表，但是保证不会引发异常，因此是安全的，只有add_or_update_mapping在if的两个分支上可能会引发异常，push_back是异常安全的，当它引发异常时，会将列表留在原始状态，因此这个分支是没问题的，在这种情况下，唯一的问题就是当替换一个现存值时所做的赋值，如果此赋值引发异常，那么就得指望它不要修改原始值，然而，这在整体上并不影响数据结构，而且完全是用户提供类型的属性，因此可以安全地把它留给用户来处理

//在本节的开头，我提到这种查找表的一个不错的功能，就是可以获取到当前状态的快照的选项，比如std::map<>为了确保能够得到该状态的一致的副本，就需要锁定整个容器，也就是需要锁定所有的捅，因为查找表中"普通的"操作一次只需要锁定一个捅，这就会是唯一一个需要锁定所有捅的操作，因此，只要每次按照相同的顺序(例如，递增捅的索引)来锁定捅，就不会有死锁的机会，清单6.12给出了一个实现

//清单6.12 获取threadsafe_lookup_talbe的内容作为std::map<>
template<typename Key, typename Value, typename Hash = std::hash<Key>>
std::map<Key, Value> threadsafe_lookup_table::get_map() const
{
    std::vector<std::unique_lock<boost::shared_mutex>> locks;
    for (unsigned i = 0; i <buckets.size(); ++i)
        locks.push_back(std::unique_lock<boost::shared_mutex>(buckets[i].mutex));
    
    std::map<Key, Value> res;
    for (unsigned i = 0; i <buckets.size(); ++i)
        for (bucket_iterator it = buckets[i].data.begin(); it != buckets[i].data.end(); ++it)
            res.insert(*it);
    return res;
}

//清单6.11中查找实现通过单独锁定每个桶以及使用一个boost::shared_mutex实例来允许基于每个捅的并发读取，这就总体上增加了查找表的并发机会，但是如果要通过更细粒度的锁来增加并发的潜力呢?在下一节，将通过使用具有迭代器支持的线程安全链表容器来实现

#pragma clang diagnostic pop

