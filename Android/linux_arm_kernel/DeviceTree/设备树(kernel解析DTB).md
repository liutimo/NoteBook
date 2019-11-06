ARM平台，内核的入口点支持两种调用约束：

1. ATAGS Interface

   信息通过一个带有标记的预定义参数列表从固件传递到内核。

     r0 : 0
     r1 : Machine type number
     r2 : Physical address of tagged list in system RAM

2. Device-Tree Block

   firmware将DTB的物理地址加载到寄存器r2.

   r0 : 0
   r1 : Valid machine type number.  When using a device tree, a single machine type number will often be assigned to
           epresent a class or family of SoCs.
    r2 : physical pointer to the device-tree block (defined in chapter II) in RAM.  Device tree can be located anywhere         in system RAM, but it should be aligned on a 64 bit boundary.

内核通过读取r2的值来区分ATAGS以及DTB的启动方式。

the flattened device tree block magic value (0xd00dfeed) 

the ATAG_CORE value at offset 0x4 from r2 (0x54410001).





`arch/arm/mach-rockchip/rk3288.c`

```c
DT_MACHINE_START(RK3288_DT, "Rockchip RK3288 (Flattened Device Tree)")
    .smp        = smp_ops(rockchip_smp_ops),
    .map_io        = rk3288_dt_map_io,
    .init_time    = rk3288_dt_init_timer,
    .dt_compat    = rk3288_dt_compat,
    .init_late    = rk3288_init_late,
    .reserve    = rk3288_reserve,
    .restart    = rk3288_restart,
MACHINE_END

//
#define MACHINE_END                \
};

#define DT_MACHINE_START(_name, _namestr)        \
static const struct machine_desc __mach_desc_##_name    \
 __used                            \
 __attribute__((__section__(".arch.info.init"))) = {    \
    .nr        = ~0,                \
    .name        = _namestr,

#endif
```

该代码片段展开如下,即定义了一个名为__mach_desc_RK3288_DT的machine_desc结构体"对象"。该对象存放在名为".arch.info.init"的数据段中。

```c
static const struct machine_desc __mach_desc_RK3288_DT    
 __used                            
 __attribute__((__section__(".arch.info.init"))) = {    
    .nr        = ~0,                
    .name        = "Rockchip RK3288 (Flattened Device Tree)",
    .smp        = smp_ops(rockchip_smp_ops),
    .map_io        = rk3288_dt_map_io,
    .init_time    = rk3288_dt_init_timer,
    .dt_compat    = rk3288_dt_compat,
    .init_late    = rk3288_init_late,
    .reserve    = rk3288_reserve,
    .restart    = rk3288_restart,
};
```



`arch/arm/kernel/vmlinux.lds`：

```c
 .init.arch.info : {
  __arch_info_begin = .;
  *(.arch.info.init)
  __arch_info_end = .;
 }
```

`.arch.info.init`位于`__arch_info_begin`和`__arch_info_end`中间。所以我们可以通过这个两个标签来遍历所有位于`.arch.info.init`段中的`machine_desc`结构。



`init/main.c ==> start_kernel ==>setup_arch ==>setup_machine_fdt`

