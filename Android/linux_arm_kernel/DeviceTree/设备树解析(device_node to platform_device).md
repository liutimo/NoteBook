# 设备树解析

## DTB转换成`device_node`

`arch/arm/kernel/setup.c`

```c
void __init setup_arch(char **cmdline_p)
{
	struct machine_desc *mdesc;

	setup_processor();
	//找到合适的machine_desc
	mdesc = setup_machine_fdt(__atags_pointer);
	if (!mdesc)
		//解析ATAGS
		mdesc = setup_machine_tags(__atags_pointer, __machine_arch_type);
	...
	//[1] 开始解析设备树
	unflatten_device_tree();
	...
}
```

[1] 将DTB转换成device_nodes树

```c
/**
 * unflatten_device_tree - create tree of device_nodes from flat blob
 *
 * unflattens the device-tree passed by the firmware, creating the
 * tree of struct device_node. It also fills the "name" and "type"
 * pointers of the nodes so the normal device-tree walking functions
 * can be used.
 */
void __init unflatten_device_tree(void)
{
	__unflatten_device_tree(initial_boot_params, &of_allnodes,
				early_init_dt_alloc_memory_arch);

	/* Get pointer to "/chosen" and "/aliasas" nodes for use everywhere */
	of_alias_scan(early_init_dt_alloc_memory_arch);
}
```

​	`__unflatten_device_tree`内部使用`unflatten_dt_node`来解析设备树，总共调用两次，第一次用来获取设备节点数，第二次才是完整解析设备树，并将所有节点按照层级关系放入根节点为`of_allnodes`的树结构中。

```c
struct device_node {
	const char *name;	
	const char *type;
	phandle phandle;	
	const char *full_name;	//从/开始的全路径

	struct	property *properties;	//属性列表
	struct	property *deadprops;	/* removed properties */
	struct	device_node *parent;	
	struct	device_node *child;
	struct	device_node *sibling;
	struct	device_node *next;	/* next device of same type */
	struct	device_node *allnext;	/* next in list of all nodes */
	struct	kobject kobj;
	unsigned long _flags;
	void	*data;
};
struct device_node *of_allnodes;
```

`unflatten_dt_node`的解析过程好麻烦的感觉，看不懂哈哈哈，先不分析。



## device_nodes 转换成 platform_device

**函数调用栈

`start_kernel` ==> `rest_init` ==> `kernel_init` ==> `kernel_init_freeable` ==> `do_basic_setup` ==> `do_initcalls`



`do_initcalls`最终会调用函数`customize_machine`:

```c
static int __init customize_machine(void)
{
	/*
	 * customizes platform devices, or adds new ones
	 * On DT based machines, we fall back to populating the
	 * machine from the device tree, if no callback is provided,
	 * otherwise we would always need an init_machine callback.
	 */
	if (machine_desc->init_machine)
		machine_desc->init_machine();
#ifdef CONFIG_OF
	else
		of_platform_populate(NULL, of_default_bus_match_table,
					NULL, NULL);
#endif
	return 0;
}
arch_initcall(customize_machine);
```

首先先分析一下`arch_initcall`

```c
#define arch_initcall(fn)		__define_initcall(fn, 3)

/* initcalls are now grouped by functionality into separate 
 * subsections. Ordering inside the subsections is determined
 * by link order. 
 * For backwards compatibility, initcall() puts the call in 
 * the device init subsection.
 *
 * The `id' arg to __define_initcall() is needed so that multiple initcalls
 * can point at the same handler without causing duplicate-symbol build errors.
 */
