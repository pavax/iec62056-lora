#include "LoRaWan_APP.h"
#include "Arduino.h"
#include "math.h"
#include "meter.h"
#include "logger.h"
#include "credentials.h"

#define INT_GPIO USER_KEY

//   300000  ->   5 min
//   600000  ->  10 min
//   900000  ->  15 min
//  1200000  ->  20 min
//  1800000  ->  30 min
//  3600000  ->  60 min
uint32_t sleepTime = 1200000;

/* BATTERY params */
#define MAXBATT 3400
#define MINBATT 3280

/* METER para */
static MeterReader reader(Serial1);
double power = 0;
double totalkWh = 0;
double totalkWhTariff1 = 0;
double totalkWhTariff2 = 0;
unsigned int uptimeCount = 0;
uint8_t batteryPct = 0;
uint16_t batteryVoltage = 0;


/* RETRY para */
const unsigned int INITIAL_RETRY_SLEEP_TIME = 5000;     // start retry time [ms]
float retrySleepTime = INITIAL_RETRY_SLEEP_TIME;        // Every time there's an error, this sleep time is multiplied by the multiplier faktor, up to the maximum, the normal sleepTime. [seconds]
const float BACKOFF_MULTIPLIER = 1.5;                   // 5    7.5   11.25   16.8    23.3    37.9    56.9    85.42     128

/* LOGGER para */
#define DEFAULT_LOG_LEVEL Info // DEBUG: set the Debug for more logging statements

/*LoraWan channelsmask, default channels 0-7*/
uint16_t userChannelsMask[6] = { 0x00FF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };

/* LoraWan region, select in arduino IDE tools*/
LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;

/*LoraWan Class, Class A and Class C are supported*/
DeviceClass_t  loraWanClass = LORAWAN_CLASS;

/*the application data transmission duty cycle.  value in [ms].*/
uint32_t appTxDutyCycle = sleepTime;

/*OTAA or ABP*/
bool overTheAirActivation = LORAWAN_NETMODE;

/*ADR enable*/
bool loraWanAdr = LORAWAN_ADR;

/* set LORAWAN_Net_Reserve ON, the node could save the network info to flash, when node reset not need to join again */
bool keepNet = LORAWAN_NET_RESERVE;

/* Indicates if the node is sending confirmed or unconfirmed messages */
bool isTxConfirmed = LORAWAN_UPLINKMODE;

/* Application port */
uint8_t appPort = 2;

/*!
  Number of trials to transmit the frame, if the LoRaMAC layer did not
  receive an acknowledgment. The MAC performs a datarate adaptation,
  according to the LoRaWAN Specification V1.0.2, chapter 18.4, according
  to the following table:

  Transmission nb | Data Rate
  ----------------|-----------
  1 (first)       | DR
  2               | DR
  3               | max(DR-1,0)
  4               | max(DR-1,0)
  5               | max(DR-2,0)
  6               | max(DR-2,0)
  7               | max(DR-3,0)
  8               | max(DR-3,0)

  Note, that if NbTrials is set to 1 or 2, the MAC will not decrease
  the datarate, in case the LoRaMAC layer did not receive an acknowledgment
*/
uint8_t confirmedNbTrials = 8;


//AT Command                Value
//+LORAWAN=1                LoRaWAN  1, LoRa 0
//+OTAA=1                   OTAA -1, ABP-0
//+Class=A                  Class A or C
//+ADR=1                    1 on 0 for off
//+IsTxConfirmed=1          LoRaWAN ACK Message 1 on, 0 off.
//+AppPort=2                The Application Port 2 for general APPs and 10 for TTN MAPPER.
//+DutyCycle=60000          The time between transmission in mS. Typically, 15000 to 3600000
//+ConfirmedNbTrials=8      The number of adaptive rate changes allowed.
//+DevEui=???               Unique (OTAA Mode)
//+AppEui=???               Unique (OTAA Mode)
//+AppKey=???               Unique (OTAA Mode)
//+NwkSKey=???              Unique (ABP Mode)
//+Passkey=???              Unique (ABP Mode)
//+DevAddr=???              Unique (ABP Mode)
//+LPM=1                    Low Power Mode
//+ChipID=?                 get ChipID
//+JOIN=1                   start join
//+DelCDKEY=1               to delete the CDKEY
//+DefaultSet=1             to reset parameter to Default setting
//AT+LogLevel=debug         set log level to none|debug|info|warn|error
//AT+SleepTime=600          set sleep time in seconds
bool checkUserAt(char * cmd, char * content) {
  if (strcmp(cmd, "LogLevel") == 0) {
    for (size_t i = 0; i < sizeof(content); i++) {
      content[i] = tolower(content[i]);
    }
    const std::string logLevel = std::string(content);
    if (logLevel.find("none") != std::string::npos) {
      logger::set_level(logger::None);
    } else if (logLevel.find("debug") != std::string::npos) {
      logger::set_level(logger::Debug);
    } else if (logLevel.find("info") != std::string::npos) {
      logger::set_level(logger::Info);
    } else if (logLevel.find("warn") != std::string::npos) {
      logger::set_level(logger::Warning);
    } else {
      logger::set_level(logger::Error);
    }
    Serial.println("Log Level Changed");
    return true;
  } else if (strcmp(cmd, "SleepTime") == 0) {
    sleepTime = (uint32_t) (atoi(content) * 1000);
    logger::info("Sleep Time changed to: %d", sleepTime);
    return true;
  }
  return false;
}


