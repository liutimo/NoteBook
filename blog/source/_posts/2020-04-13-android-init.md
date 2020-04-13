---
title: android init
date: 2020-04-13 08:40:19
categories: Android Framework
tags: 
    - Android Framework
    - Android Init进程
---

阅读《深入理解Android 卷一》所做的笔记。
实际阅读源码为Android O。

1. Init进程如何解析init.rc 
2. 属性系统初始化
3. init进入loop后，如何处理其他进程的事件



## 解析init.rc

在`init.cpp`中，`init.rc`的解析代码如下：

```c++
Parser& parser = Parser::GetInstance();
parser.AddSectionParser("service", std::make_unique<ServiceParser>(&sm));
parser.AddSectionParser("on", std::make_unique<ActionParser>(&am));
parser.AddSectionParser("import", std::make_unique<ImportParser>(&parser));
std::string bootscript = GetProperty("ro.boot.init_rc", "");
parser.ParseConfig("/init.rc");
```

`Parser`解析`init.rc`文件，并提取相应的Section，通过`SectionParser`来完成各种类型的Section解析。

### Parser

`Parser`的工作主要是读取`init.rc`文件，并调用相应的`SectionParser`来处理section。其核心函数就是`Parser::ParseData`，在进入该函数前，还有另一个函数`next_token`需要了解，这个函数才是完成实际的文本解析工作。这里主要介绍一下它的返回值及意义。

| 返回值      | 意义                                                         |
| ----------- | ------------------------------------------------------------ |
| `T_EOF`     | 表示一个Seciton结束                                          |
| `T_TEXT`    | 当匹配到' ' '\t' '\r' '\n' 0时，都会返回该值。但是'\n'会额外返回一个T_NEWLINE。 |
| `T_NEWLINE` | 匹配到\n，表示新的一行。返回该值后，`Parse`就会处理该行。    |

匹配到`\n`时返回`T_TEXT`的处理：

```c++
int next_token(struct parse_state *state)
{
    char *x = state->ptr;
    char *s;
	//[2]
    if (state->nexttoken) {
        int t = state->nexttoken;
        state->nexttoken = 0;
        return t;
    }
    ...
textdone:
    state->ptr = x;
    *s = 0;
    return T_TEXT;  
    text:
    state->text = s = x;
textresume:
    for (;;) {
        switch (*x) {
        case 0:
            goto textdone;
        case ' ':
        case '\t':
        case '\r':
            x++;
            goto textdone;
        case '\n':
            //[1]
            state->nexttoken = T_NEWLINE;
            x++;
            goto textdone;
        }
        ....
    }
	return T_EOF;
}
```

注意[1] [2]处的代码。[1]代码返回后，下次进入`next_token`时，会直接返回`T_NEWLINE`。

在了解了这些返回值后，在来看`ParseData`就要轻松得多。

```c++

void Parser::ParseData(const std::string& filename, const std::string& data) {
    //TODO: Use a parser with const input and remove this copy
    std::vector<char> data_copy(data.begin(), data.end());
    data_copy.push_back('\0');

    parse_state state;
    state.line = 0;
    state.ptr = &data_copy[0];
    state.nexttoken = 0;

    SectionParser* section_parser = nullptr;
    std::vector<std::string> args;

    for (;;) {
        //调用next_token解析数据
        switch (next_token(&state)) {
        //一个Section结束。调用其EndSection函数。
        case T_EOF:
            if (section_parser) {
                section_parser->EndSection();
            }
            return;
        //一行结束，处理改行
        case T_NEWLINE:
            state.line++;
            if (args.empty()) {
                //空白行
                break;
            }
            // If we have a line matching a prefix we recognize, call its callback and unset any
            // current section parsers.  This is meant for /sys/ and /dev/ line entries for uevent.
            //搜索了代码，只有ueventd.cpp中用到了这个 SingleLineParser。那就先ignore吧
            for (const auto& [prefix, callback] : line_callbacks_) {
                if (android::base::StartsWith(args[0], prefix.c_str())) {
                    if (section_parser) section_parser->EndSection();

                    std::string ret_err;
                    if (!callback(std::move(args), &ret_err)) {
                    }
                    section_parser = nullptr;
                    break;
                }
            }
            //[1]
            if (section_parsers_.count(args[0])) {
                if (section_parser) {
                    section_parser->EndSection();
                }
                section_parser = section_parsers_[args[0]].get();
                std::string ret_err;
                if (!section_parser->ParseSection(std::move(args), filename, state.line, &ret_err)) {
                    section_parser = nullptr;
                }
            } else if (section_parser) {
                std::string ret_err;
                if (!section_parser->ParseLineSection(std::move(args), state.line, &ret_err)) {
                }
            }
            args.clear();
            break;
        //以 on boot 为例， args的值就是 {"on", "boot"}
        case T_TEXT:
            args.emplace_back(state.text);
            break;
        }
    }
}
```

