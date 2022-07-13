#pragma once

#include "esphome/core/log.h"
#include "jhs_packets.h"
#include <esp32-hal.h>
extern "C" {
#include "freertos/FreeRTOS.h"
#include <freertos/task.h>
#include <freertos/queue.h>
}

using namespace esphome;

const size_t JHS_AC_PACKET_SIZE = (sizeof(JHSAcPacket) + 1);
const size_t JHS_PANEL_PACKET_SIZE = 3;

extern volatile QueueHandle_t ac_rx_queue;
extern volatile QueueHandle_t panel_rx_queue;

struct jhs_recv_task_config
{
    int ac_rx_pin;
    int panel_rx_pin;
};

void start_jhs_climate_recv_task(jhs_recv_task_config config);
