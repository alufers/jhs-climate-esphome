#include "jhs_climate.h"

#include "esphome/core/log.h"
#include "esp32-hal-rmt.h"
#include "soc/rmt_struct.h"

#include "jhs_recv_task.h"
#include "esp32-hal.h"

#include <sstream>

static const char *TAG = "JHSClimate";

void JHSClimate::setup()
{
    ESP_LOGI(TAG, "Setting up JHSClimate...");
    this->setup_rmt();
    jhs_recv_task_config recv_config = {
        .ac_rx_pin = this->ac_rx_pin_->get_pin(),
        .panel_rx_pin = this->panel_rx_pin_->get_pin()};
    start_jhs_climate_recv_task(recv_config);
    ESP_LOGI(TAG, "JHSClimate setup complete");
}

void JHSClimate::setup_rmt()
{

    this->rmt_panel_tx = rmtInit(this->panel_tx_pin_->get_pin(), true, RMT_MEM_192);

    rmtSetTick(this->rmt_panel_tx, 250 * 1000); // 250 microseconds per tick

    this->rmt_ac_tx = rmtInit(this->ac_tx_pin_->get_pin(), true, RMT_MEM_192);
    rmtSetTick(this->rmt_ac_tx, 250 * 1000); // 250 microseconds per tick

    // ugly hack to set all RMT channels to high on idle
    for (int i = 0; i < 8; i++)
    {
        RMT.conf_ch[i].conf1.idle_out_lv = 1;
    }

    ESP_LOGI(TAG, "RMT initialized");
}

void JHSClimate::control(const esphome::climate::ClimateCall &call)
{
    std::stringstream log;
    if (call.get_target_temperature().has_value())
    {
        this->target_temperature = call.get_target_temperature().value();
        this->steps_left_to_adjust_temp = 24;
        log << "temp set to " << this->target_temperature << " degrees ";
    }
    if (call.get_mode().has_value())
    {
        this->mode = call.get_mode().value();
        this->steps_left_to_adjust_mode = 8;
        log << "mode set to " << this->mode << " ";
    }
    if (call.get_fan_mode().has_value())
    {
        this->fan_mode = call.get_fan_mode().value();
        this->steps_left_to_adjust_fan = 1;
        log << "fan mode set to " << this->fan_mode << " ";
    }
    ESP_LOGI(TAG, log.str().c_str());
    this->publish_state();
}

esphome::climate::ClimateTraits JHSClimate::traits()
{
    // The capabilities of the climate device
    auto traits = esphome::climate::ClimateTraits();
    // traits.set_supports_current_temperature(true);
    traits.set_supported_modes({esphome::climate::CLIMATE_MODE_OFF,
                                esphome::climate::CLIMATE_MODE_DRY,
                                esphome::climate::CLIMATE_MODE_COOL,
                                esphome::climate::CLIMATE_MODE_FAN_ONLY});
    traits.set_supported_fan_modes({esphome::climate::CLIMATE_FAN_LOW,
                                    esphome::climate::CLIMATE_FAN_HIGH});
    traits.set_visual_min_temperature(16);
    traits.set_visual_max_temperature(31);
    traits.set_visual_temperature_step(1);
    // traits.set_supports_target_temperature(true);
    return traits;
}

void JHSClimate::dump_config()
{
    ESP_LOGCONFIG(TAG, "JHSClimate:");
    LOG_PIN("  AC TX Pin: ", this->ac_tx_pin_);
    LOG_PIN("  AC RX Pin: ", this->ac_rx_pin_);
    LOG_PIN("  Panel TX Pin: ", this->panel_tx_pin_);
    LOG_PIN("  Panel RX Pin: ", this->panel_rx_pin_);
}

void JHSClimate::loop()
{
    this->recv_from_ac();
    this->recv_from_panel();
    this->update_screen_if_needed();
}

