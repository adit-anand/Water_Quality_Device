/* SMART BOUY
 * A completely self contained, fully deployable remote water quality sensor
 * 
 * Features:  pH sensor
 *            Turbidity Sensor
 *            Temperature Sensor
 *            Electrical Conductivity Sensor
 *            Total Dissolved Solids (derived from conductivity)
 *            Date and Time Stamp for each reading
 *            GPS tracking
 *            Battery level indicator
 *            Solar Panel - charges onboard 3.7 volt lithium ion battery
 *            Data upload through GSM cellular network (2G)
 *            On board data logging to a micro SD card
 *            
 * Process:           
 * 1. Each sensor reading is taken and stored
 * 2. A string is bulit from the sensor data and stored on the SD card in CSV format
 * 3. Sensor readings are sent to a serial monitor
 * 4. A URL string is built from the data
 * 5. An http GET request including the URL string is sent to dweet.io API after each reading
 * 6. A freeboard dashboard has been created to pull data from dweet.io and display data graphically https://freeboard.io/board/fgJLRa       
*/

#include <OneWire.h>               // library for temp sensor, allows multiple sensors on a single pin
#include <SPI.h>                   // Serial Peripheral Interface - for communicating with peripheral devices
#include <SD.h>                    // for reading/writing SD card
#include <RTCTimer.h>              // library for real time clock
#include <Sodaq_DS3231.h>          // library for on board clock
#include <SoftwareSerial.h>        // library allowing digital pins to act as communcation pins
#include <TinyGPS.h>               // GPS library


String  APN = "wholesale";         // apn for http network access
String  URL;                       // website to be accesses
String  dateTimeStr;               // hold date and time 
String  dataRec = "";              // string for data added to SD card
int     currentminute;             // current time
long    currentepochtime = 0;      // current time

float   ECcurrent;                 // converted EC reading
float   TempCoefficient;           // used for data normalization
float   CoefficientVolatge;        // used for data normalization
float   pHOffset = -.48;              // offset for normalizing readings
float   tempCoefficient;           // used for EC conversion
float   voltCoefficient;           // used for EC conversion

float   ECReading = 0;             // variable to store the value coming from the analogRead function
int     batReading = 0;            // variable to store the value coming from the analogRead function 
int     turbReading = 0;           // variable to store the value coming from the analogRead function
int     pHReading = 0;             // variable to store the value coming from the analogRead function

byte    ECPin = A1;                // analog pin 1 - input from conductivity meter 
byte    TempPin = 6;               // digital pin 6 - input from thermometer 
int     batPin = A6;               // analog pin 6 - input from battery
int     pHPin = A5;                // analog pin 5 - input from pH meter
int     turbPin = A3;              // analog pin 3 - input from turbidity meter

float   batValue;                  // the voltage 
float   pHValue;                   // the pH
float   tempValue;                 // the temperature 
float   turbValue;                 // the turbitity
float   ecValue;                   // the conductivity 
float   tdsValue = 0;              // the total dissolved solids 
long    lat;                       // the latitude
long    lon;                       // the longitude
  
#define SD_SS_PIN 12               // pin used to write SD card
#define FILE_NAME "datafile.txt"   // name of data file
#define LOGGERNAME "DataLogger"    // title of data file table
#define DATA_HEADER "DateTime_EST,Battery_V,Temperature_C,pH Level,Turbidity_NTU,Conductivity_us/cm,TDS_mg/L"  // format for data file table header

OneWire ds(TempPin);               // create a OneWire object on digital pin 4
SoftwareSerial gpsSerial(4, 5);    // create a SoftwarSerial object for gps sensor connection on pins 4(rx) & 5(tx)
SoftwareSerial SIM900(7,8);        // create a SoftwarSerial object for modem connection on pins 7(rx) & 8(tx)
TinyGPS gps;                       // create gps object

void setup() 
{
          Serial.begin(9600);    // initialize serial monitor
          gpsSerial.begin(9600); // initialize gps sensor 
          SIM900.begin(19200);   // initialize SIM900 modem  
          rtc.begin();           // initialize real time clock
          delay(100);
                    
          pinMode(8, OUTPUT);
          pinMode(9, OUTPUT);
 
          greenred4flash();    //blink the LEDs to show the board is on
          setupLogFile();      //checks for file - creates file and formats header
          
          // header for serial output
          Serial.println("Water probe Project: CIS 111B");
          Serial.println("");
          Serial.println("Initiating sensor readings and logging data to SDcard....");
          Serial.println("");
  
}
 
