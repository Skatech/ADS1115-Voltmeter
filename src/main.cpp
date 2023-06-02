#include <Arduino.h>
// DIYMORE ILI9341 TFT
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include <TouchScreen.h> 
// ADS1X15 ADC
#include <SPI.h>
#include <Adafruit_ADS1X15.h>

#define SIMULATION // using ADS1015 instead of ADS1115

#ifdef SIMULATION
#define USE_ADS1015
#define ADC_DATA_RATE RATE_ADS1015_128SPS
#define TITLE_MESSAGE "ADS1015 VOLTMETER"
Adafruit_ADS1015 ads;
#else
#define USE_ADS1115
#define ADC_DATA_RATE RATE_ADS1115_8SPS
#define TITLE_MESSAGE "ADS1115 VOLTMETER"
Adafruit_ADS1115 ads;
#endif

#define TFT_GRAY    0x7BEF
#define TFT_DARKYELLOW 0x7BE0 // R5(F8)  G6(07e0) B5(001f)

MCUFRIEND_kbv tft;

#define MINPRESSURE 200
#define MAXPRESSURE 1000 
const int XP = 6, XM = A2, YP = A1, YM = 7; //320x480 ID=0x9486
const int TS_LEFT = 919, TS_RT = 158, TS_TOP = 958, TS_BOT = 157;
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);
uint16_t touch_x, touch_y, touch_z;
uint8_t buttonCenters[] = { 20, 60, 100, 140 };

class Channel {
    uint8_t _index;
    bool _active;
    adsGain_t _gain;
    uint16_t _mult;
    uint16_t _rate;
    int16_t _read;
    float _real;

public:
    Channel(uint8_t index, bool active = true, adsGain_t gain = GAIN_ONE, uint16_t mult = 1, uint16_t rate = ADC_DATA_RATE) {
        _index = index; _active = active; _gain = gain; _mult = mult, _rate = rate;
    }

    bool read() {
        ads.setGain(_gain);
        ads.setDataRate(_rate);
        int16_t read = ads.readADC_SingleEnded(_index);
        float real = _mult * ads.computeVolts(read);
        if (real != _real) {
            _read = read;
            _real = real;
            return true;
        }
        return false;
    }

    uint8_t getIndex() const {
        return _index;
    }

    bool isActive() const {
        return _active;
    }

