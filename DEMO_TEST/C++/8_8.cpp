//
//  8_8.cpp
//  DEMO_TEST
//
//  Created by chenyanan on 2017/6/12.
//  Copyright © 2017年 chenyanan. All rights reserved.
//

#include <iostream>
#include <numeric>
#include <thread>
#include <mutex>
#include <future>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

//8.5 在实践中设计并发代码

//当为一个特别的任务设计并发代码的时候，你需要实现考虑每个描述的问题的程度将取决于任务，为了掩饰他们是如何应用的，我们将看看C++标准库中三个函数并行版本的实现，这件给你一个类似的构造基础，提供了考虑问题的平台，作为奖励，我们也有可以用的函数实现，可用来帮助并行一个更大的任务

//我选择这些实现主要是用来演示特别的方法而不是称为最高水平的实现，在并行算法学术文献或者在多线程库例如Inter's Threading Building Blocks中可以找到更高程度的实现，他们更好地利用了可获得的硬件并发性

//概念上最简单的并行算法是std::for_each的并行版本，我们就先从它开始

//8.5.1 std::for_each的并行实现

//std::for_each在概念上很简单，它轮流在范围内的每个元素上调用用户提供的函数，std::for_each的并行实现和串行实现的最大区别就是函数调用的顺序，std::for_each用范围内的第一个元素调用函数，然后是第二个，一次类推，然而使用并行实现就不能保证元素被处理的顺序，他们可能(实际上我们希望)被并发处理

//为了实现并行版本，你只需要将这个范围划分为元素集合在每个线程上处理，你事先知道了元素数量，因此你可以在处理开始前划分数据(参见8.1.1节)，我们假设这是唯一在运行的并行任务，那么就可以使用std::thread::hardware_concurrency()来决定线程的数量，你也知道元素可以被独立的处理，因此你可以使用临近的块来避免假共享(参见8.2.3节)

//这个算法与8.4.1节中描述的std::accumulate的并行版本在概念上是类似的，但是不计算每个元素的和，你只要应用具体函数，尽管你可能推测这回大大简化代码，因为它不返回结果，如果你想传递异常给调用者，你仍然需要使用std::packaged_task和std::future方法在线程间传递异常，一个简单的实现如清单8.7所示

//清单8.7 std::for_each的并行版本

template<typename Iterator, typename Func>
void parallel_for_each(Iterator first, Iterator last, Func f)
{
    unsigned long const length = std::distance(first, last);
    
    if (!length)
        return;
    
    unsigned long const min_per_thread = 25;
    unsigned long const max_threads = (length + min_per_thread - 1) / min_per_thread;
    unsigned long const hardware_threads = std::thread::hardware_concurrency();
    unsigned long const num_threads = std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);
    unsigned long const block_size = length / num_threads;
    
    std::vector<std::future<void>> futures(num_threads - 1);
    std::vector<std::thread> threads(num_threads - 1);
    join_threads joiner(threads);
    
    Iterator block_start = first;
    for (unsigned long i = 0; i < (num_threads - 1); ++i)
    {
        Iterator block_end = block_start;
        std::advance(block_end, block_size);
        std::packaged_task<void(void)> task([=]() {
            std::for_each(block_start, block_end, f);
        });
        futures[i] = task.get_future();
        threads[i] = std::thread(std::move(task));
        block_start = block_end;
    }
    std::for_each(block_start, last, f);
    for (unsigned long i = 0; i < (num_threads - 1); ++i)
    {
        futures[i].get();
    }
}

//这段代码的基础结构与清单8.7中的代码是一样的，关键的不同之处在于futures向量存储了std::future<void>①，因为工作线程不返回值，并且在这个任务上使用一个简单lambda函数激活了从block_start到block_end范围上的函数f②，这就避免了将范围传递给线程构造函数③，因为工作线程不返回值，调用future[i].get()④，只提供取回工作线程抛出的异常的方法，如果你不希望传递异常，那么你就可以省略他

//正如你的std::accumulate的并行实现可以通过使用std::async被简化，因此你的parallel_for_each也可以被简化，它的实现如清单8.8所示

//清单8.8 使用std::async的std::for_each的并行版本

