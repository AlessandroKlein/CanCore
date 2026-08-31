#if defined(ARDUINO_ARCH_ESP32)

#include "hal/hal_can_esp32.h"

#include <driver/twai.h>

#include <string.h>

namespace pcd {

namespace {

twai_timing_config_t timingFor(uint32_t bitrate) {
    switch (bitrate) {
        case 1000000UL: {
            twai_timing_config_t t = TWAI_TIMING_CONFIG_1MBITS();
            return t;
        }
        case 800000UL: {
            twai_timing_config_t t = TWAI_TIMING_CONFIG_800KBITS();
            return t;
        }
        case 250000UL: {
            twai_timing_config_t t = TWAI_TIMING_CONFIG_250KBITS();
            return t;
        }
        case 125000UL: {
            twai_timing_config_t t = TWAI_TIMING_CONFIG_125KBITS();
            return t;
        }
        case 100000UL: {
            twai_timing_config_t t = TWAI_TIMING_CONFIG_100KBITS();
            return t;
        }
        case 500000UL:
        default: {
            twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
            return t;
        }
    }
}

}  // namespace

Esp32TwaiBus::Esp32TwaiBus(int tx_pin, int rx_pin)
    : tx_pin_(tx_pin), rx_pin_(rx_pin), started_(false), filter_(CanFilter::acceptAll()) {}

Esp32TwaiBus::~Esp32TwaiBus() {
    end();
}

CanStatus Esp32TwaiBus::begin(uint32_t bitrate) {
    twai_general_config_t general = TWAI_GENERAL_CONFIG_DEFAULT(
        static_cast<gpio_num_t>(tx_pin_), static_cast<gpio_num_t>(rx_pin_), TWAI_MODE_NORMAL);
    general.rx_queue_len = 32;
    general.tx_queue_len = 16;

    twai_timing_config_t timing = timingFor(bitrate);

    /*
     * El filtrado fino se aplica por software en receive(): el filtro de
     * aceptacion del TWAI solo admite una mascara y aqui se necesita aceptar
     * tanto el destino propio como el broadcast.
     */
    twai_filter_config_t accept = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&general, &timing, &accept) != ESP_OK) {
        LOG_ERROR("TWAI install failed");
        return CAN_ERR_INIT;
    }
    if (twai_start() != ESP_OK) {
        twai_driver_uninstall();
        LOG_ERROR("TWAI start failed");
        return CAN_ERR_INIT;
    }
    started_ = true;
    LOG_INFO("TWAI iniciado a %lu bps", static_cast<unsigned long>(bitrate));
    return CAN_OK;
}

void Esp32TwaiBus::end() {
    if (!started_) {
        return;
    }
    twai_stop();
    twai_driver_uninstall();
    started_ = false;
}

CanStatus Esp32TwaiBus::send(const CanFrame &frame, uint32_t timeout_ms) {
    if (!started_) {
        return CAN_ERR_NOT_STARTED;
    }
    twai_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.identifier = frame.id & kExtendedIdMask;
    msg.extd = 1;
    msg.data_length_code = frame.dlc;
    memcpy(msg.data, frame.data, frame.dlc);

    const esp_err_t err = twai_transmit(&msg, pdMS_TO_TICKS(timeout_ms));
    if (err == ESP_OK) {
        return CAN_OK;
    }
    return (err == ESP_ERR_TIMEOUT) ? CAN_ERR_TX_BUSY : CAN_ERR_TX_FAIL;
}

CanStatus Esp32TwaiBus::receive(CanFrame &frame, uint32_t timeout_ms) {
    if (!started_) {
        return CAN_ERR_NOT_STARTED;
    }
    twai_message_t msg;
    if (twai_receive(&msg, pdMS_TO_TICKS(timeout_ms)) != ESP_OK) {
        return CAN_ERR_NO_DATA;
    }
    if (!msg.extd || !filter_.accepts(msg.identifier)) {
        return CAN_ERR_NO_DATA;
    }
    frame.id = msg.identifier;
    frame.dlc = msg.data_length_code;
    memcpy(frame.data, msg.data, sizeof(frame.data));
    return CAN_OK;
}

void Esp32TwaiBus::setFilter(const CanFilter &filter) {
    filter_ = filter;
}

bool Esp32TwaiBus::isBusOff() const {
    twai_status_info_t status;
    if (twai_get_status_info(&status) != ESP_OK) {
        return false;
    }
    return status.state == TWAI_STATE_BUS_OFF;
}

CanStatus Esp32TwaiBus::recover() {
    if (twai_initiate_recovery() != ESP_OK) {
        return CAN_ERR_BUS_OFF;
    }
    return (twai_start() == ESP_OK) ? CAN_OK : CAN_ERR_BUS_OFF;
}

}  // namespace pcd

#endif  /* ARDUINO_ARCH_ESP32 */
