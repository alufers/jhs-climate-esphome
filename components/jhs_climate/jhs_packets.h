#pragma once

#include <cstdint>
#include <string>

#include <map>
#include <vector>
#include <array>
#include <map>
#include "esphome/core/optional.h"

using namespace esphome;

const std::array<uint8_t, 3> BUTTON_ON{0x30, 0x03, 0x8d};
const std::array<uint8_t, 3> BUTTON_LOWER_TEMP{0x30, 0x02, 0x8c};
const std::array<uint8_t, 3> BUTTON_HIGHER_TEMP{0x30, 0x08, 0x92};
const std::array<uint8_t, 3> BUTTON_MODE{0x30, 0x01, 0x8b};
const std::array<uint8_t, 3> BUTTON_FAN{0x30, 0x05, 0x8f};
const std::array<uint8_t, 3> KEEPALIVE_PACKET{0x30, 0x00, 0x8a};

///@brief Packet sent from the AC to the panel.
struct JHSAcPacket
{
    uint8_t addr = 0x90; // always 0x90

    uint8_t first_digit; // bits 0-7

    uint8_t second_digit; // bits 8-15

    uint8_t zero0; // bits 16-23

    uint8_t zero1; // bits 24-31

    uint8_t cool : 1;       // bit 32
    uint8_t dehum : 1;      // bit 33
    uint8_t fan : 1;        // bit 34
    uint8_t heat : 1;       // bit 35
    uint8_t sleep : 1;      // bit 36
    uint8_t water_full : 1; // bit 37
    uint8_t swing : 1;      // bit 38
    uint8_t timer : 1;      // bit 39

    uint8_t fan_low : 1;    // bit 40
    uint8_t fan_unused : 1; // bit 41
    uint8_t fan_high : 1;   // bit 42
    uint8_t unused2 : 1;    // bit 43
    uint8_t unused3 : 1;    // bit 44
    uint8_t power : 1;      // bit 45
    uint8_t unused4 : 1;    // bit 46
    uint8_t unused5 : 1;    // bit 47

    uint8_t beep_amount : 4;
    uint8_t beep_length : 4;

    std::string to_string();

    void set_temp(int);
    int get_temp();

    ///@brief Parses the packet and checks the checksum.
    static esphome::optional<JHSAcPacket> parse(const std::vector<uint8_t> &data);

    std::vector<uint8_t> to_wire_format();
} __attribute__((packed));
