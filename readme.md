# iec62056-lora
- Periodically read current power, energy consumption and other OBIS-values from a electricity meter that support the IEC 62056-21 protocol. 
- Use LoRa/TTN to send the data to a (public or private) gateway 
- The gateway POST's the data to a Home-Assistant WebHook where the data is applied to several [template sensors](https://www.home-assistant.io/integrations/template/)


## State 
- Project is still work in progress (feel free to contribute)
- At the beginning I was experimenting arround with the Heltec lora esp32 microcontroller. That microcontroller is fully Arduino / platformio compatible and work fine as long as you don't wanna power it by a battery as it consumed a lot of energy even in deep-sleep. Thus I switched to the Heltec Cubecell AB02 series that is specialized for battery operated use-cases.
- I ran a long dozen of times up and down the staircase (third floor) down to the basement for this project - if you want to avoid that, free to buy me a coffee 

  [!["Buy Me A Coffee"](https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png)](https://www.buymeacoffee.com/pavax)


# My Hardware

## Micro Controllers
- [Heltec Cubecell AB02-A](https://heltec.org/project/htcc-ab02a)
- [Heltec Cubecell AB02 Dev Board](https://heltec.org/project/htcc-ab02/)
- [Heltec lora esp32 v2 Dev Board](https://heltec.org/project/wifi-lora-32/)

## Others
- [Optical reading head](https://www.ebay.de/itm/313460034498)
- [Lora antenna + 2m extension cord SMA-MALE](https://shopofthings.ch/shop/prototyping/netzwerk/868mhz-lorawan-lora-atenne-mit-3m-verlaengerungskabel-5dbi-sma-male-915mhz-gsm/)
- [PEX/IPX/U.FL to SMA-FEMALE](https://shopofthings.ch/shop/prototyping/kabel/antennenverlaengerung-ipex-ipx-u-fl-auf-sma-female-rg178-25cm/)


# Installation

## Heltec Cubecell AB02 (currently used by me)
* [Based on Ardunio IDE](https://www.arduino.cc/en/software)
* Follow the instruction provided here: https://github.com/HelTecAutomation/CubeCell-Arduino#installation-instructions
  * The Cubcell Ardunio version 1.3.0 has some issues that were fixed but not released as of today. If the version 1.4.0 is not released yet, you have to follow the instructions to install the development repository of the heltec cubecell boards in ardunio.
* Checkout the project ( directory `./heltec-cubecell `) and open the `.ino` file in the Ardunio IDE


### Wiring
Pinout Diagramms:
  * Board: [AB02-A](https://resource.heltec.cn/download/CubeCell/HTCC-AB02A/HTCC-AB02A_PinoutDiagram.pdf)
  * Board: [AB02](https://resource.heltec.cn/download/CubeCell/HTCC-AB02/HTCC-AB02_PinoutDiagram.pdf)
  
Connect as following:

 * Optical Reading Head: VCC -> VEXT 
 * Optical Reading Head: GND -> GND
 * Optical Reading Head: RX -> RX2 (GPIO 29)
 * Optical Reading Head: TX -> TX2 (GPIO 39)

### Configration
 - Rename credentials_example.h to credentials.h and provide your OTAA data
    * I generated random hex values for `devEui`, `appEu` and `appKey`
 - Set the correct boards
    * Tools -> Boards -> CubeCell (in sketchbook) -> HTCC - AB02A
 - Set the correct values in your ide
    * Tools -> LORAWAN_REGION  -> your-region (eg. EU_868)
    * Tools -> LORAWAN_CLASS -> A
    * Tools -> LORAWAN_DEVEUI -> CUSTOM
    * Tools -> LORAWAN_NETMODE -> OTAA
    * Tools -> AT_SUPPORT -> ON
 - Check/Adjust the `ADJUSTME` comments 

### Debugging / Hints
* use the serial monitor in ardunio and change the `DEFAULT_LOG_LEVEL` to debug
  * **Attention**: Make sure once everything works as intented to change it back to `Info` as too much logging has a negative impact on the power consumption (even if there is not serial monitor connected)
* The smart meter is not interacted with as long as the LoRaWAN has not been initalized / registred
  * uncomment the line `deviceState = DEVICE_STATE_SEND` in the wakeup procedure to direclty read the smart meter data when the button is pressed without checking/waiting for a successfull LoRaWAN registration
* Make sure you have a decent LoRaWAN connectivity where your smart-meter is located or nearby by using an extension cord/antenna. I played arround with the a simple example sketch from Heltec to find a good spot with a decent connectivy: 

    `Examples -> CubeCell -> LoRa -> LoRaWAN -> LoRaWAN`

    And then verfied in my Lora-Gateway if the messages were received and if so, what their RSSI/SNR data were.


## Heltec Wifi LoRA 32 V2 (deprecated)

* Based on Platformio
* Not sutable for my use-cses as it consumed to much power (even in deep-sleep) and thus couldn't get it to operate by battery

### Configration
- Rename credentials_example.h to credentials.h and provide your OTAA data
- Check/Adjust the `CHANGEME` comments


# Supported Smart Meters

Feel free to test yours and let me know if it worked.

- [Elster AS3000](https://wiki.volkszaehler.org/hardware/channels/meters/power/edl-ehz/elster_as3000)
  configs
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

## Acknowledgements 
- https://github.com/mwdmwd/iec62056-mqtt
- https://wiki.volkszaehler.org/hardware/channels/meters/power/edl-ehz/elster_as1440

# Home-Assitant Template Sensors

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

    - name: "Smart Meter Up-Counter"
      unique_id: smart_meter_upcounter
      icon: mdi:counter
      unit_of_measurement: "times"
      state: >-
        {% set payloadHex = trigger.json.data.payload_hex | default(none) %}
        {% set value = payloadHex[14:16] %}
        {% if value == '00' %}
          {{ none }}
        {% else %}
          {{ value | int(value,16) }}
        {% endif %}

```


