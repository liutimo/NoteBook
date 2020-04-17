---
title: Android init -- 属性服务初始化
date: 2020-04-15 20:25:22
categories: Android Framework
tags: 
    - Android Framework
    - Android Init进程	
---













## 属性系统初始化

init进程首先调用`property_init`，随后调用`start_property_service`。这应该就是inti为属性系统初始化做的全部工作了。

### `property_init`

```c++
//  #system/core/init/property_service.cpp
void property_init() {
    if (__system_property_area_init()) {
        exit(1);
    }
}
// #bionic/libc/bionic/system_properties.cpp
int __system_property_area_init() {
  //[1] 清空之前的设置
  free_and_unmap_contexts();
  //[2]
  mkdir(property_filename, S_IRWXU | S_IXGRP | S_IXOTH);
  //[3]
  if (!initialize_properties()) {
    return -1;
  }
  bool open_failed = false;
  bool fsetxattr_failed = false;
  list_foreach(contexts, [&fsetxattr_failed, &open_failed](context_node* l) {
    if (!l->open(true, &fsetxattr_failed)) {
      open_failed = true;
    }
  });
  if (open_failed || !map_system_property_area(true, &fsetxattr_failed)) {
    free_and_unmap_contexts();
    return -1;
  }
  initialized = true;
  return fsetxattr_failed ? -2 : 0;
}
```

`[2]` 处代码创建了一个名为`/dev/__properties__`的文件夹。

```shell
rk3399:/dev/__properties__ # ls                                            
properties_serial                 
u:object_r:audio_prop:s0          
u:object_r:bluetooth_prop:s0      
...
```

`[3]`处代码调用`initialize_properties_from_file`解析`/system/etc/selinux/plat_property_contexts`文件。

```shell
# /system/etc/selinux/plat_property_contexts 内容如下：
hw.                     u:object_r:system_prop:s0
logd.logpersistd        u:object_r:logpersistd_logging_prop:s0
log.tag                 u:object_r:log_tag_prop:s0
log.tag.WifiHAL         u:object_r:wifi_log_prop:s0
log.                    u:object_r:log_prop:s0
...
```

```c++
static bool initialize_properties_from_file(const char* filename) {
  FILE* file = fopen(filename, "re");
  if (!file) {
    return false;
  }

  char* buffer = nullptr;
  size_t line_len;
  char* prop_prefix = nullptr;
  char* context = nullptr;

  while (getline(&buffer, &line_len, file) > 0) {
    //[1]
    int items = read_spec_entries(buffer, 2, &prop_prefix, &context);
      
    //ctl.* properties 用于IPC机制，不保存到内存文件
    if (!strncmp(prop_prefix, "ctl.", 4)) {
      free(prop_prefix);
      free(context);
      continue;
    }

    auto old_context =
        list_find(contexts, [context](context_node* l) { return !strcmp(l->context(), context); });
    if (old_context) {
      list_add_after_len(&prefixes, prop_prefix, old_context);
    } else {
      //[2]
      list_add(&contexts, context, nullptr);
      list_add_after_len(&prefixes, prop_prefix, contexts);
    }
    free(prop_prefix);
    free(context);
  }

  free(buffer);
  fclose(file);

  return true;
}
```

`[1]`处代码`read_spec_entries`用于解析文件中的一行数据。以`log.                    u:object_r:log_prop:s0`为例，解析后，`prop_prefix = log.` `context=u:object_r:log_prop:s0`。

在第一次添加时，`old_context`返回值肯定是`nullptr`。所以，剩下的重点就是了解`[2]`处的链表的结构了。

```c++
static prefix_node* prefixes = nullptr;
static context_node* contexts = nullptr;
```

`contexts`指向一个环状链表的最新插入的`context_node`。`context_node`内部保存一个`prop_area`结构以及上面从文件中解析出来的`context`字符串。

`prefix_node`结构如下：

```c++
struct prefix_node {
  prefix_node(struct prefix_node* next, const char* prefix, context_node* context)
      : prefix(strdup(prefix)), prefix_len(strlen(prefix)), context(context), next(next) {
  }
  char* prefix;
  const size_t prefix_len;
  context_node* context;
  struct prefix_node* next;
};
```

`prefixes`指向一个环状链表的最新插入的`prefix_node`。并且其`context`成员会指向与之相关联的`context_node`。

```c++
static void list_add_after_len(prefix_node** list, const char* prefix, context_node* context) {
  size_t prefix_len = strlen(prefix);

  auto next_list = list;

  while (*next_list) {
    //prefixs始终指向 prefix_len最大的那个。 *号表示匹配对应默认的context
    if ((*next_list)->prefix_len < prefix_len || (*next_list)->prefix[0] == '*') {
      list_add(next_list, prefix, context);
      return;
    }
    next_list = &(*next_list)->next;
  }
  list_add(next_list, prefix, context);
}

template <typename List, typename... Args>
static inline void list_add(List** list, Args... args) {
  *list = new List(*list, args...);
}
```



![1587109424757](2020-04-15-Android-init-property/1587109424757.png)

