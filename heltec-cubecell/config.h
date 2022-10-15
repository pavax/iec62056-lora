/* Uncomment to strip the unit before publishing values. For example: "230.5" instead of "230.5*V" */
#define STRIP_UNIT

/* Uncomment to verify the checksum */
#define SKIP_CHECKSUM_CHECK

/* ADJUSTME: Uncomment to override automatic mode selection, for example to limit the baud rate. Use if you have problems with your optical receiver. or if your smart-meter doesn't need a baud-rate-switch.
0: No Baud switch keep INITIAL_BAUD_RATE
1: Mode A at 600 baud
2: Mode B at 1200 baud
3: Mode C at 2400 baud
4: Mode D at 4800 baud
5: Mode E at 9600 baud
6: Mode F at 19200 baud 
*/
//#define MODE_OVERRIDE '0'                

 // ADJUSTME: After the Identifcation is read, wait another X ms before we switch the baud rate. define in [ms]
#define BAUDRATE_CHANGE_DELAY 500      

// ADJUSTME: Some Smart-Meters use SERIAL_8N1
#define PARITY_SETTING SERIAL_7E1    

// ADJUSTME: Some Smart Meters use a identification-code/password in the start-sequence. If that's the case the start sequence would look like: /?<ID_OR_PASSWORD>!\r\n
#define START_SEQUENCE "/?!\r\n"   

// ADJUSTME: Identifier string to successful recognize the identification response from the smart meter. 
//           Usually it's in the form of: /AAAB@xxxxxxx where AAA is the identifier string and B the max baud-rate info flag needed for the baud-switch.
//           Hint: set to NULL if you don't know it
#define METER_IDENTIFIER "LGZ"

 // ADJUSTME: Provide the OBIS Value from your smart-meter
#define OBIS_VALUE_POWER "1.7.0"

// ADJUSTME: Provide the OBIS Value from your smart-meter
#define OBIS_VALUE_TOTAL_ENERGY "1.8.0"   
#define OBIS_VALUE_TOTAL_ENERGY_TARIFF1 "1.8.1"  
#define OBIS_VALUE_TOTAL_ENERGY_TARIFF2 "1.8.2"

// The default baud rate at which the data is read
#define INITIAL_BAUD_RATE 300 

// How long to wait for the meter to send the id response for our request. [ms]            
#define SERIAL_IDENTIFICATION_READING_TIMEOUT 2000  

// How long to wait for the meter to send normal responses [ms]
#define SERIAL_READING_TIMEOUT  500                 

// How long it should take to read all the data/lines [seconds]
#define MAX_METER_READ_TIME  60                     
