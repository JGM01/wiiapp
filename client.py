import socket
import os

HOST = '192.168.0.117'  # Replace with your Wii's IP address
PORT = 9001

filename = 'test.txt'  # Replace with the file you want to send
filesize = os.path.getsize(filename)

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect((HOST, PORT))
    s.send(filename.encode())
    s.send(filesize.to_bytes(4, byteorder='big'))
    
    with open(filename, 'rb') as f:
        while True:
            bytes_read = f.read(4096)
            if not bytes_read:
                break
            s.sendall(bytes_read)
    
    print("File sent successfully")
