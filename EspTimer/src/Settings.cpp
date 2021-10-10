#include "Settings.h"
#include "WebServices.h"

#include "EepromIO.h"
#include "SmartHomeWiFiAppDefaults.h"

using namespace NYG;

namespace NYG
{
    extern bool gbl_DST;
};
//------------------------------------------------------
#define st_offset   0

const Settings& Settings::GetDefault()
{
    return defaults;
}
//------------------------------------------------------
void Settings::restore_to_defaults()
{
    settings = defaults;
    settings.Store();
    LOGGER << "Settings written" << NL;
}
//------------------------------------------------------
void  Settings::Load()
{
    EepromInput settings_input(st_offset);
    settings_input >> settings;

    if (settings.version != defaults.version ||
        settings.size    != defaults.size    || 
        memcmp(defaults.product_name, settings.product_name, sizeof(defaults.product_name)))
    {
        restore_to_defaults();
    }
	else
    {
        LOGGER << "Settings OK" << NL;
    }
}
//------------------------------------------------------
bool  Settings::Store()
{
    NYG::Settings temp;
    EepromInput settings_input(st_offset);
    settings_input >> temp;

    if(objequal(temp, settings))
        return false;

    EepromOutput settings_output(st_offset);
    settings_output << settings;

    return true;
}
//------------------------------------------------------
bool  Settings::Store(const NYG::Settings& temp)
{
    settings = temp;
    return settings.Store();
}
//------------------------------------------------------
namespace NYG
{
    Settings Settings::defaults =
    {
        PRODUCT_NAME,                       // product_name
		VERSION,                            // version
        (uint16_t)sizeof(Settings),         // size

        APDef(PRODUCT_NAME, "12345678"),    // AP
        APDef(),                            //prev_AP

        PRODUCT_NAME,                       //host_name
        PRODUCT_NAME,                       // instance_title

        false,                              // use_WIFI
        {   
            WiFiDef("",                 "",             IPAddress()),
            WiFiDef("HomeNet",          "1357864200",   IPAddress()),
            WiFiDef("YochaiG",          "1357864200",   IPAddress()),
        },                                  //WIFI
        "123",                              //WIFI_order
        true,                               //use_led
        DEFAULT_ON_MINUTES,                 //default_on_minutes
        MAX_RELAY_MINUTES,                  //max_on_delay_minutes
        MAX_RELAY_MINUTES,                  //max_off_delay_minutes

    };

    Settings settings;
};
//------------------------------------------------------
void Settings::InitializeWebServices(AsyncWebServer& server)
{
    server.on("/m/settings/restore", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;
        restore_to_defaults();
        Html::h1 root("Settings restored to factory default.\nRestarting ...");
        SendElement(root, *request);
        ScheduleRestart(1);
        });

    server.on("/m/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;

        SendInfo("?????", *request); //TODO

        });

}
//------------------------------------------------------
