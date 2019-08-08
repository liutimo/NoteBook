
在linux中，`kobject`几乎不会单独出现，它的主要作用就是嵌入在一个大型的数据结构中，为这个数据结构提供一些底层的功能实现。

1. 通过kobject的parent指针将所有kobject以层次结构组合起来
2. 使用引用计数来自动释放内存
3. 与sysfs文件系统结合，开放用户空间接口

## 结构体解析


```c
struct kobject {
	const char		*name;				//kobject的名字，同时对应sysfs中的目录名称。不建议直接修改，调用kobject_rename接口修改。
	struct list_head	entry;			//用于将kobject挂载到kset的list_head中。
	struct kobject		*parent;		//指向parent， 以此形成树状结构
	struct kset		*kset;				//该kobject所属kset，可以为NULL，如果存在，但是parent == NULL，会将kset视为parent。
	struct kobj_type	*ktype;			//
	struct sysfs_dirent	*sd;			//sysfs目录相关
	struct kref		kref;				//对象的引用计数
	unsigned int state_initialized:1;	//初始化标志
	unsigned int state_in_sysfs:1;		//是否在sysfs中挂载
	unsigned int state_add_uevent_sent:1;	//记录是否向用户控件发送 ADD uevent。
	unsigned int state_remove_uevent_sent:1;	
	unsigned int uevent_suppress:1;			//如果为1，忽略所有上报的uevent事件
};


struct kset {
	struct list_head list;	//用于保存该kset下所有的kobject的链表
	spinlock_t list_lock;	
	struct kobject kobj;	//kset是一个特殊的kobject，在sysfs中以目录的形式体现
	const struct kset_uevent_ops *uevent_ops;	//任何一个kobject上报uevent时，都要调用其所属kset的uevent操作函数集中的函数。
};

struct kobj_type {
	void (*release)(struct kobject *kobj);	//用于释放该种kobject的内存空间。
	const struct sysfs_ops *sysfs_ops;	//该种kobject的sysfs的文件系统接口
	struct attribute **default_attrs; //属性列表，所谓attribute，就是sysfs文件系统中的文件，将会在Kobject添加到内核时，一并注册到sysfs中
	const struct kobj_ns_type_operations *(*child_ns_type)(struct kobject *kobj);
	const void *(*namespace)(struct kobject *kobj);
};
```



`kset`表示`kobject`s的集合，集合内的`kobject`都属于同一个subsystem。



## Kobject相关函数



1. `kobject_init`

    初始化一个kobject结构体

    ```c
    void kobject_init(struct kobject *kobj, struct kobj_type *ktype);
    ```

    `kobject_init`会校验参数是否为空，并且判断`kobj`是否已经被初始化。然后会调用

    `kobject_init_internal`函数完成实际的初始化工作，并将`ktype`赋值给`kobj->ktype`。

    ```c
    static void kobject_init_internal(struct kobject *kobj)
    {
    	if (!kobj)
    		return;
        //初始化引用计数
    	kref_init(&kobj->kref);
    	INIT_LIST_HEAD(&kobj->entry);
        //初始化各种状态
    	kobj->state_in_sysfs = 0;
    	kobj->state_add_uevent_sent = 0;
    	kobj->state_remove_uevent_sent = 0;
    	kobj->state_initialized = 1;
    }
    ```