#define __define_initcall(fn, id) \
	static initcall_t __initcall_##fn##id __used \
	__attribute__((__section__(".initcall" #id ".init"))) = fn
```

由代码可知，函数`customize_machine`会被放入到名为`.initcall3.init`的section中。

回过头看`do_initcalls`：
```c
static initcall_t *initcall_levels[] __initdata = {
	__initcall0_start,
	__initcall1_start,
	__initcall2_start,
	__initcall3_start, //customize_machine 就在这个段哦！
	__initcall4_start,
	__initcall5_start,
	__initcall6_start,  
	__initcall7_start,
	__initcall_end,
};

static void __init do_initcalls(void)
{
	int level;

	for (level = 0; level < ARRAY_SIZE(initcall_levels) - 1; level++)
		do_initcall_level(level); //当level为3时，就会调用到customize_machine
}

static void __init do_initcall_level(int level)
{
	initcall_t *fn;
    //调用__initcall3_start段中所有的init函数，customize_machine也会被调用。
	for (fn = initcall_levels[level]; fn < initcall_levels[level+1]; fn++)
		do_one_initcall(*fn);
}
```

在执行`customize_machine`时，由于rk3288的arch代码中，并没有定义`init_machine`,所以会直接调用内核提供的`of_platform_populate`来完成`device_node`向`platform_device`的转换。

```c
int of_platform_populate(struct device_node *root,
			const struct of_device_id *matches,
			const struct of_dev_auxdata *lookup,
			struct device *parent)
{
	struct device_node *child;
	int rc = 0;

	//[1] 获取根节点。
	root = root ? of_node_get(root) : of_find_node_by_path("/");
	if (!root)
		return -EINVAL;

	//遍历根节点的所有子节点并创建platform_device
	for_each_child_of_node(root, child) {
		//[2]
         rc = of_platform_bus_create(child, matches, lookup, parent, true);
		if (rc)
			break;
	}
	//[1]出增加了device_node的引用计数。
	of_node_put(root);
	return rc;
}
```

[2] 处代码`of_platform_bus_create`完成实际的创建`platform_device`的工作。其内部又通过

`of_platform_device_create_pdata`来完成`platform_device`的创建。

```c
/**
 * of_platform_device_create_pdata - Alloc, initialize and register an of_device
 * @np: pointer to node to create device for
 * @bus_id: name to assign device
 * @platform_data: pointer to populate platform_data pointer with
 * @parent: Linux device model parent device.
 *
 * Returns pointer to created platform device, or NULL if a device was not
 * registered.  Unavailable devices will not get registered.
 */
struct platform_device *of_platform_device_create_pdata(
					struct device_node *np,
					const char *bus_id,
					void *platform_data,
					struct device *parent)
{
	struct platform_device *dev;

	
	//[1] OF_POPULATED device already created for the node
	if (!of_device_is_available(np) ||
	    of_node_test_and_set_flag(np, OF_POPULATED))
		return NULL;

	//[2] 申请内存并初始化 dev，计算出resource
	dev = of_device_alloc(np, bus_id, parent);
	if (!dev)
		goto err_clear_flag;

#if defined(CONFIG_MICROBLAZE)
	dev->archdata.dma_mask = 0xffffffffUL;
#endif
	dev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	dev->dev.bus = &platform_bus_type;
	dev->dev.platform_data = platform_data;

	/* We do not fill the DMA ops for platform devices by default.
	 * This is currently the responsibility of the platform code
	 * to do such, possibly using a device notifier
	 */

	if (of_device_add(dev) != 0) {
		platform_device_put(dev);
		goto err_clear_flag;
	}

	return dev;

err_clear_flag:
	of_node_clear_flag(np, OF_POPULATED);
	return NULL;
}
```

[1]处代码。根据设备树节点的status属性来判断是否需要注册为`platform_device`。

[2]处代码完成了`platform_device`的创建。

```c
struct platform_device *of_device_alloc(struct device_node *np,
				  const char *bus_id,
				  struct device *parent)
{
	struct platform_device *dev;
	int rc, i, num_reg = 0, num_irq;
	struct resource *res, temp_res;

	//创建并初始化dev
	dev = platform_device_alloc("", -1);
	if (!dev)
		return NULL;

	/* count the io and irq resources */
	//判断设备是否能够翻译地址。根据#address-cells和#size-cells属性来判断
	if (of_can_translate_address(np)) 
		//[1]
         while (of_address_to_resource(np, num_reg, &temp_res) == 0)
			//计算address数量
			num_reg++;
	//计算irq的数量
	num_irq = of_irq_count(np);

	/* Populate the resource table */
	if (num_irq || num_reg) {
		res = kzalloc(sizeof(*res) * (num_irq + num_reg), GFP_KERNEL);
		if (!res) {
			platform_device_put(dev);
			return NULL;
		}
		//设置resources
		dev->num_resources = num_reg + num_irq;
		dev->resource = res;
		for (i = 0; i < num_reg; i++, res++) {
			rc = of_address_to_resource(np, i, res);
			WARN_ON(rc);
		}
		WARN_ON(of_irq_to_resource_table(np, res, num_irq) != num_irq);
	}

	dev->dev.of_node = of_node_get(np);
#if defined(CONFIG_MICROBLAZE)
	dev->dev.dma_mask = &dev->archdata.dma_mask;
#endif
	dev->dev.parent = parent;

	if (bus_id)
		dev_set_name(&dev->dev, "%s", bus_id);
	else
		of_device_make_bus_id(&dev->dev);

	return dev;
}
```

`of_address_to_resource`将设备树中的地址翻译成CPU物理地址。并通过`resource`返回。

```c
struct resource {
	resource_size_t start;
	resource_size_t end;
	const char *name;
	unsigned long flags;
	struct resource *parent, *sibling, *child;
};
```

