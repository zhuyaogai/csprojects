# -*- coding: UTF-8 -*-

# 总体逻辑就是提取http请求的header，然后提取出其中请求的path
# 根据path进行路由
# 所谓的 Web Framework 其实就是在做 HTTP 的请求的解析，并且把结果分派给不同的函数进行处理
# WSGI 只是一种标准，大家形成一种约定，可以方便解耦和结合
# 所以如果server如果是遵循了这种协议，那么它就可以和任何的Web框架结合起来了

def app(environ, start_response):
    """A barebones WSGI application.

    This is a starting point for your own Web framework :)
    """
    status = '200 OK'
    response_headers = [('Content-Type', 'text/plain')]
    start_response(status, response_headers)     # 把状态和构造好的header返回

    # 根据路径进行路由
    path = environ['PATH_INFO'][1:]
    if path == 'index':
        return index()
    elif path == 'hello':
        return hello()
    else:
        return ['Hello world from a simple WSGI application!\n']

def hello():
    return ['Hello world!\n']

def index():
    return ['Index page!\n']

# --------------------------------
x = 1
def f():
    print x+1


def g():
    x = 10
    f()

# python 依然是静态作用域
if __name__ == '__main__':
    g()    # 2