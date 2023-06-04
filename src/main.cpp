#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include <TouchScreen.h> 
#include "config.h"
#include "channel.h"

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

void printCentered(const String& msg, uint8_t textSize, int16_t ypos,
        uint16_t xcenter = tft.width() / 2, uint16_t backgroundColor = TFT_BLACK) {
    const uint16_t textWidth = 6 * textSize * msg.length();
    const uint16_t xpos = xcenter - textWidth / 2;
    tft.setTextSize(textSize);
    tft.setCursor(xpos, ypos);
    tft.fillRect(xpos, ypos, textWidth, 8 * textSize, backgroundColor);
    tft.print(msg);
}

#define CHANNEL_BUTTON_ENABLE   0
#define CHANNEL_BUTTON_SPS      1
#define CHANNEL_BUTTON_PGA      2
#define CHANNEL_BUTTON_MUL      3

#define CHANNEL_HEIGHT 8 * 8
#define CHANNEL_MARGIN_TOP 4
#define CHANNEL_MARGIN_BOTTOM 4
#define CHANNEL_MARGIN_SIDE 4

class ChannelControl {
    uint8_t _idx;
    bool _act = true;
    uint16_t _adc = 0;
    float _val = 0;
    
    public:
    ChannelControl(uint8_t idx) {
        _idx = idx;
    }

    void updateValue() {
        if (_act) {
            drawReading(true);
            Channel& chn = Channel::getChannel(_idx);
            uint16_t adc = chn.readValue();
            float val = chn.computeVoltage(adc);
            if (adc != _adc || val != _val) {
                _adc = adc;
                _val = val;
                draw(false);
            }
            else drawReading();
        }
    }

    uint16_t getDisplayTop() {
        return CHANNEL_MARGIN_TOP + _idx *
            (CHANNEL_HEIGHT + CHANNEL_MARGIN_TOP + CHANNEL_MARGIN_BOTTOM);
    }

    void drawReading(bool acquiring = false) {
        const uint16_t top = getDisplayTop();
        const uint8_t digits = 5;
        String value = acquiring ? String("-----") : String(_adc);
        tft.setTextSize(1);
        tft.fillRect(tft.width() - 10 - 6 * digits , top + 4, 6 * digits, 8, TFT_BLACK);
        tft.setCursor(tft.width() - 10 - value.length() * 6, top + 4);
        tft.print(value);
    }

    void draw(bool full) {
        const Channel& chan = Channel::getChannel(_idx);
        const uint16_t top = getDisplayTop();
        const uint16_t color = _act ? TFT_WHITE : TFT_GRAY;

        if (full) {
            tft.fillRect(0, top, tft.width(), CHANNEL_HEIGHT, TFT_BLACK);
            tft.drawRoundRect(CHANNEL_MARGIN_SIDE, top,
                tft.width() - 2 * CHANNEL_MARGIN_SIDE, CHANNEL_HEIGHT, 4, color);
            tft.drawFastHLine(CHANNEL_MARGIN_SIDE,
                top + 16 - 2, tft.width() - 2 * CHANNEL_MARGIN_SIDE, color);
            tft.drawFastHLine(CHANNEL_MARGIN_SIDE,
                top + 16 + 33, tft.width() - 2 * CHANNEL_MARGIN_SIDE, color);
        }

        tft.setTextColor(color);
        printCentered(String(_val, 5) + "V", 4, top + 8 * 2 + 1);
        drawReading();

        if (full) {
            printCentered(_act ? "ON" : "OFF", 1, top + 4, buttonCenters[CHANNEL_BUTTON_ENABLE]);
            printCentered(String(chan.getSampleRate()) + "s", 1, top + 4, buttonCenters[CHANNEL_BUTTON_SPS]);
            printCentered(chan.getGainString(), 1, top + 4, buttonCenters[CHANNEL_BUTTON_PGA]);
            printCentered(String(chan.getMultiplier()) + 'x', 1, top + 4, buttonCenters[CHANNEL_BUTTON_MUL]);
            printCentered(chan.getInfoString(), 1, top + CHANNEL_HEIGHT - 12);
        }
    }

    bool isActive() const {
        return _act;
    }

    void switchActive() {
        _act = !_act;
    }

    void switchSampleRate() {
        Channel::getChannel(_idx).nextSampleRate();
    }

    void switchGain() {
        Channel::getChannel(_idx).nextGainValue();
    }

    void switchMult() {
        Channel::getChannel(_idx).nextMultiplier();
    }
};

ChannelControl channels[] = {
    ChannelControl(0), ChannelControl(1), ChannelControl(2), ChannelControl(3)
};

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
                touch_y = 10 + channels[c].getDisplayTop();
            }
        }
    }
    #endif
}

void handleTouch() {
    if (touch_z > 0) {
        for (int i = 0; i < 4; ++i) {
            const uint16_t top = channels[i].getDisplayTop();
            if (touch_y > top && touch_y < (top + 16)) {
                for (int n = 0; n < 4; ++n) {
                    if (abs(int(touch_x) - int(buttonCenters[n])) < 16) {
                        if (n == 0) {
                            channels[i].switchActive();
                            channels[i].draw(true);
                        }
                        else if (n == 1) {
                            channels[i].switchSampleRate();
                            channels[i].draw(true);
                        }
                        else if (n == 2) {
                            channels[i].switchGain();
                            channels[i].draw(true);
                        }
                        else if (n == 3) {
                            channels[i].switchMult();
                            channels[i].draw(true);
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

    if (!Channel::begin()) {
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
    for (ChannelControl& chan : channels) {
        chan.draw(true);
    }
}

void loop(void) {
    unsigned long msec = millis();
    for (ChannelControl& chan : channels) {
        chan.updateValue();
    }
    
    msec = (2000 - (millis() - msec)) / 20;
    for (int i = 0; i < 20; ++i) {
        readTouch();
        handleTouch();
        drawDelayMarkerPie(tft.width() - 10, tft.height() - 10, 8, PI * 2 / 20 * (i + 1));
        delay(msec);
    }
}
