# 简介

Reqests内部使用`urllib3`实现。





# 入门

## 构建URL参数

Requests 使用 `params` 关键字参数，以一个字符串字典来提供这些参数。

例如， `httpbin.org/get?key=val`

```powershell
>>> payload = {'key1' : 'value1', 'key2' : 'value2', 'key3' : None}
>>> r = requests.get('http://httpbin.org/get', params=payload)
>>> r.url
'http://httpbin.org/get?key1=value1&key2=value2'
```

根据打印可以看到，Requests自动构建了我们需要的url。

> 字典里值为 `None` 的键都不会被添加到 URL 的查询字符串里

## 构建headers

为请求添加 HTTP 头部，只要简单地传递一个 `dict` 给 `headers` 参数就可以了。

```powershell
>>> url = 'https://api.github.com/some/endpoint'
>>> headers = {'user-agent': 'my-app/0.0.1'}
>>> r = requests.get(url, headers=headers)
```

> 所有的 header 值必须是 `string`、bytestring 或者 unicode。



# POST请求

通过`post`方法的`data`参数传递。

```powershell
>>> payload = {'key1': 'value1', 'key2': 'value2'}

>>> r = requests.post("http://httpbin.org/post", data=payload)
>>> print(r.text)
{
  ...
  "form": {
    "key2": "value2",
    "key1": "value1"
  },
  ...
}
```

Requests除了能够对`dict`进行编码，还能对元组进行编码。

```powershell
>>> payload = (('key1', 'value1'), ('key1', ['1', '2', 3]), ('key2', 'value1'))
>>> r = requests.post("http://httpbin.org/post", data=payload)
>>> print(r.text)
{
  "args": {}, 
  "data": "", 
  "files": {}, 
  "form": {
    "key1": [
      "value1", 
      "1", 
      "2", 
      "3"
    ], 
    "key2": "value1"
  }, 
  }
```

