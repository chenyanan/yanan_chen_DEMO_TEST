//
//  7_8.cpp
//  123
//
//  Created by chenyanan on 2017/5/12.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <iostream>
#include <thread>
#include <mutex>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//7.2.6 编写不用锁的线程安全队列

//队列与栈有所不同，因为队列中push()和pop()操作读取了数据结构的不同部分，而栈中这两个操作读取了相同的头结点，所以同步要求就不一样了，你需要确保一端所作出的改变能被另一端正确地读取，尽管如此，清单6.6中队列的try_pop()结构与清单7.2简单无锁栈中的pop()区别并不是很大，因此可以合理假设无锁代码不会不相似

//如果以清单6.6作为基础，就需要两个节点指针，一个指针指向head，一个指针指向tail，多个线程将会读取它们，为了去掉相关的互斥元，最好是原子操作，下面我们来做一些小的改变来看看效果如何，清单7.13展示了效果

//清单7.13 单生产者单消费者的无锁队列

template<typename T>
class lock_free_queue
{
private:
    struct node
    {
        std::shared_ptr<T> data;
        node* next;
        
        node() : next(nullptr) {}
    };
    
    std::atomic<node*> head;
    std::atomic<node*> tail;
    
    node* pop_head()
    {
        node * const old_head = head.load();
        if (old_head == tail.load())   //①
        {
            return nullptr;
        }
        head.store(old_head->next);
        return old_head;
    }
public:
    lock_free_queue() : head(new node), tail(head.load()) {}
    lock_free_queue(const lock_free_queue& other) = delete;
    lock_free_queue& operator=(const lock_free_queue& other) = delete;
    ~lock_free_queue()
    {
        while (node * const old_head = head.load())
        {
            head.store(old_head->next);
            delete old_head;
        }
    }
    
    std::shared_ptr<T> pop()
    {
        node * old_head = pop_head();
        if (!old_head)
        {
            return std::shared_ptr<T>();
        }
        
        const std::shared_ptr<T> res(old_head->data);   //②
        delete old_head;
        return res;
    }
    
    void push(T new_value)
    {
        std::shared_ptr<T> new_data(std::make_shared<T>(new_value));
        node *p = new node;   //③
        node * const old_tail = tail.load();   //④
        old_tail->data.swap(new_data);   //⑤
        old_tail->next = 0;   //⑥
        tail.store(p);   //⑦
    }
};

//看起来似乎没什么不好，如果一次只有一个线程调用push()，并且只有一个线程调用pop()，就工作的很好了，在这种情况下，重要的是push()和pop()的发生顺序关系以确保可以安全获取data，tail存储⑦与tail加载①同时发生，存储先前加载data指针之前②，因此存储data发生在加载之前，这就是安全的，这是一个性能良好的单生产者，单消费者(single-producer,single-consumer,SPSC)队列

//当多个线程同时调用push()或多个线程同时调用pop()的时候就会存在问题，首先来看push()，如两个线程同时调用push()，它们都会分配新节点作为新的哑元结点③，都会读取相同的tail④，并且设置data和next指针⑤，⑥时都会同时更新同一个结点的数据成员，这就是数据竞争

//pop_head()中也存在类似的问题，如果两个线程同时调用pop_head，就会读取同一个head，并且会用同一个next指针覆盖旧值，这两个线程现在认为他们得到了相同的结点，这是有很大危害的，你不但要确保只有一个线程pop()结点，并且需要确保别的线程可以安全访问head的下一个结点，这就是无锁栈的pop()遇到的问题，因此很多方法可以用在这里

//如果pop()是一个"已解决的问题"，那么push()呢？问题就是为了得到push()和pop()的先后顺序关系，需要在更新tail前设置哑元结点的数据项，这就意味着同时调用push()会在这些相同的数据项上产生竞争，因为他们读取了相同的tail指针

//1. 处理push()中的多个线程
//一种选择是在真正的结点间增加一个哑元结点，这样，当前tail结点只需要更新它的next指针，因此可以是原子的，如果一个线程成功地将它的next指针从空改变为新结点，就代表它成功地增加了指针，否则，它就必须再次开始并且重新读取tail，这就需要对稍微改变pop()来丢弃有空数据指针的结点，并且再次循环，缺点就是每次调用pop()都会移出两个节点，并且会有两倍内存分配