template<typename Iterator, typename Func>
void parallel_for_each(Iterator first, Iterator last, Func f)
{
    unsigned long const length = std::distance(first,last);
    
    if (!length)
        return;
    
    unsigned long const min_per_thread = 25;
    
    if (length < (2 * min_per_thread))
    {
        std::for_each(first, last, f);   //①
    }
    else
    {
        Iterator const mid_point = first + length / 2;   //②
        std::future<void> first_half = std::async(&parallel_for_each<Iterator,Func>, first, mid_point, f);
        parallel_for_each(mid_point, last, f);   //③
        first_half.get();   //④
    }
}

//如同清单8.5中基于std::async的parallel_accumulate一样，你递归地划分数据而不是在执行前划分数据，因为你不知道库会使用多少线程，如以前一样，每一步都将数据划分为两部分，异步运行前半部分②并且直接运行后半部分③直到剩下的数据太小而不值得划分，在这种情况下会调用std::for_each①，使用std::async和get()成员函数std::future④提供了异常传播予以

//让我们从必须在每个元素上执行相同操作的算法专业掉稍微复杂的例子std::find

//8.5.2 std::find的并行实现

//std::find是下一个考虑的有用的算法，因为它是不用处理完所有元素就可以完成的几个算法之一，例如，如果范围内的第一个元素符合搜索准则，那么就不需要检查别的元素，稍后你将看到，这是性能的一个重要属性，并且对设计并行实现有直接影响，这是数据读取部分可能影响代码设计的一个特殊例子(参见8.3.2节)，这类别的算法包括std::equal和std::any_of

//如果你和你的妻子或者搭档在阁楼的两箱纪念品中寻找一张旧照片，如果你找到了相片就应该让他们也停止寻找，你要让他们知道你已经到找了相片(可以通过呼喊,"找到了")，这样他们就可以停止训传并且做别的事情，很多算法的天性是处理每个元素，因此他们没有呼喊"找到了"，对于算法例如std::find早日完成的能力是一个重要的特性并且不浪费任何事情，因此你需要设计你的代码来使用这个特性------当知道结果的时候用一些方式中断别的任务，因此代码不需要等待别的工作线程处理剩下的元素

//如果你不中断别的线程，串行版本比并行版本的性能更好，因为串行算法一旦找到匹配项就停止搜索并且返回，例如，如果系统可以支持四个并发线程，每个线程将检查范围内四分之一的元素，并且我们的并行算法大约花费单个线程四分之一的事件来检查每个元素，如果匹配的元素位于范围内的前四分之一，串行算法会首先返回，因为它不需要检查剩下的元素

//你可以中断别的线程，一种方法是通过使用一个原子变量作为一个标志，并且在处理完每个元素后检查这个标志，如果设置了标志，就代表有一个线程发现了匹配项，因此就可以终止执行并且反悔了，用这种方法中断别的线程，你保持了你不需要处理每个值的特性，因此在更多的情况下与串行版本相比提高了性能，这种方法的缺点是原子载入编程慢动作，这就会妨碍每个线程的前进

//关于如何返回值和如何传递异常你有两个选择，你可以使用future数组，使用std::packaged_task来转移值和异常，然后在主线程中处理返回结果，或者你使用std::promise来从工作线程中直接设置最终结果，如果你希望在第一个异常处停止(即使你没有处理完所有元素)，你可以使用std::promise来设置值和异常，另一方面，如果你希望允许别的工作线程继续搜索，你可以使用std::packaged_task，存储所有的异常，那么没有发现匹配项的时候就重新抛出其中之一

//在这种情况下，我选择使用std:promise因为行为更匹配std::find这里要注意的一件事情就是要搜索的元素不再提供的范围内，因此在从future得到结果之前你需要等待所有县城结束，如果你在future上阻塞了并且没有这个值的话，你就会永远等待，结果如清单8.9所示

//清单8.9 并行find算法的一种实现

