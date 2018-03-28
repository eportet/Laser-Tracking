import cv2
import numpy as np
import serial
import os
import RPi.GPIO as GPIO
import time


####################
# GLOBAL VARIABLES #
####################





####################
# DEFINE FUNCTIONS #
####################


def nothing(x):
	pass

def setup(u, s, w, lb, ub):
	# CAMERA SETUP 
	# sudo modprobe bcm2835-v4l2 makes the Raspberry Pi camera visible
	kernel = np.ones((5,5),np.uint8)
	cap = cv2.VideoCapture(0)
	cap.set(3,320)
	cap.set(4,240)

	# SERIAL SETUP
	port = None
	if u:
		port = os.popen("ls /dev/ttyACM*").read().rstrip()
		if port == "":
			port = None


	# WINDOWS & TRACKBAR SETUP
	makeWindows(w, lb, ub)

	# SERVO SETUP
	pan = 12
	tilt = 18
	GPIO.setmode(GPIO.BOARD)
	GPIO.setup(pan, GPIO.OUT)
	GPIO.setup(tilt, GPIO.OUT)
	pwm_p = GPIO.PWM(pan, 50)
	pwm_t = GPIO.PWM(tilt, 50)

	pwm_p.start(6.0)
	pwm_t.start(4.5)

	return cap, port, pwm_p, pwm_t

def makeWindows (w, lb, ub):
	if w == 0:
		pass
	elif w == 1:
		cv2.namedWindow('tracking')
	elif w == 2:
		cv2.namedWindow('HueComp')
		cv2.namedWindow('SatComp')
		cv2.namedWindow('ValComp')
		cv2.namedWindow('closing')
		cv2.namedWindow('tracking')
		makeTrackbars(lb, ub)

def makeTrackbars(lb, ub):
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

def moveServo(x, y, dcx, dcy):
	print("Moving Servo")
	if y > 180 and dcy < 7.5:	# MOVE UP
		dcy = dcy + 0.05
		print("UP y:" + str(y) + " dc: " + str(dcy))
	elif y < 60 and dcy > 3:	# MOVE DOWN
		dcy = dcy - 0.05
		print("DOWN y:" + str(y) + " dc: " + str(dcy))

	if x > 240 and dcx < 10.25:	# MOVE LEFT
		dcx = dcx + 0.05
		print("LEFT x:" + str(x) + " dc: " + str(dcx))
	elif x < 80 and dcx > 2.25:	# MOVE RIGHT
		dcx = dcx - 0.05
		print("RIGHT x:" + str(x) + " dc: " + str(dcx))
	

	return dcx, dcy


########################
# DEFINE MAIN FUNCTION #
########################


def main():
	
	usb = False		# True : Try to communicate with serial
				# False: Do not communicate

	servo = True	# True : Communicate with the servos
					# False: Do not initiate servos
	
	windows = 2		# 0: Do not show any windows
					# 1: Show only the tracked window 
					# 2: Show all the windows
	
	# Set the (h,s,v) lower and upper bounds
	lowerbound = (0,80,80)
	upperbound = (10,255,200)
	
	# Duty Cycle Values
	# set to rest values
	dc_p = 6.0
	dc_t = 4.5

	cap, port, pwm_p, pwm_t = setup(usb, servo, windows, lowerbound, upperbound)


	while(1):
		
		# MOVING SERVO
		#pwm_p.ChangeDutyCycle(dc_p)
		pwm_t.ChangeDutyCycle(dc_t)

		# Read frame
		_, frame = cap.read()

		# Blur the frame
		blurred = cv2.GaussianBlur(frame, (5,5), 0)

		# Converting to HSV
		hsv = cv2.cvtColor(blurred, cv2.COLOR_BGR2HSV)
		hue,sat,val = cv2.split(hsv)

		# Get info from track bar and appy to result
		if windows == 2:
			lowerbound, upperbound = getBounds()

		# construct a mask from the bounds obtained from trackbars, then perform
		# a series of dilations and erosions to remove any small
		# blobs left in the mask
		mask = cv2.inRange(hsv, lowerbound, upperbound)
		mask = cv2.erode(mask, None, iterations=2)
		closing = cv2.dilate(mask, None, iterations=2)

		# find contours in the mask and initialize the current
		# (x, y) center of the ball
		cnts = cv2.findContours(mask.copy(), cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)[-2]
		center = None
	 
		# only proceed if at least one contour was found
		if len(cnts) > 0:
			# find the largest contour in the mask, then use
			# it to compute the minimum enclosing circle and
			# centroid
			c = max(cnts, key=cv2.contourArea)
			((x, y), radius) = cv2.minEnclosingCircle(c)
			M = cv2.moments(c)
			if M["m00"] != 0:
				center = (int(M["m10"] / M["m00"]), int(M["m01"] / M["m00"]))
				#dc_p, dc_t = moveServo(center[0],center[1], dc_p, dc_t)
				port.write(str(center)+"\n") if usb else None
			
			# only proceed if the radius meets a minimum size
			if radius > 10:
				# draw the circle and centroid on the frame
				cv2.circle(frame, (int(x), int(y)), int(radius),(0, 255, 255), 2)
				cv2.circle(frame, center, 5, (0, 0, 255), -1)

		# display wanted windows
		if windows == 1 or windows == 2:
			
			frame = np.rot90(np.rot90(frame)) # Rotate frame
			cv2.imshow('tracking',frame)

			if windows == 2:
				hthresh = cv2.inRange(np.array(hue), np.array(lowerbound[0]), np.array(upperbound[0]))
				sthresh = cv2.inRange(np.array(sat), np.array(lowerbound[1]), np.array(upperbound[1]))
				vthresh = cv2.inRange(np.array(val), np.array(lowerbound[2]), np.array(upperbound[2]))
				
				hthresh = np.rot90(np.rot90(hthresh)) # Rotate frame
				sthresh = np.rot90(np.rot90(sthresh)) # Rotate frame
				vthresh = np.rot90(np.rot90(vthresh)) # Rotate frame
				
				#Show the result in frames
				cv2.imshow('HueComp',hthresh)
				cv2.imshow('SatComp',sthresh)
				cv2.imshow('ValComp',vthresh)
				cv2.imshow('closing',closing)

		# exit when 'q' key is pressed
		k = cv2.waitKey(1) & 0xFF
		if k == ord("q"):
			break

	#GPIO.cleanup()
	cap.release()
	cv2.destroyAllWindows()

if __name__ == "__main__":
	main()
