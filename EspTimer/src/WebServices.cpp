#include "main.h"
#include "WebServices.h"

#include "Location.h"
#include "Sun.h"

#include <AsyncElegantOTA.h>
#include <LittleFS.h>

AsyncWebServer server(80);
static void checkFS();

static String processor(const String& var) {
    if (var == "TEMPERATURE")
        return String(GetTemperature(), 1) + "°";

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

namespace NYG
{
	extern bool gbl_DST;
}

void InitializeWebServices()
{
    // Initialize SPIFFS
    if (!LittleFS.begin()) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    checkFS();
    
    File file = LittleFS.open("index.html", "r");
    if(!file)
    {
        Serial.println("***************** Failed to open file for reading ****************************");
        //return;
    }

	static String last_modified_s = LongTimeText(DstTime::GetBuildTime()).buffer;
    const char* last_modified = last_modified_s.c_str();

    LOGGER << "Date-Modified set to " << last_modified << NL;

    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;
        request->send(LittleFS, "/index.html", String(), false, processor);
    });
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    //server.serveStatic("/style.css",    SPIFFS,     "/style.css")   .setLastModified(last_modified);
    //server.serveStatic("/Clock.gif",    SPIFFS,     "/Clock.gif")   .setLastModified(last_modified);
    //server.serveStatic("/icon.gif",     SPIFFS,     "/icon.gif")    .setLastModified(last_modified);
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    server.on("/h/now", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;
        uint32_t now;
        bool     dst;
        
        bool now_used  = GetUnsignedLongParam(*request, "now", now);    
        bool dst_used  = GetBoolParam        (*request, "dst", dst);

        char response[256] = { 0 };

        if(now_used && dst_used)
        {
            if(dst)
                now -= SECONDS_PER_HOUR;

            FixTime::Set(FixTime(now));
            gbl_DST = dst;

            FixTime riseTime, setTime;

            if(Sun::GetTodayLocalRiseSetTimes(riseTime, setTime))
            {
                TimeText rt = DstTime(riseTime).ToText(),
                         st = DstTime(setTime).ToText();
                TimeTextFields& rtf = (TimeTextFields&)rt,
                              & stf = (TimeTextFields&)st;

                rtf.time[5] = 0;
                stf.time[5] = 0;

                sprintf(response, "%s;%s;%s;%lu;%lu;%ld",
                        BOOL::Get(settings.night_only, WC_LOWERCASE),
                        rtf.time,
                        stf.time,
                        riseTime.GetSeconds(),
                        setTime.GetSeconds(),
                        (long)(settings.default_on_minutes * SECONDS_PER_MINUTE));

                LOGGER << response << NL;                 
            }

            SendText(response, *request);

        }
    });
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    server.on("/h/data", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;
        char response[256];

        unsigned long id = GetStartId();

        if(NOT_STARTED_ID)
            sprintf(response, "%lu", id);
        else
            sprintf(response, "%lu;%ld;%ld",
                    id,
                    GetOnSeconds(),
                    GetOffSeconds());

        SendText(response, *request);
    });
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    server.on("/h/start", HTTP_POST, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;

        uint32_t on  = 0,
                 off = 0;

        bool on_used  = GetUnsignedLongParam(*request, "on",    on);    
        bool off_used = GetUnsignedLongParam(*request, "off",   off);

        LOGGER << "on=" << on << ", off=" << off << NL;

        if(on_used && off_used)
        {
            CancelRelayOnTimer(); 
            CancelRelayOffTimer(); 

            if(on)  SetRelayOnTimerSeconds(on);
            else    SetRelayStatus(true);
            
            SetRelayOffTimerSeconds(off);
        }
    });
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    server.on("/h/stop", HTTP_POST, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;
        cancel();
    });
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    server.on("/m/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;
        SendInfo(get_relay_status().c_str(), *request);
    });
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
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

        if(on_minutes_used && on_minutes)       SetRelayOnTimer(on_minutes);
        else                                    SetRelayStatus(true);

        if(off_minutes_used && off_minutes)     SetRelayOffTimer(on_minutes + off_minutes);
        else                                    CancelRelayOffTimer();

        SendInfo(get_relay_status().c_str(), *request);
    });
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    server.on("/m/stop", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;

        uint32_t off_minutes = 0;

        bool off_minutes_used = GetUnsignedLongParam(*request, "delay", off_minutes);   set_max(off_minutes, settings.max_off_delay_minutes);

        CancelRelayOffTimer();

        if(off_minutes_used && off_minutes)     SetRelayOnTimer(off_minutes);
                                                SetRelayStatus(false);

        SendInfo(get_relay_status().c_str(), *request);
    });
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    server.on("/m/cancel", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;
        cancel();

        SendInfo(get_relay_status().c_str(), *request);
    });
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    server.on("/m/temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;
        SendInfo((String(GetTemperature(), 1) + "°").c_str(), *request);
    });
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    server.on("/m/version", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;
        SendInfo(String((int)VERSION).c_str(), *request);
    });
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    server.on("/m/build", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;
        SendInfo(DstTime::GetBuildTime().ToText(), *request);
    });
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    server.on("/m/restart", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOGGER << request->url() << NL;
        Html::h1 root("Restarting in 10 seconds...");
        SendElement(root, *request);
        ScheduleRestart(10);
    });
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    settings.InitializeWebServices(server);
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
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
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
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
static void checkFS()
{
    FSInfo fs_info;
    LittleFS.info(fs_info);
 
    Serial.println("File system info:");
 
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
}
//------------------------------------------------------