template<typename Iterator, typename MatchType>
Iterator parallel_find(Iterator first, Iterator last, MatchType match)
{
    struct find_element   //①
    {
        void operator()(Iterator begin,Iterator end, MatchType match, std::promise<Iterator>* result, std::atomic<bool>* done_flag)
        {
            try
            {
                for (;(begin!=end) && !done_flag->load();++begin)   //②
                {
                    if (*begin==match)
                    {
                        result->set_value(begin);   //③
                        done_flag->store(true);   //④
                        return;
                    }
                }
            }
            catch (...)   //⑤
            {
                try
                {
                    result->set_exception(std::current_exception());   //⑥
                    done_flag->store(true);
                }
                catch(...)   //⑦
                {
                
                }
            }
        }
    };
    
    unsigned long const length = std::distance(first,last);
    
    if (!length)
        return last;
    
    unsigned long const min_per_thread = 25;
    unsigned long const max_threads = (length + min_per_thread - 1) / min_per_thread;
    unsigned long const hardware_threads = std::thread::hardware_concurrency();
    unsigned long const num_threads = std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);
    unsigned long const block_size = length / num_threads;
    
    std::promise<Iterator> result;   //⑧
    std::atomic<bool> done_flag(false);   //⑨
    std::vector<std::thread> threads(num_threads - 1);
    
    {   //⑩
        join_threads joiner(threads);
        
        Iterator block_start = first;
        for (unsigned long i = 0; i < (num_threads - 1); ++i)
        {
            Iterator block_start = first;
            std::advance(block_end, block_size);
            threads[i] = std::thread(find_element(), block_start, block_end, match, &result, &done_flag);   //⑪
            block_start = block_end;
        }
        find_element()(block_start, last, match, &result, &done_flag);   //⑫
    }
    if (!done_flag.load())   //⑬
    {
        return last;
    }
    return result.get_future().get();   //⑭
}

//清单8.9的主函数与之前的例子很相似，这次，在本地find_element类的函数调用操作上完成工作①，这个循环访问给定的块的每个元素，每一步都检查标志②，如果找到匹配项，就在promise中设置最后的结果③并且返回前设置done_flag④

//如果抛出异常，就可以被捕获⑤，并且在设置done_flag前在promise中存储异常⑥，如果promise已经被设置了，在设置值就有可能抛出异常，因此你捕获并且抛弃发生在这里的异常⑦

//这就意味着如果一个线程调用find_element，要么找到匹配项要么抛出异常，此时所有别的线程都会看到done_flag被设置了并且停止执行，如果多个线程同时找到匹配项或者抛出异常，它们就会设置promise值的嘶吼产生竞争，但是这是一个没有危害的竞争条件，无论哪一个线程成功都会成为名义上的"第一个"并且是一个可接受的结果

//回顾主函数parallel_find本身，你用promise⑧和flag⑨来停止搜索，这两者都传递给在这个范围内搜索的新线程⑪，主线程也使用find_element来搜索剩下的元素⑫，我们已经说过了，你在检查结果之前需要等待所有线程结束，因为可能没有匹配的元素，你通过在块中附入线程连接的代码来完成⑩，因此当你检查标志来看看是否发现匹配项的时候，所有线程都被联合起来了⑬，如果发现匹配项，你就可以通过在std::future<iterator>中调用get()得到结果或者抛出存储的异常⑭

//同样，这个实现假设你将使用所有可获得的硬件线程或者你有别的方法来决定线程数量，用来提前在线程间划分工作，跟以前一样，在使用C++标准库的自动扩展功能的时候，你可以使用std::async和递归数据划分来简化你的实现，使用std::async的parallel_find实现如清单8.10所示

//清单8.10 使用std::async的并行查找算法的实现

