//
//  7_5.cpp
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

//7.2.3 用风险指针检测不能被回收的结点

//术语风险指针(hazardpointers)是Maged Michael提出的一种技术，基本思想就是如果一个线程准备访问别的线程准备删除的对象，那么它会用风险指针来引用对象，因此就可以通知别的线程删除此对象可能是有风险的，如果别的线程医用此结点，并且准备通过此引用来访问结点，那么删除一个可能仍然被别的线程引用的结点是由危险的，因此它们被称为风险指针，一旦不再需要此对象，风险指针就会被清楚了，如果你看过牛静/剑桥划船比赛，就可以发现当比赛开始时使用了一个相似的方法:每艘船的舵手都可以举手示意他们没有准备好，当任何一个舵手举手的时候，裁判都不能开始比赛，如果舵手都没有举手，那么就可以看是比赛了，但是只要比赛尚未开始，任何一个舵手都可以举手，此时情况就会发生改变

//当此线程试图删除一个对象的时，它必须首先检查别的线程所持有的风险指针，如果没有风险指针引用此对象，那么久可以删除此对象，否则，它必须之后才能被处理，周期性地检查对象列表来确定现在是否可以删除它

//首先，需要一块共享内存来存储正在访问对象的指针，即风险指针本身，此地址必须对所有所有线程可见，并且每个访问次数据结构的线程都需要其中一段内存，如何正确并且有效地分配它们，我们会在后面章节介绍，先假设已经有这样一个函数get_hazard_pointer_for_current_thread()，它返回风险指针的引用，当线程试图解引用正在读取的指针的时候，需要设置它的风险指针，在这里我们以解引用列表的head值为例

template<typename T>
class another_lock_free_stack
{
    struct node
    {
        T data;
        node* next;
        node(const T& data_) : data(data_) {}
    };
    
    std::atomic<node*> head;
public:
    std::shared_ptr<T> pop()
    {
        std::atomic<void*>& hp = get_hazard_pointer_for_current_thread();
        node* old_head = head.load();   //①
        node* temp;
        do
        {
            temp = old_head;
            hp.store(old_head);   //②
            old_head = head.load();
        } while(old_head != temp);   //③
    }
};

//在while循环中，确保在读取旧的head指针①和设置风险指针②之间，结点未被删除，在此窗口内，别的线程不知道你正在读取这个特定的结点，幸运的是，如果旧的头指针将被删除，那么头结点肯定会发生改变，因此必须持续循环直到确定头指针与之前设置的风险指针相同③，使用风险指针取决于当它引用的对象被删除后，仍然可以安全地使用此指针，如果使用缺省的new和delete实现，那么就会导致未定义的行为，因此，需要确保你的实现可以保证这一点，或者使用允许这种行为的自定义分配器

//现在，我们已设置好风险指针，就可以完成pop()中剩余的代码，此时没有线程将会删除你所占用的结点了，那么每次重载old_head，都需要在解引用新读取的指针前更新风险指针，一旦从列表中获得了结点，就可以清楚风险指针，如果此时没有别的风险这孩子很引用此节点，就可以安全删除此结点，否则，就将此结点加入等待撒后删除的结点列表，清单7.6演示了使用此策略的pop()的完成实现

//清单7.6 使用风险指针的pop()实现

template<typename T>
class another_lock_free_stack_one
{
    struct node
    {
        T data;
        node* next;
        node(const T& data_) : data(data_) {}
    };
    
    std::atomic<node*> head;
public:
    std::shared_ptr<T> pop()
    {
        std::atomic<void*>& hp = get_hazard_pointer_for_current_thread();
        node* old_head = head.load();
        do
        {
            node* temp;
            do   //①一直循环到你将风险指针设置到head上
            {
                temp = old_head;
                hp.store(old_head);
                old_head = head.load();
            } while(old_head != temp);
        }while(old_head && !head.compare_exchange_strong(old_head,old_head->next));
        hp.store(nullptr);   //②当你完成时清除风险指针
        std::shared_ptr<T> res;
        if (old_head)
        {
            res.swap(old_head->data);
            if (outstanding_hazard_pointers_for(old_head))   //③在你删除一个节点前检查风险指针是否引用它
            {
                reclaim_later(old_head);   //④
            }
            else
            {
                delete old_head;   //⑤
            }
            delete_nodes_with_no_hazards();   //⑥
        }
        return res;
    }
};

//首先，将设置风险指针放到外部循环中，如果比较/交换失败，则重在old_head①，这里使用compare_exchange_strong()是因为在这个while循环中确实有效，compare_exchange_weak()中虚假的错误会导致不必要地重置风险指针，这就确保了在解引用old_head前设置了正确的风险指针，一旦声明此结点是你的，就可以清楚你的风险指针②，如果你得到一个节点，就需要检查别的线程拥有的风险指针是否引用它③，如果存在这样的风险指针，那么必须将它放入到稍后回收的列表中④，否则，就可以立刻删除它⑤，最后，调用reclaim_late()来检查所有节点，如果没有别的风险指针引用这些结点，那么可以安全删除它们⑥，任何有风险指针的结点豆浆留待下一个线程调用pop()

