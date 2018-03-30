#!/usr/bin/env python

import serial
import sys

baudRate = 14400
neutralBaudRate = 9600
portName = "/dev/ttyACM0"

if len(sys.argv) > 1:
  baudRate = int(sys.argv[1])

if len(sys.argv) > 2:
  portName = sys.argv[2]

try:
  ser = serial.Serial(portName, baudRate)
  ser.close()
except:
  print ("exception caught while opening serial port")
