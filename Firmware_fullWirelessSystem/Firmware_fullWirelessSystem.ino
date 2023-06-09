/*********************************************************************************************************************************************************

------- Author: Caio Dutra

------- Date: 05-17-2023

##### Full Wireless Communication System Project - Tractian Firmware Eginneer Interview #####

The ultimate goal of this firmware is to establish a complete wireless communication 
between a data collecting station (ESP8266EX) and an access point located 100m away 
using the WiFi protocol. The hardware collects data on-site, stores it with a size of 500kB, 
and then sends it to the access point.

For the purpose of data collection, a temperature measuring circuit was implemented
on the hardware using a TMP36 IC. A linear time-invariant system, namely the Moving 
Average Filter, is required to ensure that the measured temperature remains in a stable
state when it is saved in the flash data memory integrated into the hardware.

##
Note: The choice of the TMP36 IC is due to the fact that this sensor has a shutdown state
when its usage is not required. In this situation, the IC operates with currents of 0.5uA.
This ensures energy savings as the system operates remotely using batteries. Additionally, 
during its active state, the TMP36 operates with currents of 50uA, which are also considered
low for the application in question.

For the WiFi protocol, it is used the "ESP8266WiFi.h" library licensed under LGPL (Lesser General
Public License) Version 2.1, which in broad terms it can be interpreted as follows:

      "Primarily used for software libraries, the GNU LGPL 
       requires that derived works be licensed under the same license, but works that only link to it do not
       fall under this restriction."

The candidate chose to use this library because he doesn't have solid 
knowledge of the WiFi protocol (from the physical layer to the application layer) to develop his own
library (or subroutines). A full copy of the LGLP Ver 2.1 can be seen at: https://github.com/esp8266/Arduino/blob/master/LICENSE


For the communication between the microcontroller and the Flash Data Memory, it is used the SPI 
communication protocol.


## This is the final version of the firmware (yet to be revised). 

**********************************************************************************************************************************************************/

/* -#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#- */


/*******************************
*                              *
*           Libraries          *
*                              *
********************************/


#include <ESP8266WiFi.h>

WiFiClient _hostClient; // creates an instance of WifiClient for the host to be connected


/********     END    ***********
*                              *
*           Libraries          *
*                              *
********************************/




/* -#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#- */


/*******************************
*                              *
*  Pin Constants Definitions   *
*                              *
********************************/



/* AS we're using the NodeMCU 1.0 board to record the Program on ESP8266EX
it is needed to use the GPIO's definitions for its board to match our own
hardware. For this, we use the IO Number as the GPIO Number. For instance,
if we want to use the GPIO14 as output, we declare "pinMode(14,OUTPUT);"
*/

// Data Memory GPIO's definitions for SPI comm:
#define DMCS 15 // Data Memory Chip/Slave Select - Pin 13 on ESP8266EX -> (MTDO) GPIO15
#define DMCLK 14 // Data Memory Serial Clock - Pin 9 on ESP8266EX -> (MTMS) GPIO14
#define DMSI 13 // Data Memory SI (slave data input) (MOSI) - Pin 12 on ESP8266EX -> (MTCK) GPIO13
#define DMSO 12 // Data Memory SO (slave data output) (MISO) - Pin 10 on ESP8266EX -> (MTDI) GPIO12
#define DMWP 0 // Data Memory Write Protect - Pin 15 on ESP8266EX -> (GPIO0) GPIO0
#define DMHD 2 // Data Memory Hold - Pin 14 on ESP8266EX -> (GPIO2) GPIO2

// SPI delay for 8Mhz comm (Flash Data Memory operates up to 8Mhz for single IO mode)
// For we are using bit banging data, a (way) lower frequency will be used.
// Acording to Arduino Reference, for 'delayMicroseconds(t);' operates very accurately 
// for a minimum valeu of t = 3 (3us). Being so, The period of the SCLK shall be 6us,
// which leads to f = 166.666 kHz. That being said, every 3 seconds or so 500,000
// samples of temperature will be collected. This barely has any practical uses,
// but the principle of the challenge stands.
#define Half_SPI_Period 3 // for f = 8MHz , T = 6us


// Temperature Sensor GPIO definition
#define Temp_Sensor_ADC_Pin 17 // TMP35 Vout - Pin 6 on ESP8266EX -> (TOUT) ADC_in | On NodeMCU it is mapped to pin 17
#define Temp_Sensor_Shutdown_Pin 5 // TMP35 SHUTDOWN - Pin 24 on ESP8266EX -> (GPIO5) GPIO5




