# command line

```shell
# 创建项目
scrapy startproject project_name [project_dir]
cd project_name

# 创建一个爬虫
# Scrapy会根据指定的域名生成一个爬虫模板(设置start_urls)
scrapy genspider domain domain.com

# 运行爬虫
scrapy crawl domain
```

scrapy的命令分两种。

## 命令

### startproject

- Syntax: `scrapy startproject <project_name> [project_dir]`
- Requires project: *no*

在指定目录下创建项目

### genspider

- Syntax: `scrapy genspider [-t template] <name> <domain>`
- Requires project: *no*

在当前目录下创建一个新的爬虫。如果是在内项目内，爬虫会保存在`spiders`目录下。

```powershell
$ scrapy genspider -l  # 查看模板
Available templates:
  basic
  crawl
  csvfeed
  xmlfeed

$ scrapy genspider example example.com
Created spider 'example' using template 'basic'

$ scrapy genspider -t crawl scrapyorg scrapy.org
Created spider 'scrapyorg' using template 'crawl'
```



### crawl

- Syntax: `scrapy crawl <spider>`
- Requires project: *yes*

启用指定的爬虫

```powershell
$ scrapy crawl myspider
[ ... myspider starts crawling ... ]
```



### check

- Syntax: `scrapy check [-l] <spider>`
- Requires project: *yes*

检查爬虫文件

```powershell
$ scrapy check -l
first_spider
  * parse
  * parse_item
second_spider
  * parse
  * parse_item

$ scrapy check
[FAILED] first_spider:parse_item
>>> 'RetailPricex' field is missing

[FAILED] first_spider:parse
>>> Returned 92 requests, expected 0..4
```





### list

- Syntax: `scrapy list`
- Requires project: *yes*

列出当前项目所有可用爬虫。

```powershell
$ scrapy list
spider1
spider2
```



### fetch

- Syntax: `scrapy fetch <url>`
- Requires project: *no*

使用scrapy的下载器下载指定url的内容，并输出到 标准输出。

- `--spider=SPIDER`: bypass spider autodetection and force use of specific spider
- `--headers`: print the response’s HTTP headers instead of the response’s body
- `--no-redirect`: do not follow HTTP 3xx redirects (default is to follow them)

```powershell
$ scrapy fetch --nolog http://www.example.com/some/page.html
[ ... html content here ... ]

$ scrapy fetch --nolog --headers http://www.example.com/
{'Accept-Ranges': ['bytes'],
 'Age': ['1263   '],
 'Connection': ['close     '],
 'Content-Length': ['596'],
 'Content-Type': ['text/html; charset=UTF-8'],
 'Date': ['Wed, 18 Aug 2010 23:59:46 GMT'],
 'Etag': ['"573c1-254-48c9c87349680"'],
 'Last-Modified': ['Fri, 30 Jul 2010 15:30:18 GMT'],
 'Server': ['Apache/2.2.3 (CentOS)']}
```



### view

- Syntax: `scrapy view <url>`
- Requires project: *no*

下载url对应的网页并在浏览器中打开。

- `--spider=SPIDER`: bypass spider autodetection and force use of specific spider
- `--no-redirect`: do not follow HTTP 3xx redirects (default is to follow them)



### shell

- Syntax: `scrapy shell [url]`
- Requires project: *no*



- `--spider=SPIDER`: bypass spider autodetection and force use of specific spider
- `-c code`: evaluate the code in the shell, print the result and exit
- `--no-redirect`: do not follow HTTP 3xx redirects (default is to follow them); this only affects the URL you may pass as argument on the command line; once you are inside the shell, `fetch(url)`will still follow HTTP redirects by default.



```powershell
$ scrapy shell http://www.example.com/some/page.html
[ ... scrapy shell starts ... ]

$ scrapy shell --nolog http://www.example.com/ -c '(response.status, response.url)'
(200, 'http://www.example.com/')

# shell follows HTTP redirects by default
$ scrapy shell --nolog http://httpbin.org/redirect-to?url=http%3A%2F%2Fexample.com%2F -c '(response.status, response.url)'
(200, 'http://example.com/')

# you can disable this with --no-redirect
# (only for the URL passed as command line argument)
$ scrapy shell --no-redirect --nolog http://httpbin.org/redirect-to?url=http%3A%2F%2Fexample.com%2F -c '(response.status, response.url)'
(302, 'http://httpbin.org/redirect-to?url=http%3A%2F%2Fexample.com%2F')
```



### parse

- Syntax: `scrapy parse <url> [options]`
- Requires project: *yes*

