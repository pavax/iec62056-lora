/* Uncomment to strip the unit before publishing values. For example,
   "230.5" instead of "230.5*V" */
#define STRIP_UNIT

/* Uncomment to verify the checksum */
#define SKIP_CHECKSUM_CHECK

/* Uncomment to override automatic mode selection, for example to limit the baud
   rate. Use if you have problems with your optical receiver. */
//#define MODE_OVERRIDE '5'

/* After the Identifcation is read, wait another X ms before we switch the baud rate. define in [ms] */
#define BAUDRATE_CHANGE_DELAY 500 // ADJUSTME

#define PARITY_SETTING SERIAL_7E1 // ADJUSTME: Some Smart-Meters use SERIAL_8N1