//第二种选择是使得data指针是原子的，并且调用比较/交换来设置它，如果调用成功，那么这就是tail结点，并且可以安全地将next指针设置为新结点，然后更新为tail，如果另一个线程已经存储了此数据，导致比较/交换失败了，那么就再次循环，重新读取tail然后重新开始，如果std::shared_ptr<>的原子操作是无锁的，那么整个就是无锁的，如果不是，就需要别的方法，一种可能就是让pop()返回std::unique_ptr<>(毕竟，这是对象的唯一引用)并且将数据用普通指针存储在队列里，这就允许你将它存储为std::atomic<T*>这样你就可以使用compare_exchange_strong()，如果你使用清单7.11中的引用计数方法在多线程情况下处理pop()，那么push()就如清单7.14所示

//清单7.14 首次(很逊的)尝试修订push()

void push(T new_value)
{
    std::unique_ptr<T> new_data(new T(new_value));
    counted_node_ptr new_next;
    new_next.ptr = new node;
    new_next.external_count = 1;
    for (;;)
    {
        node * const old_tail = tail.load();
        T* old_data = nullptr;
        if (old_tail->data.compare_exchange_strong(old_data, new_data.get()))
        {
            old_tail->next = new_next;
            tail.store(new_next.ptr);
            new_data.release();
            break;
        }
    }
}

//使用引用计数方法避免了特定的竞争，但是这不是push()中唯一的竞争，如果你看看清单7.14中push()的修订版本，就会发现栈中有这样一段代码:加载一个原子指针①并且解引用那个指针②，同时，另一个线程可以更新那个指针③，最后指向再分配的结点(在pop()中)，如果在你解引用那个指针前再分配那个节点，就会禅城不确定的行为，天哪!它试图像head一样在tail中增加一个外部计数，但是每个节点在队列先前的结点的next指针中已经有一个外部计数了，同一个结点拥有两个外部计数就意味着需要修改引用计数方法来避免太早删除该节点，你可以这样处理，即在node结构体中计算外部计数的数量，并且当每个外部计数被销毁的时候(以及将相关的外部计数加到内部计数的时候)减少它的数量，如果结点的内部计数为零并且没有外部计数，此时就可以安全删除该结点，最初我是从Joe Seigh的Atomic Ptr Plus项目了解到的技术，清单7.15给出了使用这种方法的push()

//清单7.15 在无锁队列中用引用计数tail来实现push()

template<typename T>
class lock_free_queue
{
private:
    struct node;
    
    struct counted_node_ptr
    {
        int external_count;
        node* ptr;
    };
    
    std::atomic<counted_node_ptr> head;
    std::atomic<counted_node_ptr> tail;   //①
    
    struct node_counter
    {
        unsigned internal_count:30;
        unsinged external_counters:2;   //②
    };
    
    struct node
    {
        std::atomic<T*> data;
        std::atomic<node_counter> count;   //③
        counted_node_ptr next;
        
        node()
        {
            node_counter new_count;
            new_count.internal_count = 0;
            new_count.external_counters = 2;   //④
            count.store(new_count);
            
            next.ptr = nullptr;
            next.external_count = 0;
        }
    };
public:
    void push(T new_value)
    {
        std::unique_ptr<T> new_data(new T(new_value));
        counted_node_ptr new_text;
        new_next.ptr = new node;
        new_next.external_count = 1;
        counted_node_ptr old_tail = tail.load();
        
        for (;;)
        {
            increase_external_count(tail, old_tail);   //⑤
            
            T* old_data = nullptr;
            if (old_tail.ptr->data.compare_exchange_strong(old_data, new_data.get()))   //⑥
            {
                old_tail.ptr->next = new_next;
                old_tail = tail.exchange(new_next);
                free_external_counter(old_tail);   //⑦
                new_data.release();
                break;
            }
            old_tail.ptr->release_ref();
        }
    }
};

//清单7.15中，tail和head一样都是atomic<counted_node_ptr>①，并且node有一个count成员代替了之前的internal_count③，count是包含internal_count和额外的external_counters成员②的结构体，注意这里的external_counters只包含两个比特，因为最多只有两个计数器，通过使用一个比特来表示它，并且internal_count是一个30比特的值，总的计数器大小可以保持32比特，这就使得在确保整个结构体在32比特和64比特的机器上都能用一个机器字标识的情况下，还能有足够的范围来表示比较大的内部计数值，为了避免竞争条件，将这些计数作为一个值来更新是很重要的，稍后你将看到，将此结构体保存在一个机器字中在许多平台中使原子操作更容易是无锁的

