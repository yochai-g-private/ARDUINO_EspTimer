//#include <EEPROM.h>
//#include <TimeLib.h>
//#include <Wire.h>
//#include <DS3231.h>
//#include <Ticker.h>

#include "Logger.h"
#include "Timer.h"
#include "Thermistor.h"
#include "WiFiUtils.h"
//#include "InternetTime.h"
#include "MicroController.h"
#include "Observer.h"
#include "Random.h"

#include "main.h"
#include "Settings.h"
#include "WebServices.h"

static bool connect_to_WIFI(bool require_specific_address);
static void get_internet_time(bool RTC_ok);

const char*                         gbl_fatal_error_reason = NULL;

static bool                         st_wifi_OK = false;

RevertedDigitalOutputPin            gbl_led(LED_PIN);

static Timer                        st_restart_timer,
                                    st_relay_off_timer,
                                    st_relay_on_timer;
//static Thermistor*                  st_thermistor;
static SmoothThermistor*            st_thermistor;
typedef DigitalOutputPin            Relay;

static Relay                        st_relay(RELAY_PIN);
static unsigned long                st_on_at;

static unsigned long                st_start_id = NOT_STARTED_ID;

void setup()
{
    Logger::Initialize();

    const Settings& defaults = Settings::GetDefault();

    _LOGGER << NL;
    _LOGGER << NL;
    _LOGGER << NL;
    _LOGGER <<  S("****************************") << NL;
    _LOGGER <<    "*  " << defaults.product_name << ", version #" << defaults.version << "  *" << NL;
    _LOGGER <<  S("****************************") << NL;
    _LOGGER << NL;

#if 0
    Settings::Store(Settings::GetDefault());
#endif

    gbl_led.On();

    Settings::Load();

    if(settings.use_WIFI)
    {
        st_wifi_OK = connect_to_WIFI(false);

        if(st_wifi_OK)
        {
            WiFi.disconnect();
            st_wifi_OK = connect_to_WIFI(true);
        }
    }

    if(!st_wifi_OK)
    {
        st_wifi_OK = WiFiUtils::CreateAP(settings.AP.SSID(),	// SSID
                                         settings.AP.PASS(),	// PASS
                                         false);	    	    // hidden

        if(!st_wifi_OK)
        {
            LOGGER << S("Not able to create AP '") << settings.AP.SSID() << S("'") << NL;

            if(*settings.prev_AP.SSID())
            {
                LOGGER << S("Trying with the previous SSID...") << NL;

                st_wifi_OK = WiFiUtils::CreateAP(settings.prev_AP.SSID(),	// SSID
                                                 settings.prev_AP.PASS(),	// PASS
                                                 false);	    	    // hidden

                if(st_wifi_OK)
                {
                    LOGGER << S("Restoring the previous SSID...") << NL;

                    settings.AP = settings.prev_AP;
                    settings.Store();
                }
            }
        }

    }

    if(st_wifi_OK)
    {
        InitializeWebServices();
    }
    else
    {
        LOGGER << S("Not able to use WIFI") << NL;
        return;
    }

    static SmoothThermistor thermistor(TEMPERATURE_SENSOR_PIN, 3.3);
//    static Thermistor thermistor(TEMPERATURE_SENSOR_PIN, 3.3);
    st_thermistor = &thermistor;

    LOGGER << "Started!" << NL;
}
//-----------------------------------------------------------------
void loop()
{
    if(!st_wifi_OK)
        return;

    st_thermistor->OnLoop();

    static unsigned long prev_micros = micros() - 1000000;
    unsigned long now_micros = micros();

    if(now_micros - prev_micros >= 1000000)
    {
        prev_micros += 1000000;
        static bool on = false;

        on = !on;

        if(settings.use_led)
            gbl_led.Set(on);
    }

    if(st_restart_timer.Test())
    {
        MicroController::Restart();
        return;
    }

    if(st_relay_off_timer.Test())
        SetRelayStatus(false);

    if(st_relay_on_timer.Test())
        SetRelayStatus(true);
}
//-----------------------------------------------------------------
static int       WIFI_idx = -1;
int GetWiFiIndex()  { return WIFI_idx; }
//-----------------------------------------------------------------
struct LedOnOff
{
    LedOnOff()  { gbl_led.On(); }
    ~LedOnOff()
    {
        gbl_led.Off();
        delay(500);
    }
};
//-----------------------------------------------------------------
static bool connect_to_WIFI(bool require_specific_address)
{
    static IPAddress connected,
                     gateway;
    
    { LedOnOff off; }

    if (require_specific_address)
    {
        LedOnOff on_off;

        IPAddress required_address(connected[0], 
                                   connected[1],
                                   connected[2], 
                                   IP_ADDRESS_4TH_BYTE);
        
        bool OK = WiFiUtils::ConnectToAP( settings.WIFI[WIFI_idx].SSID(), 
                                          settings.WIFI[WIFI_idx].PASS(), 
                                          required_address,
                                          gateway,
                                          settings.host_name,
                                          WiFiUtils::WIFI_CONNECT_TIMEOUT_SECONDS * 5);

        if(!OK)
            WIFI_idx = -1;

        return OK;
    }

    LOGGER << "Trying to connect to WiFi..." << NL;

    for(int idx = 0; idx < countof(settings.WIFI); idx++)
    {
        LedOnOff on_off;

        WIFI_idx = settings.WIFI_order[idx] - '1';

        if(WIFI_idx < 0 || WIFI_idx >= countof(settings.WIFI))
            continue;

        if(!*settings.WIFI[WIFI_idx].SSID())
            continue;

        if (WiFiUtils::ConnectToAP(settings.WIFI[WIFI_idx].SSID(), 
                                   settings.WIFI[WIFI_idx].PASS(), 
                                   settings.host_name,
                                   WiFiUtils::WIFI_CONNECT_TIMEOUT_SECONDS))
        {
            connected = WiFi.localIP();
            gateway   = WiFi.gatewayIP();

            return true;
        }
    }

    WIFI_idx = -1;

    return false;
}
//-----------------------------------------------------------
bool GetRelayStatus()
{
    return st_relay.IsOn();
}
//-----------------------------------------------------------
void SetRelayStatus(bool on)
{
    LOGGER << "Relay set to " << ONOFF(on).Get() << NL;
    st_relay.Set(on);
    st_on_at = (on) ? millis() : 0;
}
//-----------------------------------------------------------
unsigned long GetStartId()
{
    return st_start_id;
}
//-----------------------------------------------------------
void SetRelayOffTimerSeconds(uint32_t seconds)
{
    if(seconds)     
    { 
        do  {
            st_start_id = (unsigned long) Random::Get();
        }   while(st_start_id == NOT_STARTED_ID);

        st_relay_off_timer.StartOnce(seconds * 1000); 
        LOGGER << "Relay OFF timer set for " << ConvertToHuman(seconds) << NL;  
    }
    else            
    { 
        st_start_id = NOT_STARTED_ID;
        st_relay_off_timer.Stop();                    
        LOGGER << "Relay OFF timer canceled" << NL;
    }
}
//-----------------------------------------------------------
int32_t GetRelayOffSeconds()
{
    int32_t retval = st_relay_off_timer.GetRemainder();
    return retval < 0 ? retval : retval / 1000;
}
//-----------------------------------------------------------
void SetRelayOnTimerSeconds(uint32_t seconds)
{
    SetRelayStatus(false);

    if(seconds)     { st_relay_on_timer.StartOnce(seconds * 1000); LOGGER << "Relay ON timer set for " << ConvertToHuman(seconds) << NL;  }
    else            { st_relay_on_timer.Stop();                    LOGGER << "Relay ON timer canceled" << NL;                             }
}
//-----------------------------------------------------------
int32_t GetRelayOnSeconds()
{
    int32_t retval = st_relay_on_timer.GetRemainder();
    return retval < 0 ? retval : retval / 1000;
}
//-----------------------------------------------------------
float GetTemperature()
{
    return st_thermistor->GetCelsius();
}
//-----------------------------------------------------------
void ScheduleRestart(uint32_t seconds)
{
    st_restart_timer.StartOnce(seconds * 1000);
}
//-----------------------------------------------------------
String ConvertToHuman(int32_t t)
{
    String retval;

    int s = t % SECONDS_PER_MINUTE;     t = (t - s) / SECONDS_PER_MINUTE;
    int m = t % MINUTES_PER_HOUR;       t = (t - m) / MINUTES_PER_HOUR;
    int h = t;

    String prefix;

    if(h > 0)   retval += prefix + String(h) + " hour"   + (h == 1 ? "" : "s");
    prefix = String(h > 0 ? s > 0 ? ", " : " and " : "");

    if(m > 0)   retval += prefix + String(m) + " minute" + (m == 1 ? "" : "s");
    prefix = String(h > 0 | m > 0 ? " and " : "");

    if(s > 0)   retval += prefix + String(s) + " second" + (s == 1 ? "" : "s");

    return retval;
}
//-----------------------------------------------------------
long GetOnSeconds()
{
    return ((st_relay_on_timer.IsStarted()) ? st_relay_on_timer.GetRemainder() : (long)(st_on_at - millis())) / 1000;
}
//-----------------------------------------------------------
long GetOffSeconds()
{
    return (st_relay_off_timer.IsStarted()) ? st_relay_off_timer.GetRemainder() / 1000 : 0;
}
//-----------------------------------------------------------
const char*	gbl_build_date = __DATE__;
const char*	gbl_build_time = __TIME__;
//-----------------------------------------------------------
