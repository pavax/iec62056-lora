#include <Arduino.h>
#include <esp_adc_cal.h>
#include <driver/adc.h>
#include <U8g2lib.h>
#include "RunningAverage.h"
#include <TTN_esp32.h>
#include "meter.h"
#include "credentials.h"

#define TRANSISTOR_PIN 17

RTC_DATA_ATTR unsigned int uptimeCount = 0;

enum RetryReason
{
  NONE,
  METER_ERROR,
  TTN_ERROR
};
RTC_DATA_ATTR RetryReason lastRetryReason;
RTC_DATA_ATTR float retrySleepTime;             // Every time there's an error, this delay is doubled, up to a maximum. [seconds]
const unsigned int INITAL_RETRY_SLEEP_TIME = 1; // start retry time [seconds]
const float BACKOFF_MULTIPLIER = 1.5;           // 1   2   3   5   7   11    17    25    38    57    86    129   194    291     437
const unsigned DEEP_SLEEP_TIME = 600;           // normal deep sleep time [seconds]

char sendingStatus[10];
double power, totalkWh;
int batteryPct = 0;

/**********
 * LORA
 **********/
const unsigned MAX_SENDING_TIME = 20; // max time to send the message to ttn [seconds]
TTN_esp32 ttn;

/**********
 * OLED
 **********/
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/15, /* data=*/4, /* reset=*/16);

/**********
 * BATTERY 
 **********/
#define MAXBATT 4200 // The default Lipo is 4200mv when the battery is fully charged.
//#define LIGHT_SLEEP_VOLTAGE 3750               // Point where start light sleep [not implemented yet]
#define MINBATT 3200                             // The default Lipo is 3200mv when the battery is empty...this WILL be low on the 3.3v rail specs!!!
#define VOLTAGE_DIVIDER 3.20                     // Lora has 220k/100k voltage divider so need to reverse that reduction via (220k+100k)/100k on vbat GPIO37 or ADC1_1 (early revs were GPIO13 or ADC2_4 but do NOT use with WiFi.begin())
#define DEFAULT_VREF 1100                        // Default VREF use if no e-fuse calibration
#define VBATT_SMOOTH 64                          // Number of averages in sample
#define ADC_READ_STABILIZE 5                     // in ms (delay from GPIO control and ADC connections times)
#define LO_BATT_SLEEP_TIME 10 * 60 * 1000 * 1000 // How long when low batt to stay in sleep (us)
#define HELTEC_V2_1 1                            // Set this to switch between GPIO13(V2.0) and GPIO37(V2.1) for VBatt ADC.
//#define VBATT_GPIO Vext                        // Heltec GPIO to toggle VBatt read connection ... WARNING!!! This also connects VEXT to VCC=3.3v so be careful what is on header.  Also, take care NOT to have ADC read connection in OPEN DRAIN when GPIO goes HIGH
//#define __DEBUG 1                              // DEBUG Serial output
esp_adc_cal_characteristics_t *adc_chars;
RunningAverage batteryValues(VBATT_SMOOTH);

/**********
 * OPTICAL METER
 **********/
const std::map<MeterReader::Status, std::string> METER_STATUS_MAP = {
    {MeterReader::Status::Ready, "Ready"},
    {MeterReader::Status::Busy, "Busy"},
    {MeterReader::Status::Ok, "Ok"},
    {MeterReader::Status::TimeoutError, "Timeout"},
    {MeterReader::Status::IdentificationError, "Err-Idn-1"},
    {MeterReader::Status::IdentificationError_Id_Mismatch, "Err-Idn-2"},
    {MeterReader::Status::ProtocolError, "Err-Prot"},
    {MeterReader::Status::ChecksumError, "Timeout"},
    {MeterReader::Status::TimeoutError, "Err-Chk"}};