大概就是这个样子吧，所有的`prefix_node`构成一个环状链表，所有的`context_node`构成一个环状链表。每个`prefix_node`又会指向与之相应的`context_node`。

> 每一个`context_node`都对应`/dev/__properties__`目录下的一个文件。该文件对应一个`prop_area`结构体。
>
> 所有的属性，都会根据key 和 `prefix_node`的对应关系保存到相应的`context_node`关联的文件内。

到这里，`property_init`就结束了。？？？？黑人问号脸，`/system/build.prop`这些属性文件从哪里加载的呢。老哥。

### 加载prop文件并读入内存

init程序中，我们忽略了很重要的一点，就是解析init.rc的时候。

查看init.rc，我们发现了如下一段和系统属性相关的command。

```shell
on post-fs
    load_system_props
    ...
```

其对应的实现如下:

```c++
// #system/core/init/builtins.cpp
static int do_load_system_props(const std::vector<std::string>& args) {
    load_system_props();
    return 0;
}

// #system/core/init/property_service.cpp
void load_system_props() {
    load_properties_from_file("/system/build.prop", NULL);
    load_properties_from_file("/odm/build.prop", NULL);
    load_properties_from_file("/vendor/build.prop", NULL);
    load_properties_from_file("/factory/factory.prop", "ro.*");
    load_recovery_id_prop();
}
```

`load_properties_from_file`的工作就是打开并读取文件内容，然后调用`load_properties`来解析文件。

`load_properties`的实现也比较简单，逐行解析prop文件，遇到`import`关键字就重新调用`load_properties_from_file`解析导入的文件，过滤注释和空行。根据`=`分割出`key`和`value`并调用`property_set`设置属性。

```c++
// #system/core/init/property_service.cpp
uint32_t property_set(const std::string& name, const std::string& value) {
    if (name == "selinux.restorecon_recursive") {
        return PropertySetAsync(name, value, RestoreconRecursiveAsync);
    }

    return PropertySetImpl(name, value);
}

static uint32_t PropertySetImpl(const std::string& name, const std::string& value) {
    size_t valuelen = value.size();

    //[1]
    prop_info* pi = (prop_info*) __system_property_find(name.c_str());
    if (pi != nullptr) {
        // ro.* properties are actually "write-once".
        if (android::base::StartsWith(name, "ro.")) {
            return PROP_ERROR_READ_ONLY_PROPERTY;
        }

        __system_property_update(pi, value.c_str(), valuelen);
    } else {
        //[2]
        int rc = __system_property_add(name.c_str(), name.size(), value.c_str(), valuelen);
        if (rc < 0) {
            return PROP_ERROR_SET_FAILED;
        }
    }

    // Don't write properties to disk until after we have read all default
    // properties to prevent them from being overwritten by default values.
    if (persistent_properties_loaded && android::base::StartsWith(name, "persist.")) {
        write_persistent_property(name.c_str(), value.c_str());
    }
    property_changed(name, value);
    return PROP_SUCCESS;
}
```



oh，我们第一次导入属性文件，`__system_property_find`的返回值为`nullptr`。所以先看`__system_property_add`：

```c++
// #bionic/libc/bionic/system_properties.cpp
int __system_property_add(const char* name, unsigned int namelen, const char* value,
                          unsigned int valuelen) {
  ...
  prop_area* pa = get_prop_area_for_name(name);
  bool ret = pa->add(name, namelen, value, valuelen);
  return 0;
}
```

#### 如何保存属性

`get_prop_area_for_name`该函数根据`name`在`prefixs`链表中找到匹配的`prefix_node`节点，然后找到其相应的`context_node`。之前提过，每一个`context_node`都对应`/dev/__properties__`目录下的一个文件。其会打开这个文件，并调用`mmap`将映射好的地址强转为`prop_area`并返回。

```c++
static prop_area* get_prop_area_for_name(const char* name) {
  //static prefix_node* prefixes = nullptr;
  auto entry = list_find(prefixes, [name](prefix_node* l) {
    return l->prefix[0] == '*' || !strncmp(l->prefix, name, l->prefix_len);
  });
  auto cnode = entry->context;
  if (!cnode->pa()) {
    //prop_area还没有关联的内存区域，open
    cnode->open(false, nullptr);
  }
  return cnode->pa();
}

```

`conde->open`的主要工作就是通过`map_prop_area_rw`打开名为`/dev/__properties__/context_name`的文件。

```c++
static prop_area* map_prop_area_rw(const char* filename, const char* context,
                                   bool* fsetxattr_failed) {
  const int fd = open(filename, O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC | O_EXCL, 0444);
  if (ftruncate(fd, PA_SIZE) < 0) {
    close(fd);
    return nullptr;
  }

  pa_size = PA_SIZE;
  pa_data_size = pa_size - sizeof(prop_area);

  void* const memory_area = mmap(nullptr, pa_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (memory_area == MAP_FAILED) {
    close(fd);
    return nullptr;
  }
 
  prop_area* pa = new (memory_area) prop_area(PROP_AREA_MAGIC, PROP_AREA_VERSION);

  close(fd);
  return pa;
}
```

