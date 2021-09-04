# About this project
- Periodicaly read power and energy consumption from a electricity meter that support the 62056-21 protocol. 
- Use LoRa/TTN to send the data to a (public or private) gateway 
- The gateway POST's the data to a Home-Assistant WebHook where the data is applied to several [template sensors](https://www.home-assistant.io/integrations/template/)


# My Hardware
- [Heltec Lora Esp32 Dev Board](https://www.bastelgarage.ch/heltec-automation/wifi-lora-32-v2-sx1276-868mhz-mit-oled)
- [IR Read and Write Head](https://www.ebay.de/itm/313460034498)
- [Elster AS3000](https://wiki.volkszaehler.org/hardware/channels/meters/power/edl-ehz/elster_as3000)


# Home-Assitant Template Sensors

```yaml
    sensor:
        - name: "Smart Meter Power"
        unique_id: smart_meter_power
        icon: mdi:flash-outline
        unit_of_measurement: W
        device_class: power
        state_class: measurement
        state: >-
            {% set payloadHex = trigger.json.data.payload_hex | default(none) %}
            {% if payloadHex == none or payloadHex == '00' %}
                {{ none }}
            {% else %}
            {% set power = payloadHex[0:4] %}
            {{ power | int(power,16) }}
            {% endif %}

        - name: "Smart Meter Kwh"
        unique_id: smart_meter_kwh
        icon: mdi:chart-histogram
        unit_of_measurement: "kWh"
        state_class: measurement
        device_class: energy
        state: >-
            {% set payloadHex = trigger.json.data.payload_hex | default(none) %}
            {% if payloadHex == none or payloadHex == '00' %}
                {{ none }}
            {% else %}
            {% set value = payloadHex[5:12] %}
            {{ ( value | int(value,16) /100 ) | float }}
            {% endif %}

        - name: "Smart Meter Battery"
        unique_id: smart_meter_battery
        icon: mdi:battery
        unit_of_measurement: "%"
        device_class: battery
        state: >-
            {% set payloadHex = trigger.json.data.payload_hex | default(none) %}
            {% if payloadHex == none or payloadHex == '00' %}
                {{ none }}
            {% else %}
            {% set value = payloadHex[12:14] %}
            {{ value | int(value,16) }}
            {% endif %}

        - name: "Smart Meter Up-Counter"
        unique_id: smart_meter_upcounter
        icon: mdi:counter
        unit_of_measurement: "times"
        state: >-
            {% set payloadHex = trigger.json.data.payload_hex | default(none) %}
            {% if payloadHex == none or payloadHex == '00' %}
                {{ none }}
            {% else %}
            {% set value = payloadHex[14:16] %}
            {{ value | int(value,16) }}
            {% endif %}

        - name: "Smart Meter RSSI"
        unique_id: smart_meter_rssi
        icon: mdi:wifi
        unit_of_measurement: "dBm"
        device_class: signal_strength
        state_class: measurement
        state: >-
            {{ trigger.json.data.LrrRSSI | round(0) }}

        - name: "Smart Meter  SNR"
        unique_id: smart_meter_snr
        icon: mdi:signal-distance-variant
        unit_of_measurement: "dB"
        device_class: signal_strength
        state_class: measurement
        state: >-
            {{ trigger.json.data.LrrSNR | round(0) }}
```