char const *const EXPORT_OBJECTS[] = {
    "1.7.0", // momentane leistung
    "1.8.0"  // total kwh
};
static MeterReader reader(Serial2, 12, 13, "ELS"); // CHANGEME: Adapt RX and TX Pin
//static MeterReader reader(Serial2, 12, 13, NULL);   // CHANGEME: Use this if you don't know the Identifier of your meter (for example: /ELS5\@V10.04)

void printRuntime()
{
  long seconds = millis() / 1000;
  Serial.print("Runtime: ");
  Serial.print(seconds);
  Serial.println(" seconds");
}

int retryTimeSeconds(RetryReason retryReason)
{
  Serial.printf("Last Retry-Reason: %d - Current Retry-Reason: %d\n", lastRetryReason, retryReason);
  if (lastRetryReason != retryReason)
    retrySleepTime = INITAL_RETRY_SLEEP_TIME;

  lastRetryReason = retryReason;
  retrySleepTime *= BACKOFF_MULTIPLIER;
  return retrySleepTime;
}

void resetRetryTime()
{
  lastRetryReason = NONE;
  retrySleepTime = INITAL_RETRY_SLEEP_TIME;
}

void goDeepSleep(unsigned int deepSleepTime)
{
  if (deepSleepTime > DEEP_SLEEP_TIME)
  {
    resetRetryTime();
    Serial.println("Enough retry -> Let's restart");
    ESP.restart();
  }

  Serial.printf("Go DeepSleep for %d seconds\n", deepSleepTime);
  printRuntime();
  u8g2.sleepOn();
  ttn.stop();
  Serial.flush();
  Serial2.flush();
  Serial.end();
  Serial2.end();
  digitalWrite(Vext, HIGH);          // Turn off Vext
  digitalWrite(TRANSISTOR_PIN, LOW); // turn of transistor
  pinMode(5, INPUT);
  pinMode(14, INPUT);
  pinMode(15, INPUT);
  pinMode(16, INPUT);
  //pinMode(17, INPUT);
  pinMode(18, INPUT);
  pinMode(19, INPUT);
  pinMode(26, INPUT);
  pinMode(27, INPUT);
  gpio_num_t wakeup_gpio = (gpio_num_t)KEY_BUILTIN;
  esp_sleep_enable_ext1_wakeup(1ULL << wakeup_gpio, ESP_EXT1_WAKEUP_ALL_LOW);
  esp_sleep_enable_timer_wakeup(deepSleepTime * 1000000);
  esp_deep_sleep_start();
}

void blink(int times)
{
  for (int x = 0; x <= times; x++)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
  }
}

uint16_t readBatteryVoltageSample()
{
  // Poll the proper ADC for VBatt on Heltec Lora 32 with GPIO21 toggled
  uint16_t reading = 666;
  digitalWrite(Vext, LOW);   // ESP32 Lora v2.1 reads on GPIO37 when GPIO21 is low
  delay(ADC_READ_STABILIZE); // let GPIO stabilize
#if (defined(HELTEC_V2_1))
  pinMode(ADC1_GPIO37_CHANNEL, OPEN_DRAIN); // ADC GPIO37
  reading = adc1_get_raw(ADC1_GPIO37_CHANNEL);
  pinMode(ADC1_GPIO37_CHANNEL, INPUT); // Disconnect ADC before GPIO goes back high so we protect ADC from direct connect to VBATT (i.e. no divider)
#else
  pinMode(ADC2_GPIO13_CHANNEL, OPEN_DRAIN); // ADC GPIO13
  adc2_get_raw(ADC2_GPIO13_CHANNEL, ADC_WIDTH_BIT_12, &reading);
  pinMode(ADC2_GPIO13_CHANNEL, INPUT); // Disconnect ADC before GPIO goes back high so we protect ADC from direct connect to VBATT (i.e. no divider
#endif

  uint16_t voltage = esp_adc_cal_raw_to_voltage(reading, adc_chars);
  voltage *= VOLTAGE_DIVIDER;

  return voltage;
}

