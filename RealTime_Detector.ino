//#include <SPI.h>
//#include <SD.h>


#include <SdFat.h>
SdFat sd;

//USER-DEFINED VARIABLES
//*********************************************************************************************//
const boolean peakDetection = true; //== true for peak detection; == false for trough detection
const int refractoryLength = 5; // Set how long the stimulation should last (5 seconds)
const int sDsize = 150;         // Size of the buffer that holds the output points
const int inputPin = A0;        // Sets the analog input pin value. 
const int outputPin = 51;       // Sets the output pin (i.e the stimulation pin)
int STOP_ITER = 10000;                // DETERMINES HOW LONG THE EXPERIMENT SHALL RUN (10,000 loop iterations)
                                     // This can be changed to run the experiment longer 

//FIR FILTER
//*********************************************************************************************//

float filterFIR[] = {0.00058369,0.00068901,0.00080479,0.0009313,0.0010688,0.0012176,0.0013778,0.0015495,0.0017329,0.0019281,0.002135,0.0023536,0.0025839,0.0028257,0.0030788,0.003343,0.003618,0.0039035,0.0041992,0.0045044,0.0048189,0.005142,0.0054731,0.0058117,0.006157,0.0065083,0.0068649,0.007226,0.0075906,0.0079581,0.0083273,0.0086976,0.0090677,0.0094369,0.0098041,0.010168,0.010529,0.010884,0.011233,0.011575,0.011909,0.012235,0.01255,0.012854,0.013146,0.013426,0.013692,0.013943,0.01418,0.0144,0.014603,0.01479,0.014958,0.015108,0.015238,0.01535,0.015441,0.015513,0.015564,0.015595,0.015606,0.015595,0.015564,0.015513,0.015441,0.01535,0.015238,0.015108,0.014958,0.01479,0.014603,0.0144,0.01418,0.013943,0.013692,0.013426,0.013146,0.012854,0.01255,0.012235,0.011909,0.011575,0.011233,0.010884,0.010529,0.010168,0.0098041,0.0094369,0.0090677,0.0086976,0.0083273,0.0079581,0.0075906,0.007226,0.0068649,0.0065083,0.006157,0.0058117,0.0054731,0.005142,0.0048189,0.0045044,0.0041992,0.0039035,0.003618,0.003343,0.0030788,0.0028257,0.0025839,0.0023536,0.002135,0.0019281,0.0017329,0.0015495,0.0013778,0.0012176,0.0010688,0.0009313,0.00080479,0.00068901,0.00058369}; 
const int cBufferLength = sizeof(filterFIR)/sizeof(filterFIR[0]);
float inputBuffer[cBufferLength];

// This code runs a logic for peak and trough detection, by using a predictive method.
//*********************************************************************************************//
// GLOBAL VARIABLE SETUP

SdFile myFile;                       // Initializes SD card file to ready writing
SdFile dataFile;                     // Holds Byte data
bool beganPrediction = false;        // Indicator as to whether we have already started a prediction cycle or not

const int chipSelect = 4;
const int bufferSize = 8;       // Size of the buffer that holds the convolved points

float ADCValueBinary = 0;       // Variable used to grab analog input from the Arduino
int bufferIndex = 0;            // Used to point to different values in the buffer.
int dataIndex = 0;              // To index through the data buffer sent to the SD card. 
int count = 0;                  // Counter used for initialization
int loopNumber = 0;             // Keeps track of how many times void loop has executed (used in conjunction with SD card writing) 
int cyclesUntilStim = -1;  
int refractoryCount = 0;
int startPredictCycle = 0;
int stimulate = 0;          // Holds the current stimulation state (i.e on [non-zero], or off [zero])
int threshold = 30;         // An optional threshold factor: Can be used to limit processing on the wave below a certain ceiling voltage
int dataSend[sDsize];       // Holds data as ints
int cycleIndex = 0;         // Keeps track of the current millisecond cycle

float currentTime = 0;     // Holder variable used to keep track of critical time points
float startTime = 0;       // Holds timestamp at the beginning of each loop iteration
float globalPeak = -9999;  // Global peak time
float globalTrough = 9999; // Global trough time
float prev_Point;          // Stores the value of the previous 'convolved' point
float curr_Point;          // Stores the value of the current 'convolved' point
float prev_Slope;          // Holds the previous slope value
float curr_Slope;          // Holds the current slope value