```c
/**
 * setup_machine_fdt - Machine setup when an dtb was passed to the kernel
 * @dt_phys: physical address of dt blob
 *
 * If a dtb was passed to the kernel in r2, then use it to choose the
 * correct machine_desc and to setup the system.
 */
//dt_physu-boot传入的dtb地址
struct machine_desc * __init setup_machine_fdt(unsigned int dt_phys)  //0x560000000
{
    struct boot_param_header *devtree;
    struct machine_desc *mdesc, *mdesc_best = NULL;
    unsigned int score, mdesc_score = ~1;
    unsigned long dt_root;
    const char *model;
    int i = 0;

#ifdef CONFIG_ARCH_MULTIPLATFORM
    DT_MACHINE_START(GENERIC_DT, "Generic DT based system")
    MACHINE_END

    mdesc_best = (struct machine_desc *)&__mach_desc_GENERIC_DT;
    
#endif

    if (!dt_phys)
        return NULL;
    //dtb的头信息。见struct boot_param_header 的定义。
    devtree = phys_to_virt(dt_phys);

    /* check device tree validity */
    //校验是不是合法的DTB 文件      
    if (be32_to_cpu(devtree->magic) != OF_DT_HEADER)
        return NULL;

    /* Search the mdescs for the 'best' compatible value match */
    initial_boot_params = devtree;
    //获取DTB中的根节点，返回值恒为0
    dt_root = of_get_flat_dt_root();

    //[1] 找到最合适的dtb
    for_each_machine_desc(mdesc) {
        score = of_flat_dt_match(dt_root, mdesc->dt_compat);
        if (score > 0 && score < mdesc_score) {
            mdesc_best = mdesc;
            mdesc_score = score;
        }
    }
    if (!mdesc_best) {
        const char *prop;
        int size;

        early_print("\nError: unrecognized/unsupported "
                "device tree compatible list:\n[ ");

        prop = of_get_flat_dt_prop(dt_root, "compatible", &size);
        while (size > 0) {
            early_print("'%s' ", prop);
            size -= strlen(prop) + 1;
            prop += strlen(prop) + 1;
        }
        early_print("]\n\n");

        dump_machine_table(); /* does not return */
    }
    //查找model属性
    model = of_get_flat_dt_prop(dt_root, "model", NULL);
    if (!model) {
        //3288的dts中，没有model属性，使用compatible属性的值替换
        model = of_get_flat_dt_prop(dt_root, "compatible属性的值替换", NULL);
    }
    
    if (!model)
        model = "<unknown>";
    pr_info("Machine: %s, model: %s\n", mdesc_best->name, model);

    //[2] /* Retrieve various information from the /chosen node */
    of_scan_flat_dt(early_init_dt_scan_chosen, boot_command_line);
    /* Initialize {size,address}-cells info */
    //[3] 初始化 根节点的 #size-cells 和 #address-cells
    of_scan_flat_dt(early_init_dt_scan_root, NULL);
    //[4] /* Setup memory, calling early_init_dt_add_memory_arch */
    of_scan_flat_dt(early_init_dt_scan_memory, NULL);

    //machine number仅对ATAGS有效，对于FDT而言，恒为-1（~0）,
    __machine_arch_type = mdesc_best->nr;
    return mdesc_best;
}
```

**[1]**该处代码，用于在`.arch.info.init`数据段中寻找(通过比对` mdesc->dt_compa`t)和传入dtb相符合的`machine_desc`：

```c
#define for_each_machine_desc(p)            \
    for (p = __arch_info_begin; p < __arch_info_end; p++)
```

```c
int of_fdt_is_compatible(struct boot_param_header *blob /*dtb的地址*/,
              unsigned long node, const char *compat)
{
    const char *cp;
    int cplen;
    unsigned long l, score = 0;

    //查找dts中的的compatible属性
    cp = fdt_getprop(blob, node, "compatible", &cplen);
    if (cp == NULL)
        return 0;
    while (cplen > 0) {
        score++;
        //简单的比较字符串，返回的score越小，说明匹配度越高
        if (of_compat_cmp(cp, compat, strlen(compat)) == 0)
            return score;
        //cp其实是一个字符串数组，跳转到下一个字符串的开始处
        l = strlen(cp) + 1;
        cp += l;
        cplen -= l;
    }
    return 0;
}
```

[2] `early_init_dt_scan_chosen` 从chosen中读取initrd以及bootargs。在设备树的DTS文件中， chosen节点通常是一个空节点，不代表实际硬件，其作用主要就是用来向内核传递参数。比如bootargs以及initrd都是有u-boot设置到fdt中的。