void loop() 
{         
          getDateTime();                // gets the current time and date
          getTemp();                    // gets data from temperature sensor
          getEC();                      // gets data from conductivity sensor
          getBattery();                 // gets data from battery sensor
          getPH();                      // gets data from pH sensor 
          getTurbidity();               // gets data from turbidity sensor
          getTDS();                     // converts data from conductivity sensor
          getGPS();                     // gets GPS coordinates
          createDataRecord();           // assign the data string to dataRec
          createURL();                  // builds the URL string from sensor data
          logData(dataRec);             // Save the data record to the log file
          displayData();                // Send the data to the serial connection      
          exportData();                 // Send the data to the  serial1 connection          

          delay(10000);                 //10 second delay  
}

// prints data to serial monitor
void displayData()
{
          Serial.println("==========================================================================================================================================================");
          Serial.println("   Date      Time      Battery      Temperature        pH Level      Turbidity      Conductivity        TDS             Latitude        Longitude         ");
          Serial.println(" (y/m/d)    (h/m/s)    (volts)       (degree C)                        (NTU)           (us/cm)         (mg/L)                                             ");
          Serial.print(dateTimeStr);
          Serial.print("\t");
          Serial.print(batValue);
          Serial.print("\t\t");
          Serial.print(tempValue);
          Serial.print("\t\t"); 
          Serial.print(pHValue);
          Serial.print("\t\t");
          Serial.print(turbValue);
          Serial.print("\t\t"); 
          Serial.print(ecValue); 
          Serial.print("\t\t"); 
          Serial.print(tdsValue);
          Serial.print("\t\t"); 
          Serial.print(lat); 
          Serial.print("\t"); 
          Serial.println(lon);
          Serial.println("==========================================================================================================================================================");
}

void exportData()
{     
          SIM900.println("AT+CPAS");
          delay(100);
          SIM900.read();
          
          SIM900.println("AT+CSQ");
          delay(100);
          SIM900.read();
         
          SIM900.println("AT+CGATT?");
          delay(100);
          SIM900.read();
         
          SIM900.println("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"");  //setting the SAPBR, the connection type is using gprs
          delay(1000);
          SIM900.read();
         
          SIM900.println("AT+SAPBR=3,1,\"APN\"," + APN));        //setting the APN, fill in your local apn server
          delay(4000);
          SIM900.read();
         
          SIM900.println("AT+SAPBR=1,1");                       //setting the SAPBR
          delay(2000);
          SIM900.read();
         
          SIM900.println("AT+HTTPINIT");                        //init the HTTP request
          delay(2000); 
          SIM900.read();
         
          SIM900.println("AT+HTTPPARA=\"URL\"," + URL);         // setting the website you want to access
          delay(1000);
          SIM900.read();
         
          SIM900.println("AT+HTTPACTION=0");                   //submit the request 
          delay(10000);                                        // longer delay allows time for data return from website
          SIM900.read();
         
          SIM900.println("AT+HTTPREAD");                       // read the data from the website you access
          delay(300);
          SIM900.read();
         
          SIM900.println("");
          delay(100);               
  
}

void getGPS()
{
          while(gpsSerial.available())                // check for gps data
              { 
                if(gps.encode(gpsSerial.read()))      // encode gps data
                  { 
                      gps.get_position(&lat,&lon);    // get latitude and longitude
                  }
              }
}



//get the date and time of the reading
void getDateTime()
{          
          dateTimeStr = "";
          //Create a DateTime object from the current time
          DateTime dt(rtc.makeDateTime(rtc.now().getEpoch()));      
          currentepochtime = (dt.get());     
          currentminute = (dt.minute());
          dt.addToString(dateTimeStr); //Convert it to a String
          
}


// flashes LEDs to confirm power
void greenred4flash()
{
          for (int i=1; i <= 4; i++)
          {
              digitalWrite(8, HIGH);   
              digitalWrite(9, LOW);
              delay(50);
              digitalWrite(8, LOW);
              digitalWrite(9, HIGH);
              delay(50);
          }
          digitalWrite(9, LOW);
}


// confirm file exists then adds title and header
void setupLogFile()
{
          //Initialize the SD card
           if (!SD.begin(SD_SS_PIN))
           {
              Serial.println("Error: SD card failed to initialize or is missing.");
           }
  
          //Check if the file already exists
          bool oldFile = SD.exists(FILE_NAME);  
  
          //Open the file in write mode
          File logFile = SD.open(FILE_NAME, FILE_WRITE);
  
          //Add header information if the file did not already exist
          if (!oldFile)
          {
              logFile.println(LOGGERNAME);
              logFile.println(DATA_HEADER);
          }
  
          //Close the file to save it
          logFile.close();  
}


// adds data to SD card
void logData(String rec)
{
          //Re-open the file
          File logFile = SD.open(FILE_NAME, FILE_WRITE);
  
          //Write the CSV data
          logFile.println(rec);
  
          //Close the file to save it
          logFile.close();  
}


