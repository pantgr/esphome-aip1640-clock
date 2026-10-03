# ESPHome external component: AiP1640 LED driver (raw RAM writes; the clock face lives in a yaml lambda).
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_CLK_PIN, CONF_DIO_PIN, CONF_ID

CONF_BRIGHTNESS = "brightness"

aip1640_ns = cg.esphome_ns.namespace("aip1640")
AiP1640 = aip1640_ns.class_("AiP1640", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(AiP1640),
        cv.Required(CONF_CLK_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_DIO_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_BRIGHTNESS, default=0): cv.int_range(0, 7),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_clk_pin(await cg.gpio_pin_expression(config[CONF_CLK_PIN])))
    cg.add(var.set_dio_pin(await cg.gpio_pin_expression(config[CONF_DIO_PIN])))
    cg.add(var.set_brightness(config[CONF_BRIGHTNESS]))