/********     END    ***********
*                              *
*  Pin Constants Definitions   *
*                              *
********************************/


/* -#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#- */



/*******************************
*                              *
*  Declaration of constants    *
*                              *
********************************/

const char* SSID = "Fake_WiFi_Net_To_Connect"; // service set identifier of the AP to be connected

const char* PSSWRD = "Fake_WiFi_Net_Password"; // net password of the AP to be connected

const char* HOST = "192.13.14.5/temperature_log_files"; // Fake URL to send data (500kB file)

const int _httpPort = 80; // HTTP Port to be used

/********     END    ***********
*                              *
*  Declaration of constants    *
*                              *
********************************/





/* -#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#- */


/*******************************
*                              *
*    Declaration of Arrays     *
*                              *
********************************/

// Global Array to store 256 Stable Temperature Readings (later to be used on Data Memory Write)
byte Temp_Vector[256]; // records 256 sample of stables temp measurements to record on a page on Data Memory
byte Temp_Buffer[50]; // Buffer to stores 50 samples of intantaneous Temperature readings

// 3Byte Memory Address
byte _addressBytes[3];

// 1024 array for reading from data memory and sending via WiFi (total of 500 packets of 1024 bytes to be sent -> 500kB)
byte _readData[1024];


/********     END    ***********
*                              *
*    Declaration of Arrays     *
*                              *
********************************/



/* -#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#- */




/*******************************
*                              *
*   Unislot vars Definitions   *
*                              *
********************************/


// Memory Address Word
unsigned long ADDRESS = 0x000000; // By our definition, Memory Write starts at address 0
int _packetCount = 0; // count var for Page Program


int _numOfTries = 0; // starts with 0 - count the number of times the station tried to connect with to host before it gives up and leads to another set of collecting tempereture values
// int _connectionSuccess; // indicates if the connection to the host was sucessful (1) or not (0)


int p; // count var for 500kB file (counts the number os 1024bytes packets sent to the host - total of 512,000 bytes)
int q; //count var (parameter of _readData) for 1024 bytes to be sent

/********     END    ***********
*                              *
*   Unislot vars Definitions   *
*                              *
********************************/




/* -#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#- */





/*******************************
*                              *
* Declaration of Subroutines   *
*                              *
********************************/

// Declaration of function that creates an array with the 3 addresses bytes for the Data memory
void create_Addres_Bytes (unsigned long Address);

// Declaration of the Reading of Temperature Reading Routine
void Temp_Read(void);

// Declaration of bit banging SPI subroutine | Mode 0 | MSB First
byte SPI_BB(byte _send8bitData);


// Declaration of the Data Memory Page Program Function
void Data_Memory_Write(byte _writeAddress[3], byte _writeByte[256]);

// Declaration of Data Read Function
void Data_Memory_Read(byte _readAddress[3]);

//Declaration of the Erase Chip Instruction Function
void Memory_Erase(void);


/********     END    ***********
*                              *
* Declaration of Subroutines   *
*                              *
********************************/


/* -#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#- */



void setup() {
  
  // defining the 4xIO of the Data Memory SPI and some of its states
  pinMode(DMSO, INPUT); 
  pinMode(DMSI, OUTPUT);
  pinMode(DMCLK, OUTPUT);
  pinMode(DMCS, OUTPUT);
  digitalWrite(DMCS, HIGH); // Set Slave Select to HIGH, so it is guranteed to be inactive
  digitalWrite(DMCLK, LOW); // Gurantees SCLK is set to LOW so when the COM starts the clock is on idle mode

  // defitions of the Hold and WP Pins of Data Memory
  digitalWrite(DMHD, HIGH); // sets the Hold Pin on the Data Memory to HIGH, so its inactive
  digitalWrite(DMWP, HIGH); // sets the Write Protect Pin on the Data Memory to HIGH, so its inactive

  //definition of the SHUTDOWN pin of TMP35
  pinMode(Temp_Sensor_Shutdown_Pin,OUTPUT);

  delay(1000); // For powering guarantees, wait 1s for stabilization of whole circuit (data memory, temp sensor, etc) 

}