2. `kobject_add`

    将`kobject`添加到内核中

    ```c
    int kobject_add(struct kobject *kobj, struct kobject *parent,
    		const char *fmt, ...);
    ```

    在简单的校验`kobj`的有效性后，调用`kobject_add_varg`来完成实际的`add`操作。

    ```c
    static int kobject_add_varg(struct kobject *kobj, struct kobject *parent,
    			    const char *fmt, va_list vargs);
    ```

    `kobject_add_varg`首先调用`kobject_set_name_vargs`（这个函数内部创建的是堆内存哦！）来设置`kobj->name`属性。随后调用

    `kobject_add_internal`。

    ```c
    static int kobject_add_internal(struct kobject *kobj)
    {
    	int error = 0;
    	struct kobject *parent;
    
    	if (!kobj)
    		return -ENOENT;
    	//name 字段不能为空
    	if (!kobj->name || !kobj->name[0]) {
    		WARN(1, "kobject: (%p): attempted to be registered with empty "
    			 "name!\n", kobj);
    		return -EINVAL;
    	}
    	//增加父节点的引用计数
    	parent = kobject_get(kobj->parent);
    
    	/* join kset if set, use it as parent if we do not already have one */
    	if (kobj->kset) {
    		if (!parent)
    			parent = kobject_get(&kobj->kset->kobj);
    		kobj_kset_join(kobj);
    		kobj->parent = parent;
    	}
    
    	pr_debug("kobject: '%s' (%p): %s: parent: '%s', set: '%s'\n",
    		 kobject_name(kobj), kobj, __func__,
    		 parent ? kobject_name(parent) : "<NULL>",
    		 kobj->kset ? kobject_name(&kobj->kset->kobj) : "<NULL>");
    
    	error = create_dir(kobj);
    	if (error) {
    		kobj_kset_leave(kobj);
    		kobject_put(parent);
    		kobj->parent = NULL;
    
    		/* be noisy on error issues */
    		if (error == -EEXIST)
    			WARN(1, "%s failed for %s with "
    			     "-EEXIST, don't try to register things with "
    			     "the same name in the same directory.\n",
    			     __func__, kobject_name(kobj));
    		else
    			WARN(1, "%s failed for %s (error: %d parent: %s)\n",
    			     __func__, kobject_name(kobj), error,
    			     parent ? kobject_name(parent) : "'none'");
    	} else
    		kobj->state_in_sysfs = 1;
    
    	return error;
    }
    ```

    ​	首先判断`kobj->kset`是否设置，如果设置了，首先将`kobj`加入到该`kset`中，如果其`parent kobject`为空，就将`kobj->kset->kobj`设置为其`parent`。最后，挂载`kobj`在`sysfs`中。

3. `kobject_del`

    从层次体系中删除一个`kobject`，但是并不会释放内存。

    ```c
    void kobject_del(struct kobject *kobj)
    {
    	if (!kobj)
    		return;
    	//从sysfs中移除
    	sysfs_remove_dir(kobj);
    	kobj->state_in_sysfs = 0;	//更改状态	
    	kobj_kset_leave(kobj);	//从kset中移除
    	kobject_put(kobj->parent);	//父节点引用计数减一
    	kobj->parent = NULL;
    }
    ```

> 上面三个函数就是`kobject`相关的主要函数。其余的函数基本都是对这三个函数的调用。



- `kobject_init_and_add`

    ```c
    int kobject_init_and_add(struct kobject *kobj,
    			 struct kobj_type *ktype, struct kobject *parent,
    			 const char *fmt, ...);
    ```

    initialize a kobject structure and add it to the kobject hierarchy.

    需要调用者分配好内存

- `kobject_create` 

    ```c
    struct kobject *kobject_create(void);
    ```

    返回一个已经初始化的`kobject`对象。
    
    ```c
    struct kobject *kobject_create(void)
    {
    	struct kobject *kobj;
    	//分配内存
    	kobj = kzalloc(sizeof(*kobj), GFP_KERNEL);
    	if (!kobj)
    		return NULL;
    	//完成初始化操作。
    	kobject_init(kobj, &dynamic_kobj_ktype);
    	return kobj;
    }
    
    static void dynamic_kobj_release(struct kobject *kobj)
    {
        //只是简单的释放内存
    	pr_debug("kobject: (%p): %s\n", kobj, __func__);
    	kfree(kobj);
    }
    
    static struct kobj_type dynamic_kobj_ktype = {
    	.release	= dynamic_kobj_release,
    	.sysfs_ops	= &kobj_sysfs_ops,	//完成最终对kobject在sysfs中节点的访问。
    };
    ```
    
    
    
- `kobject_create_and_add`

    ```c
    struct kobject *kobject_create_and_add(const char *name, struct kobject *parent);
    ```

    create a struct kobject dynamically and register it with sysfs.

- `kobject_get`

    增加`kobject`的引用计数。

    ```c
    struct kobject *kobject_get(struct kobject *kobj)
    {
    	if (kobj)
    		kref_get(&kobj->kref);
    	return kobj;
    }
    ```

