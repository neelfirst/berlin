#!/usr/bin/python

import socket
import cv2
import csv
from collections import namedtuple
from numpy import frombuffer, uint8
from math import cos, sin, radians
#import datetime

### BEGIN TUNING PARAMETERS ###
RTHRESHOLD = 0.5
#ITHRESHOLD = 0.2
THETA_MIN = -19
THETA_MAX = 19
WTHICK = 2
HTHICK = 14
#### END TUNING PARAMETERS ####

PHI = [-30.67,-9.33,-29.33,-8.00,-28.00,-6.66,-26.66,-5.33,
       -25.33,-4.00,-24.00,-2.67,-22.67,-1.33,-21.33,0.00,
       -20.00,1.33,-18.67,2.67,-17.33,4.00,-16.00,5.33,
       -14.67,6.67,-13.33,8.00,-12.00,9.33,-10.67,10.67]
UDP_IP = ""
UDP_PORT = 2368
PT = namedtuple("PT", ["THETA","PHI"])

def parseData (data,r):
	array = frombuffer(data, uint8)
	xylist = []
	for x in xrange(0,12):
		blk_id = array[1+x*100]*256 + array[0+x*100]
		if blk_id != 0xEEFF:
#			print "error: blk_id does not match 0xEEFF"
			continue;
		THETA = 0.01*(array[3+x*100]*256 + array[2+x*100])
		# rescale 0-360 to -180-180 and skip THETA out of bounds
		if THETA > 180:
			THETA = THETA-360
		if THETA < THETA_MIN or THETA > THETA_MAX:
			continue;
		for y in xrange(0,32):
			pt = PT(int(THETA),PHI[y])
			R = 0.002*(array[(4+x*100)+(1+y*3)]*256 + array[(4+x*100)+(0+y*3)])
			I = array[(4+x*100)+(2+y*3)]
			if R != 0 and r[pt] != 0 and (r[pt]-R)/r[pt] >= RTHRESHOLD:
#			if abs((I-i[pt])/(I+0.1)) >= ITHRESHOLD and abs((R-r[pt])/(R+0.1)) >= RTHRESHOLD:
				# linearize degrees here
				phi = -((PHI[y]+10)/21)
				theta = (2*THETA-THETA_MAX-THETA_MIN)/(THETA_MAX-THETA_MIN)
				print THETA,',',PHI[y],',',R,',',r[pt]
				xylist.append([theta+1,phi+1])
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

# load reference data
f = open('berkeley.csv','rb')
x = csv.reader(f)
r = {}
#i = {}
for row in x:
        pt = PT(THETA=int(row[0]),PHI=float(row[1]))
        r[pt] = float(row[2])
#        i[pt] = float(row[3])

while True:
#	start = datetime.datetime.now()
	data, addr = sock.recvfrom(1206*1808) # 2180448
#	if addr[1] != '2368 ' or addr[0] != '192.168.1.201':
#		print "error: captured UDP packet not from LiDAR:"
#		continue;
	xylist = parseData(data,r)
	for pt in xylist:
		W = int(pt[0]*w1/2)
		H = int(pt[1]*h1/2)
		if W > w1 or H > h1 or W < 0 or H < 0:
			print "error: converted point out of bounds: ", pt[0], ' ', pt[1], ' ', W, ' ', H
			continue;
		for a in xrange(HTHICK):
			for b in xrange(WTHICK):
				hh = H + a
				ww = W + b
				if ww >= w1 or ww < 0 or hh >= h1 or hh < 0:
					continue;
				else:
			        	src1[hh,ww] = src2[hh,ww]
				hh = H - a
				ww = W - b
				if ww >= w1 or ww < 0 or hh >= h1 or hh < 0:
					continue;
				else:
			        	src1[hh,ww] = src2[hh,ww]

#	stop = datetime.datetime.now()
#	print (stop-start).seconds,".",(stop-start).microseconds
	cv2.imshow('image',src1)
	cv2.waitKey(1)
