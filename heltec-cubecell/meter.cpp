#include "meter.h"
#include "logger.h"

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
  /* 6, F */ 19200
};

struct BaudSwitchParameters {
  bool send_acknowledgement;
  uint32_t new_baud;
};

static BaudSwitchParameters baud_char_to_params(char identification) {
  if (identification >= '0' && identification <= '6')      /* Mode C */
    return {true, BAUD_RATES[identification - '0']};       /* send acknowledgement, switch baud */
  else if (identification >= 'A' && identification <= 'F') /* Mode B */
    return {false, BAUD_RATES[identification - 'A' + 1]};  /* no acknowledgement, switch baud */
  else                                                     /* possibly Mode A */
    return {false, 0};                                     /* no acknowledgement, don't switch baud */
}

static bool is_valid_object_value(std::string value) {
  for (size_t i = 0; i < value.size(); ++i)
  {
    if (!strchr(OBJECT_VALUE_ALLOWED_CHARS, value[i]))
      return false;
  }
  return true;
}

static void postprocess_value(std::string &value) {
#ifdef STRIP_UNIT
  size_t unit_sep_pos = value.find_last_of(UNIT_SEPARATOR);
  if (unit_sep_pos != std::string::npos)
    value.erase(unit_sep_pos, std::string::npos);
#endif
}

void MeterReader::start_reading() {
  /* Don't allow starting a read when one is already in progress */
  if (status_ == Busy) {
    return;
  }
  startTime_ = millis();
  Serial1.updateBaudRate(INITIAL_BAUD_RATE);


  // Hack: Sometimes it seems as there is already some data in the rx buffer thus let's clear it.
  // THE Elster AS 3000 has sometimes a weird behaviour where the data is being send in an endless loop.
  // The only way to stop the loop is by manually pressing the meter's menu button many times to navigate through all the obis values till you reach END (ETX).
  // Thus I also added a timeout check here
  logger::debug("Clear serial buffer");
  while (Serial1.read() >= 0) {
    logger::debug(".");
    if (startTime_ + (2 * 1000) < millis()) {
      change_status(TimeoutError);
      return;
    }
    delay(20);
  }

  status_ = Busy;
  step_ = Started;
  startTime_ = millis();
}

void MeterReader::send_request() {
  logger::debug("Step -> send_request");
  logger::debug(START_SEQUENCE);
  Serial1.write(START_SEQUENCE);  
  Serial1.flush();
  step_ = RequestSent;
}

void MeterReader::read_identification() {
  logger::debug("Step -> read_identification");
  Serial1.setTimeout(SERIAL_IDENTIFICATION_READING_TIMEOUT);
  static char identification[MAX_IDENTIFICATION_LENGTH];
  size_t len = Serial1.readBytesUntil('\n', identification, MAX_IDENTIFICATION_LENGTH);
  logger::debug("identification=%s", identification);
  if (len < 6) {
    logger::err("ident too short (%u chars)\n", len);
    change_status(IdentificationError);
    return;
  }

  std::string idView = std::string(identification);
  lastReadChars_ = idView;
  if ((identifierChars_ != NULL || strlen(identifierChars_) != 0) && idView.find(identifierChars_) == std::string::npos) {
    logger::err("identification not matched: %s", identification);
    change_status(IdentificationError_Id_Mismatch);
    return;
  }

  /* Remove \r and null terminate */
  identification[len - 1] = 0;

#ifndef MODE_OVERRIDE
  baud_char_ = identification[4];
#else
  baud_char_ = MODE_OVERRIDE;
#endif

  step_ = IdentificationRead;
  delay(BAUDRATE_CHANGE_DELAY);
}

void MeterReader::switch_baud() {
  logger::debug("Step -> switch_baud");
  BaudSwitchParameters params = baud_char_to_params(baud_char_);
  if (params.send_acknowledgement) {
    Serial1.printf(ACK "0%c0\r\n", baud_char_);
    Serial1.flush();
  }
  delay(50); // TODO is this needed?

  if (params.new_baud) {
    logger::debug("switching to %d bps", params.new_baud);
    Serial1.updateBaudRate(params.new_baud);
  } else {
    Serial1.updateBaudRate(INITIAL_BAUD_RATE);
  }
  Serial1.setTimeout(SERIAL_READING_TIMEOUT);
  step_ = InData;
  /* Start with checksum=STX to avoid having to avoid xoring it */
  checksum_ = STX;
}