void downLinkDataHandle(McpsIndication_t *mcpsIndication)
{
  Serial.printf("+REV DATA:%s,RXSIZE %d,PORT %d\r\n", mcpsIndication->RxSlot ? "RXWIN2" : "RXWIN1", mcpsIndication->BufferSize, mcpsIndication->Port);
  Serial.print("+REV DATA:");
  for (uint8_t i = 0; i < mcpsIndication->BufferSize; i++)
  {
    Serial.printf("%02X", mcpsIndication->Buffer[i]);
  }
  Serial.println();

  if (mcpsIndication->Port == 4) {
    int newSleepTime = mcpsIndication->Buffer[1] | (mcpsIndication->Buffer[0] << 8);
    sleepTime  = newSleepTime * 1000;
    logger::info("Changed Sleep Time to: %d", sleepTime);
  }
}


void updateBatteryData() {
  batteryVoltage = getBatteryVoltage();
  batteryPct = map(batteryVoltage, MINBATT, MAXBATT, 0, 100);
  if (batteryPct < 0) {
    batteryPct = 0;
  } else if (batteryPct > 100) {
    batteryPct = 100;
  }
  logger::debug("Battery-Voltage: %d", batteryVoltage);
  logger::debug("Battery-Percent: %d", batteryPct);

}

void updateMeterData() {
  std::map<std::string, std::string> mymap = reader.values();
  for (std::map<std::string, std::string>::iterator it = mymap.begin(); it != mymap.end(); ++it) {
    const std::string key = it->first;
    const char *value = it->second.c_str();
    logger::debug("Result: %s \t %s", key.c_str(), value);
    if (key.compare(OBIS_VALUE_POWER) == 0) {
      power = atof(value) * 1000;
    } else if (key.compare(OBIS_VALUE_TOTAL_ENERGY) == 0) {
      totalkWh = atof(value);
    } else if (key.compare(OBIS_VALUE_TOTAL_ENERGY_TARIFF1) == 0) {
      totalkWhTariff1 = atof(value);
    } else if (key.compare(OBIS_VALUE_TOTAL_ENERGY_TARIFF2) == 0) {
      totalkWhTariff2 = atof(value);
    }
  }
}

static void prepareTxFrame( uint8_t port )
{
  /*appData size is LORAWAN_APP_DATA_MAX_SIZE which is defined in "commissioning.h".
    appDataSize max value is LORAWAN_APP_DATA_MAX_SIZE.
    if enabled AT, don't modify LORAWAN_APP_DATA_MAX_SIZE, it may cause system hanging or failure.
    if disabled AT, LORAWAN_APP_DATA_MAX_SIZE can be modified, the max value is reference to lorawan region and SF.
    for example, if use REGION_CN470,
    the max value for different DR can be found in MaxPayloadOfDatarateCN470 refer to DataratesCN470 and BandwidthsCN470 in "RegionCN470.h".
  */
  appDataSize = 18;

  // POWER (KW)
  uint16_t power_lora = power;
  appData[0] = power_lora >> 8;
  appData[1] = power_lora & 0xFF;

  // ENERGY (KWH)
  uint32_t totalkWh_lora = totalkWh * 100;
  appData[2] = totalkWh_lora >> 24;
  appData[3] = totalkWh_lora >> 16;
  appData[4] = totalkWh_lora >> 8;
  appData[5] = totalkWh_lora & 0xFF;

  // ENERGY (KWH) Tariff 1
  uint32_t totalkWh_loraTariff1 = totalkWhTariff1 * 100;
  appData[6] = totalkWh_loraTariff1 >> 24;
  appData[7] = totalkWh_loraTariff1 >> 16;
  appData[8] = totalkWh_loraTariff1 >> 8;
  appData[9] = totalkWh_loraTariff1 & 0xFF;

  // ENERGY (KWH) Tariff 1
  uint32_t totalkWh_loraTariff2 = totalkWhTariff2 * 100;
  appData[10] = totalkWh_loraTariff2 >> 24;
  appData[11] = totalkWh_loraTariff2 >> 16;
  appData[12] = totalkWh_loraTariff2 >> 8;
  appData[13] = totalkWh_loraTariff2 & 0xFF;

  // BATTERY
  appData[14] = batteryPct;
  uint16_t batteryVoltage_lora = batteryVoltage;
  appData[15] = batteryVoltage_lora >> 8;
  appData[16] = batteryVoltage_lora & 0xFF;

  // COUNTER
  appData[17] = (uint8_t)uptimeCount;
}