template<typename Iterator, typename MatchType>   //①
Iterator parallel_find_impl(Iterator first, Iterator last, MatchType match, std::atomic<bool>& done)
{
    try
    {
        unsigned long const length = std::distance(first, last);
        unsigned long const min_per_thread = 25;   //②
        if (length < (2 * min_per_thread))   //③
        {
            for (;(first != last) && !done.load(); ++first)   //④
            {
                if (*first == match)
                {
                    done = true;   //⑤
                    return first;
                }
            }
            return last;   //⑥
        }
        else
        {
            Iterator const mid_point = first + (length / 2);   //⑦
            std::future<Iterator> async_result = std::async(&parallel_find_impl<Iterator, MatchType>,   //⑧ mid_point, last, match, std::ref(done));
            Iterator const direct_result = parallel_find_impl(first, mid_point, match, done);   //⑨
            return (direct_result == mid_point) ? async_result.get() : direct_result;   //⑩
        }
    }
    catch(...)
    {
        done = true;   //⑪
        throw;
    }
}

template<typename Iterator, typename MatchType>
Iterator parallel_find(Iterator first, Iterator last, MatchType match)
{
    std::atomic<bool> done(false);
    return parallel_find_impl(first, last, match, done);   //⑫
}

//如果你找到匹配项就结束查找，意味着你需要引入一个在线程间共享的标志，用来表示已经找到该匹配项，因此这就需要传递给所有递归调用，实现它最简单的方法就是通过在实现函数上①附加参数------引用done标志，这是从主入口点传递进来的⑫
            
//核心实现在类似的代码行里继续执行，与很多实现相同，你在单线程上设置处理项的最小值②，如果你不能将它划分为都至少达到设置的大小的两部分，就在当前线程上运行所有的事情③，实际算法是处理具体范围的简单循环，一致循环直到范围结束或者设置了done标志，如果你查找到匹配项，就在返回值前设置done标志，如果你停止查找，要么你已经到达范围的末端，要么因为另一个线程设置了done标志，你返回last用来表示在这里没有匹配项⑥
            
//如果范围可以被划分，你在使用std::async前首先发现中点⑦，以便在范围的后半部分进行查找⑧，消息使用std::ref来传递done标志的引用，同时，你可以通过直接递归调用在范围的前版本部分进行查找⑨，如果原来的范围太大的话，这个异步调用和直接递归可能导致进一步的划分
            
//如果直接搜索返回mid_point，那么就没有找到匹配项，你就需要得到异步搜索的结果，如果那部分没有发现结果，结果就将是last，这是正确的返回值表明没有查找到这个值⑩，如果"异步的"调用被延迟而不是真正的异步，它在调用get()的时候才真正运行，在这种情况下，如果在后半部分查找到了就跳过搜索范围的前半部分，如果一步搜索已经在另一个线程上运行了，async_result变量的析构函数等待线程完成，这样就不会有任何泄漏线程
            
//如同以前一样，使用std::async提供了异常安全和异常传递特性，如果直接递归抛出异常，future的析构函数将确保运行异步调用的线程在函数返回前就结束了，并且如果异步调用抛出异常，这个异常就通过get()调用传递⑩，使用一个try/catch块是为了在异常上设置done标志并且确保如果抛出异常的话所有线程很快终止⑪，没有它，实现仍然是正确的，但是会继续检查元素知道每个线程都结束了
            
//这个算法的两种实现与另一种并行算法共享的主要特点就是不再保证项目按照从std::find得到的顺序来处理，如果你想并行算法这一点是很基础的，如果顺序很重要的话，你就不能兵法处理元素，如果元素是独立的，就可以使用parallel_for_each，但是这意味着你的parallel_find可能返回范围尾部的元素，即使它与范围头部的元素匹配
            
//好了，你已经处理了并行化std::find正如我在这一节开头说的那样，存在别的类似算法可以在不处理每个数据元素的情况下完成，并且可以使用同样的方法，我们将在第9章中进一步讨论中断线程的问题
            
//为了完成我们的例子，我们将从不同的方面来考虑并且看看std::partial_sum这个算法是一个很有趣的并行算法并且强调了一些附加的设计选择
            
//8.5.3 std::partial_sum的并行实现
            
//std::partial_sum计算了一个范围内的总和，因此每个元素都被这个元素与它之前的所有元素的和所代替，所以序列1，2，3，4，5就变成了1，(1 + 2) = 3，(1 + 2 + 3) = 6，(1 + 2 + 3 + 4) = 10，(1 + 2 + 3 + 4 + 5) = 15，它的并行化是很有趣的，因为你不能将范围划分为块然后单独计算每个块，例如，第一个元素的原始值需要加加到每一个别的元素上
            
//一种用来决定范围内部分和的方法就是计算独立快的部分和，然后将第一个块中计算得到的随后一个元素的值加到下一个块的元素汇总，并以此类推，如果你有元素1，2，3，4，5，6，7，8，9并且你将它分成三个块，首先你得到{1，3，6}，{4，9，15}，{7，15，24}，如果你讲6加到第二个块的所有元素上，你就得到{1，3，6}，{10，15，21}，{7，15，24}，然后你讲第二个块的最后一个元素{21}加到第三个块的元素上，这样最后一个块得到最后的值:{1，3，6}，{10，15，21}，{28，36，55}
            
//同原始划分成块一样，也可以并行加上前一个块的部分和，如果每个块的最后一个元素首先被更新，那么当第二个线程更新下一个快的时候，第一个线程可以更新这个快中剩下的元素，并且一次类推，当列表中的元素多余处理核的时候，这种放方法很有效，因为每个核每一步都要处理大量元素
            
//如果你有很多处理核(同元素数量一样，或者多余元素数量)，这种方法就不是很有效了，如果你在处理器间划分工作，第一步艺元素对计数工作，这这种条件下，这种进一步传递结果意味着很多处理器会等待，因此你需要给他们分配工作，你可以采用另一种放方法对待这个问题，你做部分传递，而不是从一个块到下一个块做全部和的传递，首先像之前一样求和相邻的元素，然后将这些和加到与它像个两个的元素上，然后再将得到的值加到与它相隔四个的元素上，以此类推，如果你以同样的九个元素开始，第一轮后你得到1，3，5，7，9，11，13，15，17，这个就到前两个元素最后的值，第二轮后你得到1，3，6，10，14，18，22，26，30，它的前八个元素是正确的，第四轮后你得到1，3，6，10，15，21，18，36，45，这就是最终答案，尽管它比第一种方法的总步骤数要多，但是如果你有很多线程的话，它有更大的余地来并行化，每个处理器每一步都能更新一个入口
            
//总的来说，第二种方法执行log2(N)个步骤，每一个步骤执行大约N个操作(每个线程处理一个)，其中N是表中的元素数量，第一种算法中，每个线程为分配给它的块的最初分段求和执行一个操作，然后为进一步的传递执行个操作N/k，k是线程数量，因此从中的操作来说，第一种方法是O(N)，第二种方法是O(Nlog(N))，尽管如此，如果你的处理器与列表中的元素一样多，那么第二种方法中每个处理器只需要log(N)个操作，而当k很大时，第一种方法本质上是串行操作，因为需要进一步传递值，对于拥有很少处理单元的系统，第一种方法可以更快结束，而对于大规模并行系统，第二种方法可以更快结束，8.2.1节讨论了这个问题的一个极端的例子
            
//清单8.11 通过划分问题来并行计算分段的和
            
template<typename Iterator>
void parallel_partial_sum(Iterator first, Iterator last)
{
    typedef typename Iterator::value_type value_type;
    
    struct process_chunk   //①
    {
        void operator()(Iterator begin, Iterator last, std::future<value_type>* previous_end_value, std::promise<value_type>* end_value)
        {
            try
            {
                Iterator end = last;
                ++end;
                std::parial_sum(begin, end, begin);   //②
                if (previous_end_value)   //③
                {
                    value_type& addend = previous_end_value->get();   //④
                    *last ++ addend;   //⑤
                    if (end_value)
                    {
                        end_value->set_value(*last);   //⑥
                    }
                    std::for_each(begin,last,[addend](value_type& item)   //⑦
                    {
                        item += addend;
                    })
                }
                else if (end_value)
                {
                    end_value->set_value(*last);   //⑧
                }
            }
            catch(...)   //⑨
            {
                if (end_value)
                {
                    end_value->set_exception(std::current_exception());   //⑩
                }
                else
                {
                    throw;   //⑪
                }
            }
        }
    };
    
    unsigned long const length = std::distance(first, last);
    
    if (!length)
        return last;
    
    unsigned long const min_per_thread = 25;   //⑫
    unsigned long const max_threads = (length + min_per_thread - 1) / min_per_thread;
    
    unsigned long const hardware_threads = std::thread::hardware_concurrency();
    
    unsigned long const num_threads = std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);
    
    unsigned long const block_size = length / num_threads;
    
    typedef typename Iterator::value_type value_type;
    
    std::vector<std::thread> threads(num_threads - 1);   //⑬
    std::vector<std::promise<value_type>> end_values(num_threads - 1);   //⑭
    std::vector<std::future<value_type>> previous_end_values;   //⑮
    previous_end_values.reserve(num_threads - 1);   //⑯
    join_threads joiner(threads);
    
    Iterator block_start = first;
    for (unsigned long i = 0; i < (num_threads - 1); ++i)
    {
        Iterator block_last = block_start;
        std::advance(block_last, block_size - 1);   //⑰
        threads[i] = std::thread(process_chunk(), block_start, block_last, (i != 0) ? &previous_end_values[i - 1] : 0, &end_values[i]);   //⑱
        block_size = block_last;
        ++block_start;   //⑲
        previous_end_values.push_back(end_values[i].get_future());   //⑳
    }
    
    Iterator final_element = block_start;
    std::advance(final_element, std::distance(block_start, last) - 1);   //㉑
    process_chunk()(block_start, final_element, (num_threads > 1) ? &previous_end_values.back() : 0, 0);   //㉒
}
            