此后，所有对`prop_area`的操作都会同步到相应的文件内。

#### 如何查找和添加属性

首先了解`prop_area`是如何管理属性的。

在Android中，属性的结构就是`key=value`这种形式，对于`value`，通常是`bool`、`int`和`string`型吧，这些都可以当做字符串来看待的。对于`key`，其形式基本都是`a.b.c`这种以`.`来间隔的字符串。

根据源码中的注释，以`ro.secure=1`为例，其在`prop_area`存储如下:

```c++
+-----+   children    +----+   children    +--------+
|     |-------------->| ro |-------------->| secure |
+-----+               +----+               +--------+
                      /    \                /   |
                left /      \ right   left /    |  prop   +===========+
                    v        v            v     +-------->| ro.secure |
                 +-----+   +-----+     +-----+            +-----------+
                 | net |   | sys |     | com |            |     1     |
                 +-----+   +-----+     +-----+            +===========+
```

实际代码，用`prop_bt`来表示一个节点。

```c++
// Represents a node in the trie.
struct prop_bt {
  uint32_t namelen; loads from the
  atomic_uint_least32_t prop;

  atomic_uint_least32_t left;
  atomic_uint_least32_t right;

  atomic_uint_least32_t children;
  //不需要记录name的长度？
  char name[0];

  prop_bt(const char* name, const uint32_t name_length) {
    this->namelen = name_length;
    memcpy(this->name, name, name_length);
    this->name[name_length] = '\0';
  }
};

```

由属性构成的树，只会由init进程更新，其他线程只会同时读，为了避免竞态，所以使用了`atomic`。





## 属性服务的启动



## 其他进程如何获取属性信息





































似乎有点牛皮的，进程在使用libc库的时候，就会完成属性相应的内存映射。

```c++
//It is called from arch-$ARCH/bionic/crtbegin_static.S which is directly invoked by the kernel when the program is launched.
__noreturn void __libc_init(void* raw_args,
                            void (*onexit)(void) __unused,
                            int (*slingshot)(int, char**, char**),
                            structors_array_t const * const structors) {
    ...
    __libc_init_common(args);
	...
}

void __libc_init_common(KernelArgumentBlock& args) {
    ...
  __system_properties_init(); // Re
}
```

直言看不懂看不懂啊，反正，根据注释来看就是`__libc_init`会在程序启动的时候被内核直接调用，所以程序启动时就已经完成了属性的初始化操作了。重点就是看看Android如何完成属性的初始化吧。

## Properity Init

```c++
int __system_properties_init() {
  // This is called from __libc_init_common, and should leave errno at 0 (http://b/37248982).
  ErrnoRestorer errno_restorer;

  if (initialized) {
    list_foreach(contexts, [](context_node* l) { l->reset_access(); });
    return 0;
  }
  
  if (is_dir(property_filename)) {
    if (!initialize_properties()) {
      return -1;
    }
    if (!map_system_property_area(false, nullptr)) {
      free_and_unmap_contexts();
      return -1;
    }
  } else {
    //prop_area* __system_property_area__ = nullptr;
    __system_property_area__ = map_prop_area(property_filename);
    if (!__system_property_area__) {
      return -1;
    }
    list_add(&contexts, "legacy_system_prop_area", __system_property_area__);
    list_add_after_len(&prefixes, "*", contexts);
  }
  initialized = true;
  return 0;
}
```

我手上的AndroidO源码，`property_filename == /dev/__properties__`，是个文件夹。那就进入到`map_prop_area`咯。

根据名称，我们猜测其实打开`/dev/__properties__`节点，并完成`mmap`操作。

```c++

```

你看[1] 处的代码，直接将映射的内存地址转成了`prop_area*`，那应该就是在某个地方将`prop_area`写入到了这个文件中?



## 属性是如何保存在内存中的



### `/dev/__properties__`中的数据从哪来的

仔细想想，系统中的大部分属性应该都是从如下属性文件中加载的。

```shell
/system/build.prop
/odm/build.prop
/vendor/build.prop
/factory/factory.prop
```

那么系统时如何加载这些文件的呢？也不买关子了，答案就是通过`Action`。











在init进程中，首先调用`property_init`完成属性的初始化，随后在调用`start_property_service`启动属性服务。

前面那个初始化初始化的什么内容先放一边，先来看看属性服务启动后做了什么。

```c++
void start_property_service() {
    property_set("ro.property_service.version", "2");
	//创建一个uds
    property_set_fd = CreateSocket(PROP_SERVICE_NAME, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
                                   false, 0666, 0, 0, nullptr, sehandle);
	//监听并注册handler
    listen(property_set_fd, 8);
    register_epoll_handler(property_set_fd, handle_property_set_fd);
}

```





## 总结

1. 通过`/system/etc/selinux/plat_property_contexts`文件建立属性名`prefix`和`context`的关联。

    通过属性名的`prefix`找到与`context`关联的`prop_area`。所有`prefix`一致的属性都会保存到这个`prop_area`中。

    









