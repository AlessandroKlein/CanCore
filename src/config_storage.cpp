#include "config_storage.h"

#include <string.h>

#include "can_protocol.h"

#if defined(__AVR__)
#include <EEPROM.h>
#endif

#if defined(ARDUINO_ARCH_ESP32)
#include <Preferences.h>
#endif

namespace pcd {

/* CRC-16/CCITT-FALSE: barato en AVR y suficiente para bloques de pocos cientos
 * de bytes. */
uint16_t crc16(const uint8_t *data, uint16_t length, uint16_t seed) {
    uint16_t crc = seed;
    for (uint16_t i = 0; i < length; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                                 : static_cast<uint16_t>(crc << 1);
        }
    }
    return crc;
}

void ConfigStore::serializeRule(const BindingRule &rule, uint8_t *out) {
    memset(out, 0, kBindingRuleBytes);
    writeUint16BE(&out[0], rule.source_node);
    out[2] = rule.source_resource;
    out[3] = rule.source_channel;
    out[4] = rule.trigger;
    out[5] = rule.event;
    writeFloatBE(&out[6], rule.threshold);
    out[10] = rule.target_resource;
    out[11] = rule.target_channel;
    out[12] = rule.action;
    writeUint32BE(&out[13], rule.param);
}

void ConfigStore::deserializeRule(const uint8_t *in, BindingRule &out) {
    out.source_node = readUint16BE(&in[0]);
    out.source_resource = in[2];
    out.source_channel = in[3];
    out.trigger = in[4];
    out.event = in[5];
    out.threshold = readFloatBE(&in[6]);
    out.target_resource = in[10];
    out.target_channel = in[11];
    out.action = in[12];
    out.param = readUint32BE(&in[13]);
}

ConfigStore::ConfigStore(StorageAdapter &storage)
    : storage_(storage), node_id_(0), node_id_mode_(NODE_ID_MANUAL), rule_count_(0) {}

bool ConfigStore::begin(uint16_t default_node_id) {
    if (!storage_.begin() || storage_.capacity() < kConfigTotalBytes) {
        reset(default_node_id);
        return false;
    }
    if (load()) {
        return true;
    }
    reset(default_node_id);
    return false;
}

void ConfigStore::reset(uint16_t node_id) {
    node_id_ = node_id;
    node_id_mode_ = NODE_ID_MANUAL;
    rule_count_ = 0;
}

bool ConfigStore::load() {
    uint8_t header[kConfigHeaderBytes];
    if (!storage_.read(0, header, 6)) {
        return false;
    }
    if (readUint16BE(&header[0]) != kConfigMagic ||
        (header[2] != 1 && header[2] != kConfigVersion)) {
        return false;
    }
    const uint16_t header_bytes = (header[2] == 1) ? 6 : kConfigHeaderBytes;
    if (header_bytes == kConfigHeaderBytes && !storage_.read(6, &header[6], 1)) {
        return false;
    }
    const uint8_t count = header[3];
    if (count > CAN_MAX_RULES) {
        return false;
    }

    const uint16_t payload_bytes =
        header_bytes + static_cast<uint16_t>(kBindingRuleBytes) * count;
    uint16_t crc = crc16(header, header_bytes);

    uint8_t raw[kBindingRuleBytes];
    BindingRule staged[CAN_MAX_RULES];
    for (uint8_t i = 0; i < count; ++i) {
        const uint16_t offset =
            kConfigHeaderBytes + static_cast<uint16_t>(kBindingRuleBytes) * i;
        if (!storage_.read(offset, raw, kBindingRuleBytes)) {
            return false;
        }
        crc = crc16(raw, kBindingRuleBytes, crc);
        deserializeRule(raw, staged[i]);
    }

    uint8_t stored_crc[2];
    if (!storage_.read(payload_bytes, stored_crc, 2)) {
        return false;
    }
    if (readUint16BE(stored_crc) != crc) {
        return false;
    }

    node_id_ = readUint16BE(&header[4]);
    node_id_mode_ = (header_bytes == kConfigHeaderBytes && header[6] == NODE_ID_AUTOMATIC)
                        ? NODE_ID_AUTOMATIC
                        : NODE_ID_MANUAL;
    rule_count_ = count;
    for (uint8_t i = 0; i < count; ++i) {
        rules_[i] = staged[i];
    }
    return true;
}

bool ConfigStore::save() {
    uint8_t header[kConfigHeaderBytes];
    writeUint16BE(&header[0], kConfigMagic);
    header[2] = kConfigVersion;
    header[3] = rule_count_;
    writeUint16BE(&header[4], node_id_);
    header[6] = node_id_mode_;

    if (!storage_.write(0, header, kConfigHeaderBytes)) {
        return false;
    }
    uint16_t crc = crc16(header, kConfigHeaderBytes);

    uint8_t raw[kBindingRuleBytes];
    for (uint8_t i = 0; i < rule_count_; ++i) {
        serializeRule(rules_[i], raw);
        const uint16_t offset =
            kConfigHeaderBytes + static_cast<uint16_t>(kBindingRuleBytes) * i;
        if (!storage_.write(offset, raw, kBindingRuleBytes)) {
            return false;
        }
        crc = crc16(raw, kBindingRuleBytes, crc);
    }

    uint8_t stored_crc[2];
    writeUint16BE(stored_crc, crc);
    const uint16_t payload_bytes =
        kConfigHeaderBytes + static_cast<uint16_t>(kBindingRuleBytes) * rule_count_;
    if (!storage_.write(payload_bytes, stored_crc, 2)) {
        return false;
    }
    return storage_.commit();
}

