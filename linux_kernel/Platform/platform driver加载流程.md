不仅仅是platform bus上的设备，对于所有驱动，内核都是先使用bus上的probe函数来挂载驱动，如果bus上的probe函数没有实现，才会使用driver上的probe函数。

```c
static int really_probe(struct device *dev, struct device_driver *drv)
{
	...
   	//优先总线上的probe
	if (dev->bus->probe) {
		ret = dev->bus->probe(dev);
		if (ret)
			goto probe_failed;
	} else if (drv->probe) 
		ret = drv->probe(dev);
		if (ret)
			goto probe_failed;
	}
	...
}
```

