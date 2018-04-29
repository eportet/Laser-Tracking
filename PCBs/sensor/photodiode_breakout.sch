EESchema Schematic File Version 2
LIBS:power
LIBS:device
LIBS:switches
LIBS:relays
LIBS:motors
LIBS:transistors
LIBS:conn
LIBS:linear
LIBS:regul
LIBS:74xx
LIBS:cmos4000
LIBS:adc-dac
LIBS:memory
LIBS:xilinx
LIBS:microcontrollers
LIBS:dsp
LIBS:microchip
LIBS:analog_switches
LIBS:motorola
LIBS:texas
LIBS:intel
LIBS:audio
LIBS:interface
LIBS:digital-audio
LIBS:philips
LIBS:display
LIBS:cypress
LIBS:siliconi
LIBS:opto
LIBS:atmel
LIBS:contrib
LIBS:valves
LIBS:sensor-cache
EELAYER 25 0
EELAYER END
$Descr User 5906 4000
encoding utf-8
Sheet 2 2
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L LED D?
U 1 1 5A7F93E1
P 3400 950
F 0 "D?" H 3400 1050 50  0000 C CNN
F 1 "LED" H 3400 850 50  0000 C CNN
F 2 "" H 3400 950 50  0001 C CNN
F 3 "" H 3400 950 50  0001 C CNN
	1    3400 950 
	0    -1   -1   0   
$EndComp
$Comp
L D_Photo D?
U 1 1 5A7F9440
P 2450 1050
F 0 "D?" H 2470 1120 50  0000 L CNN
F 1 "D_Photo" H 2410 940 50  0000 C CNN
F 2 "" H 2400 1050 50  0001 C CNN
F 3 "" H 2400 1050 50  0001 C CNN
	1    2450 1050
	1    0    0    -1  
$EndComp
$Comp
L R R?
U 1 1 5A7F94B8
P 2950 1750
F 0 "R?" V 3030 1750 50  0000 C CNN
F 1 "R" V 2950 1750 50  0000 C CNN
F 2 "" V 2880 1750 50  0001 C CNN
F 3 "" H 2950 1750 50  0001 C CNN
	1    2950 1750
	1    0    0    -1  
$EndComp
$Comp
L R R?
U 1 1 5A7F94FF
P 3400 1750
F 0 "R?" V 3480 1750 50  0000 C CNN
F 1 "R" V 3400 1750 50  0000 C CNN
F 2 "" V 3330 1750 50  0001 C CNN
F 3 "" H 3400 1750 50  0001 C CNN
	1    3400 1750
	1    0    0    -1  
$EndComp
$Comp
L Q_NPN_BCE Q?
U 1 1 5A7F9530
P 2850 1050
F 0 "Q?" H 3050 1100 50  0000 L CNN
F 1 "Q_NPN_BCE" H 3050 1000 50  0000 L CNN
F 2 "" H 3050 1150 50  0001 C CNN
F 3 "" H 2850 1050 50  0001 C CNN
	1    2850 1050
	1    0    0    -1  
$EndComp
$Comp
L Q_NPN_BCE Q?
U 1 1 5A7F957F
P 3300 1350
F 0 "Q?" H 3500 1400 50  0000 L CNN
F 1 "Q_NPN_BCE" H 3500 1300 50  0000 L CNN
F 2 "" H 3500 1450 50  0001 C CNN
F 3 "" H 3300 1350 50  0001 C CNN
	1    3300 1350
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR?
U 1 1 5A7F95B4
P 2200 1950
F 0 "#PWR?" H 2200 1700 50  0001 C CNN
F 1 "GND" H 2200 1800 50  0000 C CNN
F 2 "" H 2200 1950 50  0001 C CNN
F 3 "" H 2200 1950 50  0001 C CNN
	1    2200 1950
	1    0    0    -1  
$EndComp
$Comp
L VDD #PWR?
U 1 1 5A7F95D4
P 2200 750
F 0 "#PWR?" H 2200 600 50  0001 C CNN
F 1 "VDD" H 2200 900 50  0000 C CNN
F 2 "" H 2200 750 50  0001 C CNN
F 3 "" H 2200 750 50  0001 C CNN
	1    2200 750 
	1    0    0    -1  
$EndComp
Wire Wire Line
	3400 1950 3400 1900
Wire Wire Line
	3400 1950 2200 1950
Wire Wire Line
	2950 1900 2950 1950
Connection ~ 2950 1950
Wire Wire Line
	3400 1600 3400 1550
Wire Wire Line
	2950 1600 2950 1250
Wire Wire Line
	2950 1350 3100 1350
Connection ~ 2950 1350
Wire Wire Line
	3400 1150 3400 1100
Wire Wire Line
	3400 800  3400 750 
Wire Wire Line
	3400 750  2200 750 
Wire Wire Line
	2200 750  2200 1050
Wire Wire Line
	2200 1050 2250 1050
Wire Wire Line
	2650 1050 2550 1050
Wire Wire Line
	2950 850  2950 750 
Connection ~ 2950 750 
Wire Wire Line
	3400 1100 3550 1100
Text HLabel 3550 1100 2    60   Output ~ 0
out
$EndSCHEMATC
