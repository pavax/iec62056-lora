#include <cstdarg>
//#include <cstdint>
#include <cstdio>
#include <cstring>
#include "logger.h"
#include <HardwareSerial.h>

namespace logger
{

Level log_level = None;

HardwareSerial serialOut = NULL;

void set_serial(HardwareSerial &serial) {
  serialOut = serial;
}

void set_level(Level level) {
  log_level = level;
}

void serialPrintf(char const *level_name, char const *message) {
  if (serialOut != NULL)
    serialOut.printf("%s: %s\n", level_name, message);
}

static void log(char const *level_name, char const *fmt, va_list as)
{
  char buf[MAX_MESSAGE_LENGTH];
  vsnprintf(buf, MAX_MESSAGE_LENGTH, fmt, as);
  serialPrintf(level_name, buf);
}

#define DEFINE_LOGGING_FUNCTION(name, level) \
  void name(char const *fmt, ...)          \
  {                                        \
    if(log_level < level) return;        \
    \
    va_list as;                          \
    va_start(as, fmt);                   \
    log(#name, fmt, as);                 \
    va_end(as);                          \
  }

DEFINE_LOGGING_FUNCTION(err, Error);
DEFINE_LOGGING_FUNCTION(warn, Warning);
DEFINE_LOGGING_FUNCTION(info, Info);
DEFINE_LOGGING_FUNCTION(debug, Debug);

}
