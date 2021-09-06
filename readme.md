# iec62056-lora
- Periodically read current power, energy consumption and other OBIS-values from a electricity meter that support the IEC 62056-21 protocol. 
- Use LoRa/TTN to send the data to a (public or private) gateway 
- The gateway POST's the data to a Home-Assistant WebHook where the data is applied to several [template sensors](https://www.home-assistant.io/integrations/template/)


## State 
- Project is still work in progress (feel free to contribute)
- My project is currently not power by battery (but should work). Unfortunatelly the Hardware consumes to much current. Might need to buy a more effiecient Lora Board
- I ran a long dozen of times up and down the staircase (third floor) down into the basement for this project - if you want to avoid that, free to buy me a coffee. 
  
  [!["Buy Me A Coffee"](https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png)](https://www.buymeacoffee.com/pavax)


# My Hardware
- [Heltec lora esp32 v2 dev board](https://www.bastelgarage.ch/heltec-automation/wifi-lora-32-v2-sx1276-868mhz-mit-oled)
- [Optical reading head](https://www.ebay.de/itm/313460034498)
- [Lora antenna + 2m extension cord SMA-MALE](https://shopofthings.ch/shop/prototyping/netzwerk/868mhz-lorawan-lora-atenne-mit-3m-verlaengerungskabel-5dbi-sma-male-915mhz-gsm/)
- [PEX/IPX/U.FL to SMA-FEMALE](https://shopofthings.ch/shop/prototyping/kabel/antennenverlaengerung-ipex-ipx-u-fl-auf-sma-female-rg178-25cm/)
- [Elster AS3000](https://wiki.volkszaehler.org/hardware/channels/meters/power/edl-ehz/elster_as3000)


# Configuration
- Rename credentials_example.h to credentials.h and provide your OTAA data
- Check/Adjust the `CHANGEME` comments


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


