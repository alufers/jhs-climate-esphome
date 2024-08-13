# JHS Climate for esphome

Integration with [ESPHhome](https://esphome.io) for portable air conditioners made by [JHS (Dongguan Jinhongsheng Electric Co., Ltd.)](https://www.jhs8.com/). They are sold by the Action chain in Poland under the "Lifetime Air" brand. This integration allows you to control the Air Conditioner from Home Assistant.

This component works only on the dual core ESP32 (it does not work on the ESP8266), although it could be adapted for it (PR's welcome).

The ESP32 has to be connected between the motherboard and the control panel of the portable AC. It does not need any extra power, since the power supply is provided by the AC. The integration works by intercepting the communication between the control panel and the AC. You can do that easily by adding some jumper to the connectors joining the main board and the control panel.
You also need too pull down the `panel_rx_pin' to ground, using a 2K resistor (or similar), otherwise the panel will repeat the data multiple times, making the device unusable.

![An image of the AC control panel](./docs/control_panel.jpg)

## Example configuration

```yaml
external_components:
  - source: github://alufers/jhs-climate-esphome
climate:
binary_sensor:
jhs_climate:
    id: jhsclimate
    name: JHS Climate
    ac_tx_pin: 26 # data going from the ESP to the AC
    ac_rx_pin: 25 # data coming from the AC to the ESP
    panel_rx_pin: 33 # data coming from the control panel to the ESP (needs to be pulled down to ground with a 2K resistor)
    panel_tx_pin: 32 # data going from the ESP to the control panel
    water_full_sensor:
      name: "Water full"
```

