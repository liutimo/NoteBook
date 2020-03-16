# 静态文件

在APP目录下创建一个名为`static`目录。Django将在该目录下查找静态文件。



在需要使用静态文件的模板文件头中，加入`{% static %}`标签。该标签会生成静态文件的绝对路径。

```php+HTML
{% load static %}
<link rel="stylesheet" type="text/css" href="{% static 'polls/style.css' %}" >
```

实际生成的HTML文件：

```html
<link rel="stylesheet" type="text/css" href="/static/polls/style.css" >
```

请求的路径

```powershell
[04/Mar/2020 15:19:52] "GET /static/polls/style.css HTTP/1.1" 200 99
[04/Mar/2020 15:19:52] "GET /static/polls/images/background.gif HTTP/1.1" 200 14833644
```





# Request 和 Response

当页面被请求时，Django创建一个包含请求元数据的`HttpRequest`对象，然后搜索相应的`view`,并将`HttpRequest`对象作为第一个参数传递到view function中。每一个view function负责处理请求并返回一个`HttpResponse`对象。



## HttpRequest

 ### 属性

其所有属性都是只读的，除了一些状态属性。

1. `HttpRequest.scheme`

   指示request的类型。"HTTP" or "HTTPS".

2. `HttpRequest.body`

   The raw HTTP request body as a bytestring. 具体用到了在看。

3. `HttpRequest.path`

   不包含顶级域名的请求路径。

4. `HttpRequest.method`

   请求方法

5. `HttpRequest.encoding`

   指示提交的数据的编码方式。更改该属性后，立即生效。

6. `HttpRequest.content_type`

   指示MIME数据的类型。从HTTP协议中的**CONTENT_TYPE**获取。

7. `HttpRequest.content_params`

   A dictionary of key/value parameters included in the `CONTENT_TYPE` header.

8. `HttpRequest.GET`

   A dictionary-like object containing all given HTTP GET parameters. 

9. `HttpRequest.POST`

   A dictionary-like object containing all given HTTP POST parameters, providing that the request contains form data. 

10. `HttpRequest.FILES`

    A dictionary-like object containing all uploaded files. 

11. `HttpRequest.headers`