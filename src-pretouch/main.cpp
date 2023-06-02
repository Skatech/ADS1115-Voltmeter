#include <Arduino.h>
// DIYMORE ILI9341 TFT
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
// ADS1X15 ADC
#include <SPI.h>
#include <Adafruit_ADS1X15.h>

//#define SIMULATION // using ADS1015 instead of ADS1115

#ifdef SIMULATION
#define USE_ADS1015
#define ADC_DATA_RATE RATE_ADS1015_128SPS
Adafruit_ADS1015 ads;
#else
#define USE_ADS1115
#define ADC_DATA_RATE RATE_ADS1115_8SPS
Adafruit_ADS1115 ads;
#endif

#define	BLACK   0x0000
#define	BLUE    0x001F
#define	RED     0xF800
#define	GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
#define GRAY    0x7BEF
#define DARK_YELLOW 0x7BE0 // R5(F8)  G6(07e0) B5(001f)

MCUFRIEND_kbv tft;

struct ADS1x15_gain_info {
    float divider, range, bitmv;

    ADS1x15_gain_info(adsGain_t gain) {
        switch (gain) {
            case GAIN_TWOTHIRDS: divider = 2.f / 3.f; break;
            case GAIN_ONE: divider = 1.f; break;
            case GAIN_TWO: divider = 2.f; break;
            case GAIN_FOUR: divider = 4.f; break;
            case GAIN_EIGHT: divider = 8.f; break;
            case GAIN_SIXTEEN: divider = 16.f; break;
        }

        range = 4.096f / divider;
#ifdef USE_ADS1015
        bitmv = 4.096f * 2 * 1000 / 4096 / divider;
#endif
#ifdef USE_ADS1115
        bitmv = 4.096f * 2 * 1000 / 65536 / divider;
#endif
    }

    String toString(const char* format = "%dx  +/-%rV  %bmV/bit") const {
        String builder(format);
        builder.replace("%d", String(divider, 2).substring(0, 4));
        builder.replace("%r", String(range, 3));
        builder.replace("%b", String(bitmv, 7));
        return builder;
    }
};

uint16_t SPS_from_dataRate(uint16_t rate) {
#ifdef USE_ADS1015
    switch(rate) {
        case 0x0000: return 128;
        case 0x0020: return 250;
        case 0x0040: return 490;
        case 0x0060: return 920;
        case 0x0080: return 1600;
        case 0x00A0: return 2400;
        case 0x00C0: return 3300;
    }
#endif

#ifdef USE_ADS1115
    switch(rate) {
        case 0x0000: return 8;
        case 0x0020: return 16;
        case 0x0040: return 32;
        case 0x0060: return 64;
        case 0x0080: return 128;
        case 0x00A0: return 250;
        case 0x00C0: return 475;
        case 0x00E0: return 860;
    }
#endif
    return 0;
}

class Channel {
    uint8_t _index;
    bool _active;
    adsGain_t _gain;
    uint16_t _rate;
    int16_t _data;
    float _real;

public:
    Channel(uint8_t index, bool active = true, adsGain_t gain = GAIN_ONE, uint16_t rate = ADC_DATA_RATE) {
        _index = index; _active = active; _gain = gain; _rate = rate;
    }

    bool read() {
        ads.setGain(_gain);
        ads.setDataRate(_rate);
        int16_t read = ads.readADC_SingleEnded(_index);
        if (_data == read) {
            return false;
        }
        _real = ads.computeVolts(_data = read);
        return true;
    }

    uint8_t getIndex() const {
        return _index;
    }

    bool isActive() const {
        return _active;
    }

    String getValue() const {
        return String(_real, 5) + "V  " + _data;
    }

    String getInfo() const {
        return String(_active ? "ON   " : "OFF ") + SPS_from_dataRate(_rate)
            + "sps  " + ADS1x15_gain_info(_gain).toString();
    }
};

Channel channels[] = { Channel(0, true, GAIN_TWOTHIRDS), Channel(1, true, GAIN_TWOTHIRDS),
    Channel(2, true, GAIN_TWOTHIRDS), Channel(3, true, GAIN_TWOTHIRDS) };

void drawChannel(const Channel& chan, bool withInfo) {
    String value = chan.getValue();
    tft.setTextSize(4);
    tft.setTextColor(chan.isActive() ? WHITE : GRAY);
    tft.setCursor(0, 8 * 6 + chan.getIndex() * 8 * 6);
    tft.fillRect(0, tft.getCursorY(), tft.width(), 8 * 4, BLACK);
    tft.println(value);
    Serial.println(String(chan.getIndex()) + "# " + value);

    if (withInfo) {
        tft.setTextSize(1);
        tft.setTextColor(chan.isActive() ? YELLOW : DARK_YELLOW);
        String info = String("  ") + SPS_from_dataRate(ads.getDataRate())
            + "sps  " + ADS1x15_gain_info(ads.getGain()).toString();
        tft.println("  " + chan.getInfo());
    }
}

void drawMarker(char marker) {
    tft.setTextSize(2);
    tft.setTextColor(BLACK);
    tft.setCursor(tft.width() - 20, 16);
    tft.fillRect(tft.width() - 20, 16, 12, 16, MAGENTA);
    tft.print(marker);
    Serial.println(tft.width());
}

void setup(void) {
    Serial.begin(9600);
    tft.begin(tft.readID());
    tft.setRotation(1);
    tft.fillScreen(BLACK);
    tft.fillRect(0, 0, tft.width(), 8 * 4, MAGENTA);
    tft.setTextSize(4);
    tft.setTextColor(WHITE);
    tft.println("ADS1x15 TEST");

    if (!ads.begin()) {
        Serial.println("Failed to initialize ADS.");
        tft.println("ADC Initialization FAILED!");
        while (1);
    }
    // ads.setGain(GAIN_TWO);
    // ads.setDataRate(ADC_DATA_RATE);
    // draw_ADC_status();
    for (Channel& chan : channels) {
        drawChannel(chan, true);
    }
}

void loop(void) {
    drawMarker('*');

    for (Channel& chan : channels) {
        if (chan.isActive() && chan.read()) {
            drawChannel(chan, false);
        }
    }

    delay(200);
    drawMarker(' ');
    delay(1800);
}
