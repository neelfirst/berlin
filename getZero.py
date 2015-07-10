#!/usr/bin/python

import socket
from numpy import frombuffer, uint8, mean, isnan

PHI = [-30.67,-9.33,-29.33,-8.00,-28.00,-6.66,-26.66,-5.33,
       -25.33,-4.00,-24.00,-2.67,-22.67,-1.33,-21.33,0.00,
       -20.00,1.33,-18.67,2.67,-17.33,4.00,-16.00,5.33,
       -14.67,6.67,-13.33,8.00,-12.00,9.33,-10.67,10.67]
UDP_IP = ""
UDP_PORT = 2368


def parseData (data):
	array = frombuffer(data, uint8)
	xylist = []
	for i in xrange(0,12):
		blk_id = array[1+i*100]*256 + array[0+i*100]
		if blk_id != 0xEEFF:
			print "error: blk_id does not match 0xEEFF"
			continue;
		THETA = 0.01*(array[3+i*100]*256 + array[2+i*100])
		# rescale 0-360 to -180-180 and skip THETA out of bounds
		if THETA > 180:
			THETA = int(THETA-360)
		for j in xrange(0,32):
			R = 0.002*(array[(4+i*100)+(1+j*3)]*256 + array[(4+i*100)+(0+j*3)])
			I = array[(4+i*100)+(2+j*3)]
			xylist.append([int(THETA),PHI[j],int(R),int(I)])
#			print THETA, ',', PHI[j], ',', R, ',', I
	return xylist

##### main #####

sock = socket.socket(socket.AF_INET, # Internet
                     socket.SOCK_DGRAM) # UDP
sock.bind((UDP_IP, UDP_PORT))
fulldata = []

for i in xrange(0,1000):#while True:
#	start = datetime.datetime.now()
	data, addr = sock.recvfrom(1206*1808) # 2180448
	if addr[1] != 2368 or addr[0] != '192.168.1.201':
		print "error: captured UDP packet not from LiDAR"
		continue;
	xylist = parseData(data)
	fulldata.extend(xylist)

for theta in xrange(-19,20):
	for phi in PHI:
		R = []
		for row in fulldata:
			if row[0] == theta and row[1] == phi:
				R.append(row[2])
		if isnan(mean(R)):
			print theta,',',phi,',0'
		else:
			print theta,',',phi,',',mean(R)
#				print theta,',',phi,',',row[2]
