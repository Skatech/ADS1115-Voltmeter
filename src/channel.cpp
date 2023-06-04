#include <SPI.h>
#include <Adafruit_ADS1X15.h>
#include "channel.h"
#include "config.h"

#ifdef USE_ADS1015
#define ADC_RESOLUTION (1UL << 12)
const uint16_t __ads_spsvals[] = { 128, 250, 490, 920, 1600, 2400, 3300 };
Adafruit_ADS1015 __ads_device;
#endif

#ifdef USE_ADS1115
#define ADC_RESOLUTION (1UL << 16)
const uint16_t __ads_spsvals[] = { 8, 16, 32, 64, 128, 250, 475, 860 };
Adafruit_ADS1115 __ads_device;
#endif

const uint8_t __ads_pgavals[] = { 24, 16, 8, 4, 2, 1 };

Channel Channel::_channels[4];  // static

bool Channel::begin() {     // static
    return __ads_device.begin();
}

Channel& Channel::getChannel(uint8_t index) {   // static
    return _channels[index % 4];
}

uint8_t Channel::getIndex() const {
    return this - _channels;
}

uint16_t Channel::readValue() const {
    __ads_device.setDataRate(_sps << 5);
    __ads_device.setGain((adsGain_t)(_pga << 9));
    return __ads_device.readADC_SingleEnded(getIndex());
}

float Channel::computeVoltage(uint16_t adc) const {
    __ads_device.setGain((adsGain_t)(_pga << 9));
    return _mul * __ads_device.computeVolts(adc);
    // const float bitmv = 4.096f * _mul / getGain() * 2 / ADC_RESOLUTION;
    // return adc * bitmv;
}

void Channel::nextSampleRate() {
    const uint8_t count = sizeof(__ads_spsvals) / sizeof(uint16_t);
    _sps = (_sps + 1) % count;
}

void Channel::nextGainValue() {
    const uint8_t count = sizeof(__ads_pgavals) / sizeof(uint8_t);
    _pga = (_pga + 1) % count;
}

void Channel::nextMultiplier() {
    _mul = _mul % 10 + 1;
}

uint16_t Channel::getSampleRate() const {
    return __ads_spsvals[_sps];
}

uint8_t Channel::getMultiplier() const {
    return _mul;
}

float Channel::getGain() const {
    return 16.0f / __ads_pgavals[_pga];
}

float Channel::getRange() const {
    return 4.096f  * _mul / getGain();
}

float Channel::getMvPerBit() const {
    return getRange() * 2 * 1000 / ADC_RESOLUTION;
}

String Channel::getGainString() const {
    const uint8_t pga = __ads_pgavals[_pga];
    return pga > 16 ? "3/2x" : String("1/") + 16 / pga + 'x';
}

String Channel::getInfoString() const {
    String builder = "+/-%rV  %bmV/bit";
    // builder.replace("%d", getGainString());
    builder.replace("%r", String(getRange(), 3));
    builder.replace("%b", String(getMvPerBit(), 7));
    return builder;
}