Fetches the given URL and parses it with the spider that handles it, using the method passed with the `--callback` option, or `parse` if not given.

Supported options:

- `--spider=SPIDER`: bypass spider autodetection and force use of specific spider
- `--a NAME=VALUE`: set spider argument (may be repeated)
- `--callback` or `-c`: spider method to use as callback for parsing the response
- `--meta` or `-m`: additional request meta that will be passed to the callback request. This must be a valid json string. Example: –meta=’{“foo” : “bar”}’
- `--cbkwargs`: additional keyword arguments that will be passed to the callback. This must be a valid json string. Example: –cbkwargs=’{“foo” : “bar”}’
- `--pipelines`: process items through pipelines
- `--rules` or `-r`: use [`CrawlSpider`](https://docs.scrapy.org/en/latest/topics/spiders.html#scrapy.spiders.CrawlSpider) rules to discover the callback (i.e. spider method) to use for parsing the response
- `--noitems`: don’t show scraped items
- `--nolinks`: don’t show extracted links
- `--nocolour`: avoid using pygments to colorize the output
- `--depth` or `-d`: depth level for which the requests should be followed recursively (default: 1)
- `--verbose` or `-v`: display information for each depth level

Usage example:

```powershell
$ scrapy parse http://www.example.com/ -c parse_item
[ ... scrapy log lines crawling example.com spider ... ]

>>> STATUS DEPTH LEVEL 1 <<<
# Scraped Items  ------------------------------------------------------------
[{'name': 'Example item',
 'category': 'Furniture',
 'length': '12 cm'}]

# Requests  -----------------------------------------------------------------
[]
```



### settings

- Syntax: `scrapy settings [options]`
- Requires project: *no*

Get the value of a Scrapy setting.

If used inside a project it’ll show the project setting value, otherwise it’ll show the default Scrapy value for that setting.

Example usage:

```
$ scrapy settings --get BOT_NAME
scrapybot
$ scrapy settings --get DOWNLOAD_DELAY
0
```



### runspider

- Syntax: `scrapy runspider <spider_file.py>`
- Requires project: *no*

Run a spider self-contained in a Python file, without having to create a project.

Example usage:

```
$ scrapy runspider myspider.py
[ ... spider starts crawling ... ]
```



### version

- Syntax: `scrapy version [-v]`
- Requires project: *no*

Prints the Scrapy version. If used with `-v` it also prints Python, Twisted and Platform info, which is useful for bug reports.



### bench

*New in version 0.17.*

- Syntax: `scrapy bench`
- Requires project: *no*

# 目录结构

```shell
tutorial/
    scrapy.cfg            # deploy configuration file
    tutorial/             # project's Python module, you'll import your code from here
        __init__.py
        items.py          # project items definition file
        middlewares.py    # project middlewares file
        pipelines.py      # project pipelines file
        settings.py       # project settings file
        spiders/          # a directory where you'll later put your spiders
            __init__.py
```



# CSS 提取数据

1. `class`

    ```html
    <div class="quote">
        <span class="text">“The world as we have created it is a process of our
        thinking. It cannot be changed without changing our thinking.”</span>
        <span>
            by <small class="author">Albert Einstein</small>
            <a href="/author/Albert-Einstein">(about)</a>
        </span>
        <div class="tags">
            Tags:
            <a class="tag" href="/tag/change/page/1/">change</a>
            <a class="tag" href="/tag/deep-thoughts/page/1/">deep-thoughts</a>
            <a class="tag" href="/tag/thinking/page/1/">thinking</a>
            <a class="tag" href="/tag/world/page/1/">world</a>
        </div>
    </div>
    ```

    - 提取text

        ```pyt
        response.css('span.text::text').get()
        #.后面跟随class名称， ::text表示提取元素内容
        ```

    - 提取tags

        ```py
        response.css('div.tags a.tag::text').getall()
        层级关系用空格体现。 getall()即获取所有的a标签内的text
        ```

2. 属性

    ```html
    <ul class="pager">
        <li class="next">
            <a href="/page/2/">Next <span aria-hidden="true">&rarr;</span></a>
        </li>
    </ul>
    ```

    取href

    ```pyth
    response.css('ul.pager li.next a::attr(href))
    response.css('ul.pager li.next a').attrib['href']
    # ::attr 或者 attrib方法提取
    ```

3. 提取兄弟标签

    ```html
    <div class="quote" itemscope="" itemtype="http://schema.org/CreativeWork">
            <span class="text" itemprop="text">“The world as we have created it is a process of our thinking. It cannot be changed without changing our thinking.”</span>
            <span>by 
              <small class="author" itemprop="author">Albert Einstein</small>
              <a href="/author/Albert-Einstein">(about)</a>
            </span>
            <div class="tags">
                Tags:
                <meta class="keywords" itemprop="keywords" content="change,deep-thoughts,thinking,world">             
                <a class="tag" href="/tag/change/page/1/">change</a>            
                <a class="tag" href="/tag/deep-thoughts/page/1/">deep-thoughts</a>            
                <a class="tag" href="/tag/thinking/page/1/">thinking</a>            
                <a class="tag" href="/tag/world/page/1/">world</a>           
            </div>
    </div>
    ```

    比如，提取`<a href="/author/Albert-Einstein">(about)</a>`

    ```python
    response.css('small.author + a::attr(href)')
    ```

    

# Item

Scrapy中用于定义结构化数据通用输出数据格式的一个class。其行为类似于`dict`。

## 声明一个Item

```python
class Product(scrapy.Item):
    name = scrapy.Field()
    price = scrapy.Field()
    stock = scrapy.Field()
    tags = scrapy.Field()
    last_updated = scrapy.Field(serializer=str)
```



## scrapy.Field

其定义如下:

```python
class Field(dict):
    """Container of field metadata"""
```

可见，其就是一个`dict`。其用于保存每个Field的元数据。使用形式如下:

```python
class Product(scrapy.Item):
    last_updated = scrapy.Field(serializer=str, a=1, b=2)
```

这些元数据能够被不同的组件使用，也只有使用这些元数据的组件才知道元数据的存在。



虽然每个Field都被声明成类数据，但是`Item`的实现中，禁用了`class.attr`这种用法。

```python
class DictItem(MutableMapping, BaseItem):

    fields = {}

    def __init__(self, *args, **kwargs):
        self._values = {}  #实例属性，用于保存field 的key - value 关系，如Product中的 "name" : "liuzheng"
        if args or kwargs:  # avoid creating dict for most common case
            for k, v in six.iteritems(dict(*args, **kwargs)):
                self[k] = v

    #以dict形式获取key对应的值
    def __getitem__(self, key):
        return self._values[key]

    #以dict形式设置key对应的值
    def __setitem__(self, key, value):
        if key in self.fields:
            self._values[key] = value
        else:
            raise KeyError("%s does not support field: %s" %
                (self.__class__.__name__, key))

    # 禁用其获取fields中保存的实例数据。
    def __getattr__(self, name):
        if name in self.fields:
            raise AttributeError("Use item[%r] to get field value" % name)
        raise AttributeError(name)

    def __setattr__(self, name, value):
        if not name.startswith('_'):
            raise AttributeError("Use item[%r] = %r to set field value" %
                (name, value))
        super(DictItem, self).__setattr__(name, value)
```



`DictItem`类中，`fields = {}`在哪被使用呢？

```python
#为其添加元数据
@six.add_metaclass(ItemMeta)
class Item(DictItem):
    pass

class ItemMeta(ABCMeta):
	def __new__(mcs, class_name, bases, attrs):
        fields = getattr(_class, 'fields', {})# 这里，看到没
        new_attrs = {}
        for n in dir(_class):
            v = getattr(_class, n)
            if isinstance(v, Field):  #如果属性的类型是Field就加入到fields中
                fields[n] = v
            elif n in attrs:
                new_attrs[n] = attrs[n]

        new_attrs['fields'] = fields
        new_attrs['_class'] = _class
        if classcell is not None:
            new_attrs['__classcell__'] = classcell
        return super(ItemMeta, mcs).__new__(mcs, class_name, bases, new_attrs)
```

通过`ItemMeta`，就会将我们自定义的`Item`中的实例属性保存到`fields`中，并且禁用通过属性方式设置和获取值。

每个`Filed`对象的实际值都保存在其实例变量`values`中。

`Filed`自身是一个`dict`，但是并不保存其值。而是保存其元数据。

例:

```python
product = Product(name='a', last_updated = 2)
```

`Product`的5个Filed`对象的名称`都会保存到`Item.fields`中。

`name ： ‘a’`， 和 `last_updated:2`会被保存到`Item.values`中。没有复制的Field不会被保存。

而`serializer:str`会被保存在`Field`对象中



## Cpoy

- shallow copy

    `product2 = product.copy()` or `product2 = Product(product)`

- deep copy

    

# 其它

## 设置代理地址





# 疑问

## Item 中 的 serializer究竟有什么用



