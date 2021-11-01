/* Uncomment to strip the unit before publishing values. For example,
   "230.5" instead of "230.5*V" */
#define STRIP_UNIT

/* Uncomment to verify the checksum */
#define SKIP_CHECKSUM_CHECK

/* Uncomment to override automatic mode selection, for example to limit the baud
   rate. Use if you have problems with your optical receiver. */
//#define MODE_OVERRIDE '5'


#define BAUDRATE_CHANGE_DELAY 500           // ADJUSTME: After the Identifcation is read, wait another X ms before we switch the baud rate. define in [ms]
#define PARITY_SETTING SERIAL_7E1           // ADJUSTME: Some Smart-Meters use SERIAL_8N1
#define METER_IDENTIFIER "ELS"              // ADJUSTME: can also be set to null
#define OBIS_VALUE_POWER "1.7.0"            // ADJUSTME: Provide the OBIS Value from your smart-meter
#define OBIS_VALUE_TOTAL_ENERGY "1.8.0"     // ADJUSTME: Provide the OBIS Value from your smart-meter


#define INITIAL_BAUD_RATE 300                       // 
#define SERIAL_IDENTIFICATION_READING_TIMEOUT 2000  // How long to wait for the meter to send the id response for our request. [ms]
#define SERIAL_READING_TIMEOUT  500                 // How long to wait for the meter to send normal responses [ms]
#define MAX_METER_READ_TIME  60                     // How long it should take to read all the data/lines [seconds]