//在这个例子汇总，总体结构与之前的代码是一样的，划分问题为块，每个线程拥有最小化的块尺寸⑫，在这种情况下，以线程向量一样⑬，你有一个promise向量⑭用来存储块中最后一个元素的值，并且还有一个future向量⑮，用来得到前一个块的最后一个值，你可以保留future的空间⑯来避免在生成线程的时候再分配，因为你知道将有多少个线程
            
//主循环与以前的一样，指示这次你希望迭代器指向每个块中的最后一个元素本身，而不是通常情况下那样指向最后元素的后继⑰，因此在每个范围你都可以传递最后一个元素，真正的处理是在process_chunk函数对象中完成的，真正的处理是在process_chunk函数对象中完成的，我们稍后再分析，需要给的参数包括这个块的开始和结束的迭代器，先前范围的终值(如果存在的话)，这个范围的终值存放的位置⑱
            
//产生线程后，你就可以更新这个块的起点，记住将它的值加一使它位于最后一个元素之后⑲，并且将当前块的最后一个值的future存储到future向量中，这样下一次循环的时候就可以得到它⑳
            
//在你处理最后一个块之前，你需要得到最后一个元素的迭代器㉑，这样你就可以传递到process_chunk中㉒，std::partial_sum不返回值，因此一旦最后一个块被处理了，你不需要做任何操作，当所有线程结束的时候这个操作就完成了
            