[1] 出的代码，以如下section为例:

```shell
on boot                        	  #Action
    chmod 0777 /secret/dmb	 	  #Command
    chmod 0777 /secret/logo
    chmod 0777 /etc/logo
    chmod 0777 /log
```

首先调用`section_parser->ParseSection`机械`on boot`。然后调用`section_parser->ParseLineSection`解析剩余的行。

回到init.cpp中的相关代码，在解析前，我们看到了三个`SectionParser`,分别对应:

- on
- service
- import

那下面逐个介绍他们的是实现。

### SectionParser

前面介绍的只是解析init.rc文件，接下来的是具体到行的解析了。需要区分init.rc的每一行的具体意义。代码比较多。尽可能精简的描述吧。

#### 接口类

```C++
class SectionParser {
  public:
    virtual ~SectionParser() {}
    //解析section中的第一句。 例如 on boot
    virtual bool ParseSection(std::vector<std::string>&& args, const std::string& filename,
                              int line, std::string* err) = 0;
    //解析section中剩余的部分行
    virtual bool ParseLineSection(std::vector<std::string>&&, int, std::string*) { return true; };
    //section结束时被调用。 T_EOF
    virtual void EndSection(){};
    //文件结束时调用。
    virtual void EndFile(){};
};
```



####  on

对应的`SectionParser`为`ActionParser`。

在`action.h`中声明。包括: `ActionParser`、`ActionManager`、`Action`、`Command`。本文只介绍`ActionParser`的实现，而不去理会`Action`是如何被管理及执行的。

```c++
bool ActionParser::ParseSection(std::vector<std::string>&& args, const std::string& filename,
                                int line, std::string* err) {
    std::vector<std::string> triggers(args.begin() + 1, args.end());
    auto action = std::make_unique<Action>(false, filename, line);
    action->InitTriggers(triggers, err);
    action_ = std::move(action);
    return true;
}

bool ActionParser::ParseLineSection(std::vector<std::string>&& args, int line, std::string* err) {
    return action_ ? action_->AddCommand(std::move(args), line, err) : false;
}

void ActionParser::EndSection() {
    if (action_ && action_->NumCommands() > 0) {
        action_manager_->AddAction(std::move(action_));
    }
}
```

so，ActionParser的工作就是，创建`Action`对象，并为其添加`Command`,最后将其添加到`ActionManager`中。

#### service

对应的`SectionParser`为`ServiceParser`。

在`service.h`中声明。包括: `ServiceParser`、`ServiceManager`、`Service`。本文只介绍`ServiceParser`的实现，而不去理会`Service`是如何被管理及其启动过程的。

```c++
bool ServiceParser::ParseSection(std::vector<std::string>&& args, const std::string& filename,
                                 int line, std::string* err) {
    const std::string& name = args[1];
    Service* old_service = service_manager_->FindServiceByName(name);
    if (old_service) {
        return false;
    }

    std::vector<std::string> str_args(args.begin() + 2, args.end());
    service_ = std::make_unique<Service>(name, str_args);
    return true;
}

bool ServiceParser::ParseLineSection(std::vector<std::string>&& args, int line, std::string* err) {
    return service_ ? service_->ParseLine(std::move(args), err) : false;
}

void ServiceParser::EndSection() {
    if (service_) {
        service_manager_->AddService(std::move(service_));
    }
}
```

大致过程和`Action`类似，先创建`Service`，调用`service_->ParseLine`解析参数，最后加入到`ServiceManager`中。

#### import

三个里面最简单的吧，其作用类似于C/C++编译器处理 `#include`宏的时候吧。

```c++

bool ImportParser::ParseSection(std::vector<std::string>&& args, const std::string& filename,
                                int line, std::string* err) {
    std::string conf_file;
    expand_props(args[1], &conf_file);
    if (filename_.empty()) filename_ = filename;
    imports_.emplace_back(std::move(conf_file), line);
    return true;
}

void ImportParser::EndFile() {
    auto current_imports = std::move(imports_);
    imports_.clear();
    for (const auto& [import, line_num] : current_imports) {
        parser_->ParseConfig(import);
    }
}
```

最终，还是调用`Parser::ParserConfig`来解析导入的`rc`文件。