std::string meterStatus()
{
  MeterReader::Status status = reader.status();
  auto it = METER_STATUS_MAP.find(status);
  if (it == METER_STATUS_MAP.end())
    return "Unknw";
  return it->second;
}

void displayUpdate()
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_amstrad_cpc_extended_8f);

  u8g2.setCursor(3, 10);

  u8g2.print("Watt:");
  u8g2.printf("%10.0f", power);
  u8g2.print("");

  u8g2.setCursor(3, 22);
  u8g2.print("KWh:");
  u8g2.printf("%11.2f", totalkWh);

  u8g2.setCursor(3, 34);
  u8g2.print("Batt:");
  u8g2.printf("%10d", batteryPct);

  u8g2.setCursor(3, 46);
  u8g2.print("State:");
  u8g2.printf("%9s", meterStatus().c_str());

  std::string lastReadChars = reader.lastReadChars();
  if (lastReadChars.size() > 0)
  {
    u8g2.setCursor(3, 58);
    u8g2.printf("%s", lastReadChars.c_str());
  }

  if (strlen(sendingStatus) > 0)
  {
    u8g2.setCursor(3, 58);
    u8g2.print("TTN:");
    u8g2.printf("%11s", sendingStatus);
  }
  else
  {
    u8g2.setCursor(3, 58);
    u8g2.print("Count:");
    u8g2.printf("%9d", uptimeCount);
  }

  u8g2.sendBuffer();
}

void batteryUpdate()
{
  for (int x = 0; x <= VBATT_SMOOTH; x++)
  {
    auto value = readBatteryVoltageSample();
    batteryValues.addValue(value);
  }
  uint16_t v = batteryValues.getAverage();
  if (v < MINBATT)
    v = MINBATT;
  if (v > MAXBATT)
    v = MAXBATT;
  batteryPct = map(v, MINBATT, MAXBATT, 0, 100);
#if defined(__DEBUG) && __DEBUG > 0
  Serial.printf("Batt Value: %d (%4.2f%) \n", v, batteryPct);
#endif
}

void updateMeterData()
{
  for (auto const &entry : reader.values())
  {
    auto value = entry.second.c_str();
    Serial.printf("Result: %s \t %s \n", entry.first.c_str(), value);
    if (entry.first.compare("1.7.0") == 0)
      power = atof(value) * 1000;
    if (entry.first.compare("1.8.0") == 0)
      totalkWh = atof(value);
  }
  displayUpdate();
}

void onMessage(const uint8_t *payload, size_t size, int rssi)
{
  Serial.println("-- MESSAGE");
  Serial.print("Received " + String(size) + " bytes RSSI=" + String(rssi) + "db");
  for (int i = 0; i < size; i++)
  {
    Serial.print(" " + String(payload[i]));
    // Serial.write(payload[i]);
  }

  Serial.println();
}

void waitForTransactions()
{
  auto waitTime = ttn.waitForPendingTransactions();
  Serial.println("Waiting for pending transactions... ");
  Serial.println("Waiting took " + String(waitTime) + "ms");
}

bool sendBytes()
{
  waitForTransactions();
  uint8_t LORA_DATA[8];

  uint16_t power_lora = power;
  LORA_DATA[0] = power_lora >> 8;
  LORA_DATA[1] = power_lora & 0xFF;

  uint32_t totalkWh_lora = totalkWh * 100;
  LORA_DATA[2] = totalkWh_lora >> 24;
  LORA_DATA[3] = totalkWh_lora >> 16;
  LORA_DATA[4] = totalkWh_lora >> 8;
  LORA_DATA[5] = totalkWh_lora & 0xFF;

  uint8_t battery_lora = batteryPct;
  LORA_DATA[6] = battery_lora;

  uint8_t uptimeCount_lora = uptimeCount;
  LORA_DATA[7] = uptimeCount_lora;

  if (ttn.sendBytes(LORA_DATA, sizeof(LORA_DATA), 2, false))
  {
    Serial.println("Paket send");
    waitForTransactions();
    return true;
  }
  else
  {
    waitForTransactions();
    return false;
  }
}