```c
int __init early_init_dt_scan_chosen(unsigned long node, const char *uname,
                     int depth, void *data)
{
    int l;
    const char *p;
    char *cmdline = data;

    pr_debug("search \"chosen\", depth: %d, uname: %s\n", depth, uname);

    //chosen 位于根节点下，所以depth只能是 1。
    if (depth != 1 || !cmdline ||
        (strcmp(uname, "chosen") != 0 && strcmp(uname, "chosen@0") != 0))
        return 0;
    //initrd-{start, end}由u-boot代码设置。
    early_init_dt_check_for_initrd(node);

    /* Put CONFIG_CMDLINE in if forced or if data had nothing in it to start */
    if (overwrite_incoming_cmdline || !cmdline[0])
        strlcpy(cmdline, config_cmdline, COMMAND_LINE_SIZE);
    /* Retrieve command line unless forcing */
    if (read_dt_cmdline)
        p = of_get_flat_dt_prop(node, "bootargs", &l);
    
    if (p != NULL && l > 0) {
        if (concat_cmdline) {
            int cmdline_len;
            int copy_len;
            strlcat(cmdline, " ", COMMAND_LINE_SIZE);
            cmdline_len = strlen(cmdline);
            copy_len = COMMAND_LINE_SIZE - cmdline_len - 1;
            copy_len = min((int)l, copy_len);
            strncpy(cmdline + cmdline_len, p, copy_len);
            cmdline[cmdline_len + copy_len] = '\0';
        } else {
            strlcpy(cmdline, p, min((int)l, COMMAND_LINE_SIZE));
        }
    }
    return 1;
}

void __init early_init_dt_check_for_initrd(unsigned long node)
{
    u64 start, end;
    int len;
    const __be32 *prop;
    prop = of_get_flat_dt_prop(node, "linux,initrd-start", &len);
    if (!prop) {
        return;
    }
    start = of_read_number(prop, len/4);
    prop = of_get_flat_dt_prop(node, "linux,initrd-end", &len);
    if (!prop) {
        return;
    }
    end = of_read_number(prop, len/4);
    //early_init_dt_setup_initrd_arch
    //phys_initrd_start = start;
    // phys_initrd_size = end - start;
    early_init_dt_setup_initrd_arch(start, end);
}
```

[3] 处代码读取根节点下的`#address-cells`和`#size-cells`属性，并保存到相应的全局变量中

```c
int __init early_init_dt_scan_root(unsigned long node, const char *uname,
                   int depth, void *data)
{
    const __be32 *prop;

    if (depth != 0)
        return 0;

    dt_root_size_cells = OF_ROOT_NODE_SIZE_CELLS_DEFAULT;
    dt_root_addr_cells = OF_ROOT_NODE_ADDR_CELLS_DEFAULT;

    prop = of_get_flat_dt_prop(node, "#size-cells", NULL);
    if (prop)
        dt_root_size_cells = be32_to_cpup(prop);

    prop = of_get_flat_dt_prop(node, "#address-cells", NULL);
    if (prop)
        dt_root_addr_cells = be32_to_cpup(prop);

    return 1;
}
```

[4] 对内存进行初始化。有点懵逼，并没有在dts中找到内存相关的节点。

本来想以3288的dts来看这段代码的，可惜啊，3288 dts中并没有内存的配置信息。所以先看一下其他平台的。

```dtd
/ {
    #address-cells = <1>;
     #size-cells = <1>;
    memory@80000000 {
        device_type = "memory";
        reg = <0x80000000 0x40000000>;
    };
}
```

```c
int __init early_init_dt_scan_memory(unsigned long node, const char *uname,
                     int depth, void *data)
{
    const char *type = of_get_flat_dt_prop(node, "device_type", NULL);
    const __be32 *reg, *endp;
    int l;
    
    if (type == NULL) {
        if (depth != 1 || strcmp(uname, "memory@0") != 0)
            return 0;
    } else if (strcmp(type, "memory") != 0) {
        return 0;
    }

    reg = of_get_flat_dt_prop(node, "linux,usable-memory", &l);
    if (reg == NULL)
        reg = of_get_flat_dt_prop(node, "reg", &l);
    if (reg == NULL)
        return 0;

    endp = reg + (l / sizeof(__be32));

    //计算内存基址以及内存大小，可能存在多块内存。
    while ((endp - reg) >= (dt_root_addr_cells + dt_root_size_cells)) {
        u64 base, size;
        base = dt_mem_next_cell(dt_root_addr_cells, &reg);
        size = dt_mem_next_cell(dt_root_size_cells, &reg);

        if (size == 0)
            continue;
        //将当前内存块加入到内核。
        early_init_dt_add_memory_arch(base, size);
    }
    return 0;
}
```