bool ConfigStore::addRule(const BindingRule &rule) {
    if (rule_count_ >= CAN_MAX_RULES) {
        return false;
    }
    rules_[rule_count_++] = rule;
    return true;
}

bool ConfigStore::replaceRule(uint8_t index, const BindingRule &rule) {
    if (index >= rule_count_) {
        return false;
    }
    rules_[index] = rule;
    return true;
}

bool ConfigStore::removeRule(uint8_t index) {
    if (index >= rule_count_) {
        return false;
    }
    for (uint8_t i = index; i + 1 < rule_count_; ++i) {
        rules_[i] = rules_[i + 1];
    }
    --rule_count_;
    return true;
}

/* ------------------------------------------------------------------ */

MemoryStorage::MemoryStorage() {
    memset(buffer_, 0xFF, sizeof(buffer_));
}

bool MemoryStorage::begin() {
    return true;
}

bool MemoryStorage::read(uint16_t offset, uint8_t *data, uint16_t length) {
    if (static_cast<uint32_t>(offset) + length > sizeof(buffer_)) {
        return false;
    }
    memcpy(data, &buffer_[offset], length);
    return true;
}

bool MemoryStorage::write(uint16_t offset, const uint8_t *data, uint16_t length) {
    if (static_cast<uint32_t>(offset) + length > sizeof(buffer_)) {
        return false;
    }
    memcpy(&buffer_[offset], data, length);
    return true;
}

bool MemoryStorage::commit() {
    return true;
}

void MemoryStorage::corrupt(uint16_t offset) {
    if (offset < sizeof(buffer_)) {
        buffer_[offset] = static_cast<uint8_t>(~buffer_[offset]);
    }
}

/* ------------------------------------------------------------------ */

#if defined(__AVR__)
EepromStorage::EepromStorage(uint16_t base_address) : base_address_(base_address) {}

bool EepromStorage::begin() {
    return capacity() >= kConfigTotalBytes;
}

bool EepromStorage::read(uint16_t offset, uint8_t *data, uint16_t length) {
    if (static_cast<uint32_t>(offset) + length > capacity()) {
        return false;
    }
    for (uint16_t i = 0; i < length; ++i) {
        data[i] = EEPROM.read(base_address_ + offset + i);
    }
    return true;
}

bool EepromStorage::write(uint16_t offset, const uint8_t *data, uint16_t length) {
    if (static_cast<uint32_t>(offset) + length > capacity()) {
        return false;
    }
    /* EEPROM.update evita ciclos de borrado innecesarios. */
    for (uint16_t i = 0; i < length; ++i) {
        EEPROM.update(base_address_ + offset + i, data[i]);
    }
    return true;
}

bool EepromStorage::commit() {
    return true;
}

uint16_t EepromStorage::capacity() const {
    const uint16_t total = static_cast<uint16_t>(E2END) + 1;
    return (base_address_ >= total) ? 0 : static_cast<uint16_t>(total - base_address_);
}
#endif

/* ------------------------------------------------------------------ */

#if defined(ARDUINO_ARCH_ESP32)
NvsStorage::NvsStorage(const char *nvs_namespace, const char *key)
    : namespace_(nvs_namespace), key_(key), dirty_(false) {
    memset(buffer_, 0xFF, sizeof(buffer_));
}

bool NvsStorage::begin() {
    Preferences prefs;
    if (!prefs.begin(namespace_, true)) {
        return true; /* namespace todavia inexistente: bloque vacio */
    }
    prefs.getBytes(key_, buffer_, sizeof(buffer_));
    prefs.end();
    return true;
}

bool NvsStorage::read(uint16_t offset, uint8_t *data, uint16_t length) {
    if (static_cast<uint32_t>(offset) + length > sizeof(buffer_)) {
        return false;
    }
    memcpy(data, &buffer_[offset], length);
    return true;
}

bool NvsStorage::write(uint16_t offset, const uint8_t *data, uint16_t length) {
    if (static_cast<uint32_t>(offset) + length > sizeof(buffer_)) {
        return false;
    }
    memcpy(&buffer_[offset], data, length);
    dirty_ = true;
    return true;
}

/* El bloque se escribe entero en una sola clave NVS: o se persiste completo o
 * queda el anterior, nunca una mezcla de ambos. */
bool NvsStorage::commit() {
    if (!dirty_) {
        return true;
    }
    Preferences prefs;
    if (!prefs.begin(namespace_, false)) {
        return false;
    }
    const size_t written = prefs.putBytes(key_, buffer_, sizeof(buffer_));
    prefs.end();
    dirty_ = (written != sizeof(buffer_));
    return !dirty_;
}
#endif

}  // namespace pcd
