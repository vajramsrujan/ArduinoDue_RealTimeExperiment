// The following program carries out a predictive peak detection routine for peaks or troughs of the Theta brainwave. 
// It is used in conjunction with an Arduino Due and an SD card, to store real-time data externally (and not on flash) 

// ********************************************************************************************* //
// LIBRARIES

include <SdFat.h>
SdFat sd;

// ********************************************************************************************* //
// USER-DEFINED VARIABLES
const boolean peakDetection = true; // == true for peak detection; == false for trough detection
const int refractoryLength = 5;     // Set how long the stimulation should last (5 milliseconds)
const int sDsize = 150;             // Size of the buffer that holds the output points
const int inputPin = A0;            // Sets the analog input pin value. 
const int outputPin = 51;           // Sets the output pin (i.e the stimulation pin)
int STOP_ITER = 100000;             // DETERMINES HOW LONG THE EXPERIMENT SHALL RUN (100,000 loop iterations ~ 1.66 minutes )
                                    // This can be changed to run the experiment longer 

// ********************************************************************************************* //
// FIR FILTER

// A low pass software filter 
float filterFIR[] = {0.00058369,0.00068901,0.00080479,0.0009313,0.0010688,0.0012176,0.0013778,0.0015495,0.0017329,0.0019281,0.002135,0.0023536,0.0025839,0.0028257,0.0030788,0.003343,0.003618,0.0039035,0.0041992,0.0045044,0.0048189,0.005142,0.0054731,0.0058117,0.006157,0.0065083,0.0068649,0.007226,0.0075906,0.0079581,0.0083273,0.0086976,0.0090677,0.0094369,0.0098041,0.010168,0.010529,0.010884,0.011233,0.011575,0.011909,0.012235,0.01255,0.012854,0.013146,0.013426,0.013692,0.013943,0.01418,0.0144,0.014603,0.01479,0.014958,0.015108,0.015238,0.01535,0.015441,0.015513,0.015564,0.015595,0.015606,0.015595,0.015564,0.015513,0.015441,0.01535,0.015238,0.015108,0.014958,0.01479,0.014603,0.0144,0.01418,0.013943,0.013692,0.013426,0.013146,0.012854,0.01255,0.012235,0.011909,0.011575,0.011233,0.010884,0.010529,0.010168,0.0098041,0.0094369,0.0090677,0.0086976,0.0083273,0.0079581,0.0075906,0.007226,0.0068649,0.0065083,0.006157,0.0058117,0.0054731,0.005142,0.0048189,0.0045044,0.0041992,0.0039035,0.003618,0.003343,0.0030788,0.0028257,0.0025839,0.0023536,0.002135,0.0019281,0.0017329,0.0015495,0.0013778,0.0012176,0.0010688,0.0009313,0.00080479,0.00068901,0.00058369}; 
const int cBufferLength = sizeof(filterFIR)/sizeof(filterFIR[0]); // Setting buffer size (explained below)  
float inputBuffer[cBufferLength];                                 // A buffer which will hold raw analog input (LF data) prior to convolution

//*********************************************************************************************//
// GLOBAL VARIABLE SETUP

SdFile dataFile;           // Initializes the file which will hold Byte data

const int chipSelect = 4;  // Sets SD card to read-write

float ADCValueBinary = 0;  // Variable used to grab analog input from the Arduino
int dataIndex = 0;         // To index through the data buffer sent to the SD card. 
int loopNumber = 0;        // Keeps track of how many times void loop has executed (used in conjunction with SD card writing) 
int cyclesUntilStim = -1;  // Keeps track of how many cycles are left until stimulation
int refractoryCount = 0;   // Keeps track of how long it has been since the stimulation has begun
int startPredictCycle = 0; // Holds a reference to the last peak or trough of interest to calculate the prediction time
int stimulate = 0;         // Holds the current stimulation state (i.e ON [non-zero], or OFF [zero])
int threshold = 30;        // An optional threshold factor: Can be used to limit processing on the wave below a certain ceiling voltage
int dataSend[sDsize];      // Holds stimulation and current voltage point data as ints (for stimulation: 1 = ON, 0 = OFF)
int cycleIndex = 0;        // Keeps track of the current millisecond cycle

float currentTime = 0;     // Variable used to keep track of whether 1 millisecond has passed prior to re-executing void loop 
float startTime = 0;       // Holds timestamp at the beginning of each loop iteration
float globalPeak = -9999;  // Global peak value
float globalTrough = 9999; // Global trough value
float prev_Point;          // Stores the value of the previous 'convolved' point
float curr_Point;          // Stores the value of the current 'convolved' point
float curr_Slope;          // Holds the current slope value between current voltage point and last voltage point
float prev_Slope;          // Holds the previous slope value (of the previous and second last point)  

//*********************************************************************************************//
void setup() {

  // Initialize Serial port
  Serial.begin(9600);
  while (!Serial) {}  // wait for Leonardo
  Serial.println("Type any character to start");
  while (Serial.read() <= 0) {}
  delay(400);         // catch Due reset problem
   
  // ========================================================================== //
  pinMode(outputPin, OUTPUT); // Sets outputPin
  pinMode(inputPin, INPUT);   // Sets inputPin
  
  // Initialize the point values before they are updated 
  curr_Point = 0;             // Initialize the current convolved value to '0'
  prev_Point = 0;             // Initialize the previous convolved value to '0'
  prev_Slope = 0;             // Set the first 'slope' as zero

  // ========================================================================== //

  // Create a new file 'dataFile' to store stimulation and current LFP (voltage point) data
  if (!sd.begin(chipSelect, SPI_HALF_SPEED)) sd.initErrorHalt();

  // open the file for write at end like the Native SD library
  if (!dataFile.open("data.txt", O_RDWR | O_CREAT | O_AT_END)) {
    sd.errorHalt("opening data.txt for write failed");
    }
  }
}

