
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

    `kobject_add_varg`首先调用`kobject_set_name_vargs`来设置`kobj->name`属性。随后调用

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
    
    ``
    
    