void JHSClimate::recv_from_panel()
{
    uint8_t packet[JHS_PANEL_PACKET_SIZE];
    while (xQueueReceive(panel_rx_queue, &packet, 0))
    {
        bool is_keepalive = false;
        for (int i = 0; i < JHS_PANEL_PACKET_SIZE; i++)
        {
            if (packet[i] == KEEPALIVE_PACKET[i])
            {
                is_keepalive = true;
            }
        }
        if (is_keepalive)
        {
            ESP_LOGI(TAG, "Received keepalive packet from panel");
            continue;
        }
        uint32_t now = esphome::millis();
        if (now - last_packet_from_panel < PANEL_MIN_PACKET_INTERVAL)
        {
            ESP_LOGD(TAG, "Received packet from panel too soon, ignoring");
            continue;
        }
        std::vector<uint8_t> packet_vector(packet, packet + JHS_PANEL_PACKET_SIZE);
        this->send_rmt_data(this->rmt_ac_tx, packet_vector);
    }
}

bool JHSClimate::is_adjusting()
{
    return this->steps_left_to_adjust_fan > 0 || this->steps_left_to_adjust_temp > 0 || this->steps_left_to_adjust_mode > 0;
}

void JHSClimate::recv_from_ac()
{
    uint8_t packet[JHS_AC_PACKET_SIZE];
    bool did_receive = false;

    while (xQueueReceive(ac_rx_queue, &packet, 0))
    {
        std::vector<uint8_t> packet_vector(packet, packet + JHS_AC_PACKET_SIZE);
        esphome::optional<JHSAcPacket> packet_optional = JHSAcPacket::parse(packet_vector);
        if (!packet_optional)
        {
            ESP_LOGW(TAG, "Received invalid packet from AC");
            continue;
        }
        JHSAcPacket packet = *packet_optional;
        this->last_packet_from_ac = esphome::millis();
        last_packet_from_ac_vector = packet_vector;
        did_receive = true;
        ESP_LOGD(TAG, "Received packet from AC: %s", packet.to_string().c_str());
        esphome::climate::ClimateMode mode_from_packet = esphome::climate::CLIMATE_MODE_OFF;
        if (packet.cool)
        {
            mode_from_packet = esphome::climate::CLIMATE_MODE_COOL;
        }
        else if (packet.heat)
        {
            mode_from_packet = esphome::climate::CLIMATE_MODE_HEAT;
        }
        else if (packet.fan)
        {
            mode_from_packet = esphome::climate::CLIMATE_MODE_FAN_ONLY;
        }
        else if (packet.dehum)
        {
            mode_from_packet = esphome::climate::CLIMATE_MODE_DRY;
        }
        float temp_from_packet = packet.get_temp();
        esphome::climate::ClimateFanMode fan_from_packet = esphome::climate::CLIMATE_FAN_LOW;
        if (packet.fan_high)
        {
            fan_from_packet = esphome::climate::CLIMATE_FAN_HIGH;
        }
        if (packet.fan_low)
        {
            fan_from_packet = esphome::climate::CLIMATE_FAN_LOW;
        }
        if (!this->is_adjusting())
        {
            // if we are not adjusting anything we can copy the state from the packet to the climate

            bool did_change = false;
            if (temp_from_packet > 0 && this->target_temperature != temp_from_packet && packet.cool)
            {
                this->target_temperature = temp_from_packet;
                did_change = true;
            }
            if (this->mode != mode_from_packet)
            {
                this->mode = mode_from_packet;
                did_change = true;
            }
            if (this->fan_mode != fan_from_packet)
            {
                this->fan_mode = fan_from_packet;
                did_change = true;
            }
            if (did_change)
            {
                this->publish_state();
            }
        }
        else
        {
            // we are adjusting
            if (this->steps_left_to_adjust_mode > 0)
            {
                if (this->mode != mode_from_packet)
                {
                    auto packet_to_send = BUTTON_MODE;
                    if (this->mode == esphome::climate::ClimateMode::CLIMATE_MODE_OFF)
                    {
                        packet_to_send = BUTTON_ON;
                    }
                    // create a vector from BUTTON_MODE, which is an std::array
                    std::vector<uint8_t> packet_vector(packet_to_send.begin(), packet_to_send.end());
                    ESP_LOGD(TAG, "Sending BUTTON_MODE packet to AC");
                    this->send_rmt_data(this->rmt_ac_tx, packet_vector);
                    this->steps_left_to_adjust_mode--;
                }
                else
                {
                    this->steps_left_to_adjust_mode = 0;
                }
            }
            if (this->steps_left_to_adjust_temp > 0)
            {
                if (this->target_temperature != temp_from_packet)
                {
                    auto packet_to_send = BUTTON_HIGHER_TEMP;
                    if (this->target_temperature > temp_from_packet)
                    {
                        packet_to_send = BUTTON_LOWER_TEMP;
                    }
                    // create a vector from BUTTON_UP, which is an std::array
                    std::vector<uint8_t> packet_vector(packet_to_send.begin(), packet_to_send.end());
                    ESP_LOGD(TAG, "Sending BUTTON_UP packet to AC");
                    this->send_rmt_data(this->rmt_ac_tx, packet_vector);
                    this->steps_left_to_adjust_temp--;
                }
                else
                {
                    this->steps_left_to_adjust_temp = 0;
                }
            }
            if (this->steps_left_to_adjust_fan > 0)
            {
                if (this->fan_mode != fan_from_packet)
                {
                    auto packet_to_send = BUTTON_FAN;

                    // create a vector from BUTTON_FAN, which is an std::array
                    std::vector<uint8_t> packet_vector(packet_to_send.begin(), packet_to_send.end());
                    ESP_LOGD(TAG, "Sending BUTTON_FAN packet to AC");
                    this->send_rmt_data(this->rmt_ac_tx, packet_vector);
                    this->steps_left_to_adjust_fan--;
                }
                else
                {
                    this->steps_left_to_adjust_fan = 0;
                }
            }
        }
    }
    if (esphome::millis() - last_packet_from_ac > 700 && !did_receive && is_adjusting() && this->mode != esphome::climate::CLIMATE_MODE_OFF)
    {
        last_packet_from_ac = esphome::millis();
        // turn on the ac
        auto packet_to_send = BUTTON_ON;
        // create a vector from BUTTON_MODE, which is an std::array
        std::vector<uint8_t> packet_vector(packet_to_send.begin(), packet_to_send.end());
        ESP_LOGD(TAG, "Sending BUTTON_ON packet to AC");
        this->send_rmt_data(this->rmt_ac_tx, packet_vector);
    }
    if (did_receive)
    {
        this->update_screen_if_needed();
    }
}

