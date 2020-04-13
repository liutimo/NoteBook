---
title: Android Init -- Action
date: 2020-04-13 10:16:56
categories: Android Framework
tags: 
    - Android Framework
    - Android Init进程
---
前面介绍了android init中init.rc的解析过程，但是很笼统，没有详细介绍`action`、`service`的执行或启动过程，也不知道init.rc 中的参数是如何传递的。

本文就详细介绍`Action`相关的实现。我们知道，action对应于init.rc中`on`开头的section。前面也说过了，和`Action`相关的类有如下几个:

1. `ActionParser`
2. `ActionManager`
3. `Action`
4. `Command`

`ActionParser`已经介绍过了，本文就介绍后三个class。



## Action

首先介绍一下init.rc关于action的语法。

```shell
on boot # boot表示一个trigger条件 
    chmod 0777 /secret/dmb #command
    chmod 0777 /secret/logo
    chmod 0777 /etc/logo
    chmod 0777 /log
```

这段数据，最终就会实例化为一个`Action`对象。

回顾一下`Action`对象的实例化：

```c++
bool ActionParser::ParseSection(std::vector<std::string>&& args, const std::string& filename,
                                int line, std::string* err) {
    std::vector<std::string> triggers(args.begin() + 1, args.end());
    //oneshot 恒为 false
    auto action = std::make_unique<Action>(false, filename, line);
    //trigger对应on boot中的 boot。 
    action->InitTriggers(triggers, err);
    action_ = std::move(action);
    return true;
}
```





`Action`类的定义如下：

```c++
class Action {
  public:
    explicit Action(bool oneshot, const std::string& filename, int line);

    bool AddCommand(const std::vector<std::string>& args, int line, std::string* err);
    void AddCommand(BuiltinFunction f, const std::vector<std::string>& args, int line);
    bool InitTriggers(const std::vector<std::string>& args, std::string* err);
    bool InitSingleTrigger(const std::string& trigger);
    std::size_t NumCommands() const;
    void ExecuteOneCommand(std::size_t command) const;
    void ExecuteAllCommands() const;
    bool CheckEvent(const EventTrigger& event_trigger) const;
    bool CheckEvent(const PropertyChange& property_change) const;
    bool CheckEvent(const BuiltinAction& builtin_action) const;
    std::string BuildTriggersString() const;
    void DumpState() const;

    bool oneshot() const { return oneshot_; }
    const std::string& filename() const { return filename_; }
    int line() const { return line_; }
    static void set_function_map(const KeywordMap<BuiltinFunction>* function_map) {
        function_map_ = function_map;
    }
private:
    void ExecuteCommand(const Command& command) const;
    bool CheckPropertyTriggers(const std::string& name = "",
                               const std::string& value = "") const;
    bool ParsePropertyTrigger(const std::string& trigger, std::string* err);

    std::map<std::string, std::string> property_triggers_;
    std::string event_trigger_;
    std::vector<Command> commands_;
    bool oneshot_;
    std::string filename_;
    int line_;
    static const KeywordMap<BuiltinFunction>* function_map_;
};
```

### function_map_

一个function map，每个`Action`都包含若干条`Command`。android实现了init.rc中所有支持的command，每个实现对应一个`FunctionInfo`:

```c++
//minimum number of arguments, maximum number of arguments, function pointer
using FunctionInfo = std::tuple<std::size_t, std::size_t, Function>;
using Map = std::map<std::string, FunctionInfo>;
```

`Map`的`key`其实就是命令名称咯， 比如`mkdir`。

`KeywordMap`类是一个抽象类，其实现了`FindFunction`的逻辑。我们需要继承该类，并实现其纯虚函数`map`来设置我们自己的函数表。

```c++
template <typename Function>
class KeywordMap {
  public:
    const Function FindFunction(const std::vector<std::string>& args, std::string* err) const {
     	...
        return std::get<Function>(function_info);
    }

  private:
    // Map of keyword ->
    // (minimum number of arguments, maximum number of arguments, function pointer)
    virtual const Map& map() const = 0;
};
```

在`builtins.cpp`中，android完成了init.rc支持的所有shell命令的实现。