void loop() {

 
  /* -#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#- */

  // First firmware assignment:
  /*************************************************
  *                                                *
  * Collects 512,000 (500kB) values of temperature *
  * and stores it on the Data Memory               *
  *                                                *
  *************************************************/


  Memory_Erase(); // Erases entire memory chip for a new temperature recording cycle on the loop
  delay(10); // little delay so the Erase Chip opcde is properly executed 

  
  digitalWrite(Temp_Sensor_Shutdown_Pin,HIGH); // Enables the TMP35 to operate
  delay(1); // little delay so the TMP35 is functional

  // Writes 2000 Packets of 256Bytes (500kB) of Temperature values on Data Memory
  for(_packetCount = 0; _packetCount< 2000; _packetCount++){

    Temp_Read(); // reads 256 values of temperature

    create_Addres_Bytes(ADDRESS); // creates the 3Byte address from a 24bit word, to be used on data record

    Data_Memory_Write(_addressBytes, Temp_Vector); // Stores 256 values (Bytes) of data (temperature) on Data Memory

    ADDRESS += 0x100; // Sums 256 to the address word, for new start address

  }
  digitalWrite(Temp_Sensor_Shutdown_Pin,LOW); // disables the TMP35 and puts it on ultra low current comsuption mode (maximum of 0.5uA)

  ADDRESS = 0x000000; // Resets the Address to 0 - data memory read and transmission by WiFi.


  /****************      END      ******************
  *                                                *
  * Collects 512,000 (500kB) values of temperature *
  * and stores it on the Data Memory               *
  *                                                *
  *************************************************/

  /* -#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#- */


  // Second firmware assignment:
  /**********************************************
  *                                             *
  * Reads 512,000 (500kB) values of temperature *
  * from data memory and sends via WiFi.        *
  *                                             *
  ***********************************************/

  //if ESP is disconnected from the AP, reconects it
  if(WiFi.status() != WL_CONNECTED){

    WiFi.begin(SSID,PSSWRD); // set the SSID and password to connect on the AP
    while(WiFi.status() != WL_CONNECTED){} // wait until the ESP connects to the AP, otherwise doesn't proceed
    delay(500); // wait a little bit so the connection is stable

  }

  // _hostClient.setInsecure(); // for simplicity, disables SSL certificate as we're assuming the URL to send data is in localhost server, forinstance
  // tries to connect 10 times to the host before gives up and restart the temperature collecting process
  while( (_hostClient.connect(HOST, _httpPort) == false) && (_numOfTries < 10)){

    delay(5000);// waits five second
    _numOfTries++; // increments the number of times that the station tried to connect to the host
  }


  //tests if the the connection to the host was stabilshed within less than 10 tries. If so, will send the 500kB file to the Host
  if(_numOfTries < 10){


    //continuously check if the host client still connected in order to send data and do not waste energy/time with RF transmissions
    while(_hostClient.connected()){

       
      // loops 500 times to sent 500kB files (1024byte packets each time) 
      for(p = 0; p< 500; p++){

        create_Addres_Bytes(ADDRESS); // creates the 3Byte address from a 24bit word, to be used on data read
        Data_Memory_Read(_addressBytes); // creates the 1024 byte array to be sent
        for(q = 0; q < 1024; q++){

          _hostClient.write(_readData[q]); // sends the qth byte of the pth packet of data

        }

        ADDRESS += 0x400; // sums 1024 to the addreess to take next four pages of temperature values

      }

      

    }

    // if the transmission ocurred successfuly (whole packt of 500kB), then closes the connection with the host
    if(_hostClient.connected()){

      _hostClient.print("GET / HTTP/1.1/\r\n"); // \r indicates an end of command, while \n creates a new line
      _hostClient.print("Host: 192.13.14.5/temperature_log_files\r\n");
      _hostClient.print("Connection: close\r\n\r\n");

    }

    WiFi.disconnect(); // turns the connection with the AP off to save energy.

    _numOfTries = 0; // resets try-to-connect counting var;

    ADDRESS = 0x000000; // Resets the Address to 0 - temperature log write on data memory.

  }

  /***************     END     ******************
  *                                             *
  * Reads 512,000 (500kB) values of temperature *
  * from data memory and sends via WiFi.        *
  *                                             *
  ***********************************************/


  /* -#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#- */


}









/* -#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#- */

/************************
********   SPI   ******** 
*************************/