//现在我们来看看process_chunk函数对象在这个工作中所起的作用①，首先在这个块上调用std::partial_sum，包括最后一个元素②，但是然后就需要知道这是否为第一个块③，如果这不是第一个块，那么在先前块中就有previous_end_value，因此你就需要等待这个值④，为了最大化算法的并发性，你就需要首先更新最后一个元素⑤，因此你将值传递给下一个块(如果存在的话)⑥，一旦完成了这些操作，你就可使用std::for_each和一个简单的lambda函数⑦来更新这个范围内剩下的元素
            
//如果没有previous_end_value，那么这就是第一个块，因此你可以只更新下一个块的end_value()(同样，如果存在下一个块的话------这可能是仅有的块)⑧
            
//最后，如果任何操作抛出异常，你捕捉到它⑨并且将它存储在promise中⑩，这样当它试图得到先前的最后一个值的时候就会传递给下一个块了④，这回将所有的异常都传递给最后一块，然后被重新抛出⑪，因为你知道这是运行在主线程上
            
//因为线程间的同步，这断代码就不容易控制与std::async一起重写，等待结果的任务中途通过别的任务的执行，因此所有任务都必须同时运行
            
//使用基于块，向前传输的方法是很少见的，让我们来看看计算范围内部分和的第二种方法
            
//为部分求和实现增量对偶算法
            
//当你的处理器可以按部就班地执行加法，通过累加越来越远的元素来计算部分和的方法可以工作得很好，在这种情况下，不需要进一步的同步，因为所有中间结果都会直接传递给下一个需要它们的处理器，但是实际上，你很少可以再这样的系统上工作，除非你的系统支持少量数据元素上同时执行操作的指令，即所谓的单指令/多数据(Single-Instruction/Multiple-Data, SIMD)指令，因此，你必须为通常情况设计代码并且每一步都明确地同步线程
            
//一种方法是使用屏障(barrier)，一种同步方法使得线程等待直到要求的线程已经到达了屏障，一旦所有线程到达屏障，它们就可以继续执行而不阻塞了，C++11线程没有直接提供这样的工具，因此你需要自己设计
            
//想想一下游乐园里的过山车，如果有适合数量的人在等待，游乐园和资源就会确保在过山车离开平台之前每个座位都有人，屏障的工作原理也是一样的，你提前指定"座位"的数量，并且线程必须等待直到所有"座位"都是满的，一旦有足够的等待线程，就可以继续执行，屏障被重置并且开始等待下一批线程，通常，在循环中使用此构造，在那里同一线程再次出现并且等待下一次，这个想法是为了使线程按部就班地工作，因此一个线程不会再别的线程前面离开以及掉队，对于这个算法，这就会造成灾难，因为离开的线程可能会修改别的线程正在使用的数据，或者使用还没有被正确更新的数据
            
//无论如何，清单8.12给出了屏障的一个简单的实现
            
//清单8.12 一个简单的屏障类
            