void prepareTTN()
{
  ttn.join();
  unsigned long startJoiningTime = millis();
  Serial.print("Joining TTN ");
  strncpy(sendingStatus, "Joining", sizeof(sendingStatus) - 1);
  displayUpdate();
  bool joined = false;
  while ((!joined) == (startJoiningTime + (MAX_SENDING_TIME * 1000) > millis()))
  {
    joined = ttn.isJoined();
    Serial.print(".");
    displayUpdate();
    delay(500);
  }

  if (!joined)
  {
    Serial.println("\nJoining failed go back to sleep");
    strncpy(sendingStatus, "Failed", sizeof(sendingStatus) - 1);
    displayUpdate();
    ttn.deleteSession();
    delay(100);
    goDeepSleep(retryTimeSeconds(TTN_ERROR));
    return;
  }
  else
  {
    Serial.println("\njoined!");
    strncpy(sendingStatus, "Joined", sizeof(sendingStatus) - 1);
    displayUpdate();
  }
}

void sendData()
{
  if (!sendBytes())
  {
    Serial.println("Send Failed");
    strncpy(sendingStatus, "Failed", sizeof(sendingStatus) - 1);
    displayUpdate();
    ttn.deleteSession();
    delay(100);
    goDeepSleep(retryTimeSeconds(TTN_ERROR));
    return;
  }
  Serial.println("SENT!");
  strncpy(sendingStatus, "SENT", sizeof(sendingStatus) - 1);
  displayUpdate();
}

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    delay(20);

  uptimeCount++;
  sendingStatus[0] = 0;
  Serial.printf("Starting: %d \n", uptimeCount);

//BATTERY
#if (defined(HELTEC_V2_1))
  adc_chars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_6, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_1, ADC_ATTEN_DB_6);
#else
  // Use this for older V2.0 with VBatt reading wired to GPIO13
  adc_chars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_2, ADC_ATTEN_DB_6, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
  adc2_config_channel_atten(ADC2_CHANNEL_4, ADC_ATTEN_DB_6);
#endif
  if (val_type)
    ; // Suppress warning

  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);   // ESP32 Lora v2.1 reads on GPIO37 when GPIO21 is low
  delay(ADC_READ_STABILIZE); // let GPIO stabilize

  // LED
  pinMode(LED_BUILTIN, OUTPUT);

  // METER
  pinMode(TRANSISTOR_PIN, OUTPUT);
  digitalWrite(TRANSISTOR_PIN, HIGH);
  for (char const *obis : EXPORT_OBJECTS)
  {
    reader.start_monitoring(obis);
  }

  // LORA INIT
  ttn.begin();
  if (uptimeCount % 20 == 0) // delete session every x count;
  {
    Serial.printf("Delete TTN Session");
    ttn.deleteSession();
  }
  ttn.onMessage(onMessage);
  ttn.provision(devEui, appEui, appKey);

  // DISPLAY
  u8g2.begin();
  u8g2.enableUTF8Print();
}

void loop()
{
  reader.loop();
  const MeterReader::Status status = reader.status();
  if (status == MeterReader::Status::Ready)
  {
    reader.start_reading();
    batteryUpdate();
    displayUpdate();
  }
  else if (status == MeterReader::Status::Ok)
  {
    updateMeterData();
    prepareTTN();
    sendData();
    resetRetryTime();
    blink(1);
    goDeepSleep(DEEP_SLEEP_TIME);
  }
  else if (status != MeterReader::Status::Busy) /* Not Ready, Ok or Busy => error */
  {
    displayUpdate();
    blink(5);
    delay(2000);
    goDeepSleep(retryTimeSeconds(METER_ERROR));
  }
}