//creates the URL string for data upload to dweet.io API
void createURL()
{            
          //Create a url string
          String urlString = "http://www.dweet.io/dweet/for/WaterProbe111B?"; // beginning of url
          urlString += "BATT=";    
          addFloatToString(urlString, batValue, 3, 1);        //add battery data
          urlString += "&PH=";  
          addFloatToString(urlString, pHValue, 4, 2);         //add temperature data
          urlString += "&TURB=";
          addFloatToString(urlString, turbValue, 3, 1);       //add pH data
          urlString += "&ECOND=";  
          addFloatToString(urlString, ecValue, 3, 1);         //add turbidity data 
          urlString += "&TDS=";
          addFloatToString(urlString, tdsValue, 3, 1);        //add conductivity data
          urlString += "&TEMP=";
          addFloatToString(urlString, tempValue, 3, 1);       //add TDS data
          urlString += "&LON=";
          addFloatToString(urlString, lat, 1, 6);             //add lat data
          urlString += "&LAT=";
          addFloatToString(urlString, lon, 1, 6);             //add lon data
          urlString += "&DT=";
          urlString += dateTimeStr;                           //add date and time
          URL = urlString;
          Serial.println(URL);
}


//creates the data string to hold all sensor readings
void createDataRecord()
{            
          //Create a String type data record in csv format
          String data = dateTimeStr;                   //add date and time data
          data += ",";    
          addFloatToString(data, batValue, 3, 1);      //add battery data
          data += ",";  
          addFloatToString(data, tempValue, 4, 2);     //add temperature data
          data += ",";
          addFloatToString(data, pHValue, 3, 1);       //add pH data
          data += ",";  
          addFloatToString(data, turbValue, 3, 1);     //add turbidity data 
          data += ",";
          addFloatToString(data, ecValue, 3, 1);       //add conductivity data
          data += ",";
          addFloatToString(data, tdsValue, 3, 1);      //add TDS data
          data += ",";
          addFloatToString(data, lat, 1, 6);           //add lat data
          data += ",";
          addFloatToString(data, lon, 1, 6);           //add lon data
          dataRec = data;
}


//converts sensor data to a string
static void addFloatToString(String & str, float val, char width, unsigned char precision)
{
          char buffer[10];
          dtostrf(val, width, precision, buffer);
          str += buffer;
}


//gets battery reading and converts to usable data
void getBattery()
{   
          batReading = analogRead(batPin);                // reads input from meter
          batValue = batReading * (15.75/1024);           // convert the analog input
}


//gets pH reading and converts to usable data
void getPH()  
{
          
          pHReading = analogRead(pHPin);                  // reads input from meter
          pHValue = (pHReading * (17.5/6144)) + pHOffset; // convert the analog input      
}

    
//gets turbidity reading and converts to usable data
void getTurbidity()
{
          turbReading = analogRead(turbPin);              // reads input from meter  
          turbValue = turbReading * (5/1024);             // convert the analog input
          if(turbValue < 0)
            turbValue = 0;
}
      

//converts the conductivity reading to TDS
void getTDS()
{
          tdsValue = ecValue * .67;
}


// gets the conductivity reading
void getEC() 
{
          if(ECReading<=448)
                ECcurrent=(6.84*ECReading-64.32);    //1ms/cm<EC<=3ms/cm
          else 
          
          if(ECReading<=1457)
                ECcurrent=(6.98*ECReading-127);      //3ms/cm<EC<=10ms/cm
          else 
                ECcurrent=(5.3*ECReading+2278);      //10ms/cm<EC<20ms/cm
                
          ecValue = ECcurrent/1000;  

          if(ecValue < 0)
                ecValue = 0;
}


//gets temperature from one tempPin in DEG Celsius
void getTemp()
{
          byte data[12];
          byte addr[8];

          if ( !ds.search(addr)) 
          {
              //no sensors, reset search
              ds.reset_search();
              tempValue = -1000;
          }

          if ( OneWire::crc8( addr, 7) != addr[7]) 
          {
               tempValue = -1000;
          }

          if ( addr[0] != 0x10 && addr[0] != 0x28) 
          {
              tempValue = -1000;
          }

          ds.reset();
          ds.select(addr);
          ds.write(0x44,1); // start conversion, with parasite power on at the end

          byte present = ds.reset();
          ds.select(addr);    
          ds.write(0xBE); // Read Scratchpad
  
          for (int i = 0; i < 9; i++) // we need 9 bytes
          { 
              data[i] = ds.read();
          }
  
          ds.reset_search();
  
          byte MSB = data[1];
          byte LSB = data[0];

          float tempRead = ((MSB << 8) | LSB); //using two's compliment
          tempValue = tempRead / 16;
}