//node初始化的时候，internal_count被设为零，external_counters的值设为2④，因为一旦你将结点添加到队列中，每个新节点都会引用tail以及先前结点的next指针，push()与清单7.14中类似，除了你为了调用结点data成员的comapre_exchange_strong()而解引用从tail加载的值之外⑥，你还调用一个新函数increase_external_count()来增加计数⑤，并且之后在旧的tail上调用free_external_counter()⑦

//处理好push()之后，我们来看看pop()，清单7.16展示了它，并且将清单7.11中pop()实现的引用计数逻辑与清单7.13中的队列pop逻辑结合在一起

//清单7.16 从使用引用计数tail的无锁队列中将结点出队列

template<typename T>
class lock_free_queue
{
private:
    struct node
    {
        void release_ref();
    };
public:
    std::unique_ptr<T> pop()
    {
        counted_node_ptr old_head = head.load(std::memory_order_relaxed);
        for (;;)
        {
            increase_external_count(head, old_head);
            node* const ptr = old_head.ptr;
            if (ptr == tail.load().ptr)
            {
                ptr->release_ref();
                return std::unique_ptr<T>();
            }
            
            if (head.compare_exchange_strong(old_head, ptr->next))
            {
                T* const res = ptr->data.exchange(nullptr);
                free_external_counter(old_head);
                return std::unique_ptr<T>(res);
            }
            ptr->release_ref();
        }
    }
};

//你在喀什循环前①以及增加加载值的外部引用前②加载old_head值，如果head与tail是同一个结点，那么就可以释放该引用③并且返回一个空指针，因为队列中没有数据，如果队列中有数据，你就想获得此数据，并且调用compare_exchange_strong()来实现④，如同清单7.11中的栈一样，它将外部计数和指针作为一个值来比较，如果任何一个改变了，就在释放引用后重新循环⑥，如果comapre_exchange_strong()成功了，你就获得了节点中的数据，因此你咋释放移出的结点的外部计数后，可以将此值返回给调用者⑤，一旦外部引用计数都被释放了并且内部计数值变为零，就可以删除结点了，清单7.17，清单7.18和请安7.19展示了处理这些的引用计数函数

//清单7.17 释放无锁队列的结点引用

template<typename T>
class lock_free_queue
{
private:
    struct node
    {
        void release_ref()
        {
            node_counter old_counter = count.load(std::memory_order_relaxed);
            node_counter new_counter;
            do
            {
                new_counter = old_counter;
                --new_counter.internal_count;   //①
            } while(!count.compare_exchnge_strong(old_counter, new_counter,   //②
                                                  std::memory_order_acquire,
                                                  std::memory_order_relaxed));
            
            if (!new_counter.internal_count && !new_counter.external_counters)
            {
                delete this;   //③
            }
        }
    };
};

//node::release_ref()的实现只在清单7.11的lock_free_stack::pop()上改变了一些对应的代码，清单7.11中的代码只需要处理一个外部计数，因此可以用一个简单fetch_sub，而现在即使只想修改internal_count域，也需要原子更新整个count①，因此需要一个比较/交换循环②，一旦你减少internal_count，如果内部计数和外部计数都变为零，那么这就是最后一个引用，就可以安全删除该节点了③

//清单7.18 在无锁队列中获得结点的新引用

template<typename T>
class lock_free_queue
{
private:
    static void increase_external_count(std::atomic<counted_node_ptr>& counter, counted_node_ptr& old_counter)
    {
        counted_node_ptr new_counter;
        
        do
        {
            new_counter = old_counter;
            ++new_counter.external_count;
        } while (!counter.comapre_exchange_strong(old_counter, new_counter,
                                                  std::memory_order_acquire,
                                                  std::memory_order_relaxed));
        
        old_counter.external_count = new_counter.external_count;
    }
};

//清单7.18则相反，这次，你得到一个新的引用并且增加外部计数，而不是释放一个引用，increase_external_count()与清单7.12中的increase_head_count()函数类似，除了它使用一个静态成员函数来将外部计数作为第一个参数来更新，而不是在固定计数器上进行操作

//清单7.19 在无锁队列中释放结点的外部计数

