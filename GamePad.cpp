#include "GamePad.h"

int16_t channel[16] = {0};
ControllerPtr myControllers[BP32_MAX_GAMEPADS];

long mapAxis(int x) {
    if (x < -511) x = -511;
    if (x > 512) x = 512;
    return 1000 + ((long)(-x + 511) * 1000) / 1023;
}

long mapTrigger(int x) {
    if (x < 0) x = 0;
    if (x > 1023) x = 1023;
    return 1000 + ((long)x * 1000) / 1023;
}

void onConnectedController(ControllerPtr ctl) {
    bool foundEmptySlot = false;
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == nullptr) {
            Serial.printf("CALLBACK: Controller is connected, index=%d\n", i);
            ControllerProperties properties = ctl->getProperties();
            Serial.printf("Controller model: %s, VID=0x%04x, PID=0x%04x\n", ctl->getModelName().c_str(), properties.vendor_id,
                           properties.product_id);
            myControllers[i] = ctl;
            foundEmptySlot = true;
            break;
        }
    }
    if (!foundEmptySlot) {
        Serial.println("CALLBACK: Controller connected, but could not found empty slot");
    }
}

void onDisconnectedController(ControllerPtr ctl) {
    bool foundController = false;
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == ctl) {
            Serial.printf("CALLBACK: Controller disconnected from index=%d\n", i);
            myControllers[i] = nullptr;
            foundController = true;
            break;
        }
    }
    if (!foundController) {
        Serial.println("CALLBACK: Controller disconnected, but not found in myControllers");
    }
}

void dumpGamepad(ControllerPtr ctl) {
    long raw[16];
    raw[0]  = ctl->axisRX();
    raw[1]  = ctl->axisRY();
    raw[2]  = ctl->axisY();
    raw[3]  = ctl->axisX();
    raw[4]  = ctl->brake();
    raw[5]  = ctl->throttle();

    for (int i = 0; i < 16; ++i) {
        if (i == 0 || i == 1 || i == 2 || i == 3) {
            channel[i] = mapAxis(raw[i]) - 500;
        } else if (i == 4 || i == 5) {
            channel[i] = mapTrigger(raw[i]) - 500;
        } else {
            channel[i] = 1500 - 500;
        }
    }
}

void processGamepad(ControllerPtr ctl) {
    if (ctl->a()) {
        static int colorIdx = 0;
        switch (colorIdx % 3) {
            case 0: ctl->setColorLED(255, 0, 0); break;
            case 1: ctl->setColorLED(0, 255, 0); break;
            case 2: ctl->setColorLED(0, 0, 255); break;
        }
        colorIdx++;
    }
    if (ctl->b()) {
        static int led = 0;
        led++;
        ctl->setPlayerLEDs(led & 0x0f);
    }
    if (ctl->x()) {
        ctl->playDualRumble(0, 250, 0x80, 0x40);
    }
    dumpGamepad(ctl);
}

void processControllers() {
    for (auto myController : myControllers) {
        if (myController && myController->isConnected() && myController->hasData()) {
            if (myController->isGamepad()) {
                processGamepad(myController);
            } else {
                Serial.println("Unsupported controller");
            }
        }
    }
}