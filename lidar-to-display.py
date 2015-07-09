#!/usr/bin/python

import socket
import cv2
from numpy import frombuffer, uint8
from math import cos, sin, radians
#import datetime

### BEGIN TUNING PARAMETERS ###
RTHRESHOLD = 1
THRESHOLD = 0
THETA_MIN = -30
THETA_MAX = 30
WTHICK = 2
HTHICK = 14
#### END TUNING PARAMETERS ####

PHI = [-30.67,-9.33,-29.33,-8.00,-28.00,-6.66,-26.66,-5.33,
       -25.33,-4.00,-24.00,-2.67,-22.67,-1.33,-21.33,0.00,
       -20.00,1.33,-18.67,2.67,-17.33,4.00,-16.00,5.33,
       -14.67,6.67,-13.33,8.00,-12.00,9.33,-10.67,10.67]
UDP_IP = ""
UDP_PORT = 2368


def parseData (data):
	array = frombuffer(data, uint8)
	xylist = []
	sphere = []
	for i in xrange(0,12):
		blk_id = array[1+i*100]*256 + array[0+i*100]
		if blk_id != 0xEEFF:
#			print "error: blk_id does not match 0xEEFF"
			continue;
		THETA = 0.01*(array[3+i*100]*256 + array[2+i*100])
		# rescale 0-360 to -180-180 and skip THETA out of bounds
		if THETA > 180:
			THETA = THETA-360
		if THETA < THETA_MIN or THETA > THETA_MAX:
			continue;
		for j in xrange(0,32):
			R = 0.002*(array[(4+i*100)+(1+j*3)]*256 + array[(4+i*100)+(0+j*3)])
			I = array[(4+i*100)+(2+j*3)]
			if I > THRESHOLD and R > RTHRESHOLD:
				# linearize degrees here
				phi = ((PHI[j]+10)/21)
				theta = (2*THETA-THETA_MAX-THETA_MIN)/(THETA_MAX-THETA_MIN)
				sphere.append([theta,phi])
	#			print R, ' ', theta, ' ', phi, ' ', I
	for pt in sphere:
#		x = sin(radians(pt[1]))*cos(radians(pt[0])) + 1
#		y = sin(radians(pt[1]))*sin(radians(pt[0])) + 1
		y = pt[0] + 1 # instead of sin*sin
		z = pt[1] + 1 # instead of cosine
		xylist.append([y,z])
	return xylist

##### main #####

sock = socket.socket(socket.AF_INET, # Internet
                     socket.SOCK_DGRAM) # UDP
sock.bind((UDP_IP, UDP_PORT))

src1 = cv2.imread('map.png')
src2 = cv2.imread('sat.png')
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
		W = int(pt[0]*w1/2)
		H = int(pt[1]*h1/2)
		if W > w1 or H > h1 or W < 0 or H < 0:
			print "error: converted point out of bounds: ", pt[0], ' ', pt[1], ' ', W, ' ', H
			continue;
		for i in xrange(HTHICK):
			for j in xrange(WTHICK):
				hh = H + i
				ww = W + j
				if ww >= w1 or ww < 0 or hh >= h1 or hh < 0:
					continue;
				else:
			        	src1[hh,ww] = src2[hh,ww]
				hh = H - i
				ww = W - j
				if ww >= w1 or ww < 0 or hh >= h1 or hh < 0:
					continue;
				else:
			        	src1[hh,ww] = src2[hh,ww]

#	stop = datetime.datetime.now()
#	print (stop-start).seconds,".",(stop-start).microseconds
	cv2.imshow('image',src1)
	cv2.waitKey(1)
