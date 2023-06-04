#pragma once
#include <Arduino.h>

class Channel {
    uint8_t _sps = 0;
    uint8_t _pga = 0;
    uint8_t _mul = 1;
    static Channel _channels[4];

    Channel() {}
    Channel(const Channel&) = delete;
    Channel(Channel&&) = delete;
    Channel& operator=(const Channel&) = delete;
    Channel& operator=(Channel&&) = delete;

public:
    static Channel& getChannel(uint8_t index);
    static bool begin();
    void nextSampleRate();
    void nextGainValue();
    void nextMultiplier();

    uint16_t readValue() const;
    float computeVoltage(uint16_t adc) const;
    uint8_t getIndex() const;
    uint16_t getSampleRate() const;
    uint8_t getMultiplier() const;
    float getGain() const;
    float getRange() const;
    float getMvPerBit() const;
    String getGainString() const;
    String getInfoString() const;
};