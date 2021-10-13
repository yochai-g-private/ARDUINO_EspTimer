#pragma once

#include "main.h"
//#include "InternetTime.h"
#include "WiFiUtils.h"

namespace NYG 
{
    struct Settings
    {
        enum { DEFAULT_ON_MINUTES =  2 * MINUTES_PER_HOUR,
               MAX_RELAY_MINUTES  = 12 * MINUTES_PER_HOUR };
        
        char                product_name[16];
        uint8_t             version;
        uint16_t            size;
	
        APDef               AP,
                            prev_AP;

        char                host_name[16];
        char                instance_title[24];

        bool                use_WIFI;
		WiFiDef				WIFI[3];
        char                WIFI_order[countof(WIFI) + 1];
        
        bool                use_led;
        
        uint16_t            default_on_minutes,
                            max_on_delay_minutes,
                            max_off_delay_minutes;

        bool                night_only;

    	//=============================================================

        static const Settings& GetDefault();
        static void Load();
        static bool Store(const NYG::Settings& temp);

        void InitializeWebServices(AsyncWebServer& server);

        bool Store();
        
        static void AddWebServices(AsyncWebServer& server);

        enum { SUNRISE = 0xFFFF };

        static void WriteApplicationInfoToLog()
        {
            _LOGGER << "Application: " << defaults.product_name << NL;
            _LOGGER << "Version    : " << defaults.version << NL;
        }

    private:

        static void restore_to_defaults();
        
        static Settings defaults;

    };

    extern Settings settings;

};