template<typename T>
class lock_free_queue
{
private:
    static void free_external_counter(counted_node_ptr& old_node_ptr)
    {
        node* const ptr = old_node_ptr.ptr;
        int const count_increase = old_node_ptr.external_count - 2;
        
        node_counter old_counter = ptr->count.load(std::memory_order_relaxed);
        
        node_counter new_counter;
        
        do
        {
            new_counter = old_counter;
            --new_counter.external_counters;   //①
            new_counter.internal_count += count_increase;   //②
        } while(!ptr->count.compare_exchange_strong(old_counter, new_counter,
                                                    std::memory_order_acquire,
                                                    std::memory_order_relaxed));   //③
        
        if (!new_counter.internal_count && !new_counter.external_counters)
        {
            delete ptr;   //④
        }
    }
};

//与increase_external_count()相对的是free_external_counter()，这与清单7.11中lock_free_stack::pop()的对应代码是类似的，但是被修改为可以处理external_counters计数，它在整个count上使用单个compare_exchange_strong()来处理两个计数③，正如你在release_ref()中减少internal_count所做的一样，正如清单7.11一样，internal_count的值被更新了②，并且external_counters的值减少1①，如果这两个值现在都为零，那么此节点就没有医用，因此可以安全你删除它④，这需要作为一个操作来执行(因此需要比较/交换循环)以避免竞争条件，如果分别更新这两个值，那么两个线程够可能认为它们自己是最后一个线程，因此都删除这个节点，这就会导致未定义的行为

//尽管这种方法是有效的并且是无竞争的，但是它有性能问题，一旦一个线程通过成功完成old_tail.ptr->data上的compare_exchange_strong()(如清单7.15中⑤所示)，开始执行push()操作，此时没有线程可以执行push()操作，任何试图执行push()操作的线程都会看到新值而不nullptr，这就使得comapre_exchange_strong()失败并且使得线程再次循环，这是一个忙则等待现象，会消耗CPU周期而没有任何收益，因此，这是一个锁，一个调用push()的线程阻塞别的线程，直到它完成了操作，因此这个代码不是无锁的，正常情况下，当线程阻塞时，操作系统可以给拥有互斥锁的线程优先权，因此阻塞的线程会一直消耗CPU周期直到地一个线程完成操作，这就需要下一个方法，等待的线程可以帮助正在执行push()操作的线程

//2. 通过协助另一个线程使得队列无锁

//为了使代码无锁，就需要找到一种方法使得即使执行push()操作的线程拖延了，等待的线程依然可以继续执行，一种方法就是帮助拖延的线程做它要完成的操作

//在这种情况下，你清楚地直到将要做哪些操作，tail的next指针需要指向一个新的哑元结点，然后更新tail指针，哑元结点是没有区别的，因此无论是使用成功将数据入队列的线程创造的哑元结点，还是使用等待将数据入队列的线程创造的哑元结点，都是可以的，如果使得结点的next指针成为原子的，就可以使用comapre_exchange_strong()来设置该指针，一旦设置好next指针，就可以在确保它仍然引用同一个最初的结点的情况下，使用compare_exchange_weak()循环来设置tail，如果它没有引用同一个最初的结点，那么就表示别的线程已经更新它了，此时就停止尝试并且再次循环，这就需要稍微改变pop()来载入next指针，如清单7.20所示

//清单7.20 修改pop()来允许帮助push()

template<typename T>
class lock_free_queue
{
private:
    struct node
    {
        std::atomic<T*> data;
        std::atomic<node_counter> count;
        std::atomic<counted_node_ptr> next;   //①
    };
public:
    std::unique_ptr<T> pop()
    {
        counted_node_ptr old_head = head.load(std::memory_order_relaxed);
        for (;;)
        {
            increase_external_count(head,old_head);
            node* const ptr = old_head.ptr;
            if (ptr == tail.load().ptr)
            {
                return std::unique_ptr<T>();
            }
            counted_node_ptr next = ptr->next.load();   //②
            if (head.compare_exchange_strong(old_head, next))
            {
                T* const res = ptr->data.exchange(nullptr);
                free_external_couner(old_head);
                return std::unique_ptr<T>(res);
            }
            ptr->release_ref();
        }
    }
};

