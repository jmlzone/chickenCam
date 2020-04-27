# chickenCam

This is a arduino sketch for a esp23cam based on the ususal example.
changes:
1) ripped out facial rec.
2) stream starts when connected.
3) support for a 1 wire temperature sensor.
4) pan and tilt serco control.
5) LED flashlight control.

Note: I modified the esp32 servo (and analog write) to start at timer 2 to co-exist with camera.

clicking to the outside of the window moves the camera in that
direction (including diagonals) clicking the center increases the led
brigtness.


