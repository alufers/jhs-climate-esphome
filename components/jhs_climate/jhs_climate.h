#pragma once
#include <esp32-hal-log.h>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/gpio.h"
#include "esphome/components/climate/climate.h"
#include "esphome.h"

#include "esp32-hal-rmt.h"
#include "soc/rmt_struct.h"
#include "jhs_packets.h"
#include <vector>

// logging macros won't work if we don't do this
using namespace esphome;

class JHSClimate : public esphome::Component, public esphome::climate::Climate
{
public:
    // pin setters
    void set_ac_tx_pin(esphome::InternalGPIOPin *ac_tx_pin) { ac_tx_pin_ = ac_tx_pin; }
    void set_ac_rx_pin(esphome::InternalGPIOPin *ac_rx_pin) { ac_rx_pin_ = ac_rx_pin; }
    void set_panel_tx_pin(esphome::InternalGPIOPin *panel_tx_pin) { panel_tx_pin_ = panel_tx_pin; }
    void set_panel_rx_pin(esphome::InternalGPIOPin *panel_rx_pin) { panel_rx_pin_ = panel_rx_pin; }

    // esphome handlers
    void setup() override;
    void dump_config() override;
    esphome::climate::ClimateTraits traits() override;
    void control(const esphome::climate::ClimateCall &call) override;

    void loop() override;

protected:
    esphome::InternalGPIOPin *ac_tx_pin_;
    esphome::InternalGPIOPin *ac_rx_pin_;
    esphome::InternalGPIOPin *panel_tx_pin_;
    esphome::InternalGPIOPin *panel_rx_pin_;

    rmt_obj_t *rmt_ac_tx;
    rmt_obj_t *rmt_panel_tx;

    // timings
    uint32_t last_packet_from_panel = 0;
    const uint32_t PANEL_MIN_PACKET_INTERVAL = 900;
    uint32_t last_packet_from_ac = 0;
    bool did_try_turn_on = false;
    std::string last_ac_packet_string;
    uint32_t same_ac_packets_count = 0;

    uint32_t last_screen_update = 0;
    const uint32_t SCREEN_UPDATE_INTERVAL = 600;

    uint32_t last_adjustment = 0;
    const int ADJUSTMENT_INTERVAL = 350;
    int steps_left_to_adjust_mode = 0;
    int steps_left_to_adjust_temp = 0;
    int steps_left_to_adjust_fan = 0;

    JHSAcPacket last_ac_packet;

    // is_adjusting is set to true when a change was made externally (e.g. homeassistant) and we are in the process of pressing button
    bool is_adjusting();

    std::vector<uint8_t> last_packet_from_ac_vector;

    uint32_t loop_idx = 0;

private:
    // setup helpers
    void setup_rmt();

    void send_rmt_data(rmt_obj_t *rmt, std::vector<uint8_t> data);

    void recv_from_panel();

    void recv_from_ac();

    void update_screen_if_needed();
};
