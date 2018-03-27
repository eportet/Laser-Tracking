import cv2
import numpy as np
import serial
import os

from helper_functions import setup, getBounds, moveServo

display_windows = True # Set to true to open all windows or false for none
run_serial = False # Do not change

# Pinkish
lowerbound = (165,90,40)
upperbound = (179,190,205)

cap, port, p_h, p_v = setup(lowerbound, upperbound, display_windows)

if port != None:
	port = serial.Serial(port_name)
	run_serial = True

dc_h = 6.0
dc_v = 4.5
p_h.start(dc_h)
p_v.start(dc_v)

while(1):
	# MOVING SERVO
	p_h.ChangeDutyCycle(dc_h)
	p_v.ChangeDutyCycle(dc_v)

	# Read frame
	_, frame = cap.read()

	# Blur the frame
	blurred = cv2.GaussianBlur(frame, (5,5), 0)

	# Converting to HSV
	hsv = cv2.cvtColor(blurred, cv2.COLOR_BGR2HSV)
	hue,sat,val = cv2.split(hsv)

	# Get info from track bar and appy to result
	if display_windows:
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
			dc_h, dc_v = moveServo(center[0],center[1], dc_h, dc_v)
			port.write(str(center)+"\n") if run_serial else None
		
		# only proceed if the radius meets a minimum size
		if radius > 10:
			# draw the circle and centroid on the frame
			cv2.circle(frame, (int(x), int(y)), int(radius),(0, 255, 255), 2)
			cv2.circle(frame, center, 5, (0, 0, 255), -1)


	if display_windows:
		
		hthresh = cv2.inRange(np.array(hue), np.array(lowerbound[0]), np.array(upperbound[0]))
		sthresh = cv2.inRange(np.array(sat), np.array(lowerbound[1]), np.array(upperbound[1]))
		vthresh = cv2.inRange(np.array(val), np.array(lowerbound[2]), np.array(upperbound[2]))

		#Show the result in frames
		cv2.imshow('HueComp',hthresh)
		cv2.imshow('SatComp',sthresh)
		cv2.imshow('ValComp',vthresh)
		cv2.imshow('closing',closing)

	#frame = np.rot90(np.rot90(frame)) # Rotate frame
	cv2.imshow('tracking',frame)

	k = cv2.waitKey(1) & 0xFF
	if k == ord("q"):
		break

cap.release()
cv2.destroyAllWindows()
