---
title: vim commands
date: 2020-04-09 11:21:08
tags: 
    - vim
categories: 工具
---

## 替换指定字符串
**语法**
```js
[addr]s/src/des/[option]
```
**参数**

1. `addr`表示检索范围。

   `m,n` 表示从m行到n行。
   
   `.`表示当前行，`$`表示末尾文件末尾。 
   
   此外，`%`相当于`1,$`，即整个文件范围
   
2. `s`

    表示替换操作。`substitute`。

3. `src` `des`

    将`src`替换成`des`。遇到特殊字符需要转义。

4. `option`表示操作类型。

    缺省，即表示仅对每行的一个匹配串进行替换

    `g`，globe,表示全局替换。

    `c`，confirm，表示替换时需要进行确认

    `i`，ignore，表示忽略大小写。

**实例**    

```shell
:s/a/b/ #将a替换成b
:%s/a/b/i  
:1,100s/a/b/c
```

> 这些都是很简单的，复杂的包括正则表达式啥的，感觉用处不大，用到了再加吧。。。。