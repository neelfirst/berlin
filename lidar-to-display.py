#!/usr/bin/python

import socket
import cv2
import numpy
import os
import datetime
import math

UDP_IP = ""
UDP_PORT = 2368
THRESHOLD = 1
PHI = [-30.67,-9.33,-29.33,-8.00,-28.00,-6.66,-26.66,-5.33,
       -25.33,-4.00,-24.00,-2.67,-22.67,-1.33,-21.33,0.00,
       -20.00,1.33,-18.67,2.67,-17.33,4.00,-16.00,5.33,
       -14.67,6.67,-13.33,8.00,-12.00,9.33,-10.67,10.67]

def parseData (data):
	array = numpy.frombuffer(data, numpy.uint8)
	xylist = []
	sphere = []
	for i in xrange(0,12):
		# error check on blk_id
		blk_id = array[1+i*100]*256 + array[0+i*100]
		theta = array[3+i*100]*256 + array[2+i*100]
		# future: filter by theta
		for j in xrange(0,32):
			# ignore radius R
			I = array[(4+i*100)+(2+j*3)]
			if I > THRESHOLD:
				sphere.append([theta*0.01,PHI[j]])
	for pt in sphere:
		x = math.cos(pt[1])*math.sin(pt[0]) + 1
		y = math.cos(pt[1])*math.cos(pt[0]) + 1
		xylist.append([x,y])
	return xylist


sock = socket.socket(socket.AF_INET, # Internet
                     socket.SOCK_DGRAM) # UDP
sock.bind((UDP_IP, UDP_PORT))

src1 = cv2.imread('image3.jpg')
src2 = cv2.imread('image2.jpg')
# error check: w/h of src1 = w/h of src2 (exit)
w = src2.shape[1]
h = src2.shape[0]

while True:
	start = datetime.datetime.now()
	# error check: if packet not from velo IP discard & continue
	data, addr = sock.recvfrom(1206*1808) # 2180448
	xylist = parseData(data)
	for pt in xylist:
		W = int(pt[1]*w/2)
		H = int(pt[0]*h/2)
		# error check for boundary conditions
	        src1[H,W] = src2[H,W]
	stop = datetime.datetime.now()
	print (stop-start).seconds,".",(stop-start).microseconds
#	cv2.imshow('image',src1)
#	cv2.waitKey(1)
#       time.sleep(0.01)


