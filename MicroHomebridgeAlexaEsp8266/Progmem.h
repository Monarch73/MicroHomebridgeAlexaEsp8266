#pragma once
#ifndef PROGMEM_H
#define PROGMEM_H

#include <pgmspace.h>

char HTML_HEADER_SETUP[] PROGMEM = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\"><title>Homebridge</title>"
"</head><body>";


char HTML_SSID[] PROGMEM =
"<h1>Alexa Homebridge SETUP</h1>"
"<p><form method=\"POST\" action=\"/setup\">SSID:<input type=\"TEXT\" name=\"ssid\" /> WIFI-Password:<input type=\"TEXT\" name=\"password\" />Homebridge-Username:<input type=\"TEXT\" name=\"homebridgeusername\" />Homebridge-Password:<input type=\"TEXT\" name=\"homebridgepassword\" /><input type=\"checkbox\" id=\"format\" name=\"format\" value=\"format\" /><label for=\"format\">initialize (format) Storage</lable><input type=\"submit\" name=\"send\" value=\"save\" /></form></p>"
"</body>";

char HTML_SSIDOK[] PROGMEM =
"<h1>Configuration OK - Device restarting in a couple of seconds.</h1>"
"</body>";


#endif // PROHMEM_H
