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


## This version of the firmware implements the subroutine for the SPI protocol
on mode 0, with 8 send/read bits per loop.

**********************************************************************************************************************************************************/

















byte SPI_Comm(byte _send8bitData){



  
}



void setup() {
  // put your setup code here, to run once:

}

void loop() {
  // put your main code here, to run repeatedly:

}
