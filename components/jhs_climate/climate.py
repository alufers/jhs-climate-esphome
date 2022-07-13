import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate
from esphome.const import CONF_ID
from esphome import pins

print("ASSSS", climate)


DEPENDENCIES = []


JHSClimateComponent = cg.global_ns.class_(
    "JHSClimate", cg.Component, climate.Climate)

CONF_AC_TX_PIN = 'ac_tx_pin'
CONF_AC_RX_PIN = 'ac_rx_pin'
CONF_PANEL_TX_PIN = 'panel_tx_pin'
CONF_PANEL_RX_PIN = 'panel_rx_pin'

CONFIG_SCHEMA = climate.CLIMATE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(JHSClimateComponent),
        cv.Required(CONF_AC_TX_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_AC_RX_PIN): pins.gpio_input_pin_schema,
        cv.Required(CONF_PANEL_TX_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_PANEL_RX_PIN): pins.gpio_input_pin_schema,
    }
)


async def to_code(config):
    print("TO CODE")
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)
    ac_tx_pin = await cg.gpio_pin_expression(config[CONF_AC_TX_PIN])
    ac_rx_pin = await cg.gpio_pin_expression(config[CONF_AC_RX_PIN])
    panel_tx_pin = await cg.gpio_pin_expression(config[CONF_PANEL_TX_PIN])
    panel_rx_pin = await cg.gpio_pin_expression(config[CONF_PANEL_RX_PIN])
    cg.add(var.set_ac_tx_pin(ac_tx_pin))
    cg.add(var.set_ac_rx_pin(ac_rx_pin))
    cg.add(var.set_panel_tx_pin(panel_tx_pin))
    cg.add(var.set_panel_rx_pin(panel_rx_pin))
