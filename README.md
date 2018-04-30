# Laser-Tracking

Code
====

Three folders in this directory:
* CameraTracking: Contains code that does corse tracking on RasPi
* Transmitter: Contains code that runs on photon plugged inito receiver PCBs
* Receiver: Contains code that runs on photon controlling servos and MEMS


Flashing Photon Code
====================

* Download Particle CLI on Mac or Linux. See Particle help for details.
* Place Photon in DFU mode by holding down both buttons, release Setup button. Light should blink purple, continue to hold the Reset button until light blinks yellow.
* Run ```particle photon compile --saveTo out.bin .``` in directory with *.ino file.
* Run ```particle flash --usb out.bin```
* When program is running, particle light should be constant white. If the light blinks green or blue, something is wrong, check that system mode is set to manual in code and that photon firmware is up to date.