    uint16_t getSampleRate() const {
        #ifdef USE_ADS1015
        switch(_rate) {
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
        switch(_rate) {
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

    float getGain() const {
        switch (_gain) {
            case GAIN_TWOTHIRDS: return 2.f / 3.f;
            case GAIN_ONE: return 1.f;
            case GAIN_TWO: return 2.f;
            case GAIN_FOUR: return 4.f;
            case GAIN_EIGHT: return 8.f;
            case GAIN_SIXTEEN: return 16.f;
        }
        return 0;
    }

    uint16_t getMult() const {
        return _mult;
    }

    void switchActive() {
        _active = !_active;
    }

    void switchSampleRate() {
        #ifdef USE_ADS1015
        _rate = (((_rate >> 5) + 1) % 7) << 5;
        #endif
        #ifdef USE_ADS1115
        _rate = (((_rate >> 5) + 1) % 8) << 5;
        #endif
    }

    void switchGain() {
        _gain = adsGain_t((((uint16_t(_gain) >> 9) + 1) % 6) << 9);
    }

    void switchMult() {
        _mult = _mult > 9 ? 1 : _mult + 1;
    }

    float getVoltage() const {
        return _real;
    }

    uint16_t getReading() const {
        return _read;
    }

    String getInfo() const {
        const float gain = getGain();
        const float range = 4.096f / gain * _mult;
        #ifdef USE_ADS1015
        const float bitmv = 4.096f * 2 * 1000 / 4096 / gain * _mult;
        #endif
        #ifdef USE_ADS1115
        const float bitmv = 4.096f * 2 * 1000 / 65536 / gain * _mult;
        #endif

        String builder = "+/-%rV  %bmV/bit";
        // builder.replace("%d", String(divider, 2).substring(0, 4));
        builder.replace("%r", String(range, 3));
        builder.replace("%b", String(bitmv, 7));
        return builder;
    }
};

Channel channels[] = {
    Channel(0, true, GAIN_TWOTHIRDS),
    Channel(1, true, GAIN_TWOTHIRDS),
    Channel(2, true, GAIN_TWOTHIRDS),
    Channel(3, true, GAIN_TWOTHIRDS)
};

void printCentered(const String& msg, uint8_t textSize, int16_t ypos,
        uint16_t xcenter = tft.width() / 2, uint16_t backgroundColor = TFT_BLACK) {
    const uint16_t textWidth = 6 * textSize * msg.length();
    const uint16_t xpos = xcenter - textWidth / 2;
    tft.setTextSize(textSize);
    tft.setCursor(xpos, ypos);
    tft.fillRect(xpos, ypos, textWidth, 8 * textSize, backgroundColor);
    tft.print(msg);
}

uint16_t getChannelHeight() {
    return 8 * 8;
}

uint16_t getChannelTop(uint8_t index) {
    const uint8_t margin = 4;
    return margin + index * (2 * margin + getChannelHeight());
}

void drawChannelReading(const Channel& chan, bool acquiring = false) {
    const uint16_t chan_vtop = getChannelTop(chan.getIndex());
    const uint8_t digits = 5;
    String value = acquiring ? "-----" : String(chan.getReading());
    tft.setTextSize(1);
    tft.fillRect(tft.width() - 10 - 6 * digits , chan_vtop + 4, 6 * digits, 8, TFT_BLACK);
    tft.setCursor(tft.width() - 10 - value.length() * 6, chan_vtop + 4);
    tft.print(value);
}

void drawChannel(const Channel& chan, bool withInfo) {
    const uint16_t chan_vtop = getChannelTop(chan.getIndex());
    const uint16_t chan_height = getChannelHeight();
    const uint16_t color = chan.isActive() ? TFT_WHITE : TFT_GRAY;

    if (withInfo) {
        tft.fillRect(0, chan_vtop, tft.width(), chan_height, TFT_BLACK);
        tft.drawRoundRect(4, chan_vtop, tft.width() - 8, chan_height, 4, color);
        tft.drawFastHLine(4, chan_vtop + 16 - 2, tft.width() - 8, color);
        tft.drawFastHLine(4, chan_vtop + 16 + 33, tft.width() - 8, color);
    }

    tft.setTextColor(color);
    printCentered(String(chan.getVoltage(), 5) + "V", 4, chan_vtop + 8 * 2 + 1);
    drawChannelReading(chan);

    if (withInfo) {
        printCentered(chan.isActive() ? "ON" : "OFF", 1, chan_vtop + 4, buttonCenters[0]);
        printCentered(String(chan.getSampleRate()) + "s", 1, chan_vtop + 4, buttonCenters[1]);
        printCentered(String(chan.getGain(), 2).substring(0, 4) + 'x', 1, chan_vtop + 4, buttonCenters[2]);
        printCentered(String(chan.getMult()) + 'x', 1, chan_vtop + 4, buttonCenters[3]);
        printCentered(chan.getInfo(), 1, chan_vtop + chan_height - 12);
    }
}

void readTouch() {
    TSPoint tp = ts.getPoint();
    pinMode(XM, OUTPUT);
    pinMode(YP, OUTPUT); 

    if (tp.z > MINPRESSURE && tp.z < MAXPRESSURE) {
        touch_z = tp.z;
        switch (tft.getRotation()) {
            case 0:
                touch_x = map(tp.x, TS_LEFT, TS_RT, 0, tft.width());
                touch_y = map(tp.y, TS_TOP, TS_BOT, 0, tft.height());
                break;
            case 1:
                touch_x = map(tp.y, TS_TOP, TS_BOT, 0, tft.width());
                touch_y = map(tp.x, TS_RT, TS_LEFT, 0, tft.height());
                break;
            case 2:
                touch_x = map(tp.x, TS_RT, TS_LEFT, 0, tft.width());
                touch_y = map(tp.y, TS_BOT, TS_TOP, 0, tft.height());
                break;
            case 3:
                touch_x = map(tp.y, TS_BOT, TS_TOP, 0, tft.width());
                touch_y = map(tp.x, TS_LEFT, TS_RT, 0, tft.height());
                break;
        }
    }
    else touch_z = 0;

    #ifdef SIMULATION
    if (Serial.available() > 1) {
        int8_t c = (char)Serial.read() - '1';

        if (c >= 0 && c <= 3) {
            int8_t r = (char)Serial.read() - '1';
            if (r >= 0 && r <= 3) {
                touch_z = 400;
                touch_x = buttonCenters[r];
                touch_y = 10 + getChannelTop(c);
            }
        }
    }
    #endif
}

void handleTouch() {
    if (touch_z > 0) {
        for (int i = 0; i < 4; ++i) {
            const uint16_t chan_vtop = getChannelTop(i);
            if (touch_y > chan_vtop && touch_y < (chan_vtop + 16)) {
                for (int n = 0; n < 4; ++n) {
                    if (abs(int(touch_x) - int(buttonCenters[n])) < 16) {
                        if (n == 0) {
                            channels[i].switchActive();
                            drawChannel(channels[i], true);
                        }
                        else if (n == 1) {
                            channels[i].switchSampleRate();
                            drawChannel(channels[i], true);
                        }
                        else if (n == 2) {
                            channels[i].switchGain();
                            drawChannel(channels[i], true);
                        }
                        else if (n == 3) {
                            channels[i].switchMult();
                            drawChannel(channels[i], true);
                        }
                        // Serial.println(String("touched channel ") + i + "  button " + n);
                        break;
                    }
                }
            }
        }
    }
    // tft.drawCircle(touch_x, touch_y, 2, RED);
    // Serial.println(String("TOUCH { ")  + touch_x + ", " + touch_y + ", " + touch_z + " }");
}

void drawDelayMarkerPie(uint16_t cx, uint16_t cy, uint8_t r, float angle) {
    static uint16_t px = cx, py = cy - r;
    static float pa = angle;
    if (pa >= angle) {
        pa = angle;
        px = cx;
        py = cy - r;
        tft.fillCircle(cx, cy, r, TFT_BLACK);
        tft.drawCircle(cx, cy, r, TFT_WHITE);
    }
    uint16_t x = cx + r * sinf(angle);
    uint16_t y = cy - r * cosf(angle);
    tft.fillTriangle(cx, cy, px, py, x, y, TFT_WHITE);
    px = x;
    py = y;
}

void setup(void) {
    Serial.begin(9600);

    tft.begin(tft.readID());
    tft.setRotation(0);    
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    printCentered(String(TITLE_MESSAGE), 2, tft.height() / 2 - 2 * 8);

    if (!ads.begin()) {
        const char* msg = "ADS1x15 NOT FOUND";
        Serial.println(msg);
        tft.setTextColor(TFT_RED);
        printCentered(msg, 2, tft.height() / 2 + 2 * 8);
        while (1);
    }

    delay(1000);
    tft.fillScreen(TFT_BLACK);

    // ads.setGain(GAIN_TWO);
    // ads.setDataRate(ADC_DATA_RATE);
    // draw_ADC_status();
    for (Channel& chan : channels) {
        drawChannel(chan, true);
    }
}

void loop(void) {
    for (Channel& chan : channels) {
        if (chan.isActive()) {
            drawChannelReading(chan, true);
            if (chan.read()) {
                drawChannel(chan, false);
            }
            else drawChannelReading(chan);
        }
    }

    for (int i = 0; i < 20; ++i) {
        readTouch();
        handleTouch();
        drawDelayMarkerPie(tft.width() - 10, tft.height() - 10, 8, PI * 2 / 20 * (i + 1));
        delay(100);
    }
}
