# platform

## What

**什么是platform bus**

有这样一类设备：他们拥有各自的Device Controller和CPU直接相连，CPU通过普通的寻址操作就可以访问它们。这种连接方式，并不属于传统意义上的总线连接。但是设备模型应该具备普适性。所以linux虚拟处了platform bus，让这些设备挂载。



## 相关数据结构

1. platform_device

```c
//平台设备
struct platform_device {
	const char	*name;
	int		id;
	bool		id_auto;
	struct device	dev;
    //资源数目， num_irq + num_reg
	u32		num_resources;
    //资源数组，看源码主要是mem资源和irq
	struct resource	*resource;

	const struct platform_device_id	*id_entry;

	/* MFD cell pointer */
	struct mfd_cell *mfd_cell;

	/* arch specific additions */
	struct pdev_archdata	archdata;
};
```



