#pragma once
#include <Bluepad32.h>

extern int16_t channel[16];
extern ControllerPtr myControllers[BP32_MAX_GAMEPADS];

long mapAxis(int x);
long mapTrigger(int x);
void onConnectedController(ControllerPtr ctl);
void onDisconnectedController(ControllerPtr ctl);
void dumpGamepad(ControllerPtr ctl);
void processGamepad(ControllerPtr ctl);
void processControllers();