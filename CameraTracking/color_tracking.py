import cv2
import numpy as np
import serial
import os

# open serial communication
run_serial = False
port_name = os.popen("ls /dev/ttyACM*").read().rstrip()
if (port_name != ""):
    run_serial = True
    port = serial.Serial(port_name)

kernel = np.ones((5,5),np.uint8)

# Take input from webcam
# sudo modprobe bcm2835-v4l2 makes the Raspberry Pi camera visible
cap = cv2.VideoCapture(0)
print("Video Capture Opened")

# Reduce the size of video to 320x240 so rpi can process faster
cap.set(3,320)
cap.set(4,240)

def nothing(x):
    pass

def makeWindows(lb, ub):
    # Creating a windows for later use
    cv2.namedWindow('HueComp')
    cv2.namedWindow('SatComp')
    cv2.namedWindow('ValComp')
    cv2.namedWindow('closing')
    cv2.namedWindow('tracking')


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

lowerbound = (12,145,186)
upperbound = (37,255,255)
makeWindows(lowerbound, upperbound)

while(1):

    buzz = 0
    _, frame = cap.read()

    #Blur
    blurred = cv2.GaussianBlur(frame, (5,5), 0)

    #converting to HSV
    hsv = cv2.cvtColor(blurred, cv2.COLOR_BGR2HSV)
    hue,sat,val = cv2.split(hsv)

    # get info from track bar and appy to result
    lowerbound, upperbound = getBounds()

    # Apply thresholding
    hthresh = cv2.inRange(np.array(hue), np.array(lowerbound[0]), np.array(upperbound[0]))
    sthresh = cv2.inRange(np.array(sat), np.array(lowerbound[1]), np.array(upperbound[1]))
    vthresh = cv2.inRange(np.array(val), np.array(lowerbound[2]), np.array(upperbound[2]))

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
            port.write(str(center)+"\n") if run_serial else None
        
        # only proceed if the radius meets a minimum size
        if radius > 10:
            # draw the circle and centroid on the frame
            cv2.circle(frame, (int(x), int(y)), int(radius),(0, 255, 255), 2)
            cv2.circle(frame, center, 5, (0, 0, 255), -1)

    #Show the result in frames
    cv2.imshow('HueComp',hthresh)
    cv2.imshow('SatComp',sthresh)
    cv2.imshow('ValComp',vthresh)
    cv2.imshow('closing',closing)
    cv2.imshow('tracking',frame)

    k = cv2.waitKey(1) & 0xFF
    if k == ord("q"):
        break

cap.release()
cv2.destroyAllWindows()
