//
//  5_5.cpp
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

//5.3.3 原子操作的内存顺序
//有六种内存顺序选项可以应用到原子类型上的操作memory_order_relaxed，memory_order_consume，memory_order_acquire，memory_order_release, memory_order_acq_rel，memory_order_seq_cst，除非你是为某个特定的操作做出指定，原子类型上的所有操作的内存顺序选项都是memory_order_seq_cst，这是最严格的可用选项，尽管有六种顺序选项，他们其实代表了三种模型:顺序一致(sequentiallyconsistent)式顺序(memory_order_seq_cst),获得-释放(acquire-release)式顺序(memory_order_consume,memory_order_acquire,memory_order_release,memory_order_acq_rel),以及松散(relaxed)式顺序(memory_order_relaxed)

//这些不同的内存顺序模型在不同的CPU构架上可能有着不同的成本，例如，在基于具有通过处理器而非做更改者对操作的可见性进行良好控制构架上的系统中，顺序一致的顺序相对于获得-释放顺序或松散顺序，以及获得-释放顺序相对于松散顺序，可能会要求额外的同步指令，如果这些系统拥有很多处理器，这些额外的同步指令可能占据显著的时间量，从而降低该系统的性能，另一方面，为了确保原子性，对于超出需要的获得-释放排序，使用X86或X86-64构架(如在台式PC中常见的Inter和AMD处理器)的CPU不会要求外的指令，甚至对于载入操作，顺序一致顺序不需要任何特殊的处理，尽管在存储时，会有一点额外的成本

//不同的内存顺序模型的可用性，允许高手们利用更细粒度的顺序关系来提升性能，在不太关键的情况下，当运行使用默认的顺序一致顺序时，它们是有有时的

//为了选择使用哪个顺序模型，或是为了理解使用不同模型的代码中的顺序关系，了解该选择会如何影响程序行为是很重要的，因此让我们来看一看每种选择对于操作顺序和synchronizes-with的后果

#pragma clang diagnostic pop
