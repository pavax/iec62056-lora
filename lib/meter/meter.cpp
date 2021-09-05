#include <cinttypes>
#include <cstdint>
#include "config.h"
#include "meter.h"

#define STX '\x02'
#define ETX '\x03'

#define ACK "\x06"

#define UNIT_SEPARATOR '*'

uint16_t const BAUD_RATES[] = {
    /* 0 */ 300,
    /* 1, A */ 600,
    /* 2, B */ 1200,
    /* 3, C */ 2400,
    /* 4, D */ 4800,
    /* 5, E */ 9600,
    /* 6, F */ 19200};

struct BaudSwitchParameters
{
  bool send_acknowledgement;
  uint32_t new_baud;
};

static BaudSwitchParameters baud_char_to_params(char identification)
{
  if (identification >= '0' && identification <= '6')      /* Mode C */
    return {true, BAUD_RATES[identification - '0']};       /* send acknowledgement, switch baud */
  else if (identification >= 'A' && identification <= 'F') /* Mode B */
    return {false, BAUD_RATES[identification - 'A' + 1]};  /* no acknowledgement, switch baud */
  else                                                     /* possibly Mode A */
    return {false, 0};                                     /* no acknowledgement, don't switch baud */
}

static bool is_valid_object_value(std::string value)
{
  for (size_t i = 0; i < value.size(); ++i)
  {
    if (!strchr(OBJECT_VALUE_ALLOWED_CHARS, value[i]))
      return false;
  }
  return true;
}

static void postprocess_value(std::string &value)
{
#ifdef STRIP_UNIT
  size_t unit_sep_pos = value.find_last_of(UNIT_SEPARATOR);
  if (unit_sep_pos != std::string::npos)
    value.erase(unit_sep_pos, std::string::npos);
#endif
}

enum class MeterReader::Step : uint8_t
{
  Ready,
  Started,
  RequestSent,
  IdentificationRead,
  InData,
  AfterData,
};

/* status != Busy => status = Busy => continued on next line
   status = Busy => step = Started => ... => step = AfterData => status = Ok            => status = Ready
                                      ... => status = ProtocolError                     => status = Ready
                                                              => status = ChecksumError => status = Ready
                                                              => status = ProtocolError => status = Ready
*/

void MeterReader::start_reading()
{
  /* Don't allow starting a read when one is already in progress */
  if (status_ == Status::Busy)
    return;

  status_ = Status::Busy;
  step_ = Step::Started;
  startTime_ = millis();

  // prepare serial
  serial_.setTimeout(SERIAL_TIMEOUT);
  serial_.begin(INITIAL_BAUD_RATE, SERIAL_7E1, rx_, tx_, IRINVERTED);

  // Hack: Sometimes it seems as there is already some data in the rx buffer thus let's clear it.
  // THE Elster AS 3000 has sometimes a weird behaviour where the data is being send in an endless loop.
  // The only way to stop the loop is by manually pressing the meter's menu button many times to navigate through all the obis values till you reach END (ETX).
  // Thus I also added a timeout check here
  Serial.print("Clear serial buffer");
  while (serial_.read() >= 0)
  {
    Serial.print(".");
    if (startTime_ + (2 * 1000) < millis())
      return change_status(Status::TimeoutError);
    delay(20);
  }
  Serial.println("");
}

void MeterReader::send_request()
{
  Serial.println("Step -> send_request");
  Serial.print("/?!\r\n");
  serial_.write("/?!\r\n");
  serial_.flush();

  step_ = Step::RequestSent;
}

void MeterReader::read_identification()
{
  Serial.println("Step -> read_identification");
  serial_.setTimeout(SERIAL_TIMEOUT * 2); // double the normal timeout at the beginning
  char identification[MAX_IDENTIFICATION_LENGTH];
  size_t len = serial_.readBytesUntil('\n', identification, MAX_IDENTIFICATION_LENGTH);
  std::string idView = std::string(identification);
  lastReadChars_ = idView;
  Serial.printf("identification=%s\n", identification);

  if (len < 6)
  {
    Serial.printf("ident too short (%u chars)\n", len);
    return change_status(Status::IdentificationError);
  }

  if (identifierChars_ != NULL && idView.find(identifierChars_) == std::string::npos)
  {
    Serial.printf("identification not matched: %s \n", identification);
    return change_status(Status::IdentificationError_Id_Mismatch);
  }

  identification[len - 1] = 0; /* Remove \r and null terminate */
  Serial.printf("identification=%s\n", identification);

#ifndef MODE_OVERRIDE
  baud_char_ = identification[4];
#else
  baud_char_ = MODE_OVERRIDE;
#endif

  step_ = Step::IdentificationRead;
  serial_.setTimeout(SERIAL_TIMEOUT); // switch back to the normal timeout
  delay(1000);                        // not sure if needed anymore....
}