void JHSClimate::update_screen_if_needed()
{
    if(this->is_adjusting() || this->last_packet_from_ac_vector.size() == 0) return;
    if (esphome::millis() - this->last_screen_update < SCREEN_UPDATE_INTERVAL)
    {
        return;
    }
    this->last_screen_update = esphome::millis();

    this->send_rmt_data(this->rmt_panel_tx, this->last_packet_from_ac_vector);
}
void JHSClimate::send_rmt_data(rmt_obj_t *rmt, std::vector<uint8_t> data)
{
    std::vector<rmt_data_t> rmt_data_to_send = {};
    rmt_data_to_send.reserve((data.size() * 8) + 2); // 8 bits per byte + 2 bits for start/stop
    rmt_data_t leadin;
    leadin.level0 = 0;
    leadin.duration0 = 18;
    leadin.level1 = 1;
    leadin.duration1 = 9;
    rmt_data_to_send.push_back(leadin);
    for (size_t i = 0; i < data.size() * 8; i++)
    {
        uint8_t bit = (data[i / 8] >> (7 - (i % 8))) & 1;

        if (bit)
        {
            rmt_data_t bit1;
            bit1.level0 = 0;
            bit1.duration0 = 1;
            bit1.level1 = 1;
            bit1.duration1 = 3;
            rmt_data_to_send.push_back(bit1);
        }
        else
        {
            rmt_data_t bit0;
            bit0.level0 = 0;
            bit0.duration0 = 1;
            bit0.level1 = 1;
            bit0.duration1 = 1;
            rmt_data_to_send.push_back(bit0);
        }
    }

    rmt_data_t leadout;
    leadout.level0 = 0;
    leadout.duration0 = 1; // zero at end
    leadout.level1 = 1;
    leadout.duration1 = 3;
    rmt_data_to_send.push_back(leadout);
    rmtWrite(rmt, rmt_data_to_send.data(), rmt_data_to_send.size());
}
