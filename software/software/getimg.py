# -*- coding: utf-8 -*-
"""
Created on Tue Aug 19 01:40:03 2014

@author: fl@c@
"""
import matplotlib.pyplot as plt
import serial
import struct
import time
import numpy as np
import os

fname = 'spectra.dat'																																	# set the file name to spectra.dat
fmode = 'ab'
read_mode = 'textual'
ping_pong_com = 1
uC_endianity = 'little_endian'
uC_ADC_bit_count = 12
uC_ADC_max_val = pow(2, uC_ADC_bit_count)-1

data = []

def killOldData():																																	# set the file mode to append binary
     global data
     data = []

def getSpectra3():
    global data    
    with serial.Serial('COM8', 230400, serial.EIGHTBITS, serial.PARITY_NONE, serial.STOPBITS_ONE, 0.1) as mcu: 												# setup serial for communication to Nucelo

        chunk_len = 1000;
        total_len = 7296;
        to_read = chunk_len;  
        buffer = bytes
        main_bfr = b''
        read = 0
        errors = 0
        i = 0        
        all_read = 0
        
        mcu.flush()
        mcu.write(b'g')  
        
        
       # time.sleep(0.024)
        while all_read == 0:
            if i == int(total_len/chunk_len):
                to_read = total_len%chunk_len;
            buffer = mcu.read(to_read)
            read = len(buffer)
            if read == to_read:
                time.sleep(0.002)
                mcu.flush()
                #print("writing y cycle: %d" %i)
                mcu.write(b'y')
                #print(" --written");
                i = i+1
                main_bfr = main_bfr + buffer
            else:
                #print("Read only %dB of %d" % (read,to_read))
                time.sleep(0.002)
                mcu.flush()
                #print("writing r cycle: %d" %i)
                mcu.write(b'r')
                #print(" --written");
                errors = errors+1
            if i == int(total_len/chunk_len+1):
                all_read = 1
        
        print ("errors: ", errors)
        #print("read: %d" %len(main_bfr))
        fmt = "<%dh" % (3648)
        data = struct.unpack(fmt, main_bfr)
        mcu.write(b's')
        


def plotSpectra():
	x1 = []                                                                 																					# initialize the X coord
	y1 = []                                                                 																					# initialize the y coord
	curx = 0
     
	for intensity in data:                                                     																				# set the array for y
          x1.append(curx)
          curx += 1
          y1.append(intensity)

	fig = plt.figure(1)
	ax = fig.add_subplot(111, axisbg='black')
	rect = fig.patch # a rectangle instance
	rect.set_facecolor('lightslategray')	
	ax.autoscale_view()
	plt.title(plot_Title, fontsize=24, color='black')
	plt.xlabel('pixelNumber', fontsize=16, color='red')																				# set the x label on the graph
	plt.ylabel('intensity', fontsize=16, color='blue')																		# set the y label on the graph
	xv = np.array(x1)                                                       																				# set the array for x
	yv = np.array(y1)
	plt.plot(xv, yv, color = 'white', lw=1)                                                        													# plot the data
	plt.show()                                                              																					# show the graph

def getNewSample():
    with serial.Serial('COM8', 230400, serial.EIGHTBITS, serial.PARITY_NONE, serial.STOPBITS_ONE, 0.1) as mcu: 												# setup serial for communication to Nucelo
        mcu.write(b's') #tell mcu to get a sample
        
def startSpectraCapture():
            print ("Aquiring spectra......")
            getSpectra3()
            #if (ping_pong_com == 0):
             #  getSpectraFast()
            #else:
             #  getSpectraPingPong()
		#print ("Reading specta......")
		#readSpectra()
            print ("Plotting spectra.....")
            plotSpectra()
            print ("Done...")


plot_Title = 'meridianScientific DIY 3D Printable RaspberryPi Raman Spectrometer'
os.system('cls')
getNewSample()

while(1):
     #os.system('cls')
     print ("meridianScientific DIY 3D Printable RaspberryPi Raman Spectrometer")
     print ("SpectraSide_01  by fl@c@")
     print ("")
     killOldData()
     print ("0 - Quit")
     print ("1 - Get sample and plot")
     print ("2 - Get new sample from CCD")
     req = input("Enter your choice..")
     
     print(req)
     if req == "1":
          startSpectraCapture()
     if req == "1":
          getNewSample()
     if req == "0":
          print ("Quitting")
          break