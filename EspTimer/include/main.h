#pragma once

#include <ESPAsyncWebServer.h>

#include "NYG.h"
#include "IInput.h"
#include "IOutput.h"

#include "TimeEx.h"

#include "AsyncWebServerEx.h"
#include "Html.h"
#include "SmartHomeWiFiApp.h"
#include "MicroController.h"

#define _BUILD_PRODUCTION   1

#if _BUILD_PRODUCTION
#define PRODUCT_NAME            "EspTimer"
#define IP_ADDRESS_4TH_BYTE     123
#else
#define PRODUCT                 "TestEspTimer"
#define IP_ADDRESS_4TH_BYTE     199
#endif //_BUILD_PRODUCTION

#define VERSION			2

//#define IP_ADDRESS_4TH_BYTE 123

#define UNDER_QA        0

class AsyncWebServer;

#include "Settings.h"

enum IoPins
{
    // https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/
#if _USE_ESP_01
    RELAY_PIN               = 0,          
#else
    TEMPERATURE_SENSOR_PIN  = A0,       
    RELAY_PIN               = D3,          
#endif
    LED_PIN                 = LED_BUILTIN, 
};

extern RevertedDigitalOutputPin     gbl_led;

bool GetRelayStatus();
void SetRelayStatus(bool on);

long GetOnSeconds();
long GetOffSeconds();

unsigned long GetStartId();
#define NOT_STARTED_ID  0

void SetRelayOffTimerSeconds(uint32_t seconds);
inline void SetRelayOffTimer(uint32_t minutes)             { SetRelayOffTimerSeconds(minutes * SECONDS_PER_MINUTE); }
#define CancelRelayOffTimer()   SetRelayOffTimerSeconds(0)
int32_t GetRelayOffSeconds();

void SetRelayOnTimerSeconds(uint32_t seconds);
inline void SetRelayOnTimer(uint32_t minutes)             { SetRelayOnTimerSeconds(minutes * SECONDS_PER_MINUTE); }
#define CancelRelayOnTimer()   SetRelayOnTimerSeconds(0)
int32_t GetRelayOnSeconds();

float GetTemperature();
void ScheduleRestart(uint32_t seconds);

String ConvertToHuman(int32_t seconds);
int GetWiFiIndex();