void onWakeUp() {
  delay(10);
  if (digitalRead(INT_GPIO) == 0) {
    Serial.println("Woke up by GPIO");
    // resetRetryTime();
    // deviceState = DEVICE_STATE_SEND; // DEBUGME: After the button is pressed direclty read data from the smart-meter
  }
}

int determineRetryTime() {
  retrySleepTime *= BACKOFF_MULTIPLIER;
  if (retrySleepTime > sleepTime) {
    retrySleepTime = sleepTime;
  }
  return retrySleepTime;
}

void resetRetryTime() {
  retrySleepTime = INITIAL_RETRY_SLEEP_TIME;
}

void setup() {
  Serial.begin(115200);

  logger::set_serial(Serial);
  logger::set_level(logger::DEFAULT_LOG_LEVEL);

  Serial1.begin(INITIAL_BAUD_RATE, PARITY_SETTING);
  Serial1.setTimeout(10);

  pinMode(Vext, OUTPUT);
  pinMode(INT_GPIO, INPUT);

  attachInterrupt(INT_GPIO, onWakeUp, FALLING);

  reader.start_monitoring(OBIS_VALUE_POWER);
  reader.start_monitoring(OBIS_VALUE_TOTAL_ENERGY);
  reader.start_monitoring(OBIS_VALUE_TOTAL_ENERGY_TARIFF1);
  reader.start_monitoring(OBIS_VALUE_TOTAL_ENERGY_TARIFF2);

#if(AT_SUPPORT)
  enableAt();
#endif

  deviceState = DEVICE_STATE_INIT;

  LoRaWAN.ifskipjoin();
  logger::info("Setup done");
}

void loop() {
  switch ( deviceState ) {
    case DEVICE_STATE_INIT:
      {
#if(AT_SUPPORT)
        getDevParam();
#endif
        printDevParam();
        LoRaWAN.init(loraWanClass, loraWanRegion);
        deviceState = DEVICE_STATE_JOIN;
        break;
      }
    case DEVICE_STATE_JOIN:
      {
        LoRaWAN.join();
        break;
      }
    case DEVICE_STATE_SEND:
      {
        Status readerState = reader.status();
        reader.loop();
        if (readerState == Ready) {
          uptimeCount ++;
          updateBatteryData();
          // cubecell cannot format float/double values (%f) -> thus let's cast to int
          logger::debug("Uptime Count:    %d", uptimeCount);
          logger::debug("Battery:         %d [%]", batteryPct);
          logger::debug("Energy:          %d [kWh]", (int)(totalkWh));
          logger::debug("Energy Tariff 1:          %d [kWh]", (int)(totalkWhTariff1));
          logger::debug("Energy Tariff 2:          %d [kWh]", (int)(totalkWhTariff2));
          logger::debug("Power:           %d [w]", (int)(power));
          logger::debug("sleepTime:       %d [s]", (int)(sleepTime / 1000.0));
          logger::debug("retrySleepTime:  %d [s]", (int)(retrySleepTime / 1000.0 ));
          logger::debug("appTxDutyCycle:  %d [s]", (int)(appTxDutyCycle / 1000.0));
          digitalWrite(Vext, LOW);
          delay(50); // TODO is this needed?
          reader.start_reading();
        } else if (readerState == Ok) {
          logger::debug("Reader OK");
          updateMeterData();
          reader.acknowledge();
          prepareTxFrame( appPort );
          LoRaWAN.send();
          resetRetryTime();
          appTxDutyCycle = sleepTime;
          deviceState = DEVICE_STATE_CYCLE;
        } else if (readerState != Busy) {
          logger::err("Reader Error with Status: %d", readerState);
          reader.acknowledge();
          appTxDutyCycle = determineRetryTime();
          deviceState = DEVICE_STATE_CYCLE;
        }
        break;
      }
    case DEVICE_STATE_CYCLE:
      {
        digitalWrite(Vext, HIGH);
        // Schedule next packet transmission
        txDutyCycleTime = appTxDutyCycle + randr( 0, APP_TX_DUTYCYCLE_RND );
        LoRaWAN.cycle(txDutyCycleTime);
        deviceState = DEVICE_STATE_SLEEP;
        logger::debug("Go to sleep for: %d ms", appTxDutyCycle);
        delay(50);
        break;
      }
    case DEVICE_STATE_SLEEP:
      {
        LoRaWAN.sleep();
        break;
      }
    default:
      {
        deviceState = DEVICE_STATE_INIT;
        break;
      }
  }
}
