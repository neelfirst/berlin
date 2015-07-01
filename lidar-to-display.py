#!/usr/bin/python

import socket
import cv2
import numpy
import os
#import datetime
import math

UDP_IP = ""
UDP_PORT = 2368
THRESHOLD = 1 # modify in live installation setting
THETA_MIN = 0 # modify in live installation setting
THETA_MAX = 36000 # modify in live installation setting
PHI = [-30.67,-9.33,-29.33,-8.00,-28.00,-6.66,-26.66,-5.33,
       -25.33,-4.00,-24.00,-2.67,-22.67,-1.33,-21.33,0.00,
       -20.00,1.33,-18.67,2.67,-17.33,4.00,-16.00,5.33,
       -14.67,6.67,-13.33,8.00,-12.00,9.33,-10.67,10.67]

def parseData (data):
	array = numpy.frombuffer(data, numpy.uint8)
	xylist = []
	sphere = []
	for i in xrange(0,12):
		blk_id = array[1+i*100]*256 + array[0+i*100]
		if blk_id != 0xEEFF:
			print "error: blk_id does not match 0xEEFF"
			continue;
		theta = array[3+i*100]*256 + array[2+i*100]
		if theta < THETA_MIN or theta > THETA_MAX:
			print "warning: theta out of defined bounds"
			continue;
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

##### main #####

sock = socket.socket(socket.AF_INET, # Internet
                     socket.SOCK_DGRAM) # UDP
sock.bind((UDP_IP, UDP_PORT))

src1 = cv2.imread('image1.jpg')
src2 = cv2.imread('image2.jpg')
# error check: w/h of src1 = w/h of src2 (exit)
w1 = src1.shape[1]
h1 = src1.shape[0]
w2 = src2.shape[1]
h2 = src2.shape[0]
if w1 != w2 or h1 != h2:
	print "fatal: source image dimensional mismatch"
	exit(1)

while True:
#	start = datetime.datetime.now()
	data, addr = sock.recvfrom(1206*1808) # 2180448
	if addr[1] != 2368 or addr[0] != '192.168.1.201':
		print "error: captured UDP packet not from LiDAR"
		continue;
	xylist = parseData(data)
	for pt in xylist:
		W = int(pt[1]*w/2)
		H = int(pt[0]*h/2)
		if W >= w or H >= h or W < 0 or H < 0:
			print "error: converted point out of bounds"
			continue;
	        src1[H,W] = src2[H,W]
#	stop = datetime.datetime.now()
#	print (stop-start).seconds,".",(stop-start).microseconds
	cv2.imshow('image',src1)
	cv2.waitKey(1)