// Bit banging SPI subroutine (mode 0) - MSB First:
byte SPI_BB(byte _sendByte){


  byte _receivedByte = 0;
  byte  k = 0; // auxiliar couting byte var

  for(k = 0x80; k != 0; k /= 2) // \frac{k}{2} shifts the kth bit to the right >>
  {

    if ((k & _sendByte) !=0x00){ // tests if the kth send bit is 1 or 0 with the k mask and Writes the kth bit of the sending word on the MOSI PIN
      digitalWrite(DMSI,HIGH);
    }   
    else {
      digitalWrite(DMSI,LOW);
    }

    digitalWrite(DMCLK, HIGH); // Sets SCK (Clock) to HIGH so the slave reads the DMSI GPIO on rising edge of clock
    delayMicroseconds(Half_SPI_Period); // COM delay
    digitalWrite(DMCLK, LOW); // Sets SCK (Clock) to LOW so the slave sends the kth bit to the master
      
    if(digitalRead(DMSO) == HIGH){ // if the Master Input is in HIGH state (a bit has been sent), then stores this bit in the kth position of the received var
      _receivedByte |= k ;
    } 
   delayMicroseconds(Half_SPI_Period); // COM delay
   
      
  }

  
  return _receivedByte; // returns the received byte

}


/*******   END   ********
********   SPI   ******** 
*************************/



/* -#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#- */







/************************
**** Memory Write    **** 
*************************/
// Writes 256 Bytes of memory per page (full page write)
// 3 Bytes Address are needed as reference to start recording,
// in the form _writeAddress[B2 B1 B0], B2 being Most Significative Byte

void Data_Memory_Write(byte _writeAddress[3], byte _writeByte[256]){

  byte WREN = 0x06; // Write enable instruction (06h)

  byte dummy = 0; // dummy byte var as the Writing process doesn't return any data

  byte PP_INSTRUCTION = 0x02; // Page Program instruction 

  dummy = 0; // 
  int i = 0; // counting var


  // Write Enable command:
  digitalWrite(DMCS,LOW); // sets the slave to active mode
  digitalWrite(DMCLK,LOW); // initialize SCLK at idle state
  delayMicroseconds(Half_SPI_Period); // COM delay for half of a period
  dummy = SPI_BB(WREN); // execute the Write Enable instruction
  digitalWrite(DMCS,HIGH); // resets the slave to disabled mode
  delayMicroseconds(50); // 50us delay so the Page program command can be accurately interpreted after the WREN instruction 



  digitalWrite(DMCS,LOW); // sets the slave to active mode
  digitalWrite(DMCLK,LOW); // initialize SCLK at idle state
  delayMicroseconds(Half_SPI_Period); // COM delay for half of a period
  dummy = SPI_BB(PP_INSTRUCTION); // execute the Page Program instruction

  // sends the start page address, Address_MSB first
  for(i = 2; i>= 0; i--){

    dummy = SPI_BB(_writeAddress[i]);

  }

  //sends the 256 Bytes to be stored
  for(i = 0; i<256; i++){ // runs 256 times
  
    dummy = SPI_BB(_writeByte[i]);

  } 
  digitalWrite(DMCS,HIGH); // resets the slave to disabled mode

}


/*******   END    ********
**** Memory Write     **** 
*************************/


/* -#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#- */






/*************************
****   Memory Read    **** 
*************************/
// Reads 1024 Bytes of memory per call
// 3 Bytes Address are needed as reference to start reading,
// in the form _readAddress[B2 B1 B0], B2 being Most Significative Byte


void Data_Memory_Read(byte _readAddress[3]){


  byte READ_INSTRUCTION = 0x03; // Read opcode 

  byte dummy = 0; // dummy byte for SI and SO
  int i = 0; // counting var


  digitalWrite(DMCS,LOW); // sets the slave to active mode
  digitalWrite(DMCLK,LOW); // initialize SCLK at idle state
  delayMicroseconds(Half_SPI_Period); // COM delay for half of a period

  dummy = SPI_BB(READ_INSTRUCTION); // execute the Page Program instruction

  // sends the start reading address, Address_MSB first
  for(i = 2; i>= 0; i--){

    dummy = SPI_BB(_readAddress[i]);

  }

  for(i = 0; i<1024; i++){

    _readData[i] = SPI_BB(dummy);

  }

  digitalWrite(DMCS,HIGH); // resets the slave to disabled mode


}



/*******   END    ********
****   Memory Read    **** 
*************************/



/* -#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#- */



