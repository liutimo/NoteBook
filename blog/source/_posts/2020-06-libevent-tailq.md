---
title: libevent-tailq
permalink: libevent-tailq
date: 2020-06-10 12:49:47
tags: 
    - linux C/C++
    - 事件驱动
    - 网络编程
categories: libevent
---



## 队列操作

```c++
#define TAILQ_HEAD(name, type)						\
struct name {								\
	struct type *tqh_first;	/* first element */			\
	struct type **tqh_last;	/* addr of last next element */		\
}

/**
 * 和 TAILQ_HEAD不一样，这个结构体没有 #name。即没有结构体名，
 * 其只能作为一个匿名结构体，通常是作为一个另一个结构体或者联合体的成员。
 * 这点，和内核代码中的用法一致。
 */
#define TAILQ_ENTRY(type)						\
struct {								\
	struct type *tqe_next;	/* next element */			\
	struct type **tqe_prev;	/* address of previous next element */	\
}
```



定义如下结构：

```c++
//队列中的元素结构体。它有一个值，并且有前向指针和后向指针
//通过前后像指针，把队列中的节点(元素)连接起来
struct queue_entry_t
{
    int value;
    //从TAILQ_ENTRY的定义可知，它只能是结构体或者共用体的成员变量
    TAILQ_ENTRY(queue_entry_t)entry;
};

//定义一个结构体，结构体名为queue_head_t，成员变量类型为queue_entry_t
//就像有头节点的链表那样，这个是队列头。它有两个指针，分别指向队列的头和尾
TAILQ_HEAD(queue_head_t, queue_entry_t);
```



1. 初始化头结点

    ```c++
    TAILQ_INIT(&queue_head);
    //没有节点的情况
    printf("queue_head.tqh_first: 0x%08x\n", queue_head.tqh_first);
    printf("*queue_head.tqh_last: 0x%08x\n", *queue_head.tqh_last);
    printf("queue_head.tqh_last: 0x%08x\n",  queue_head.tqh_last);;
    printf("&queue_head.tqh_first: 0x%08x\n", &queue_head.tqh_first);
    ```

    打印如下：

    ```shell
    queue_head.tqh_first: 0x00000000
    *queue_head.tqh_last: 0x00000000
    queue_head.tqh_last: 0xe49e6140
    &queue_head.tqh_first: 0xe49e6140
    ```

    结合`TAILQ_INIT`的源码:

    ```c++
    #define	TAILQ_INIT(head) do {						\
    	(head)->tqh_first = NULL;					\
    	(head)->tqh_last = &(head)->tqh_first;				\
    } while (0)
    ```

    没毛病，我们可以得出这样的结论：

    ```c++
    *(head->tah_last) = (head->tqh_first)
    ```

2. 尾部插入一个节点

    ```c++
    struct 
    ```

    