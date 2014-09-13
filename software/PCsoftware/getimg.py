# -*- coding: utf-8 -*-

import matplotlib.pyplot as plt
import serial
import struct
import time
import numpy as np
import os




uC_endianity = 'little_endian'
uC_ADC_bit_count = 12
uC_ADC_max_val = pow(2, uC_ADC_bit_count)-1
signalElements = 3648

xaxis = range(0, signalElements)
data = [0]*signalElements
line = ""


mcu = serial.Serial('COM8', 921600, serial.EIGHTBITS, serial.PARITY_NONE, serial.STOPBITS_TWO, timeout=0.02, xonxoff=False, rtscts=True)


def killOldData():																																	# set the file mode to append binary
     global data
     data = [0]*signalElements

def getSpectra3():
        global data    
    #with serial.Serial('COM8', 921600, serial.EIGHTBITS, serial.PARITY_NONE, serial.STOPBITS_ONE, timeout=0.02) as mcu: 												# setup serial for communication to Nucelo

        chunk_len = 180;
        total_len = 7296;
        to_read = chunk_len;  
        buffer = bytes
        main_bfr = b''
        read = 0
        errors = 0
        i = 0        
        all_read = 0
        delay_t = 0.005
        
        mcu.flush()
        mcu.flushInput()
        mcu.flushOutput()
        mcu.write(b'g')
        time.sleep(delay_t)
        
        
       # time.sleep(0.024)
        while all_read == 0:
            if i == int(total_len/chunk_len):
                to_read = total_len%chunk_len
            
            buffer = mcu.read(1)
            #if len(buffer) != 0:
             #   print(buffer[0])
            
            if (len(buffer) == 0 or buffer[0] != i):
                read = 0
            else:
                buffer = mcu.read(to_read)
                read = len(buffer)
            
            if read == to_read:
                mcu.flushInput()
                mcu.flushOutput()
                time.sleep(0.002)
               # print("Cycle: %d OK" %i)
                mcu.write(b'y')
                time.sleep(delay_t)
                #print(" --written");
                i = i+1
                main_bfr = main_bfr + buffer
            else:
               # print("Read only %dB of %d in cycle %d" % (read,to_read,i))
                #mcu.read(mcu.inWaiting())
                mcu.flushInput()
                mcu.flushOutput()
                #print("writing r cycle: %d" %i)
                mcu.write(b'r')
                time.sleep(delay_t)
                #print(" --written");
                errors = errors+1
            if i == int(total_len/chunk_len+1):
                all_read = 1
        
        print ("errors: ", errors)
        #print("read: %d" %len(main_bfr))
        fmt = "<%dh" % (3648)
        data = struct.unpack(fmt, main_bfr)
        mcu.write(b's')

def moveon(event):
    plt.close()

fig = None
line = None
def setupPlot():
    global fig
    global line
    plt.ion()
    fig = plt.figure()
    subplot = fig.add_subplot(1, 1, 1, axisbg='black')
    plt.title(plot_Title, fontsize=24, color='black')
    plt.xlabel('pixelNumber', fontsize=16, color='red')																				# set the x label on the graph
    plt.ylabel('intensity', fontsize=16, color='blue')
    plt.ylim([0,uC_ADC_max_val])
    plt.xlim([0,signalElements]) 

    line, = subplot.plot(xaxis, data, 'r-', color = 'white')
    plt.show(block=False)

def replotSpectra():
    global fig
    global line
#    line.set_xdata(xaxis)
    line.set_ydata(data)  # update the data
    fig.canvas.draw()#.plot(xaxis, data, color = 'white', lw=1)
    #plt.draw() # update the plot

def plotSpectra():
	x1 = []                                                                 																					# initialize the X coord
	y1 = []                                                                 																					# initialize the y coord
	curx = 0
     
	for intensity in data:                                                     																				# set the array for y
          x1.append(curx)
          curx += 1
          y1.append(intensity)

	fig = plt.figure()
	fig.add_subplot(1, 1, 1, axisbg='black')
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
    #with serial.Serial('COM8', 230400, serial.EIGHTBITS, serial.PARITY_NONE, serial.STOPBITS_ONE, 0.1) as mcu: 												# setup serial for communication to Nucelo
        mcu.write(b's') #tell mcu to get a sample

def continuousPlot():
    setupPlot()
    while (True):
        getSpectra3()
        replotSpectra()

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

mcu.sendBreak()
os.system('cls')
getNewSample()


#plt.ion()

while(1):
     #os.system('cls')
     print ("meridianScientific DIY 3D Printable RaspberryPi Raman Spectrometer")
     print ("SpectraSide_01  by fl@c@")
     print ("")
     killOldData()
     print ("0 - Quit")
     print ("1 - Get sample and plot")
     print ("2 - Get new sample from CCD")
     print ("3 - Plot continuously")
     req = input("Enter your choice..")
     
     print(req)
     if req == "1":
          startSpectraCapture()
     if req == "2":
          getNewSample()
     if req == "3":
          continuousPlot()
     if req == "0":
          print ("Quitting")
          break

mcu.close()