/*************************
****  Memory Erase    **** 
*************************/
// Erases the entire Data Memory
void Memory_Erase(void){


  byte WREN = 0x06; // Write enable instruction (06h)

  byte dummy = 0; // dummy byte var as the Writing process doesn't return any data

  byte ERASE_INSTRUCTION = 0xC7; // Page Program instruction 

  dummy = 0; // 


  // Write Enable command:
  digitalWrite(DMCS,LOW); // sets the slave to active mode
  digitalWrite(DMCLK,LOW); // initialize SCLK at idle state
  delayMicroseconds(Half_SPI_Period); // COM delay for half of a period
  dummy = SPI_BB(WREN); // execute the Write Enable instruction
  digitalWrite(DMCS,HIGH); // resets the slave to disabled mode
  delayMicroseconds(50); // 50us delay so the Erase Chip command can be accurately interpreted after the WREN instruction


  digitalWrite(DMCS,LOW); // sets the slave to active mode
  digitalWrite(DMCLK,LOW); // initialize SCLK at idle state
  delayMicroseconds(Half_SPI_Period); // COM delay for half of a period
  dummy = SPI_BB(ERASE_INSTRUCTION); // execute the Page Program instruction
  digitalWrite(DMCS,HIGH); // // resets the slave to disabled mode


}



/*******   END   ********
****  Memory Erase    **** 
*************************/





/* -#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#- */



/************************
****    Temp Read    **** 
*************************/
// Makes the reading of 256 stable temperature values to be stored on a data memory page
// Stabilizes the instantaneous temperature read using a Moving Average Filter

void Temp_Read(void){

  
  int h; // counting variable for MAF
  int m; // counting variable for the Temp_Vector
  int Temp_Stable = 0; //Reading var of the ADC_Pin - Average sum starts at 0;

  // First 50 entries of Moving Average Filter 
  // stores the temp samples in reverse, so the last slot contains the first reading (to be trashed later)
  for(h = 49; h >= 0; h--){
    Temp_Buffer[h] = analogRead(Temp_Sensor_ADC_Pin)/(1023*0.01); // Reads the temperature measured on ADC_Pin, in ºC.
    Temp_Stable += Temp_Buffer[h]/50; // Calculate the mean of the Temp_Buffer

  }

  Temp_Vector[0] = Temp_Stable; // saves the first stable temp read

  for(m = 1; m<256; m++){


    Temp_Stable -= Temp_Vector[49]/50; // Withdraws the weight of the first instantaneous temp measurement

    for(h = 0; h<49; h++){
      Temp_Buffer[h+1] = Temp_Buffer[h]; // shifts the rest of the Temp_buffer to the right,starting at Temp_Buffer[1] (Temp_Buffer[0] remains the same, for now). 
    }

    Temp_Buffer[0] = analogRead(Temp_Sensor_ADC_Pin)/(1023*0.01) ; // Completes the vector with the new sample
    Temp_Stable += Temp_Buffer[0]/50; // Calculates the new mean with the newest sample read
    Temp_Vector[m] = Temp_Stable; // saves the new stable measure of the temperature

  }


  
}

/*******   END   ********
****    Temp Read    **** 
*************************/



/* -#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#- */


/********************************
**** Creates 3Byte Address   **** 
********************************/

void create_Addres_Bytes (unsigned long Address){

  
  unsigned long Aux_1 = 0; // Auxiliar var1
  unsigned long Aux_2 = 0; // Auxiliar var2
  unsigned long Mask = 0xF00000; // Start Mask 
  int i; // Counting var to repeat the loop 3 times (3 byte address)

  for (i = 2; i>=0; i--){

  Aux_1 = Address & Mask; // Masks the MSB nibble (1st, 2nd and 3rd pairs) of the address word
  Mask = Mask / 16; // Shifts the mask by four positions (1 nibble ->)
  Aux_2 = Address & Mask; // Masks the LSB nibble (1st, 2nd and 3rd pairs) of the address word
  Aux_1 = Aux_1 + Aux_2; // Sums the MSB and LSB nibbles (1st, 2nd and 3rd pairs) to be transformed into a byte sized value
    
  if(i != 0){

    _addressBytes[i] = Aux_1 / pow(2,8*i); // Shifts the pair of nibbles (1st, 2nd pairs) to a byte sized value
    
    }
    else{
    
      _addressBytes[i] = Aux_1; // 3rd nibble pair, already in a byte sized value
    
    }

    Mask = Mask / 16; // Shifts the mask by four positions (1 nibble ->)

  } 

  


}


/*******       END       ********
**** Creates 3Byte Address   **** 
********************************/



/* -#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#--#-#- */








