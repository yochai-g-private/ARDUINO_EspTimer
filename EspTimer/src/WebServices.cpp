#include "main.h"
#include "WebServices.h"
#include <AsyncElegantOTA.h>
#include <LittleFS.h>

AsyncWebServer server(80);

static String processor(const String& var) {
    if (var == "TEMPERATURE")
        return String(GetTemperature(), 1) + "C";

    if (var == "TITLE")
        return settings.instance_title;

    return "";
}

#define FONT_SIZE   64
void SendInfo(const char* s, AsyncWebServerRequest& request)
{
    SendInfoText(s, request, NULL, FONT_SIZE);
}

void SendError(const char* s, AsyncWebServerRequest& request, unsigned int code)
{
    SendErrorText(s, request, code, NULL, FONT_SIZE);
}

static void set_wifi(int idx, const String& ssid_pass)
{
    idx--;

    int sepidx = ssid_pass.indexOf(':');
    if(sepidx < 1)
        return;

    String SSID = ssid_pass.substring(0, sepidx);
    String pass = ssid_pass.substring(sepidx + 1);

    settings.WIFI[idx].Set(SSID.c_str(), pass.c_str(), IPAddress());
    settings.Store();
}

static String get_relay_status()
{
    int32_t switch_to_on_seconds  = GetRelayOnSeconds(),
            switch_to_off_seconds = GetRelayOffSeconds();

//LOGGER << "switch_to_on_seconds=" << switch_to_on_seconds << " switch_to_off_seconds=" << switch_to_off_seconds << NL;

    String retval;
    
    if(switch_to_on_seconds > 0)    
        retval += String("The relay will be switched ON in ") + ConvertToHuman(switch_to_on_seconds);
    else
        retval += String("The relay is ") + ONOFF(GetRelayStatus(), WC_UPPERCASE).Get();

    retval += "\n";

    if(switch_to_off_seconds > 0) 
        retval += String("The relay will be switched OFF in ") + ConvertToHuman(switch_to_off_seconds);

    retval.trim();

//LOGGER << retval << NL;

    return retval;
}

static void cancel()
{
    SetRelayStatus(false);
    CancelRelayOffTimer();
    CancelRelayOnTimer();
}

void InitializeWebServices()
{
    // Initialize SPIFFS
#define _USE_SPIFFS 1

#if _USE_SPIFFS
    if (!LittleFS.begin()) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }
#endif //_USE_SPIFFS

    File file = LittleFS.open("index.html", "r");
    if(!file)
    {
        Serial.println("***************** Failed to open file for reading ****************************");
        //return;
    }
#if 0
                                FSInfo fs_info;
                                LittleFS.info(fs_info);
                            
                                Serial.println("File sistem info.");
                            
                                Serial.print("Total space:      ");
                                Serial.print(fs_info.totalBytes);
                                Serial.println("byte");
                            
                                Serial.print("Total space used: ");
                                Serial.print(fs_info.usedBytes);
                                Serial.println("byte");
                            
                                Serial.print("Block size:       ");
                                Serial.print(fs_info.blockSize);
                                Serial.println("byte");
                            
                                Serial.print("Page size:        ");
                                Serial.print(fs_info.totalBytes);
                                Serial.println("byte");
                            
                                Serial.print("Max open files:   ");
                                Serial.println(fs_info.maxOpenFiles);
                            
                                Serial.print("Max path lenght:  ");
                                Serial.println(fs_info.maxPathLength);
                            
                                Serial.println();
                            
                                // Open dir folder
                                Dir dir = LittleFS.openDir("/");
                                // Cycle all the content
                                while (dir.next()) {
                                    // get filename
                                    Serial.print(dir.fileName());
                                    Serial.print(" - ");
                                    // If element have a size display It else write 0
                                    if(dir.fileSize()) {
                                        File f = dir.openFile("r");
                                        Serial.println(f.size());
                                        f.close();
                                    }else{
                                        Serial.println("0");
                                    }
                                }
