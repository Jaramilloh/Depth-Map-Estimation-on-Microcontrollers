#!/usr/bin/python3
import re
import cv2
import numpy as np
import datetime
import os
import socket
import matplotlib.pyplot as plt

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
host_ip = '192.168.229.240'
port = 9879

socket_address = (host_ip, port)
s.bind(socket_address)

s.listen(5)
print('LISTENING AT:', socket_address)
cont = 0

def decode_image(pixformat, size, data):
        timestamp = datetime.datetime.now().strftime("%Y-%m-%d_%H.%M.%S")
        if pixformat == 'gray':
                img_array = np.reshape(np.frombuffer(data, dtype=np.uint8), (-1,size))
                filename = "uploads/" + timestamp + '_depth.png'
                #th = cv2.imwrite(filename, img_array)
                #print(th)
                plt.figure()
                plt.imshow(img_array)
                plt.axis('off')
                plt.savefig(filename)
                #if th == True:
                print(f"frame {filename} saved")
        elif pixformat == 'rgb':
                img_array = np.reshape(np.frombuffer(data, dtype=np.uint8), (-1,size,3))
                filename = "uploads/" + timestamp + '_frame.png'
                #img_array = cv2.cvtColor(img_array, cv2.COLOR_BGR2RGB)
                th = cv2.imwrite(filename, img_array)
                print(th)
                if th == True:
                        print(f"frame {filename} saved")
        #return img_array

def decode_msg(msg):
        msg = msg.decode('UTF-8')
        pixformat = ''
        size = 0
        data = b''
        for chunk in re.split('/', msg):
                att = re.split('=',chunk)
                if att[0] == 'Image-Format':
                        pixformat = att[1]
                elif att[0] == 'Image-Size':
                        size = int(att[1])
                elif att[0] == 'Image-Data':
                        data = bytes.fromhex(att[1])
        print(pixformat)
        print(size)
        return pixformat, size, data

while True:
        client, addr = s.accept()
        data = b''
        while True:
                msg = client.recv(4096)
                data += msg
                if len(msg) ==0:
                        pixformat, size, data = decode_msg(data)
                        decode_image(pixformat, size, data)
                        print("message received correctly")
                        break
        print("Closing connection")
        client.close()
