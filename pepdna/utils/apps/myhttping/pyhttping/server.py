#!/usr/bin/python

#
# (C) Kr1stj0n C1k0
#

from socket import error as SocketError
import sys
import socket

HOST, PORT = '', 8888

http_response = """HTTP/1.1 200 OK
                   Date: Thu, Jul  3 15:27:54 2014
                   Content-Type: text/xml; charset="utf-8"
                   Connection: keep-alive
                   Content-Length: 626"""

listen_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
listen_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
listen_socket.bind((HOST, PORT))
listen_socket.listen(1)
print('Serving HTTP on port' + PORT + '...')
client_conn, client_addr = listen_socket.accept()
sys.stdout.write('serving client')
sys.stdout.flush()
while True:
    try:
        request_data = client_conn.recv(1328)
        sys.stdout.write('.')
        sys.stdout.flush()

        client_conn.sendall(http_response)
    except SocketError as e:
        print(e)
        break

client_conn.close()
