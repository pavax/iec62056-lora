#include "config.h"
#include <map>
#include <string>
#include "Arduino.h"
#include <HardwareSerial.h>

size_t const MAX_OBIS_CODE_LENGTH = 16;
size_t const MAX_IDENTIFICATION_LENGTH = 5 + 16 + 1; /* /AAAbi...i\r */
size_t const MAX_VALUE_LENGTH = 32 + 1 + 16 + 1;   /* value: 32, *, unit: 16 */
size_t const MAX_LINE_LENGTH = 78;

unsigned int const INITIAL_BAUD_RATE = 300;

unsigned int const SERIAL_IDENTIFICATION_READING_TIMEOUT = 2000; // How long to wait for the meter to send the id response for our request. [ms]

unsigned int const SERIAL_READING_TIMEOUT = 500; // How long to wait for the meter to send normal responses [ms]

unsigned long const MAX_METER_READ_TIME = 60; // How long it should take to read all the data/lines [seconds]

/* An additional layer of protection against bit flips: the values of all exported
   objects are checked, and if they contain any characters other than these, the
   the newly-read value is discarded. This might not be needed if your optical reading
   head is very well-protected from outside light, but since the checksum is only
   1 byte, it might be worth keeping. */
char const *const OBJECT_VALUE_ALLOWED_CHARS = "0123456789.,:-";

enum Status
{
  Ready,
  Busy,
  Ok,
  TimeoutError,
  IdentificationError,
  IdentificationError_Id_Mismatch,
  ProtocolError,
  ChecksumError,
};

enum Step
{
  Initalized,
  Started,
  RequestSent,
  IdentificationRead,
  InData,
  AfterData,
};

class MeterReader {
  public:
    MeterReader(HardwareSerial &serial, const char *identifierChars): serial_(serial)
    {
      identifierChars_ = identifierChars;
    }
    //MeterReader(MeterReader const &) = delete;
    //MeterReader(MeterReader &&) = delete;
    /* Start monitoring an object. Returns true if monitoring was just started
       for the specified object, false otherwise */
    bool start_monitoring(std::string obis);

    /* Stop monitoring an object. Returns true if monitoring was just stopped
       for the specified object, false otherwise */
    bool stop_monitoring(std::string obis);

    void start_reading();

    /* Must be called frequently to advance the reading process */
    void loop();

    Status status() const {
      return status_;
    }

    /* Call this after status() returns Ok or an error to reset it to Ready */
    void acknowledge()
    {
      if (status_ != Busy) {
        status_ = Ready;
        step_ = Initalized;
      }
    }

    std::string lastReadChars()
    {
      return lastReadChars_;
    }

    size_t errors() const {
      return errors_;
    }

    size_t checksum_errors() const {
      return checksum_errors_;
    }

    size_t successes() const {
      return successes_;
    }

    std::map<std::string, std::string> const &values() const {
      return values_;
    }

  private:
    void send_request();
    void read_identification();
    void switch_baud();
    void read_line();
    void handle_object(std::string obis, std::string valuex);
    void verify_checksum();
    void change_status(Status to);

    HardwareSerial &serial_;
    Step step_;
    Status status_ = Ready;
    unsigned int baud_char_, checksum_;
    std::map<std::string, std::string> values_;
    size_t errors_ = 0, checksum_errors_ = 0, successes_ = 0;
    unsigned long startTime_;
    const char *identifierChars_;
    std::string lastReadChars_;
};
