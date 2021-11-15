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
        //==================================
        // Constants
        //==================================
        PRODUCT_NAME,                       // product_name
		VERSION,                            // version
        (uint16_t)sizeof(Settings),         // size

        //==================================
        // Configurable
        //==================================
        APDef(PRODUCT_NAME, "12345678"),    // AP
        APDef(),                            // prev_AP

        PRODUCT_NAME,                       // host_name
        PRODUCT_NAME,                       // instance_title

        false,                              // use_WIFI
        {   
            WiFiDef("",                 "",             IPAddress()),
            WiFiDef(HOME_TEST_SSID,          HOME_WIFI_COMMON_PASS,   IPAddress()),
            WiFiDef("YochaiG",          HOME_WIFI_COMMON_PASS,   IPAddress()),
        },                                  // WIFI
        "123",                              // WIFI_order

        true,                               // use_led

        DEFAULT_ON_MINUTES,                 // default_on_minutes
        MAX_RELAY_MINUTES,                  // max_on_delay_minutes
        MAX_RELAY_MINUTES,                  // max_off_delay_minutes

        true,                               //night_only
    };

    Settings settings;
};
//------------------------------------------------------
static bool set_AP(APDef& AP, const String& ssid_pass, AsyncWebServerRequest *request)
{
    int sepidx = ssid_pass.indexOf(':');

    if(sepidx < 1)
    {
        SendError("Invalid SSIP:pass format", *request);
        return false;
    }

    String SSID = ssid_pass.substring(0, sepidx);
    String pass = ssid_pass.substring(sepidx + 1);

    const char* error = NULL;

    if(SSID.length() < 1)
        error = "Invalid SSID";
    else if(pass.length() < 8)
        error = "Invalid password";

    if(error)
    {
        SendError(error, *request);
        return false;
    }

    AP.Set(SSID.c_str(), pass.c_str());

    return true;
}
//------------------------------------------------------
static bool set_wifi(int idx, const String& ssid_pass, AsyncWebServerRequest *request)
{
    return set_AP(settings.WIFI[idx-1], ssid_pass, request);
}
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
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    server.on("/m/settings/AP", HTTP_GET, [](AsyncWebServerRequest *request) {
        String arg;
        if (GetStringParam(*request, "set",  arg))
        {
            if(!set_AP(settings.AP, arg, request))
                return;
        }

        settings.Store();
        SendInfo(settings.AP.SSID(), *request);
    });
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    server.on("/m/settings/host_name", HTTP_GET, [](AsyncWebServerRequest *request) {
        String arg;
        if (GetStringParam(*request, "set",  arg))
        {
            if(arg == "")
                arg = PRODUCT_NAME;
            
            strncpy(settings.host_name, arg.c_str(), countof(settings.host_name));
            settings.Store();
        }

        SendInfo(settings.host_name, *request);
    });
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    server.on("/m/settings/instance_title", HTTP_GET, [](AsyncWebServerRequest *request) {
        String arg;
        if (GetStringParam(*request, "set",  arg))
        {
            if(arg == "")
                arg = PRODUCT_NAME;
            
            strncpy(settings.instance_title, arg.c_str(), countof(settings.instance_title));
            settings.Store();
        }

        SendInfo(settings.instance_title, *request);
    });
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    server.on("/m/settings/wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;

        String arg;
        Times  times;

        struct Changed
        {
            Changed()   : m_save_it(false)    {   }
            ~Changed()  { if(m_save_it) settings.Store(); }
            void Set()  { m_save_it = true; }

        private:
            bool m_save_it;
        };

        // UNCONDITIONED
        {
            Changed changed;

            bool use;
            if (GetBoolParam(*request, "use",  use))
            {
                settings.use_WIFI = use;
                changed.Set();
            }

            if (GetStringParam(*request, "1",  arg))
            {
                if(!set_wifi(1, arg, request))
                    return;

                changed.Set();
            }
            if (GetStringParam(*request, "2",  arg))
            {
                if(!set_wifi(2, arg, request))
                    return;

                changed.Set();
            }
            if (GetStringParam(*request, "3",  arg))
            {
                if(!set_wifi(3, arg, request))
                    return;

                changed.Set();
            }

            if (GetStringParam(*request, "order",  arg))
            {
                const char* order = arg.c_str();
                if(!*order)
                    order = "321";

                bool OK = (0 != *order);

                for(int idx = 0; OK && order[idx]; idx++)
                {
                    int order_idx = order[idx] - '0';
                    OK = order_idx >= 1 && order_idx <= (int)countof(settings.WIFI);
                }

                if(OK)
                {
                    strncpy(settings.WIFI_order, order, countof(settings.WIFI));
                    changed.Set();
                }
            }
        }

        int WIFI_idx = GetWiFiIndex();
        const char* connected = (WIFI_idx < 0 || WIFI_idx >= (int)countof(settings.WIFI)) ? "?" : settings.WIFI[WIFI_idx].SSID();

        String text = "use=";
        text = text + ONOFF(settings.use_WIFI).Get() + "\n";

        for(int idx = 0; idx < (int)countof(settings.WIFI); idx++)
        {
            int WIFI_idx = settings.WIFI_order[idx] - '1';

            if((WIFI_idx < 0) || (WIFI_idx >= (int)countof(settings.WIFI)))
                continue;

            if(!*settings.WIFI[WIFI_idx].SSID())
                continue;

            char line[128];

            sprintf(line, "%d. %s%s\n", 
                    WIFI_idx + 1, 
                    strequal(settings.WIFI[WIFI_idx].SSID(),connected) ? "*" : "",
                    settings.WIFI[WIFI_idx].SSID());

            text += line;
        }

        text = text + "order=" + settings.WIFI_order;

        SendInfo(text.c_str(), *request);
    });
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    server.on("/m/settings/led", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;
        bool arg;
        if (GetBoolParam(*request, "set",  arg))
        {
            if(!arg)
                gbl_led.Off();
                
            settings.use_led = arg;

            settings.Store();
        }

        SendInfo(ONOFF(settings.use_led).Get(), *request);
    });
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    server.on("/m/settings/default_on_minutes", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;
        uint16_t arg;
        if (GetUnsignedShortParam(*request, "set",  arg))
        {
            settings.default_on_minutes = arg;
            settings.Store();
        }

        SendInfo(String(settings.default_on_minutes).c_str(), *request);
    });
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    server.on("/m/settings/night_only", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;
        bool arg;
        if (GetBoolParam(*request, "set",  arg))
        {
            settings.night_only = arg;
            settings.Store();
        }

        SendInfo(ONOFF(settings.night_only).Get(), *request);
    });
}
//------------------------------------------------------