void MeterReader::switch_baud()
{
  Serial.println("Step -> switch_baud");
  BaudSwitchParameters params = baud_char_to_params(baud_char_);

  if (params.send_acknowledgement)
  {
    serial_.printf(ACK "0%c0\r\n", baud_char_);
    serial_.flush();
  }

  if (params.new_baud)
  {
    Serial.printf("switching to %d bps\n", params.new_baud);
    serial_.begin(params.new_baud, SERIAL_7E1, rx_, DUMMY_PIN, IRINVERTED);
  }
  else
  {
    serial_.begin(INITIAL_BAUD_RATE, SERIAL_7E1, rx_, DUMMY_PIN, IRINVERTED);
  }

  step_ = Step::InData;
  checksum_ = STX; /* Start with checksum=STX to avoid having to avoid xoring it */
}

void MeterReader::read_line()
{
  static char line[MAX_LINE_LENGTH];

  size_t len = serial_.readBytesUntil('\n', line, MAX_LINE_LENGTH);
  if (len == MAX_LINE_LENGTH)
  {
    Serial.println("probably truncated a line, expect a checksum error");
  }
  else if (len < 2) /* A valid line will never be shorter than this */
  {
    Serial.println("read short line or timed out");
    return;
  }

  for (size_t i = 0; i < len; ++i)
  {
    checksum_ ^= line[i];
  }
  /* readBytesUntil doesn't include the terminator, so take it into account separately */
  checksum_ ^= '\n';

  line[len - 1] = 0; /* Cut off \r before logging the line */
  Serial.printf("line: %s \n", line);

  if (line[len - 2] == '!') /* End of data, ETX and checksum will follow */
  {
    Serial.printf("ETX\n");
    step_ = Step::AfterData;
  }
  else
  {
    std::string lineView = std::string(line);

    if (lineView[0] == STX) /* The first data line starts with an STX, remove it */
      lineView.erase(0, 1);

    auto openParen = lineView.find_first_of('(');
    auto closeParen = lineView.find_last_of(')');
    if (openParen != std::string::npos && closeParen != std::string::npos)
    {
      auto obis = lineView.substr(0, openParen);
      auto value = lineView.substr(openParen + 1, closeParen - (openParen + 1));
      handle_object(obis, value);
    }
    else
    {
      Serial.println("improper data line format");
    }
  }
}

void MeterReader::handle_object(std::string obis, std::string value)
{
  auto entry = values_.find(obis);
  if (entry != values_.end())
  {
    postprocess_value(value);
    if (is_valid_object_value(value))
    {
      entry->second = value;
    }
  }
}

void MeterReader::verify_checksum()
{
#ifndef SKIP_CHECKSUM_CHECK
  /* Expecting ETX and then the checksum */
  uint8_t etx_bcc[2];
  if (serial_.readBytes(etx_bcc, 2) != 2 || etx_bcc[0] != ETX)
  {
    Serial.println("failed to read checksum");
    return change_status(Status::ProtocolError);
  }

  checksum_ ^= ETX;
  if (checksum_ != etx_bcc[1])
  {
    Serial.printf("checksum mismatch: %02" PRIx8 " != %02" PRIx8, checksum_, etx_bcc[1]);
    return change_status(Status::ChecksumError);
  }
#endif

  return change_status(Status::Ok); /* Data readout successful */
}

void MeterReader::change_status(Status to)
{
  if (to == Status::ProtocolError || to == Status::IdentificationError || to == Status::IdentificationError_Id_Mismatch || to == Status::TimeoutError)
    ++errors_;
  else if (to == Status::ChecksumError)
    ++checksum_errors_;
  else if (to == Status::Ok)
    ++successes_;

  status_ = to;
}

bool MeterReader::start_monitoring(std::string obis)
{
  /* Don't allow adding a new monitored object in the middle of a readout */
  if (status_ == Status::Busy)
    return false;

  values_.emplace(obis, "");
  return true;
}

bool MeterReader::stop_monitoring(std::string obis)
{
  /* Don't allow removing a monitored object in the middle of a readout */
  if (status_ == Status::Busy)
    return false;

  return values_.erase(obis) == 1;
}

void MeterReader::loop()
{
  if (status_ != Status::Busy)
    return;

  if (startTime_ + (MAX_METER_READ_TIME * 1000) < millis())
    return change_status(Status::TimeoutError);

  switch (step_)
  {
  case Step::Ready: /* nothing to do, this should never happen */
    break;
  case Step::Started:
    send_request();
    break;
  case Step::RequestSent:
    read_identification();
    break;
  case Step::IdentificationRead:
    switch_baud();
    break;
  case Step::InData:
    read_line();
    break;
  case Step::AfterData:
    verify_checksum();
    break;
  }
}