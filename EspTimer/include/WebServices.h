#pragma once

#include <ESPAsyncWebServer.h>
#include "AsyncWebServerEx.h"
#include "Html.h"

void InitializeWebServices();

void SendInfo(const char* s, AsyncWebServerRequest& request);
void SendError(const char* s, AsyncWebServerRequest& request, unsigned int code = 200);

String GetElementValue(const char* field, const String& value);
Html::Element& CreateField(const char* field, const String& value, Html::TextGeneratorCtx& ctx);

void SendInstantHtml(AsyncWebServerRequest& request, Html::Element& e);

#define _IndentAttribute(mul, add)  Html::StyleAttribute::Indent(mul * (ctx.depth + add))
#define _BackgroundImageAttribute()  Html::StyleAttribute::BackgroundImage("Tris_1.png")
#define FieldIndentAttribute  _IndentAttribute(2, 1)
#define RootIndentAttribute   _IndentAttribute(1, 0)

#define SetRoot(name, url)    \
    Html::Element& _root = *new Html::Paragraph();\
    Html::Element* proot;\
    const char* element_name = #name ":";\
    if (ctx.depth)    {\
        Html::h1& h1 = *new Html::h1();\
        h1.AddAttribute(RootIndentAttribute);\
        _root.AddChild(h1);\
        h1.AddChild(*new Html::Anchor(element_name, url)); \
        proot = &_root; }\
    else { \
        _root.AddChild(*new Html::h1(element_name)); \
        Html::Div & div = *new Html::Div("english");\
        _root.AddChild(div);\
        proot = &div; }\
    Html::Element& root = *proot


#define SendHtml(func_id)  \
    Html::TextGeneratorCtx ctx;\
    Html::Element& e = get##func_id(ctx);\
    SendInstantHtml(*request, e)

const char* GetYesNo(const bool& b);

#define GET_NUMERIC_PARAM(type, fld, min, max, val, parent_fld)    if (Get##type##Param(*request, #fld, min, max, true, val))   { temp.parent_fld.fld = val; } else;

#define GET_UNSIGNED_BYTE_PARAM(fld, min, max, val, parent_fld)     GET_NUMERIC_PARAM(UnsignedByte, fld, min, max, val, parent_fld)
#define GET_DOUBLE_PARAM(fld, min, max, val, parent_fld)            GET_NUMERIC_PARAM(Double, fld, min, max, val, parent_fld)
#define GET_BOOL_PARAM(fld, val, parent_fld)                        if (GetBoolParam(*request, #fld, val))      temp.parent_fld.fld = val; else;

bool SetActionDisabled(const String& var, String& action_disabled_reason);
#define CheckActionDisabled(var)    \
    String action_disabled_reason;\
    if (SetActionDisabled(var, action_disabled_reason))\
        return action_disabled_reason;;
