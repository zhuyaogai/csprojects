# -*- coding: UTF-8 -*-
import socket

HOST, PORT = '', 8888

# socket.AF_INET 服务器之间网络通信    socket.SOCK_STREAM 流式socket
listen_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
listen_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
listen_socket.bind((HOST, PORT))
listen_socket.listen(1)
print 'Serving HTTP on port %s ...' % PORT
while True:
    client_connection, client_address = listen_socket.accept()
    request = client_connection.recv(1024)
    # print client_address
    print request

    http_response = """
HTTP/1.1 200 OK

Hello, World!
"""
    # print http_response
    client_connection.sendall(http_response)
    client_connection.close()