class barrier
{
private:
    unsigned const count;
    std::atomic<unsigned> spaces;
    std::atomic<unsigned> generation;
public:
    explicit barrier(unsigned count_) : count(count_), spaces(count), generation(0) {}   //①
    void wait()
    {
        unsigned const my_generation = generation;   //②
        if (!--spaces)   //③
        {
            spaces = count;   //④
            ++generation;   //⑤
        }
        else
        {
            while (generaiton == my_generation)   //⑥
                std::this_thread::yield();   //⑦
        }
    }
};
            
//在这个实现中，你构造了一个barrier，它具有一定数量"座位"①，存储在count变量中，最初，屏障中spaces的数量与count相等，当每个线程等待的时候spaces的数量就会减一③，当它的值为零的时候，spaces的值就会重置为count④，并且generation的值曾一来给别的线程发信号表示它们可以继续执行⑤，如果空闲spaces的数量没有到达零，你就必须等待，这个实现使用一种简单旋转锁⑥，检查在wait()开始的时候得到的值，因为当所有线程到达屏障的是偶才会更新generation的值⑤，等待的时候调用yield()⑦，因此在一个繁忙的等待中，等待的线程不会独占CPU
            
//当我说这个实现是简单的，我的意思是它使用一个旋转锁，因此它不适合线程可能等待很长时间的情况，并且如果在任何一个时间有多于count数量的线程可能调用wait()，那么它就不起作用了，如果你想处理这些情况中的任何一种，你就必须使用一个更健壮性(但是更复杂)的实现来代替，我遵循了在原子变量上的顺序连续操作，因为这使得事情更容易解释，但是你可以放松一些顺序约束，在大规模兵法结构上，这样的全局同步是代价很高的，因为缓存线持有屏障的状态必须在所有涉及到的线程间穿梭(参见8.2.2节)，因此你必须注意确保在这里这是最好的选择
            
//无论如何，在这里这就是你所需要的，你有固定数量的线程在循规蹈矩的循环中运行，当然，这只是几乎固定数量的线程，你可能记得，在一些步骤后，列表开始的想获得它的最终值，这就意味着要么你保持这些线程循环直到处理完整个范围，要么你需要允许屏障处理线程退出并且减少count，我倾向于后面这一种选择，因为它避免了使线程只是循环而不做任何有用的工作知道最后一步完成
            
//这就意味着你需要将count变成一个原子变量，因此你可以从多个线程更新它而不需要额外的同步
            
std::stomic<unsigned> count;
            
//它的初始化是一样的，但是现在当你重置spaces的值的时候就必须明确在count中load()
            
//这些都是在wait()上所做的改变，现在你需要一个新的成员来减少count，我们称之为done_waiting()，因为一个线程生成在等待的时候完成它
            
void done_waiting()
{
    --count;
    if (!--spaces)
    {
        spaces = count.load();
        ++generation;
    }
}
            
//你做的第一件事就是递减count的值①，这样下一次重置spaces就反映新的等待线程的数量，然后你需要将空闲spaces的数目递减②，如果不这么做，别的线程就会永远等待，因为spaces被初始化为旧的，更大的值，如果这是分支上的最后一个线程，你就需要重置counter并且增加generation③，就如你在wait()中所做的那样，在完成所有的等待后就结束了
            
//现在你准备好写部分和的第二种实现了，在每一步，每个线程在屏障上调用wait()来保证线程步骤一起，并且一旦每个线程都完成了，就在屏障上调用done_waiting()来减少count，如果你在初始范围内使用第二个缓冲器，屏障提供你所需要的额所有同步，在每一步中，线程从原先的范围或者缓冲器中读取，并且将新值写入相关元素中，如果线程在一个步骤中读取了原先的范围，它们在下一个步骤中读取缓冲器，反之亦然，这就保证了在不同线程结束循环，它必须保证正确的最终值被写入最初的范围中，清单8.13个除了所有的操作
            
//清单8.13 通过成对更新的partial_sum的并行实现
            
struct barrier
{
    std::atomic<unsigned> count;
    std::atomic<unsigned> spaces;
    std::atomic<unsigned> generation;
    
    barrier(unsigned count_) : count(count_), spaces(count_), generation(0) {}
    
    void wait()
    {
        unsigned const gen = generation.load();
        if (!--spaces)
        {
            spaces = count.load();
            ++generation;
        }
        else
        {
            while(generation.load()==gen)
            {
                std::this_thread::yield();
            }
        }
    }
    