//如我所言，这里的改变是很简单的，next指针现在是原子的①，因此②中的load也是原子的，在这个例子中，使用了默认的memory_order_seq_cst顺序，因此你可以省略明确调用load()，并且依靠counted_node_ptr的隐式载入，但是使用明确的调用可以提醒你稍后在哪里增加明确的内存顺序

//清单7.21列出了push()的更多代码

//清单7.21无锁队列中使用帮助的push()

template<typename T>
class lock_free_queue
{
private:
    void set_new_tail(counted_node_ptr& old_tail, const counted_node_ptr &new_tail)   //①
    {
        const node * current_tail_ptr = old_tail.ptr;
        while(!tail.compare_exchange_weak(old_tail, new_tail) && old_tail.ptr == current_tail_ptr);   //②
        if (old_tail.ptr == current_tail_ptr)   //③
            free_external_counter(old_tail);   //④
        else
            current_tail_ptr->release_ref();   //⑤
    }
public:
    void push(T new_value)
    {
        std::unqiue_ptr<T> new_data(new T(new_value));
        counted_node_ptr new_ptr;
        new_next.ptr = new node;
        new_next.external_count = 1;
        counted_node_ptr old_tail = tail.load();
        
        for (;;)
        {
            increase_external_count(tail, old_tail);
            T* old_data = nullptr;
            if (old_tail.ptr->data.compare_exchange_strong(old_data, new_data.get()))   //⑥
            {
                if (old_tail.ptr->next.compare_exchange_strong(old_next, new_next))   //⑦
                {
                    delete new_next.ptr;   //⑧
                    new_next = old_next;   //⑨
                }
                set_new_tail(old_tail, new_next);
                new_data.release();
                break;
            }
            else   //⑩
            {
                counted_node_ptr old_next = {0};
                if (old_tail.ptr->next.compare_exchange_strong(old_next, new_next))   //⑪
                {
                    old_next = new_next;   //⑫
                    new_next.ptr = new node;   //⑬
                }
                set_new_tail(old_tail, old_next);   //⑭
            }
        }
    }
};

//这与清单7.15中的最初的push()是类似的，但是也有一些很重要的不同之处，如果你的确设置了data指针⑥，就需要处理这样一种情况，那就是另一个线程已经帮助你了，现在有一个else子句也在帮助你⑩

//已经设置了结点的data指针⑥，push()的新版本使用compare_exchange_strong()来跟新next指针⑦，使用compare_exchange_strong()来避免循环，如果交换失败了，就可以得知另一个线程已经设置了next指针，因此就不再需要最初分配的新节点了，就可以删除它⑧，你仍然想使用另一个线程更新tail设置的next值⑨

//tail指针的真正更新放生在set_new_tail()中①，这就使用compare_exchange_weak()循环②来更新tail，因为如果别的线程试图push()一个新节点，那么external_count的值就发生了改变并且你不想失去她，尽管如此，你要注意如果另一个线程已经成功改变了它，那么你就不能更换此值，否则，就可能以在队列中循环作为结束，而这不是一个好主意，所以，你要确保如果比较/交换失败了，载入值的ptr是同样的，如果退出循环时ptr是同样的③，那么你就必须成功设置tail，因此需要释放旧的外部计数④，如果退出循环时ptr是不一样的，就说明另一个线程将释放此计数器，因此你只需要通过该线程释放单个引用⑤

//如果线程调用push()，并且这次没有成功通过循环设置data指针，那么它可以帮助成功的线程完成更新，首先，你尝试更新这个线程新分配结点的next指针⑪，如果成功了，你将使用你分配的结点作为新的tail⑫，并且需要分配另一个新节点预期可以真正入队列⑬，然后你就可以在再次循环前通过调用set_new_tail来设置tail⑭

//你可能已经注意到这一段代码中有很多的new和delete调用，因为push()分配新节点，而pop()销毁结点，内存分配器的效率在很大程度上影响了这段代码的性能，一个不好的内存分配器可以完全破坏无锁容器的可扩展性，选择和实现该分配器超出了该书的范围，但是请记住判别分配器是好是坏的唯一办法就是使用它并且测试使用它前后代码的性能，优化内存分配器的通用办法包括在每个线程上都有一个独立的内存分配器，以及使用空闲表来回收结点而不是将它们返回给分配器

//已经举了很多例子了，现在，我们从这些例子里找出写无锁数据结构的一些准则



#pragma clang diagnostic pop
