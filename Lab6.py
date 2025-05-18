import time
import serial
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import sys, math
# Number of lines to keep in the serial viewer
MAX_SERIAL_LINES = 10

# Global variables to store the latest measurements
C = 0.0
C_saved = [0.0] * 30
factor = 136752.136752

# Buffer for recent serial lines
lines_buffer = []

def process_serial():
    global C, C_saved, lines_buffer, frequency 
    
    try:
        # Read a line from the serial port and decode it
        strin = ser.readline()
        line_str = strin.decode("utf-8").rstrip()  # Remove trailing newline
        if line_str:
            # Append the line to the buffer and maintain its size
            print(line_str)
            lines_buffer.append(line_str)
            if len(lines_buffer) > MAX_SERIAL_LINES:
                lines_buffer.pop(0)
            
            # Split the line using space as the delimiter
            datain = line_str.split(",")
            # Expecting 31 tokens: 1 for C and 30 for C_saved
            if len(datain) >= 31:
                C = float(datain[0])
                C_saved = [float(val) for val in datain[1:31]]
                frequency = factor / C 

                # Print the values to test
                # print("Received line:", line_str)
                # print("C =", C)
                # print("C_saved =", C_saved)
            # else:
            #     print("Error: Expected 31 values but received", len(datain))
            #     print("Line content:", line_str)
            if line_str.startswith("Enter the Capacitance"):
                user_input = input("Capacitance: ")
                ser.write((user_input).encode())
                ser.flush()
                
            if line_str.startswith("Enter the Error"):
                user_input = input("Error: ")
                ser.write((user_input + "\r\n").encode())  
                ser.flush()
            
            

    except Exception as e:
        print("Error processing serial line:", e)

# -----------------------------------
# Configure the serial port
# -----------------------------------
ser = serial.Serial(
    port='COM19',          # Adjust this to match your serial port
    baudrate=115200,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_TWO,
    bytesize=serial.EIGHTBITS,
    timeout=1              # 1-second timeout for readline()
)

x = np.linspace(0, 0.1, 100000)

if ser.isOpen():
    print("Serial port opened successfully.")

try:
    while True:
        process_serial()
        time.sleep(0.05)  # Adjust the delay as needed
except KeyboardInterrupt:
    print("Exiting...")
finally:
    ser.close()