```c++
using BuiltinFunction = std::function<int(const std::vector<std::string>&)>;
class BuiltinFunctionMap : public KeywordMap<BuiltinFunction> {
  public:
    BuiltinFunctionMap() {}

  private:
    const Map& map() const override;
};

const BuiltinFunctionMap::Map& BuiltinFunctionMap::map() const {
    constexpr std::size_t kMax = std::numeric_limits<std::size_t>::max();
    // clang-format off
    static const Map builtin_functions = {
        {"bootchart",               {1,     1,    do_bootchart}},
        {"chmod",                   {2,     2,    do_chmod}},
        {"chown",                   {2,     3,    do_chown}},
        {"class_reset",             {1,     1,    do_class_reset}},
        {"class_restart",           {1,     1,    do_class_restart}},
        {"class_start",             {1,     1,    do_class_start}},
        {"class_stop",              {1,     1,    do_class_stop}},
        {"copy",                    {2,     2,    do_copy}},
        {"domainname",              {1,     1,    do_domainname}},
        {"enable",                  {1,     1,    do_enable}},
        {"exec",                    {1,     kMax, do_exec}},
        {"exec_start",              {1,     1,    do_exec_start}},
        {"export",                  {2,     2,    do_export}},
        {"hostname",                {1,     1,    do_hostname}},
        {"ifup",                    {1,     1,    do_ifup}},
        {"init_user0",              {0,     0,    do_init_user0}},
        {"insmod",                  {1,     kMax, do_insmod}},
        {"installkey",              {1,     1,    do_installkey}},
        {"load_persist_props",      {0,     0,    do_load_persist_props}},
        {"load_system_props",       {0,     0,    do_load_system_props}},
        {"loglevel",                {1,     1,    do_loglevel}},
        {"mkdir",                   {1,     4,    do_mkdir}},
        {"mount_all",               {1,     kMax, do_mount_all}},
        {"mount",                   {3,     kMax, do_mount}},
        {"umount",                  {1,     1,    do_umount}},
        {"restart",                 {1,     1,    do_restart}},
        {"restorecon",              {1,     kMax, do_restorecon}},
        {"restorecon_recursive",    {1,     kMax, do_restorecon_recursive}},
        {"rm",                      {1,     1,    do_rm}},
        {"rmdir",                   {1,     1,    do_rmdir}},
        {"setprop",                 {2,     2,    do_setprop}},
        {"setrlimit",               {3,     3,    do_setrlimit}},
        {"start",                   {1,     1,    do_start}},
        {"stop",                    {1,     1,    do_stop}},
        {"swapon_all",              {1,     1,    do_swapon_all}},
        {"symlink",                 {2,     2,    do_symlink}},
        {"sysclktz",                {1,     1,    do_sysclktz}},
        {"trigger",                 {1,     1,    do_trigger}},
        {"verity_load_state",       {0,     0,    do_verity_load_state}},
        {"verity_update_state",     {0,     0,    do_verity_update_state}},
        {"wait",                    {1,     2,    do_wait}},
        {"wait_for_prop",           {2,     2,    do_wait_for_prop}},
        {"write",                   {2,     2,    do_write}},
    };
    // clang-format on
    return builtin_functions;
}
```

至此，`function_map_`是个啥应该就清楚了，其实际就是`builtin_functions`。



### AddCommand

```c++
bool Action::AddCommand(const std::vector<std::string>& args, int line, std::string* err) {
    auto function = function_map_->FindFunction(args, err);
    if (!function) {
        return false;
    }

    AddCommand(function, args, line);
    return true;
}

void Action::AddCommand(BuiltinFunction f, const std::vector<std::string>& args, int line) {
    commands_.emplace_back(f, args, line);
}
```

没啥可介绍的，将command保存到 `commands_`中。



### InitTriggers

其实现代码就不贴了，比较简单，就是区分普通的trigger name 和 属性更改的trigger。后者其实就是常用的：

```shell
on property:sys.boot_completed=1		
```

前者将trigger name保存到`event_trigger_`，后者保存到`property_triggers_`。



### ExecuteCommand

直接调用`Command::InvokeFunc`来执行命令。后面再`Command`一节在介绍。

如果单条命令执行时间超过了50ms，就会以log形式告知用户。

```c++
void Action::ExecuteCommand(const Command& command) const {
    android::base::Timer t;
    int result = command.InvokeFunc();
    auto duration = t.duration();
    // Any action longer than 50ms will be warned to user as slow operation
    if (duration > 50ms || android::base::GetMinimumLogSeverity() <= android::base::DEBUG) {
      ...
    }
}
```



## Command

在init.rc中，定义了一些常用的shell命令，比如 `mkdir`、`chmod`等。

对于这些命令，android使用`Command`类来抽象。在这个类的主要工作就是完成执行相应的cmd。

```c++
class Command {
  public:
    Command(BuiltinFunction f, const std::vector<std::string>& args, int line);

    int InvokeFunc() const;
    std::string BuildCommandString() const;

    int line() const { return line_; }

  private:
    BuiltinFunction func_;
    std::vector<std::string> args_;
    int line_;
};

int Command::InvokeFunc() const {
    std::vector<std::string> expanded_args;
    expanded_args.resize(args_.size());
    expanded_args[0] = args_[0];
    for (std::size_t i = 1; i < args_.size(); ++i) {
        expand_props(args_[i], &expanded_args[i]);
    }
    return func_(expanded_args);
}
```

通过`func_()`来完成cmd的实际执行工作。`func_`由`Action::AddCommand`时设置，根据Command name 查找`function_map_`而来，其是一个函数指针，指向与command相应的执行函数。以`mkdir`为例，其指向`do_mkdir`。



## ActionManager

负责管理`Action`。

在init.cpp中：

```c++
ActionManager& am = ActionManager::GetInstance();
parser.AddSectionParser("service", std::make_unique<ServiceParser>(&sm));
parser.AddSectionParser("on", std::make_unique<ActionParser>(&am));
parser.AddSectionParser("import", std::make_unique<ImportParser>(&parser));
am.QueueEventTrigger("early-init");
am.QueueBuiltinAction(keychord_init_action, "keychord_init");
...
am.ExecuteOneCommand();    
...
    
```

`parser`内部，会将解析出来的`Action`添加到`ActionManager`中。

### QueueEventTrigger

```c++
std::queue<std::variant<EventTrigger, PropertyChange, BuiltinAction>> event_queue_;
void ActionManager::QueueEventTrigger(const std::string& trigger) {
    event_queue_.emplace(trigger);
}
```

### QueueBuiltinAction]

根据代码创建一个`Action`对象，而不是解析init.rc。主要这里`oneshot`标志为true。

```c++

void ActionManager::QueueBuiltinAction(BuiltinFunction func, const std::string& name) {
    auto action = std::make_unique<Action>(true, "<Builtin Action>", 0);
    std::vector<std::string> name_vector{name};

    if (!action->InitSingleTrigger(name)) {
        return;
    }

    action->AddCommand(func, name_vector, 0);

    event_queue_.emplace(action.get());
    actions_.emplace_back(std::move(action));
}
```

### ExecuteOneCommand

