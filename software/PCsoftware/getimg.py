# -*- coding: utf-8 -*-

import matplotlib.pyplot as plt
import serial
import struct
import time
import numpy as np
import os




uC_endianity = 'little_endian'
uC_ADC_max_val = 65536-1
signalElements = 3648

xaxis = range(0, signalElements)
data = [0]*signalElements
line = ""

#470588#921600#460800
mcu = serial.Serial('COM8', 460800, serial.EIGHTBITS, serial.PARITY_NONE, serial.STOPBITS_TWO, timeout=0.04, writeTimeout=1, xonxoff=False, rtscts=True)


def killOldData():																																	# set the file mode to append binary
     global data
     data = [0]*signalElements

def toMenu():
  inMenu = False
  while not inMenu:
    mcu.write(b'm')
    while mcu.inWaiting() > 0:
      a = mcu.readline()
      #print(a)
      if b"Menu" in a:
        #print("In menu")
        inMenu = True
        break
  if mcu.inWaiting() > 0:
    while mcu.inWaiting() > 0:
      mcu.read(mcu.inWaiting())
      time.sleep(0.02)

def getSpectra3():
        global data    
    #with serial.Serial('COM8', 921600, serial.EIGHTBITS, serial.PARITY_NONE, serial.STOPBITS_ONE, timeout=0.02) as mcu: 												# setup serial for communication to Nucelo

        chunk_len = 522;
        total_len = 7296;
        to_read = chunk_len;
        buffer = b''
        main_bfr = b''
        read = 0
        errors = 0
        i = 0        
        all_read = 0
        delay_t = 0.005
        
        
        mcu.write(b'g')
        while True:
          a = mcu.readline()
          if b"Sending data." in a:
            break
          if mcu.inWaiting() == 0:
            errors = errors + 1
            time.sleep(0.02)
          if errors > 5:
            print("Failed to sync.")
            toMenu()
            return False
        
        
       # time.sleep(0.024)
        while not all_read:
            if i == int(total_len/chunk_len):
                to_read = total_len%chunk_len
            
            read_ok = True
           
            
            mcu.write(b'c')
            a = b""
            mcu.flush()
            mcu.flushInput()
            mcu.flushOutput()
            mcu.write(bytes([i]))
            #print(b"Written: " + bytes([i]))
            time.sleep(0.001)
            
            buffer = mcu.read(1)
            if (len(buffer) == 0 or buffer[0] != i):
                read_ok = False
                if (len(buffer) != 0):
                  print("Chunk number read: %d, should be: %d. Data remaining: %d" % (buffer[0], i, mcu.inWaiting()))
                  print("remaining: " + str(mcu.read(mcu.inWaiting())))
                else:
                  print("Chunk number timed out in c. %d" % i)
            else:
                buffer = mcu.read(to_read)
                read = len(buffer)
            
            if read_ok and read == to_read:
                checksum = mcu.read(1)
                if (len(checksum) == 0 or checksum[0] != (sum(buffer)+1)%256):
                  read_ok = False
                  if (len(checksum) != 0):
                    print("Checksum read: %d, should be: %d. Data remaining: %d" % (checksum[0], (sum(buffer)+1)%256, mcu.inWaiting()))
                    mcu.read(mcu.inWaiting())
                  else:
                    print("Checksum timed out")
            else:
                if (read_ok):
                  print("Read only %dB of %dB in cycle %d" % (read,to_read,i))
                read_ok = False
                
            if read_ok:
                #mcu.flushInput()
                #mcu.flushOutput()
                #time.sleep(delay_t)
               # print("Cycle: %d OK" %i)
                #resp = (b'y')
                #time.sleep(delay_t)
                #print(" --written");
                i = i+1
                main_bfr = main_bfr + buffer
            else:
                errors = errors+1
                print("ERROR")
                toMenu()
                return False
                #print("Read only %dB of %d in cycle %d" % (read,to_read,i))
                #mcu.read(mcu.inWaiting())
                #mcu.flushInput()
                #mcu.flushOutput()
                #print("writing r cycle: %d" %i)
                #time.sleep(delay_t)
                #resp = (b'r')
                #time.sleep(delay_t)
                #print(" --written");
            if i == int(total_len/chunk_len+1):
                all_read = True
        
        toMenu()
        #print("read: %d" %len(main_bfr))
        fmt = "<%dH" % (3648)
        data = struct.unpack(fmt, main_bfr)
        mcu.write(b's')
        return True

def moveon(event):
    plt.close()

fig = None
line = None
subplot = None
plot_bg = None
def setupPlot():
    global fig
    global line
    global subplot
    global plot_bg
    plt.ion()
    fig = plt.figure()
    subplot = fig.add_subplot(1, 1, 1, axisbg='black')
    plt.title(plot_Title, fontsize=24, color='black')
    plt.xlabel('pixelNumber', fontsize=16, color='red')																				# set the x label on the graph
    plt.ylabel('intensity', fontsize=16, color='blue')
    plt.ylim([0,uC_ADC_max_val])
    plt.xlim([0,signalElements])
    
    line, = subplot.plot(xaxis, data, 'r-', color = 'white')
    #subplot.hold(True)
    fig.canvas.draw()  
    fig.show(False)
    plot_bg = fig.canvas.copy_from_bbox(subplot.bbox)
    # cache the background

def replotSpectra():
    global fig
    global line
    global subplot
    global plot_bg
#    line.set_xdata(xaxis)
    line.set_ydata(data)  # update the data
    # restore background
    fig.canvas.restore_region(plot_bg)
    # redraw just the graph
    subplot.draw_artist(line)
    # fill in the axes rectangle
    fig.canvas.blit(subplot.bbox)
    
    #fig.canvas.draw()#.plot(xaxis, data, color = 'white', lw=1)
    plt.pause(0.0001)
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
    errors = 0
    frames = 0
    
    while (True):
        frames = frames + 1
        start_T = time.clock()
        if getSpectra3():
          #print(data)
          replotSpectra()
        else:
          errors = errors + 1
        print("Errornes: %f%%"%(errors/(frames/100)))
        delta_T = time.clock() - start_T
        print ("FPS: %fHz" % (1/delta_T))

def startSpectraCapture():
            print ("Aquiring spectra......")
            while not getSpectra3():
              print("Retrying")
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
toMenu()
getNewSample()
toMenu()

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