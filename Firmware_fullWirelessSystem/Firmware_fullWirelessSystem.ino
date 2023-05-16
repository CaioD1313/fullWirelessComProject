/*********************************************************************************************************************************************************

------- Author: Caio Dutra

------- Date: 05-16-2023

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
communication protocol. The routine for the SPI (by bit banging), writing, reading and erasing data
on the memory chip will be implemented by the candidate him self.


## This version of the firmware implements the subroutine of Page Program (Write on Data Memory)

**********************************************************************************************************************************************************/







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

// Declaration of bit banging SPI subroutine | Mode 0 | MSB First
byte SPI_BB(byte _send8bitData);





// Declaration of the Data Memory Page Program 
void Data_Memory_Write(byte writeByte);





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

  delay(1000); // wait 1s for stabilization of whole circuit (data memory, temp sensor, etc) 
}

void loop() {


}


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


/************************
**** Memory Write    **** 
*************************/
// Writes 256 Bytes of memory per page
// 3 Bytes Address are needed as reference to start recording,
// in the form _writeAddress[B2 B1 B0], B2 being Most Significative Byte

void Data_Memory_Write(byte _writeAddress[3], byte _writeByte[256]){

byte WREN = 0x06; // Write enable instruction (06h)

byte dummy = 0; // dummy byte var as the Writing process doesn't return any data

byte PP_INSTRUCTION = 0x02; // Page Program instruction 

dummy = 0; // 
int i = 0; // counting var


// Write Enable instruction:
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

// sends the start page address
for(i = 0; i<3; i++){

  dummy = SPI_BB(_writeAddress[i]);

}

//sends the 256 Bytes to be stored
for(i = 0; i<256; i++){ // runs 256 times
  
  dummy = SPI_BB(_writeByte[i]);

} 
digitalWrite(DMCS,HIGH); // resets the slave to disabled mode

}


/*******   END   ********
**** Memory Write    **** 
*************************/
