void MeterReader::read_line() {
  static char line[MAX_LINE_LENGTH];
  size_t len = Serial1.readBytesUntil('\n', line, MAX_LINE_LENGTH);
  if (len == MAX_LINE_LENGTH) {
    logger::warn("probably truncated a line, expect a checksum error");
    return;
  } else if (len < 2) {
    /* A valid line will never be shorter than this */
    logger::warn("read short line or timed out");
    return;
  }
  for (size_t i = 0; i < len; ++i) {
    checksum_ ^= line[i];
  }
  /* readBytesUntil doesn't include the terminator, so take it into account separately */
  checksum_ ^= '\n';

  line[len - 1] = 0; /* Cut off \r before logging the line */

  logger::debug("line -> %s", line);

  if (line[len - 2] == '!') {
    /* End of data, ETX and checksum will follow */
    logger::debug("ETX");
    step_ = AfterData;
  } else {
    std::string lineView = std::string(line);
    if (lineView[0] == STX) {
      /* The first data line starts with an STX, remove it */
      lineView.erase(0, 1);
    }
    int openParen = lineView.find_first_of('(');
    int closeParen = lineView.find_last_of(')');
    if (openParen != std::string::npos && closeParen != std::string::npos) {
      std::string obis = lineView.substr(0, openParen);
      std::string measuredValue = lineView.substr(openParen + 1, closeParen - (openParen + 1));
      handle_object(obis, measuredValue);
    } else {
      logger::warn("improper data line format");
    }
  }
}

void MeterReader::handle_object(std::string obisNr, std::string obisValue) {
  std::map<std::string, std::string>::iterator entry = values_.find(obisNr);
  if (entry != values_.end()) {
    postprocess_value(obisValue);
    if (is_valid_object_value(obisValue)) {
      logger::debug(" -> found valid obis entry: %s", obisNr.c_str());
      entry->second = obisValue;
    }
  }
}

void MeterReader::verify_checksum() {
#ifndef SKIP_CHECKSUM_CHECK
  /* Expecting ETX and then the checksum */
  uint8_t etx_bcc[2];
  if (Serial1.readBytes(etx_bcc, 2) != 2 || etx_bcc[0] != ETX) {
    logger::warn("failed to read checksum");
    change_status(ProtocolError);
    return;
  }
  checksum_ ^= ETX;
  if (checksum_ != etx_bcc[1]) {
    logger::warn("checksum mismatch: %02" PRIx8 " != %02" PRIx8, checksum_, etx_bcc[1]);
    change_status(ChecksumError);
    return;
  }
#endif
  change_status(Ok);
  return;
}

void MeterReader::change_status(Status to) {
  if (to == ProtocolError || to == IdentificationError || to == IdentificationError_Id_Mismatch || to == TimeoutError)
    ++errors_;
  else if (to == ChecksumError)
    ++checksum_errors_;
  else if (to == Ok)
    ++successes_;

  status_ = to;
}

bool MeterReader::start_monitoring(std::string obis) {
  /* Don't allow adding a new monitored object in the middle of a readout */
  if (status_ == Busy) {
    return false;
  }

  values_.insert(std::pair<std::string, std::string>(obis, "") );
  return true;
}

bool MeterReader::stop_monitoring(std::string obis) {
  /* Don't allow removing a monitored object in the middle of a readout */
  if (status_ == Busy) {
    return false;
  }

  return values_.erase(obis) == 1;
}

void MeterReader::loop() {
  if (status_ != Busy) {
    return;
  }

  if (startTime_ + (MAX_METER_READ_TIME * 1000) < millis()) {
    change_status(TimeoutError);
    return;
  }

  switch (step_) {
    case Initalized: /* nothing to do, this should never happen */
      break;
    case Started:
      send_request();
      break;
    case RequestSent:
      read_identification();
      break;
    case IdentificationRead:
      switch_baud();
      break;
    case InData:
      read_line();
      break;
    case AfterData:
      verify_checksum();
      break;
  }
}