#endif     
	static String last_modified_s = LongTimeText(DstTime::GetBuildTime()).buffer;
    const char* last_modified = last_modified_s.c_str();

    LOGGER << "Date-Modified set to " << last_modified << NL;

    //AsyncStaticWebHandler* handler;

#if _USE_SPIFFS
    // ARDUINO IDE EspTimer.spiffs.bin FILE LOCATION:
    // C:\Users\yochai.glauber\AppData\Local\Temp\arduino_build_950057\EspTimer.spiffs.bin
    // C:\Users\yochai.glauber\Documents\Arduino\VscProjects\EspTimer\EspTimer\.pio\build\d1_mini\firmware.bin
    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;
        //SendInfo("INDEX.HTML requested", *request);
        request->send(LittleFS, "/index.html", String(), false, processor);
        });

    server.on("----------------/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;
        //SendInfo("INDEX.HTML requested", *request);
        request->send(LittleFS, "/style.css", String(), false, [](const String& var) { return var; });
        });

    //server.serveStatic("/style.css",    SPIFFS,     "/style.css")   .setLastModified(last_modified);
    //server.serveStatic("/Clock.gif",    SPIFFS,     "/Clock.gif")   .setLastModified(last_modified);
    //server.serveStatic("/icon.gif",     SPIFFS,     "/icon.gif")    .setLastModified(last_modified);
