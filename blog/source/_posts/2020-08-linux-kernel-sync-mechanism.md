---
title: linux_kernel_sync_mechanism
permalink: linux_kernel_sync_mechanism
date: 2020-08-26 14:23:51
tags:
	- linux
 	- 同步机制
categories: linux
---



## linux 内核同步机制

以ARM为例

###  关键的汇编指令

在ARM6前，其不支持SMP，其上实现原子变量的操作是通过关闭中断来实现的。

```c++
//arch/arm/include/asm/atomic.h
#if __LINUX_ARM_ARCH__ >= 6
...
#else /* ARM_ARCH_6 */

#ifdef CONFIG_SMP
#error SMP not supported on pre-ARMv6 CPUs
#endif  
    

#define ATOMIC_OP(op, c_op, asm_op)					\
static inline void atomic_##op(int i, atomic_t *v)			\
{									\
	unsigned long flags;						\
									\
	raw_local_irq_save(flags);					\
	v->counter c_op i;						\
	raw_local_irq_restore(flags);					\
}									\

#endif /* __LINUX_ARM_ARCH__ */  
```

而在ARM6以后的系统中，原子变量通过两个关键的汇编指令`ldrex`和`strex`实现。

[ldrex和strex](http://blog.chinaunix.net/uid-23046336-id-3262445.html)

`ldrex`读取数据时会进行独占标记，防止其他内核路径访问，直至调用`strex`完成写入后清除标记。自然`strex`也不能写入被别的内核路径独占的内存，若是写入失败则循环至成功写入。





### atomic

```c++
#define ATOMIC_OP(op, c_op, asm_op)					\
static inline void atomic_##op(int i, atomic_t *v)			\
{									\
	unsigned long tmp;						\
	int result;							\
									\
	prefetchw(&v->counter);						\
	__asm__ __volatile__("@ atomic_" #op "\n"			\
"1:	ldrex	%0, [%3]\n"						\
"	" #asm_op "	%0, %0, %4\n"					\   //执行相关的 op操作
"	strex	%1, %0, [%3]\n"						\	
"	teq	%1, #0\n"						\		 // strex 会将执行结果放入到 %1 也就是tmp中，判断其是否为0
"	bne	1b"							\			// tmp != 0的话就重新执行 到 ldrex那一步。
	: "=&r" (result), "=&r" (tmp), "+Qo" (v->counter)		\
	: "r" (&v->counter), "Ir" (i)					\
	: "cc");							\
}									\
```

`%0` `%1` 这种表示占位符，对应后面的`result`、`tmp`等。



从注释来看，实现是容易理解的，要想彻底弄明白`atomic`的实现，就要深入了解`ldrex`和`strex`的实现原理，这里我们大致了解就行，深挖就出不来了。



![img](images/2020-08-linux-kernel-sync-mechanism/640)