- `kobject_put`

    ```c
    void kobject_put(struct kobject *kobj)
    {
    	if (kobj) {
    		if (!kobj->state_initialized)
    		kref_put(&kobj->kref, kobject_release);
    	}
    }
    
    static void kobject_release(struct kref *kref)
    {
    	kobject_cleanup(container_of(kref, struct kobject, kref));
    }
    
    static void kobject_cleanup(struct kobject *kobj)
    {
    	struct kobj_type *t = get_ktype(kobj);
    	const char *name = kobj->name;
    
    	pr_debug("kobject: '%s' (%p): %s\n",
    		 kobject_name(kobj), kobj, __func__);
    
    	if (t && !t->release)
    		pr_debug("kobject: '%s' (%p): does not have a release() "
    			 "function, it is broken and must be fixed.\n",
    			 kobject_name(kobj), kobj);
    
    	/* send "remove" if the caller did not do it but sent "add" */
    	if (kobj->state_add_uevent_sent && !kobj->state_remove_uevent_sent) {
    		pr_debug("kobject: '%s' (%p): auto cleanup 'remove' event\n",
    			 kobject_name(kobj), kobj);
    		kobject_uevent(kobj, KOBJ_REMOVE);
    	}
    
    	/* remove from sysfs if the caller did not do it */
    	if (kobj->state_in_sysfs) {
    		pr_debug("kobject: '%s' (%p): auto cleanup kobject_del\n",
    			 kobject_name(kobj), kobj);
    		kobject_del(kobj);
    	}
    
    	if (t && t->release) {
    		pr_debug("kobject: '%s' (%p): calling ktype release\n",
    			 kobject_name(kobj), kobj);
    		t->release(kobj); //释放内存
    	}
    
    	/* free name if we allocated it */
    	if (name) { 
            //kobject_set_name_vargs创建的是堆内存，需要手动释放
    		pr_debug("kobject: '%s': free name\n", name);
    		kfree(name);
    	}
    }
    ```

    当`kobj->kref`的值为0时，就会调用`kobject_release`，后者最终调用`kobject_cleanup`来完成清理工作。

    

    

## 实例

驱动安装后，会在`/sys/kernel`目录下生成`kobject_example`目录。目录下有`foo`、`baz`和`bar`三个文件。我们可以通过`cat`和`echo n > *`来操作相关文件。

**源文件**

```c
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>


static int foo;
static int baz;
static int bar;

/*
 * The "foo" file where a static variable is read from and written to.
 */
static ssize_t foo_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%d\n", foo);
}

static ssize_t foo_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	sscanf(buf, "%du", &foo);
	return count;
}

static struct kobj_attribute foo_attribute =
	__ATTR(foo, 0666, foo_show, foo_store);

static ssize_t b_show(struct kobject *kobj, struct kobj_attribute *attr,
		      char *buf)
{
	int var;

	if (strcmp(attr->attr.name, "baz") == 0)
		var = baz;
	else
		var = bar;
	return sprintf(buf, "%d\n", var);
}

static ssize_t b_store(struct kobject *kobj, struct kobj_attribute *attr,
		       const char *buf, size_t count)
{
	int var;

	sscanf(buf, "%du", &var);
	if (strcmp(attr->attr.name, "baz") == 0)
		baz = var;
	else
		bar = var;
	return count;
}

static struct kobj_attribute baz_attribute =
	__ATTR(baz, 0666, b_show, b_store);
static struct kobj_attribute bar_attribute =
	__ATTR(bar, 0666, b_show, b_store);

static struct attribute *attrs[] = {
	&foo_attribute.attr,
	&baz_attribute.attr,
	&bar_attribute.attr,
	NULL,	/* need to NULL terminate the list of attributes */
};


static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *example_kobj;

static int __init example_init(void)
{
	int retval;

	//指定其parent 为 kernel_kobj
	example_kobj = kobject_create_and_add("kobject_example", kernel_kobj);
	if (!example_kobj)
		return -ENOMEM;

	//创建和example_kobj相关的属性文件
	retval = sysfs_create_group(example_kobj相关的属性文件, &attr_group);
	if (retval)
		kobject_put(example_kobj);

	return retval;
}

static void __exit example_exit(void)
{
	kobject_put(example_kobj);
}

module_init(example_init);
module_exit(example_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Greg Kroah-Hartman <greg@kroah.com>");
```

**Makefile**

```makefile
ifneq ($(KERNELRELEASE),)
	obj-m := kobject.o
else
KERNEL_DIR = ... #指定内核路径
FLAGS = ARCH=arm CORSS_COMPILE=arm-eabi #指定编译平台，我这里是arm
.PHONY:clean

all: 
	make -C $(KERNEL_DIR) M=$(PWD) $(FLAGS)  modules

clean:
	rm -rf *.mod.* *.o *.ko *.order *.builtin *.symvers .*.*.cmd .tmp_versions .proc.*
endif

```

