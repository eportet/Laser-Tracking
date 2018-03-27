import cv2
import numpy as np
import serial
import os

import RPi.GPIO as GPIO

def nothing(x):
	pass

def setup(lb, ub, windows):
	# SERIAL SETUP
	# Open serial communication
	port = os.popen("ls /dev/ttyACM*").read().rstrip()
	if (port == ""):
		port = None

	# CAMERA SETUP 
	kernel = np.ones((5,5),np.uint8)

	# Take input from webcam
	# sudo modprobe bcm2835-v4l2 makes the Raspberry Pi camera visible
	cap = cv2.VideoCapture(0)

	# Reduce the size of video to 320x240 so rpi can process faster
	cap.set(3,320)
	cap.set(4,240)

	cv2.namedWindow('tracking')
	if windows:
		makeTrackbars(lb, ub)

	# SERVO CONTROLS
	GPIO.setmode(GPIO.BOARD)
	GPIO.setup(12, GPIO.OUT)
	GPIO.setup(18, GPIO.OUT)
	p_h = GPIO.PWM(12, 50)
	p_v = GPIO.PWM(18, 50)

	return cap, port, p_h, p_v

def makeTrackbars(lb, ub):
	# Creating windows for later use
	cv2.namedWindow('HueComp')
	cv2.namedWindow('SatComp')
	cv2.namedWindow('ValComp')
	cv2.namedWindow('closing')
	


	# Creating track bar for min and max for hue, saturation and value
	# You can adjust the defaults as you like
	cv2.createTrackbar('hmin', 'HueComp',lb[0],179,nothing)
	cv2.createTrackbar('hmax', 'HueComp',ub[0],179,nothing)

	cv2.createTrackbar('smin', 'SatComp',lb[1],255,nothing)
	cv2.createTrackbar('smax', 'SatComp',ub[1],255,nothing)

	cv2.createTrackbar('vmin', 'ValComp',lb[2],255,nothing)
	cv2.createTrackbar('vmax', 'ValComp',ub[2],255,nothing)

def getBounds():
	hmn = cv2.getTrackbarPos('hmin','HueComp')
	hmx = cv2.getTrackbarPos('hmax','HueComp')
	
	smn = cv2.getTrackbarPos('smin','SatComp')
	smx = cv2.getTrackbarPos('smax','SatComp')

	vmn = cv2.getTrackbarPos('vmin','ValComp')
	vmx = cv2.getTrackbarPos('vmax','ValComp')
	
	return (hmn,smn,vmn), (hmx,smx,vmx)


def moveServo(x, y, dc_h, dc_v):
	# MOVE UP
	if y > 180 and dc_v < 7.5:
		dc_v = dc_v + 0.05
		print("UP y:" + str(y) + " dc: " + str(dc_v))

	# MOVE DOWN
	if y < 60 and dc_v > 3:
		dc_v = dc_v - 0.05
		print("DOWN y:" + str(y) + " dc: " + str(dc_v))

	# MOVE LEFT
	if x > 240 and dc_h < 10.25:
		dc_h = dc_h + 0.05
		print("LEFT x:" + str(x) + " dc: " + str(dc_h))

	# MOVE RIGHT
	if x < 80 and dc_h > 2.25:
		dc_h = dc_h - 0.05
		print("RIGHT x:" + str(x) + " dc: " + str(dc_h))

	return dc_h, dc_v