//*********************************************************************************************//
void setup() {

  Serial.begin(9600);
  while (!Serial) {}  // wait for Leonardo
  Serial.println("Type any character to start");
  while (Serial.read() <= 0) {}
  delay(400);  // catch Due reset problem
  
  // ==========================================================================//
  // This part of the code is used to load the entire data window to test the ARTIFICIAL LFP
  // into an array we can use. In other words, all 23 seconds of data is stored locally in an array. 
  // We can simulate a 1KHz sampling rate by 'picking' data points from this float array every 1 ms.
  
  // =================================================//
  Serial.print("Initializing SD card...");                        // We initialize the SD card

  if (!sd.begin(chipSelect, SPI_HALF_SPEED)) sd.initErrorHalt();
  if (!myFile.open("LFP_DATA.txt", O_READ)) {                     // Open file for reading
    sd.errorHalt("LFP_DATA.txt file opening failed.");            // If file fails to open, throw an error. 
  } else {   
 
  // ==========================================================================//
  pinMode(outputPin, OUTPUT); // Sets outputPin
  pinMode(inputPin, INPUT);   // Sets inputPin
  // Initialize the point values before they are updated 
  curr_Point = 0;             // Initialize the current convolved value to '0'
  prev_Point = 0;             // Initialize the previous convolved value to '0'
  prev_Slope = 0;             // Set the first 'slope' as zero

  // Initializes the prediction times to zero (the prediction subroutine has not started.)
  // These values will later be updated to a non-zero number before they are used for stimulaton
  // Current_predTime = 0;
  // ==========================================================================//
  // This section of the code is meant to initialize the global peak and trough times,
  // and the 'states' (i.e are we at peaks or troughs.) Since we are only concerned about
  // initialization, this segment is not concerned with whether the peaks and troughs fall
  // within the 4Hz to 20Hz range. We run the code for 3 seconds to initialize these values.
  // ==========================================================================//

  if (!sd.begin(chipSelect, SPI_HALF_SPEED)) sd.initErrorHalt();

  // open the file for write at end like the Native SD library
  if (!dataFile.open("data.txt", O_RDWR | O_CREAT | O_AT_END)) {
    sd.errorHalt("opening data.txt for write failed");
    }
  }
}
//*********************************************************************************************//
void loop() {

  
  startTime = micros();
  loopNumber += 1;

  
  // Store the last 'current point values' (in the last loop iteration) as the present current point values. 
  prev_Point = curr_Point;                                 // Update the last point
  ADCValueBinary = analogRead(inputPin);                   // Read new analog input
  inputBuffer[loopNumber%cBufferLength] = ADCValueBinary;

  // Convolution
  if(loopNumber>=cBufferLength) {
    //convolution
    for(int i = 0; i<cBufferLength; i++) {
      curr_Point += filterFIR[i]*inputBuffer[(loopNumber-i)%cBufferLength];
    }
  }
  
  curr_Slope = (curr_Point - prev_Point);                  // Get the slope (i.e y2 - y1 / 1)
  
  if ( (curr_Slope < 0) and (prev_Slope > 0) and (prev_Point > globalPeak) and (prev_Point > threshold)) {         // If the slope change indicates a possible peak

    globalPeak = prev_Point;
    
    if (peakDetection == true) {
      globalTrough = 9999;
      startPredictCycle = cycleIndex;
    }
    
     else if (  ((cycleIndex - startPredictCycle) > 41.66667) and ( (cycleIndex - startPredictCycle) < 125)  ) {   // If the wave lies between 4 and 12 Hz
      cyclesUntilStim = cycleIndex - startPredictCycle;
      if(cyclesUntilStim < 0) {
        cyclesUntilStim = 0;
      }
    }
  }
  
  else if ( (curr_Slope > 0) and (prev_Slope < 0) and (prev_Point < globalTrough) and (prev_Point < -threshold)) { // If the slope change indicates a possible trough

    globalTrough = prev_Point;

    if(peakDetection==false) {
      globalPeak = -9999;
      startPredictCycle = cycleIndex;
    }
    else if (  ((cycleIndex - startPredictCycle) > 41.66667) and ( (cycleIndex - startPredictCycle) < 125)  ) {   // If the wave lies between 4 and 12 Hz
      cyclesUntilStim = cycleIndex - startPredictCycle;
      if(cyclesUntilStim < 0) {
        cyclesUntilStim = 0;
      }
    }
  }
  // ==========================================================================//
  //determines whether we are in a stimulation cycle or not
  //if we are, then we will stimulate until the refractory period is over

  if(refractoryCount > 0) {
    refractoryCount--;
    
    //STIMULATE
    stimulate = 1;
    digitalWrite(outputPin, HIGH);
  }
  else if(cyclesUntilStim == 0) {
    globalPeak = -9999;
    globalTrough = 9999;
    refractoryCount = refractoryLength - 1;
    
    //STIMULATE
    stimulate = 1;
    cyclesUntilStim = -1;
  }
  else {
    //NO STIMULATE
    stimulate = 0;
    digitalWrite(outputPin, LOW);
  }

  cyclesUntilStim--;
  //myFile.println(stimulate);
  cycleIndex++;

  // ==========================================================================//
  // SD CARD WRITING DATA 
  
  if (dataIndex != sDsize-1) {
    dataSend[dataIndex] = cycleIndex;
    dataIndex++;
    dataSend[dataIndex] = (int) curr_Point * 10000;
    dataIndex++;
    dataSend[dataIndex] = stimulate;
  }
  
  
  if (dataIndex >= sDsize-1) {
    dataFile.write(dataSend,sDsize);
    dataIndex = 0;
    
  }
  
  if (loopNumber >= STOP_ITER) {
    Serial.println("Program Done!");
    dataFile.close();
    Serial.end();
    }

  while (currentTime - startTime < 1000) {
    currentTime = micros();
  }
  // ==========================================================================// 
  
}
//*********************************************************************************************//