    void done_waiting()
    {
        --count;
        if (!--spaces)
        {
            spaces = count.load();
            ++generation;
        }
    }
};
            
template<typename Iterator>
void parallel_partial_sum(Iterator first, Iterator last)
{
    typedef typename Iterator::value_type value_type;
    
    struct process_element   //①
    {
        void operator()(Iterator first, Iterator last, std::vector<value_type>& buffer, unsigned i, barrier& b)
        {
            value_type& ith_element = *(first + i);
            bool update_source = false;
            
            for (unsigned step = 0; stride = 1; stride <= i; ++step, stride *= 2)
            {
                value_type& ith_element = *(first + i);
                bool update_source = false;
                
                for (unsigned step = 0, stride = 1; stride <= i; ++step, stride *= 2)
                {
                    value_type const& source = (step % 2) ? buffer[i] : ith_element;   //②
                    value_type & dest = (step % 2) ? ith_element:buffer[i];
                    value_type const& addend = (step % 2) ? buffer[i - stride] : *(first + i - stride);   //③
                    
                    dest = source + added;   //④
                    update_source = !(step % 2);
                    b.wait();   //⑤
                }
                
                if (update_source)   //⑥
                {
                    ith_element = buffer[i];
                }
                b.done_waiting();   //⑦
            }
        }
    };
    
    unsigned long const length = std::distance(first, last);
    
    if (length <= 1)
        return;
    
    std::vector<value_type> buffer(length);
    barrier b(length);
    
    std::vector<std::thread> threads(length - 1);
    join_threads joiner(threads);
    
    Iterator block_start = first;
    for (unsigned long i = 0; i < (length - 1); ++i)
    {
        threads[i] = std::thread(process_element(), first, last, std::ref(buffer), i, std::ref(b));
    }
    prcess_element()(first, last, buffer, length - 1, b);
}
            
//先在你已经很熟悉这段代码的总体结构了，你用一个拥有函数调用操作的类(process_element)来做这个工作①，你在存储在向量中⑧的一些线程⑨上运行它并且在主线程调用它⑩，这次的主要不同之处自傲与线程数量屈居于列表中项的数量而不是取决于std::thread::hardware_concurrency，正如我所说，除非你是在一个线程很便宜的大量并行机器上运行，这不是一个好方法，但是它显示了总体结构，也可以用较少的线程，每个线程处理源范围的多个值，但是这也会出现一个问题，那就是有足够少的线程使得它比向前传输算法的效率还要低
            
//无论如何，在process_element函数调用凑走中完成了关键工作，每一步你要么从原先的范围得到第i个元素要么从缓冲器中得到第i个元素②，并且将它添加到stride元素的值中③，如果从原先的范围开始就将它存储在缓冲器汇总，或者如果从缓冲器开会就存储在原先的范围中④，然后在开始下一步之前你在屏幕上等待⑤，当stride使你离开范围的起点时你就完成操作了，在这种情况下，如果你的最终结果存储在缓冲器中的话，就更新原先范围里的元素⑥，最后，你告诉屏障你正在done_waiting()⑦，
            
//注意这种情况不是异常安全的，如果一个工作线程在process_element上抛出异常，就会终止此引用，你可以使用std::promise存储异常来处理这种情况，正如你在清单8.9的parallel_find实现中所作的一样，或者使用互斥元保护的std::exception_ptr
            
//这总结了我们的三个例子，希望它们有助于具体化8.1，8.2和8.3中强调的设计考虑，并且证明在真实代码中可以使用这些方法
            
//8.6 总结
//本章我们设计很多基础知识，我们从在线程间划分工作的多种方法开始，如提前划分数据或者使用需多线程来形成管道，然后我们从低水平角度考虑与多线程代码的性能紧密相关的问题，在转移到数据读取是如何影响代码性能之前考虑了假共享和数据竞争问题，然后我们考虑了设计并发代码时候要考虑的问题，如异常安全和可扩展性，最后，我们以一些并发算法实现的例子作为结束，每一个例子都强调了当设计多线程代码时可能出现的具体问题。
            
//本章出现几次的一个事情就是线程池的想法------一组实现配置好的线程运行分批给线程池的任务，很多想法被用在设计一个好的线程池上，因此下一章我们将考虑一些问题，以及高级线程管理的别的方面
#pragma clang diagnostic pop