#endif //_USE_SPIFFS

    server.on("/h/data", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;
        char response[256];

        sprintf(response, "%ld;%ld;%ld",
                GetOnSeconds(),
                GetOffSeconds(),
                (long)(settings.default_on_minutes * SECONDS_PER_MINUTE));

        SendText(response, *request);
        });

    server.on("/h/start", HTTP_POST, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;

        uint32_t on  = 0,
                 off = 0;

        bool on_used  = GetUnsignedLongParam(*request, "on", on);    
        bool off_used = GetUnsignedLongParam(*request, "off",   off);

        LOGGER << "on=" << on << ", off=" << off << NL;

        if(on_used && off_used)
        {
            CancelRelayOnTimer(); 
            CancelRelayOffTimer(); 

            if(on)  SetRelayOnTimerSeconds(on);
            else    SetRelayStatus(true);
            
            SetRelayOffTimerSeconds(off);
        }});

    server.on("/h/stop", HTTP_POST, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;
        cancel();
        });

    settings.InitializeWebServices(server);

    server.on("/m/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;
        SendInfo(get_relay_status().c_str(), *request);
        });

    server.on("/m/start", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;

        #define set_max(val, max_val)   if(val > max_val)   val = max_val; else

        uint32_t on_minutes  = 0,
                 off_minutes = 0;

        bool on_minutes_used  = GetUnsignedLongParam(*request, "delay", on_minutes);    set_max(on_minutes,  settings.max_on_delay_minutes);
        bool off_minutes_used = GetUnsignedLongParam(*request, "off",   off_minutes);   set_max(off_minutes, settings.max_off_delay_minutes);

        if(!off_minutes_used)
        {
            off_minutes_used = true;
            off_minutes      = settings.default_on_minutes;
        }

        CancelRelayOnTimer(); 

//LOGGER << "on_minutes=" << on_minutes << ", off_minutes=" << off_minutes << NL;
//LOGGER << "on_minutes_used=" << on_minutes_used << ", off_minutes_used=" << off_minutes_used << NL;

        if(on_minutes_used && on_minutes)       SetRelayOnTimer(on_minutes);
        else                                    SetRelayStatus(true);

        if(off_minutes_used && off_minutes)     SetRelayOffTimer(on_minutes + off_minutes);
        else                                    CancelRelayOffTimer();

        SendInfo(get_relay_status().c_str(), *request);
        });

    server.on("/m/stop", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;

        uint32_t off_minutes = 0;

        bool off_minutes_used = GetUnsignedLongParam(*request, "delay", off_minutes);   set_max(off_minutes, settings.max_off_delay_minutes);

        CancelRelayOffTimer();

        if(off_minutes_used && off_minutes)     SetRelayOnTimer(off_minutes);
                                                SetRelayStatus(false);

        SendInfo(get_relay_status().c_str(), *request);
        });

    server.on("/m/cancel", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;
        cancel();

        SendInfo(get_relay_status().c_str(), *request);
        });

    int GetWiFiIndex();

    server.on("/m/wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;

        String arg;
        Times  times;

        if (GetStringParam(*request, "1",  arg))
        {
            set_wifi(1, arg);
        }
        if (GetStringParam(*request, "2",  arg))
        {
            set_wifi(2, arg);
        }
        if (GetStringParam(*request, "3",  arg))
        {
            set_wifi(3, arg);
        }
        bool use;
        if (GetBoolParam(*request, "use",  use))
        {
            settings.use_WIFI = use;
            settings.Store();
        }

        int WIFI_idx = GetWiFiIndex();
        const char* connected = (WIFI_idx < 0 || WIFI_idx >= countof(settings.WIFI)) ? "?" : settings.WIFI[WIFI_idx].SSID();

        String text = "use=";
        text = text + ONOFF(settings.use_WIFI).Get() + "\n";

        for(int idx = 0; idx < countof(settings.WIFI); idx++)
        {
            int WIFI_idx = settings.WIFI_order[idx] - '1';

            if(WIFI_idx < 0 || WIFI_idx >= countof(settings.WIFI))
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

        SendInfo(text.c_str(), *request);
        });

    server.on("/m/wifi_order", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;
        String arg;
        if (GetStringParam(*request, "set",  arg))
        {
            const char* order = arg.c_str();
            if(!*order)
                order = "321";

            bool OK = (0 != *order);

            for(int idx = 0; OK && order[idx]; idx++)
            {
                int order_idx = order[idx] - '0';
                OK = order_idx >= 1 && order_idx <= countof(settings.WIFI);
            }

            if(OK)
            {
                strncpy(settings.WIFI_order, order, countof(settings.WIFI));
                settings.Store();
            }
        }

        SendInfo(settings.WIFI_order, *request);
        });

   server.on("/m/led", HTTP_GET, [](AsyncWebServerRequest *request) {
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

    server.on("/m/temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;
        SendInfo((String(GetTemperature(), 1) + "C").c_str(), *request);
        });

    server.on("/m/version", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;
        SendInfo(String((int)VERSION).c_str(), *request);
        });

    server.on("/m/build", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;
        SendInfo(DstTime::GetBuildTime().ToText(), *request);
        });

    server.on("/m/restart", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;
        Html::h1 root("Restarting in 10 seconds...");
        SendElement(root, *request);
        ScheduleRestart(10);
        });

    server.onNotFound([](AsyncWebServerRequest *request) {
        if (request->url() == "/favicon.ico")    return;

        if(LittleFS.exists(request->url()))
        {
            request->send(LittleFS, request->url(), String(), false, processor);
            return;
        }
        
        LOGGER << "URL NOT FOUND: " << request->url() << NL;
        SendError("Page not found", *request, 400);
        //request->send_P(400, "text/plain", );
        });

    // Start server

    AsyncElegantOTA.begin(&server);    // Start ElegantOTA
    server.begin();
}
//--------------------------------------------------------------------
String GetElementValue(const char* field, const String& value)
{
    char retval[64];
    sprintf(retval, "%s = %s", field, value.c_str());
    return retval;
}
//------------------------------------------------------
Html::Element& CreateField(const char* field, const String& value, Html::TextGeneratorCtx& ctx)
{
    Html::Element& retval = *new Html::h3(GetElementValue(field, value).c_str());
    retval.AddAttribute(FieldIndentAttribute);
    return retval;
}
//------------------------------------------------------
void SendInstantHtml(AsyncWebServerRequest& request, Html::Element& e)
{
    Html::html html;
    //    html.SetTitle(#func_id);
    html.SetMeta();
    html.AddIcon("icon.gif", "image/gif", "32x32");
    html.AddStyleSheet("style.css");
    html.Body().AddChild(e);
    SendElement(html, request);
}
//------------------------------------------------------