//当然，在这里有很多新函数，get_hazard_pointer_for_current_thread()，reclaim_later()，outstanding_hazard_pointers_for()，以及delete_nodes_with_no_hazards()中有很多细节部分，让我们来了解这些函数并且看看它们是如何工作的

//调用get_hazard_pointer_for_current_thread()给线程分配风险指针的具体机制并不影响程序的逻辑(稍后就可以看到对效率有影响)，因此我们先用一个简单的结构来实现，一个固定大小数据存放线程ID和指针对，get_hazard_pointer_for_current_thread()检索整个数组来寻找第一个空闲的位置，并且将此位置的ID值设为当前线程的ID，当线程退出时，此位置ID值被重置为默认值std::thread::id()从而此位置就被释放出来了，如清单7.7所示

//清单7.7 get_hazard_pointer_for_current_thread()的简单实现

const unsigned max_hazard_pointers = 100;

struct hazard_pointer
{
    std::atomic<std::thread::id> id;
    std::atomic<void*> pointer;
};
hazard_pointer hazard_pointers[max_hazard_pointers];

class hp_owner
{
    hazard_pointer* hp;
public:
    hp_owner(const hp_owner&) = delete;
    hp_owner operator=(const hp_owner&) = delete;
    hp_owner() : hp(nullptr)
    {
        for (unsigned i = 0; i < max_hazard_pointers; ++i)
        {
            std::thread::id old_id;
            if (hazard_pointers[i].id.compare_exchange_strong(old_id, std::this_thread::get_id()))
            {
                hp = &hazard_pointers[i];
                break;
            }
        }
        if (!hp)   //①
        {
            throw std::runtime_error("No hazard pointers available");
        }
    }
    
    std::atomic<void*>& get_pointer()
    {
        return hp->pointer;
    }
    
    ~hp_owner()   //②
    {
        hp->pointer.store(nullptr);
        hp->id.store(std::thread::id());
    }
};

std::atomic<void*>& get_hazard_pointer_for_current_thread()   //③
{
    static thread_local hp_owner hazard;   //④每个线程有自己的风险指针
    return hazard.get_pointer();   //⑤
}

//get_hazard_pointer_for_current_thread()的实现看似简单，其实不然③，它用hp_owner④类型的thread_local变量来存储当前线程的风险指针，然后它返回此对象的指针⑤，它的原理如下:每个贤臣第一次调用此函数的时候，创建一个新的hp_owner实例，这个心实例构造器搜索所有者/指针对的表格来寻找下一个值，此值没有所有者，它使用compare_exchange_strong()来检查没有所有者的值并且获得它②，如果compare_exchange_strong()失败了，那么就说明另一个线程拥有此值，那么就需要去检查下一个值，如果compare_exchange_strong()成功了，那么当前线程在整个列表中都没有一个空闲的值④，那么就表示有太多线程使用了风险指针，此时就抛出异常

//一旦为一个给定的线程创造了hp_owner实例，那么之后的存取就变得更快了，因为缓存了指针，就不需要再次扫描表格了

//当每个线程退出时，为该线程创造的hp_owner实例就被销毁了，析构函数在设置所有者的ID的值为std::thread::id()前将指针的值重置为nullptr，这样稍后别的线程就可以重新使用此值⑤

//用这种方式实现get_hazard_pointer_for_current_thread()，那么outstanding_hazard_pointer_for_current_thread()的实现就变得简单了，只需要检索整个风险指针来寻找这个位置

bool outstanding_hazard_pointers_for(void* p)
{
    for(unsigned i = 0; i < max_hazard_pointers; ++i)
        if (hazard_pointers[i].pointer.load() == p)
            return true;
    return false;
}

//现在甚至都不需要检索表来得知每个位置是否拥有所有者，没有所有者的位置将会有一个空指针，因此这个比较函数将返回false，这样就简化了代码

//在简单链表中，reclaim_later()和delete_nodes_with_no_hazards()可以工作，reclaim_later()只添加结点至列表中，delete_nodes_with_no_hazards()扫描整个列表，删除没有风险的值，清单7.8就是一个实现

//清单7.8 回收函数的简单实现

template<typename T>
void do_delete(void *p)
{
    delete static_cast<T*>(p);
}

struct data_to_reclaim
{
    void* data;
    std::function<void(void*)> deleter;
    data_to_reclaim* next;
    
    template<typename T>
    data_to_reclaim(T* p) : data(p), deleter(&do_delete<T>), next(0) {}   //①
    ~data_to_reclaim()
    {
        deleter(data);   //②
    }
};

std::atomic<data_to_reclaim*> nodes_to_reclaim;

void add_to_reclaim_list(data_to_reclaim* node)   //③
{
    node->next = nodes_to_reclaim.load();
    while(!nodes_to_reclaim.compare_exchange_weak(node->next, node));
}

template<typename T>
void reclaim_later(T* data)   //④
{
    add_to_reclaim_list(new data_to_reclaim(data));   //⑤
}

