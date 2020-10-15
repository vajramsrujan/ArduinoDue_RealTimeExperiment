# CREDIT
This was a joint effort between me, and my fellow B.U students Carlo Taglietti (ctagliet@bu.edu) and Amina Zidane (zidane@bu.edu)

# OVERVIEW: 
Our implementation aims to solve two challenging problems in the context of Real-Time Optogenetic Experimentation: 

1. How can we detect physiologically relevant features of LFP data in Real_Time under the constraint of filter delay?
2. How can we succesfully implement such an algorithm on a single board micrcontroller  instead of an Operating System running a dedicated Real-time kernel (such as RTXI?) 

The following program 'RealTime_Detector.ino' is a predictive peak / trough detection routine for the purpose of capturing physiologically relevant signal attributes in Local Field Potential Data. Specifically, this project is aimed at real-time prediction and stimulation of peaks and troughs for the Theta brainwave (between 4 and 12 Hz.) 

# TECHNICAL DOCUMENTS 
You can find the technical specifications of the filters and performance of the Arduino Due (on simulated LFP data) in the 'Platform Guide.' 

# REQUIRED LIBRARIES
This script requires the installation of the SdFat library which can be found here: 
https://www.arduinolibraries.info/libraries/sd-fat

# SD CARD SCRIPT
You can find the relevant SD card script titled 'SDFormatter.ino' that is used to format the SD card when necessary. This script can be found in the Arduino IDE library under 'examples' after downloading SdFat. 

# COVID 19 CONSEQUENCES
Due to COVID 19, we were unable to test our board in the lab with live LFP data from mice. As an alternative, we utilized artificial LFP data fed to the Arduino to simulate real-time acquisition. 

# DEMONSTRATION
A simple example of what this script is doing can be found here: 
https://www.youtube.com/watch?v=Uwm83XhcJ_A
