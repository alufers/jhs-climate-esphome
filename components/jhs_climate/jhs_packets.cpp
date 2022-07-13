#include "jhs_packets.h"
#include <sstream>
#include <cstring>
#include <iomanip>

#include "esphome/core/log.h"

static const char *TAG = "JHSClimate";

static char seven_segment_to_char(uint8_t s7)
{
    switch (s7)
    {
    case 0x3F:
        return '0';
    case 0x06:
        return '1';
    case 0x5B:
        return '2';
    case 0x4F:
        return '3';
    case 0x66:
        return '4';
    case 0x6D:
        return '5';
    case 0x7D:
        return '6';
    case 0x07:
        return '7';
    case 0x7F:
        return '8';
    case 0x6F:
        return '9';
    case 0x77:
        return 'A';
    case 0x7C:
        return 'B';
    case 0x39:
        return 'C';
    case 0x5E:
        return 'D';
    case 0x79:
        return 'E';
    case 0x71:
        return 'F';
    case 0x76:
        return 'D';
    case 0x38:
        return 'H';
    case 0x3A:
        return 'H';
    default:
        return '?';
    }
}
static uint8_t char_to_seven_segment(char c)
{
    switch (c)
    {
    case '0':
        return 0x3F;
    case '1':
        return 0x06;
    case '2':
        return 0x5B;
    case '3':
        return 0x4F;
    case '4':
        return 0x66;
    case '5':
        return 0x6D;
    case '6':
        return 0x7D;
    case '7':
        return 0x07;
    case '8':
        return 0x7F;
    case '9':
        return 0x6F;
    case 'A':
        return 0x77;
    case 'B':
        return 0x7C;
    case 'C':
        return 0x39;
    case 'D':
        return 0x5E;
    case 'E':
        return 0x79;
    case 'F':
        return 0x71;
    case 'd':
        return 0x76;
    case 'h':
        return 0x38;
    case 'H':
        return 0x3A;
    default:
        return 0x00;
    }
}

std::string bytes_to_hex(std::vector<uint8_t> bytes)
{
    std::stringstream ss;
    for (auto b : bytes)
    {
        ss << std::hex << std::setfill('0') << std::setw(2) << (int)b;
    }
    return ss.str();
}

std::string JHSAcPacket::to_string()
{
    std::stringstream ss;
    ss << "DISP:";
    ss << seven_segment_to_char(first_digit);
    ss << seven_segment_to_char(second_digit);
    if (this->power)
        ss << " POWER";
    if (this->cool)
        ss << " COOL";
    if (this->dehum)
        ss << " DEHUM";
    if (this->heat)
        ss << " HEAT";
    if (this->fan)
        ss << " FANONLY";
    if (this->fan_low)
        ss << " FANLOW";
    if (this->fan_high)
        ss << " FANHIGH";
    if (this->water_full)
        ss << " WATERFULL";
    if (this->sleep)
        ss << " SLEEP";
    if (this->timer)
        ss << " TIMER";
    if (this->beep_amount > 0 && this->beep_length > 0)
    {
        ss << " BEEP " << (int)this->beep_amount << " times, " << (int)this->beep_length;
    }
    return ss.str();
}

int JHSAcPacket::get_temp()
{
    char d1 = seven_segment_to_char(first_digit);
    char d2 = seven_segment_to_char(second_digit);
    if (d1 > '9' || d2 > '9' || d1 < '0' || d2 < '0')
        return -1;
    return (d1 - '0') * 10 + (d2 - '0');
}

void JHSAcPacket::set_temp(int temp)
{
    if (temp < 0 || temp > 99)
        return;
    first_digit = char_to_seven_segment(temp / 10 + '0');
    second_digit = char_to_seven_segment(temp % 10 + '0');
}

esphome::optional<JHSAcPacket> JHSAcPacket::parse(const std::vector<uint8_t> &data)
{

    if (data.size() != sizeof(JHSAcPacket) + 1)
    {
        ESP_LOGE(TAG, "Invalid packet size: %d, should be %d", data.size(), sizeof(JHSAcPacket) + 1);
        return esphome::optional<JHSAcPacket>();
    }
    JHSAcPacket packet;
    std::memcpy(&packet, data.data(), sizeof(JHSAcPacket));
    uint8_t checksum = data.back();
    uint8_t checksum_calculated = 90;
    for (size_t i = 0; i < data.size() - 1; i++)
    {
        checksum_calculated += data[i];
    }
    if (checksum != checksum_calculated)
    {
        ESP_LOGE(TAG, "JHS AC packet checksum mismatch: %d != %d in packet: %s", checksum, checksum_calculated, bytes_to_hex(data).c_str());
        return esphome::optional<JHSAcPacket>();
    }
    return esphome::make_optional(packet);
}

std::vector<uint8_t> JHSAcPacket::to_wire_format()
{
    std::vector<uint8_t> data;
    data.resize(sizeof(JHSAcPacket) + 1);
    std::memcpy(data.data(), &this->first_digit, sizeof(JHSAcPacket));
    uint8_t checksum = 90;
    for (size_t i = 0; i < data.size() - 1; i++)
    {
        checksum += data[i];
    }
    data.back() = checksum;
    return data;
}