void delete_nodes_with_no_hazards()
{
    data_to_reclaim* current = nodes_to_reclaim.exchange(nullptr);   //⑥
    while (current)
    {
        data_to_reclaim * const next = current->next;
        if (!outstanding_hazard_pointers_for(current->data))
        {
            delete current;   //⑧
        }
        else
        {
            add_to_reclaim_list(current);   //⑨
        }
        current = next;
    }
}

//首先，我希望你发现reclaim_later()不是一个普通的函数，而是一个函数模板④，这是因为风险指针是一种通用的工具，因此不希望绑定到具体的结点，你已经使用std::atomic<void>来存储指针了，因此需要处理任何指针类型，但是不能使用void类型，因为当你删除数据项的时候，delete函数需要指针的实际类型，data_to_reclaim的构造器可以很好地处理这个问题，就如以下所示，reclaim_later()只需要为你的指针生成一个新的date_to_reclaim实例，并且将它加入到回收列表中⑤，add_to_reclaim_list()本身③是一个基于列表头结点的简单compare_exchange_weak()循环

//因此，回到data_to_reclaim的构造函数①，这个构造函数也是一个模板，它将被删除数据存储为data成员的void类型，然后存储指向do_delete()实例的指针，do_delete()是一个简单的函数，将提供void类型确定为选好的指针类型，然后删除它所指向的对象，std::function<>可以安全地实现这个函数指针，因此data_to_reclaim的析构函数可以调用存储的函数来删除数据②

//当你在列表中增加结点时，不会调用data_to_reclaim的析构器，当没有风险指针指向此结点时就会调用此析构函数，这是delete_nodes_with_no_hazards()的责任

//delete_nodes_with_no_hazards()首先用一个简单的exchange()来声明所有将被回收的结点列表⑥，这一简单但是关键的步骤确保了这是讲回收这个节点集合的唯一线程，别的线程可以自由向列表中增加结点或者试图回收它们，并且不会影响此线程的操作

//然后，只要列表中人仍然有节点，就轮流检查每个节点来看是否存在风险指针⑦，如果没有，则安全删除此位置的值(即清除了存储的数据)⑧，否则，就将此项增加到稍后回收列表中⑨

//尽管这种简单实现能够安全回收删除的结点，但是它会增加很多处理难度，扫描风险指针数组需要检查max_hazard_pointers原子变量，并且每次调用pop()的时候都会执行这个操作，原子操作必定是很慢的，通常在计算机CPU上运行时会比实现同样效果的非原子操作慢100倍，这就使得pop()变成很耗资源的操作，不仅需要扫描将要删除结点的风险指针列表，而且需要扫描等待列表中每个节点的风险指针列表，这当然不是个好主意，如果列表中有mac_hazard_pointers个结点，那么就得扫描这些结点存储的风险指针，天啊，必须找到一种更好的方法

//使用风险指针的更佳的回收策略

//当然，有更好的方法，这里我将介绍一种简单的风险指针实现来解释这种机制，第一件事就是用内存资源换取效率，你不在试图回收任何结点除非表中的结点数多于max_hazard_pointers，而不再每次都调用pop()来检查回收列表中每个结点，用这种方式可以保证至少回收一个节点，这种方法也没有更好，一旦你得到max_hazard_pointers个结点，就开始调用pop()来回收结点，这种方法也没有更好，但是如果你等到表中有2 * max_hazard_pointers个结点，就可以确保回收至少max_hazard_pointers次调用pop()，这种方法就比较好了，你在max_hazard_pinters次调用pop()的时候都会检查2 * max_hazard_pointers个结点，并且至少回收max_hazard_pointers个结点，这样是很有效的，每次调用pop()都会检查两个结点，回收一个结点

//这种方案也有缺点(除了增加内存使用)，需要计数回收列表中的结点，这就意味着要使用原子计数，并且多个线程还在竞争访问此回收列表，如果又共享内存，就可以用增加的内存使用换区一个更好的回收策略，每个线程在线程本地变量上有自己的回收列表，因此就不需要用来技术的原子变量以及存取列表，相对的，就分配了max_hazard_pointers * max_hazard_pointers个结点，如果线程在回收完它所有的结点前就退出了，它们就可以像以前一样存储在全局列表中，并且加入到下一个执行回收操作的线程的本地列表中

//风险指针的另一个缺点是它们涉及到IBM提交的专利申请，如果在一个承认此专利有效的国家写软件，那么就需要确保获得一个合适的许可，一些无锁内存回收机制可以共用此技术，这是一个很活跃的研究领域，很多公司都在竭尽所能地提交专利申请，你可能会提出这样的疑问，为什么我花了这么多篇幅介绍一种很多人都不能使用的一种技术，这是一个合理的问题，首先，有可能在不获得许可的情况下使用该技术，例如，如果你在GPL下开发免费软件，你的软件可以被IBM的非不主张条款所覆盖，就可以使用该技术，第二，更重要的一点是，对这项技术的解释展示了在写无锁代码的时候需要考虑哪些重要的事情，如原子操作的开销

//因此，是否存在可以用在无锁代码的非专利内存技术?幸运的是，确实有，一种技术就是引用计数

#pragma clang diagnostic pop