// ********************************************************************************************* //
// MAIN PREDICTIVE PEAK DETECTION ROUTINE

void loop() {

  startTime = micros();
  loopNumber += 1;

  // Store the last 'current point values' (in the last loop iteration) as the present current point values. 
  prev_Point = curr_Point;                                 // Update the last point
  ADCValueBinary = analogRead(inputPin);                   // Read new analog input
  inputBuffer[loopNumber%cBufferLength] = ADCValueBinary;  // Fill the buffer with analog input 
  
  // ========================================================================== //
 
  if(loopNumber>=cBufferLength) {           // Once the buffer is full
    
    for(int i = 0; i<cBufferLength; i++) {  // Carry out the convolution to extract low frequency LFP data
      curr_Point += filterFIR[i]*inputBuffer[(loopNumber-i)%cBufferLength];
    }
  }
  
  curr_Slope = (curr_Point - prev_Point);   // Acquire the slope (i.e y2 - y1 / 1)
  // ========================================================================== //
  
  if ( (curr_Slope < 0) and (prev_Slope > 0) and (prev_Point > globalPeak) and (prev_Point > threshold)) { // If the slope change indicates a possible peak
                                                                                                           // and we are above a voltage threshold
    globalPeak = prev_Point;                // Update the global peak                                      // and the peak is greater than the last global peak
    
    if (peakDetection == true) {            // If we are in peak detection mode
      globalTrough = 9999;                  // Initialize the trough value to a big number (so that we can capture new troughs after the global peak) 
      startPredictCycle = cycleIndex;       // Keep a reference to the millisecond cycle where the peak was detected 
    }
    
    // This else statement is used when we are in TROUGH detection mode
    // We use information about where relevant peaks occur to improve the accuracy of Trough detection (since twice the interval between a succesive 
    // trough and peak is a good predictor of the period between troughs.) 
     else if (  ((cycleIndex - startPredictCycle) > 41.66667) and ( (cycleIndex - startPredictCycle) < 125)  ) {   // Else if the peak is between 4 and 12 Hz (Theta) 
      cyclesUntilStim = (cycleIndex - startPredictCycle) * 2;                                                      // Compute the number of cycles to stimulation                                                     
    }
  }
  
  // ========================================================================== //
  
  else if ( (curr_Slope > 0) and (prev_Slope < 0) and (prev_Point < globalTrough) and (prev_Point < -threshold)) { // If the slope change indicates a possible trough
                                                                                                                   // and we are above a voltage threshold
    globalTrough = prev_Point;                                                                                     // and the trough is smaller than the last global trough

    if(peakDetection==false) {
      globalPeak = -9999;
      startPredictCycle = cycleIndex;
    }
    
    // This else statement is used when we are in PEAK detection mode
    // We use information about where relevant troughs occur to improve the accuracy of Peak detection (since twice the interval between a succesive 
    // peak and trough is a good predictor of the period between peaks.) 
    else if (  ((cycleIndex - startPredictCycle) > 41.66667) and ( (cycleIndex - startPredictCycle) < 125)  ) {   // If the trough lies between 4 and 12 Hz
      cyclesUntilStim = (cycleIndex - startPredictCycle) * 2;                                                     // Compute the number of cycles to stimulation
    }
  }
  // ==========================================================================//
  
  // Determines whether we are in a stimulation cycle or not
  if(refractoryCount > 0) { // If we are in a stimulation cycle
    refractoryCount--;      // Reduce the refractory count 
    
    //STIMULATE
    stimulate = 1;
    digitalWrite(outputPin, HIGH);
  }
  
  else if(cyclesUntilStim == 0) { // Else if we just reached the start of a stimulation cycle
    globalPeak = -9999;           // Reinitialize the global peaks and troughs 
    globalTrough = 9999;
    refractoryCount = refractoryLength - 1; // Set the refractory count to the stimulation period 
    
    //STIMULATE
    stimulate = 1;
    cyclesUntilStim = -1;                   // Reset the cycle number until stimulation 
  }
  else {                                    // Otherwise we are not in a stimulation cycle, so carry on
    //NO STIMULATE
    stimulate = 0;                      
    digitalWrite(outputPin, LOW);
  }

  cyclesUntilStim--;                        // Reduce the number of cycles left until stimulation 
  cycleIndex++;                             // Cycle index goes up by 1 to reflect 1 millisecond has passed

  // ==========================================================================//
  // SD CARD WRITING DATA 
  
  if (dataIndex != sDsize-1) {              // If the data buffer for stimulation data is not full
    dataSend[dataIndex] = cycleIndex;       // Fill the buffer
    dataIndex++;                            
    dataSend[dataIndex] = (int) curr_Point * 10000; // The buffer carries information about the current voltage point
    dataIndex++;                                    
    dataSend[dataIndex] = stimulate;                // and whether we are stimulating 
  }
  
  if (dataIndex >= sDsize-1) {                      // If the buffer is full
    dataFile.write(dataSend,sDsize);                // Send the data to the SD card as byte data
    dataIndex = 0;                                  // Reset the data index to zero 
    
  }
  
  if (loopNumber >= STOP_ITER) {                    // If we have reached the specified loop number
    Serial.println("Program Done!");                
    dataFile.close();
    Serial.end();                                   // End the program 
    }

  while (currentTime - startTime < 1000) {          // Ensures that we have waited exactly 1 millisecond before executing next loop cycle
    currentTime = micros();
  }
  // ==========================================================================// 
}
//*********************************************************************************************//
