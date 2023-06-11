#include "jhs_climate.h"

#include "esphome/core/log.h"
#include "esp32-hal-rmt.h"
#include "soc/rmt_struct.h"

#include "jhs_recv_task.h"
#include "esp32-hal.h"

#include <sstream>
#include <iomanip>

static const char *TAG = "JHSClimate";

namespace esphome
{
namespace JHSClimate
{
static std::string bytes_to_hex2(std::vector<uint8_t> bytes)
{
    std::stringstream ss;
    for (auto b : bytes)
    {
        ss << std::hex << std::setfill('0') << std::setw(2) << (int)b;
    }
    return ss.str();
}

void JHSClimate::setup()
{
    ESP_LOGI(TAG, "Setting up JHSClimate...");
    this->setup_rmt();
    jhs_recv_task_config recv_config = {
        .ac_rx_pin = this->ac_rx_pin_->get_pin(),
        .panel_rx_pin = this->panel_rx_pin_->get_pin()};
    start_jhs_climate_recv_task(recv_config);
    ESP_LOGI(TAG, "JHSClimate setup complete");

    // send hello packet to panel
    JHSAcPacket hello_packet;
    hello_packet.beep_amount = 3;
    hello_packet.beep_length = 1;
    hello_packet.set_display("dd");
    this->send_rmt_data(this->rmt_panel_tx, hello_packet.to_wire_format());
    // auto ota = esphome::App.get_component<ota::OTAComponent>("ota");
    // OTAComponent->add_on_state_callback([this](esphome::ota::OTAState state, float progress, uint8_t error) {
    //   if (state == esphome::ota::OTA_IN_PROGRESS) {
    //     ESP_LOGD(TAG, "OTA in progress %f", progress);
    //     JHSAcPacket ota_progress_packet;    
    //     // Display progress on panel
    //     ota_progress_packet.set_temp(int(progress));
    //     ota_progress_packet.wifi = 1;
    //     ota_progress_packet.unused_above_timer = 1;
    //     this->send_rmt_data(this->rmt_panel_tx, ota_progress_packet.to_wire_format());
    //   }
    // });

}

void JHSClimate::setup_rmt()
{

    this->rmt_panel_tx = rmtInit(this->panel_tx_pin_->get_pin(), true, RMT_MEM_192);
    this->rmt_panel_tx_tick = rmtSetTick(this->rmt_panel_tx, 2500); // papieska wartość
    ESP_LOGI(TAG, "RMT panel tx tick: %f", this->rmt_panel_tx_tick);

    this->rmt_ac_tx = rmtInit(this->ac_tx_pin_->get_pin(), true, RMT_MEM_192);
    this->rmt_ac_tx_tick = rmtSetTick(this->rmt_ac_tx, 2500); // papieska wartość
    ESP_LOGI(TAG, "RMT ac tx tick: %f", this->rmt_ac_tx_tick);

    // ugly hack to set all RMT channels to high on idle
    for (int i = 0; i < 8; i++)
    {
        RMT.conf_ch[i].conf1.idle_out_lv = 1;
    }

    ESP_LOGI(TAG, "RMT initialized");
}

void JHSClimate::control(const esphome::climate::ClimateCall &call)
{
    if (call.get_target_temperature().has_value())
    {
        this->target_temperature = call.get_target_temperature().value();
        this->steps_left_to_adjust_temp = 24;
    }
    if (call.get_mode().has_value())
    {
        this->mode = call.get_mode().value();
        this->steps_left_to_adjust_mode = 8;
    }
    if (call.get_fan_mode().has_value())
    {
        this->fan_mode = call.get_fan_mode().value();
        this->steps_left_to_adjust_fan = 1;
    }
    if (call.get_preset().has_value())
    {
        this->preset = call.get_preset().value();
        this->adjust_preset = true;
    }
    this->publish_state();
}

esphome::climate::ClimateTraits JHSClimate::traits()
{
    // The capabilities of the climate device
    auto traits = esphome::climate::ClimateTraits();
    traits.set_supports_current_temperature(true);
    traits.set_supported_modes({esphome::climate::CLIMATE_MODE_OFF,
                                esphome::climate::CLIMATE_MODE_DRY,
                                esphome::climate::CLIMATE_MODE_COOL,
                                esphome::climate::CLIMATE_MODE_FAN_ONLY});
    traits.set_supported_fan_modes({esphome::climate::CLIMATE_FAN_LOW,
                                    esphome::climate::CLIMATE_FAN_HIGH});
    traits.set_supported_presets({esphome::climate::CLIMATE_PRESET_NONE,
                                  esphome::climate::CLIMATE_PRESET_SLEEP});
    traits.set_visual_min_temperature(16);
    traits.set_visual_max_temperature(31);
    traits.set_visual_temperature_step(1);
    return traits;
}

void JHSClimate::dump_config()
{
    ESP_LOGCONFIG(TAG, "JHSClimate:");
    LOG_PIN("  AC TX Pin: ", this->ac_tx_pin_);
    LOG_PIN("  AC RX Pin: ", this->ac_rx_pin_);
    LOG_PIN("  Panel TX Pin: ", this->panel_tx_pin_);
    LOG_PIN("  Panel RX Pin: ", this->panel_rx_pin_);
    ESP_LOGCONFIG(TAG, "  RMT panel tx tick: %f", this->rmt_panel_tx_tick);
    ESP_LOGCONFIG(TAG, "  RMT ac tx tick: %f", this->rmt_ac_tx_tick);
}

void JHSClimate::loop()
{
    this->recv_from_ac();
    this->recv_from_panel();
}


void JHSClimate::recv_from_panel()
{
    uint8_t packet[JHS_PANEL_PACKET_SIZE];
    while (xQueueReceive(panel_rx_queue, &packet, 0))
    {
        std::vector<uint8_t> packet_vector(packet, packet + JHS_PANEL_PACKET_SIZE);

        if (memcmp(packet, &KEEPALIVE_PACKET, JHS_PANEL_PACKET_SIZE) == 0)
        {
            ESP_LOGVV(TAG, "Received keepalive packet from panel");
        }
        else if (memcmp(packet, &BUTTON_ON, JHS_PANEL_PACKET_SIZE) == 0)
        {
            ESP_LOGI(TAG, "Received BUTTON_ON from panel");
        }
        else if (memcmp(packet, &BUTTON_LOWER_TEMP, JHS_PANEL_PACKET_SIZE) == 0)
        {
            ESP_LOGI(TAG, "Received BUTTON_LOWER_TEMP from panel");
        }
        else if (memcmp(packet, &BUTTON_HIGHER_TEMP, JHS_PANEL_PACKET_SIZE) == 0)
        {
            ESP_LOGI(TAG, "Received BUTTON_HIGHER_TEMP from panel");
        }
        else if (memcmp(packet, &BUTTON_MODE, JHS_PANEL_PACKET_SIZE) == 0)
        {
            ESP_LOGI(TAG, "Received BUTTON_MODE from panel");
        }
        else if (memcmp(packet, &BUTTON_FAN, JHS_PANEL_PACKET_SIZE) == 0)
        {
            ESP_LOGI(TAG, "Received BUTTON_FAN from panel");
        }
        else if (memcmp(packet, &BUTTON_SLEEP, JHS_PANEL_PACKET_SIZE) == 0)
        {
            ESP_LOGI(TAG, "Received BUTTON_SLEEP from panel");
        }
        else if (memcmp(packet, &BUTTON_TIMER, JHS_PANEL_PACKET_SIZE) == 0)
        {
            ESP_LOGI(TAG, "Received BUTTON_TIMER from panel");
        }
        else if (memcmp(packet, &BUTTON_UNIT_CHANGE, JHS_PANEL_PACKET_SIZE) == 0)
        {
            ESP_LOGI(TAG, "Received BUTTON_UNIT_CHANGE from panel, ignoring");
            JHSAcPacket hello_packet;
            hello_packet.beep_amount = 3;
            hello_packet.beep_length = 2;
            hello_packet.power = 0;
            hello_packet.cool = 1;
            hello_packet.set_display("dd");
            this->send_rmt_data(this->rmt_panel_tx, hello_packet.to_wire_format());
            continue;
        }
        else
        {
            ESP_LOGI(TAG, "Received unknown packet from panel: %s", bytes_to_hex2(packet_vector).c_str());
        }
        this->send_rmt_data(this->rmt_ac_tx, packet_vector);
    }
}

bool JHSClimate::is_adjusting()
{
    return this->steps_left_to_adjust_fan > 0 || this->steps_left_to_adjust_temp > 0 || this->steps_left_to_adjust_mode > 0 || this->adjust_preset;
}

void JHSClimate::recv_from_ac()
{
    uint8_t packet[JHS_AC_PACKET_SIZE];

    while (xQueueReceive(ac_rx_queue, &packet, 0))
    {
        std::vector<uint8_t> packet_vector(packet, packet + JHS_AC_PACKET_SIZE);
        esphome::optional<JHSAcPacket> packet_optional = JHSAcPacket::parse(packet_vector);
        if (!packet_optional)
        {
            ESP_LOGV(TAG, "Received invalid packet from AC");
            continue;
        }
        JHSAcPacket packet = *packet_optional;
        ESP_LOGVV(TAG, "Received new packet from AC: %s", packet.to_string());


        // Modify the packet 
        packet.wifi = !wifi::global_wifi_component->is_connected();
        if (is_adjusting()){
            packet.beep_amount = 0;
            packet.beep_length = 0;
        }

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
        esphome::climate::ClimateFanMode fan_from_packet = esphome::climate::CLIMATE_FAN_LOW;
        if (packet.fan_high)
        {
            fan_from_packet = esphome::climate::CLIMATE_FAN_HIGH;
        }
        if (packet.fan_low)
        {
            fan_from_packet = esphome::climate::CLIMATE_FAN_LOW;
        }
        esphome::climate::ClimatePreset preset_from_packet = esphome::climate::CLIMATE_PRESET_NONE;
        if (packet.sleep) preset_from_packet = esphome::climate::CLIMATE_PRESET_SLEEP;

        if (!this->is_adjusting())
        {
            // if we are not adjusting anything we can copy the state from the packet to the climate

            bool did_change = false;
            if (packet.get_temp() > 0 && this->target_temperature != packet.get_temp() && packet.cool)
            {
                this->target_temperature = packet.get_temp();
                did_change = true;
            }
            if (this->current_temperature != packet.get_temp())
            {
                this->current_temperature = packet.get_temp(); // Fake the current temperature
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
            if (this->preset != preset_from_packet)
            {
                this->preset = preset_from_packet;
                did_change = true;
            }
            
            if (did_change)
            {
                this->publish_state();
            }
            if (this->water_full != packet.water_full)
            {
                if (packet.water_full){
                    last_water_full = esphome::millis();
                    this->water_full = true;
                }else{
                    if (esphome::millis() - last_water_full > WATER_FULL_INTERVAL){
                        this->water_full = false;
                    }else{
                        this->water_full = true;
                    }
                    last_water_full = esphome::millis();
                }
                this->water_full_sensor->publish_state(this->water_full);
            }
        }
        else
        {
            if (esphome::millis() - last_adjustment < ADJUSTMENT_INTERVAL)
            {
                continue;
            }
            last_adjustment = esphome::millis();
            // we are adjusting
            if (this->steps_left_to_adjust_mode > 0)
            {
                if (this->mode != mode_from_packet)
                {
                    auto packet_to_send = BUTTON_MODE;
                    if (this->mode == esphome::climate::ClimateMode::CLIMATE_MODE_OFF || mode_from_packet == esphome::climate::CLIMATE_MODE_OFF)
                    {
                        packet_to_send = BUTTON_ON;
                        ESP_LOGD(TAG, "Sending BUTTON_ON packet to AC");
                    }
                    else
                    {
                        ESP_LOGD(TAG, "Sending BUTTON_MODE packet to AC");
                    }
                    // create a vector from BUTTON_MODE, which is an std::array
                    std::vector<uint8_t> packet_vector(packet_to_send.begin(), packet_to_send.end());

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
                if (this->target_temperature != packet.get_temp())
                {
                    auto packet_to_send = BUTTON_LOWER_TEMP;
                    if (this->target_temperature > packet.get_temp())
                    {
                        packet_to_send = BUTTON_HIGHER_TEMP;
                        ESP_LOGD(TAG, "Sending BUTTON_HIGHER_TEMP packet to AC");
                    }
                    else
                    {
                        ESP_LOGD(TAG, "Sending BUTTON_LOWER_TEMP packet to AC");
                    }
                    // create a vector from BUTTON_UP, which is an std::array
                    std::vector<uint8_t> packet_vector(packet_to_send.begin(), packet_to_send.end());

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
            if (this->preset != preset_from_packet)
            {
                auto packet_to_send = BUTTON_SLEEP;
                // create a vector from BUTTON_SLEEP, which is an std::array
                std::vector<uint8_t> packet_vector(packet_to_send.begin(), packet_to_send.end());
                ESP_LOGD(TAG, "Sending BUTTON_SLEEP packet to AC");
                this->send_rmt_data(this->rmt_ac_tx, packet_vector);
                this->adjust_preset = false;
            }

        }
        this->send_rmt_data(this->rmt_panel_tx, packet.to_wire_format());
    }
}


void JHSClimate::send_rmt_data(rmt_obj_t *rmt, std::vector<uint8_t> data)
{
    ESP_LOGVV(TAG, "Sending RMT data: %s", bytes_to_hex2(data).c_str());

    std::vector<rmt_data_t> rmt_data_to_send = {};
    rmt_data_to_send.reserve((data.size() * 8) + 2); // 8 bits per byte + 2 bits for start/stop
    rmt_data_t leadin;
    leadin.level0 = 0;
    leadin.duration0 = 1800;
    leadin.level1 = 1;
    leadin.duration1 = 900;
    rmt_data_to_send.push_back(leadin);
    for (size_t i = 0; i < data.size() * 8; i++)
    {
        uint8_t bit = (data[i / 8] >> (7 - (i % 8))) & 1;

        if (bit)
        {
            rmt_data_t bit1;
            bit1.level0 = 0;
            bit1.duration0 = 100;
            bit1.level1 = 1;
            bit1.duration1 = 300;
            rmt_data_to_send.push_back(bit1);
        }
        else
        {
            rmt_data_t bit0;
            bit0.level0 = 0;
            bit0.duration0 = 100;
            bit0.level1 = 1;
            bit0.duration1 = 100;
            rmt_data_to_send.push_back(bit0);
        }
    }

    rmt_data_t leadout;
    leadout.level0 = 0;
    leadout.duration0 = 100;
    leadout.level1 = 1;
    leadout.duration1 = 100;
    rmt_data_to_send.push_back(leadout);
    rmt_data_t end;
    end.level0 = 0;
    end.duration0 = 200;
    end.level1 = 1;
    end.duration1 = 200;
    rmt_data_to_send.push_back(end);
    rmtWrite(rmt, rmt_data_to_send.data(), rmt_data_to_send.size());
}


} // namespace jhs
} // namespace esphome