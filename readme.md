# iec62056-lora
- Periodically read current power, energy consumption and other OBIS-values from a electricity meter that support the IEC 62056-21 protocol. 
- Use LoRa/TTN to send the data to a (public or private) gateway 
- The gateway POST's the data to a Home-Assistant WebHook where the data is applied to several [template sensors](https://www.home-assistant.io/integrations/template/)


## State 
- Project is still work in progress (feel free to contribute)
- In the beginning I was experimenting around with the Heltec lora esp32 micro-controller. That micro-controller is fully Arduino / Platformio compatible and work fine as long as you don't wanna power it by a battery as it consumed a lot of energy even in deep-sleep. Thus I switched to the Heltec CubeCell AB02 series that is specialized for battery operated use-cases.
- I ran a long dozen of times up and down the staircase (third floor) down to the basement for this project - if you want to avoid that, free to buy me a coffee 

  [!["Buy Me A Coffee"](https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png)](https://www.buymeacoffee.com/pavax)


# My Hardware

## Micro Controllers
- [Heltec Cubecell AB02-A](https://heltec.org/project/htcc-ab02a)
  * Powered by [1/2 AA Battery (Saft LS14250 3.6V)](https://www.reichelt.com/ch/de/lithium-batterie-1-2aa-1100-mah-1er-pack-tadiran-sl750s-p247522.html)
- [Heltec Cubecell AB02 Dev Board](https://heltec.org/project/htcc-ab02/)
- ~~[Heltec lora esp32 v2 Dev Board](https://heltec.org/project/wifi-lora-32/)~~

## Others
- [Optical reading head](https://www.ebay.de/itm/313460034498)
- [Lora antenna + 2m extension cord SMA-MALE](https://shopofthings.ch/shop/prototyping/netzwerk/868mhz-lorawan-lora-atenne-mit-3m-verlaengerungskabel-5dbi-sma-male-915mhz-gsm/)
- [PEX/IPX/U.FL to SMA-FEMALE](https://shopofthings.ch/shop/prototyping/kabel/antennenverlaengerung-ipex-ipx-u-fl-auf-sma-female-rg178-25cm/)


# Installation

## Heltec Cubecell AB02 (recommended)
* [Based on Arduino IDE](https://www.arduino.cc/en/software)
* Follow the instruction provided here: https://github.com/HelTecAutomation/CubeCell-Arduino#installation-instructions
  * The CubeCell Arduino version 1.3.0 has some issues that were fixed but not released as of today. If the version 1.4.0 is not released yet, you have to follow the instructions to install the development repository of the Heltec CubeCell boards in Arduino.
* Checkout the project ( directory `./heltec-cubecell `) and open the `.ino` file in the Arduino IDE


### Wiring
Pinout Diagramms:
  * Board: [AB02-A](https://resource.heltec.cn/download/CubeCell/HTCC-AB02A/HTCC-AB02A_PinoutDiagram.pdf)
  * Board: [AB02](https://resource.heltec.cn/download/CubeCell/HTCC-AB02/HTCC-AB02_PinoutDiagram.pdf)
  
Connect as following:

 * Optical Reading Head: VCC -> VEXT 
 * Optical Reading Head: GND -> GND
 * Optical Reading Head: RX -> RX2 (GPIO 29)
 * Optical Reading Head: TX -> TX2 (GPIO 39)

### Configuration
 - Rename credentials_example.h to credentials.h and provide your OTAA data
    * I generated random hex values for `devEui`, `appEu` and `appKey`
 - Select the correct board in the Arduino IDE:
    * Tools -> Boards -> CubeCell (in sketchbook) -> HTCC - AB02A
 - Set the LORAWAN values from within in the Arduino IDE:
    * Tools -> LORAWAN_REGION  -> your-region (eg. EU_868)
    * Tools -> LORAWAN_CLASS -> A
    * Tools -> LORAWAN_DEVEUI -> CUSTOM
    * Tools -> LORAWAN_NETMODE -> OTAA
    * Tools -> AT_SUPPORT -> ON
 - Check/Adjust the `ADJUSTME` comments 


### Debugging / Hints
* use the serial monitor in Arduino and change the `DEFAULT_LOG_LEVEL` to `debug` in the ino file
  * **Attention**: Make sure once everything works as intended to change it back to `Info` as too much logging has a negative impact on the power consumption (even if there is not serial monitor connected)
* The smart meter is not interacted with as long as the LoRaWAN has not been initialized / OTAA-registered
  * uncomment the line `deviceState = DEVICE_STATE_SEND` in the wakeup procedure to directly read the smart meter data when the on-board user button is pressed without checking/waiting for a successfully LoRaWAN registration
* Send a LoRaWan downlink message from your gateway to change the sleep time on demand. The message is read the next time the node wakes up.
    ```
    Port: 4
    payload: <desired-sleep-time-seconds-in-hex>  // 04B0 = 1200 seconds  = 20 min
    ```
* Make sure you have a decent LoRaWAN connectivity where your smart-meter is located or nearby by using an extension cord/antenna. I played around with a simple LoRaWAN example sketch from Heltec to find a good spot with a decent connectivity: 

    `Examples -> CubeCell -> LoRa -> LoRaWAN -> LoRaWAN`

    And then verified in my Lora-Gateway if the messages were received and if so, what their RSSI/SNR data were.


## ~~Heltec Wifi LoRA 32 V2 (deprecated)~~

* Based on Platformio
* Not suitable for my use-case as it consumed to much power (even in deep-sleep) and thus couldn't get it to operate by battery

# Supported Smart Meters

- [Elster AS3000](https://wiki.volkszaehler.org/hardware/channels/meters/power/edl-ehz/elster_as3000)
  ```
  SKIP_CHECKSUM_CHECK
  METER_IDENTIFIER "ELS"
  BAUDRATE_CHANGE_DELAY: 500
  PARITY_SETTING SERIAL_7E1 
  OBIS_VALUE_POWER "1.7.0 "
  OBIS_VALUE_TOTAL_ENERGY "1.8.0"
  INITIAL_BAUD_RATE 300
  SERIAL_IDENTIFICATION_READING_TIMEOUT 2000
  SERIAL_READING_TIMEOUT  500
  MAX_METER_READ_TIME  60
  ```

- [Landys&Gyr ZMD120AR21](https://wiki.volkszaehler.org/hardware/channels/meters/power/edl-ehz/landisgyr_zmd120ap?redirect=1)
  ```
  SKIP_CHECKSUM_CHECK
  METER_IDENTIFIER "LGZ"
  BAUDRATE_CHANGE_DELAY: 500
  PARITY_SETTING SERIAL_7E1 
  OBIS_VALUE_POWER "1.7.0 "
  OBIS_VALUE_TOTAL_ENERGY "1.8.0"
  OBIS_VALUE_TOTAL_ENERGY_TARIFF1 "1.8.1"
  OBIS_VALUE_TOTAL_ENERGY_TARIFF2 "1.8.2"
  INITIAL_BAUD_RATE 300
  SERIAL_IDENTIFICATION_READING_TIMEOUT 2000
  SERIAL_READING_TIMEOUT  500
  MAX_METER_READ_TIME  60
  ```


## Acknowledgements 
- https://github.com/mwdmwd/iec62056-mqtt
- https://wiki.volkszaehler.org/hardware/channels/meters/power/edl-ehz/elster_as1440


# TheThingsNetwork Integration
For TTN we can define a uplink payload formater to convert the data to a be used later in Home-Assistant via MQTT

```js
function decodeUplink(input) {
  return {
    data: {
      bytes: input.bytes,
      power: (input.bytes[0] << 8) + (input.bytes[1]),
      energy: (input.bytes[2] << 24) + (input.bytes[3] << 16) + (input.bytes[4] << 8) + (input.bytes[5]),
      energy1: (input.bytes[6] << 24) + (input.bytes[7] << 16) + (input.bytes[8] << 8) + input.bytes[9],
      energy2: (input.bytes[10] << 24) + (input.bytes[11] << 16) + (input.bytes[12] << 8) + input.bytes[13],
      batteryPCT: input.bytes[14],
      battery:(input.bytes[15] << 8) + input.bytes[16],
      counter: input.bytes[17]
    },
    warnings: [],
    errors: []
  };
}
```

The coresponding MQQT Sensor in HomeAssistant can be configured like this. Replace APPLICAITON and DEVICE_EUI with the name of you TTN aplication and ID of your device. You can find the required data in the TTN console.

```yaml
mqtt:
  sensor:
    - state_topic: "v3/APPLICATION@ttn/devices/DEVICE_EUI/up"
      name: "Smart Meter Kwh"
      unique_id: smart_meter_kwh
      icon: mdi:chart-histogram
      unit_of_measurement: "kWh"
      state_class: total_increasing
      device_class: energy
      value_template: >-
        {% set value = value_json.uplink_message.decoded_payload.energy | default(none) %}
        {% if value == '00000000' %}
          {{ none }}
        {% else %}
          {{ value | int(value,16)/100|float }}
        {% endif %}
    - state_topic: "v3/APPLICATION@ttn/devices/DEVICE_EUI/up"
      name: "Smart Meter Kwh Peak"
      unique_id: smart_meter_kwh_peak
      icon: mdi:chart-histogram
      unit_of_measurement: "kWh"
      state_class: total_increasing
      device_class: energy
      value_template: >-
        {% set value = value_json.uplink_message.decoded_payload.energy1 | default(none) %}
        {% if value == '00000000' %}
          {{ none }}
        {% else %}
          {{ value | int(value,16)/100|float }}
        {% endif %}
    - state_topic: "v3/APPLICATION@ttn/devices/DEVICE_EUI/up"
      name: "Smart Meter Kwh OffPeak"
      unique_id: smart_meter_kwh_offPeak
      icon: mdi:chart-histogram
      unit_of_measurement: "kWh"
      state_class: total_increasing
      device_class: energy
      value_template: >-
        {% set value = value_json.uplink_message.decoded_payload.energy2 | default(none) %}
        {% if value == '00000000' %}
          {{ none }}
        {% else %}
          {{ value | int(value,16)/100|float }}
        {% endif %}
    - state_topic: "v3/APPLICATION@ttn/devices/DEVICE_EUI/up"
      name: "Smart Meter Power"
      unique_id: smart_meter_power
      icon: mdi:flash-outline
      unit_of_measurement: "W"
      state_class: measurement
      device_class: power
      value_template: >-
        {% set value = value_json.uplink_message.decoded_payload.power | default(none) %}
        {% if value == '0000' %}
          {{ none }}
        {% else %}
          {{ value | int(value,16) }}
        {% endif %}
    - state_topic: "v3/APPLICATION@ttn/devices/DEVICE_EUI/up"
      name: "Smart Meter Battery"
      unique_id: smart_meter_battery
      icon: mdi:battery
      unit_of_measurement: "%"
      device_class: battery
      value_template: >-
        {% set value = value_json.uplink_message.decoded_payload.batteryPCT | default(none) %}
        {% if value == '00' %}
          {{ none }}
        {% else %}
          {{ value | int(value,16) }}
        {% endif %}
    - state_topic: "v3/APPLICATION@ttn/devices/DEVICE_EUI/up"
      name: "Smart Meter Battery Voltage"
      unique_id: smart_meter_battery_voltage
      icon: mdi:battery
      unit_of_measurement: "mV"
      state_class: measurement
      device_class: battery
      value_template: >-
        {% set value = value_json.uplink_message.decoded_payload.battery | default(none) %}
        {% if value == '0000' %}
          {{ none }}
        {% else %}
          {{ value | int(value,16) }}
        {% endif %}
    - state_topic: "v3/APPLICATION@ttn/devices/DEVICE_EUI/up"
      name: "Smart Meter Up-Counter"
      unique_id: smart_meter_upcounter
      icon: mdi:counter
      unit_of_measurement: "times"
      value_template: >-
        {% set value = value_json.uplink_message.decoded_payload.counter | default(none) %}
        {% if value == '00' %}
          {{ none }}
        {% else %}
          {{ value | int(value,16) }}
        {% endif %}
```


# Home-Assistant Template Sensors

```yaml

- trigger:
    - platform: webhook
      webhook_id: lorawan-data
      id: lorawan-data
  unique_id: smart_meter
  sensor:
    - name: "Smart Meter Power"
      unique_id: smart_meter_power
      icon: mdi:flash-outline
      unit_of_measurement: W
      device_class: power
      state_class: measurement
      state: >-
        {% set payloadHex = trigger.json.data.payload_hex | default(none) %}
        {% set value = payloadHex[0:4] %}
        {% if value == '0000' %}
          {{ none }}
        {% else %}
          {{ value | int(value,16) }}
        {% endif %}

    - name: "Smart Meter Kwh"
      unique_id: smart_meter_kwh
      icon: mdi:chart-histogram
      unit_of_measurement: "kWh"
      state_class: measurement
      device_class: energy
      state: >-
        {% set payloadHex = trigger.json.data.payload_hex | default(none) %}
        {% set value = payloadHex[4:12] %}
        {% if value == '00000000' %}
          {{ none }}
        {% else %}
          {{ value | int(value,16)/100|float }}
        {% endif %}

    - name: "Smart Meter Battery"
      unique_id: smart_meter_battery
      icon: mdi:battery
      unit_of_measurement: "%"
      device_class: battery
      state: >-
        {% set payloadHex = trigger.json.data.payload_hex | default(none) %}
        {% set value = payloadHex[12:14] %}
        {% if value == '00' %}
          {{ none }}
        {% else %}
          {{ value | int(value,16) }}
        {% endif %}

    - name: "Smart Meter Battery Voltage"
      unique_id: smart_meter_battery_voltage
      icon: mdi:battery
      unit_of_measurement: "mV"
      state_class: measurement
      device_class: voltage
      state: >-
        {% set payloadHex = trigger.json.data.payload_hex | default(none) %}
        {% set value = payloadHex[14:18] %}
        {% if value == '0000' %}
          {{ none }}
        {% else %}
          {{ value | int(value,16) }}
        {% endif %}

    - name: "Smart Meter Up-Counter"
      unique_id: smart_meter_upcounter
      icon: mdi:counter
      unit_of_measurement: "times"
      state: >-
        {% set payloadHex = trigger.json.data.payload_hex | default(none) %}
        {% set value = payloadHex[18:20] %}
        {% if value == '00' %}
          {{ none }}
        {% else %}
          {{ value | int(value,16) }}
        {% endif %}

```
