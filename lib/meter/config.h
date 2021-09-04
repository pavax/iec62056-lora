#define IRINVERTED false // set this to: true for local-dev

/* Uncomment to strip the unit before publishing values. For example,
   "230.5" instead of "230.5*V" */
#define STRIP_UNIT

/* Set to an unused pin (needed to switch between RX and TX only) */
#define DUMMY_PIN 23

/* Uncomment to verify the checksum */
#define SKIP_CHECKSUM_CHECK

/* An additional layer of protection against bit flips: the values of all exported
   objects are checked, and if they contain any characters other than these, the
   the newly-read value is discarded. This might not be needed if your optical reading
   head is very well-protected from outside light, but since the checksum is only
   1 byte, it might be worth keeping. */
char const *const OBJECT_VALUE_ALLOWED_CHARS = "0123456789.,:-";

/* Uncomment to override automatic mode selection, for example to limit the baud
   rate. Use if you have problems with your optical receiver. */
//#define MODE_OVERRIDE '5'