#pragma once
#include <ESP8266WebServer.h>
#include "Progmem.h"
#include "Estore.h"
class WebInterface
{
private:
	Estore* _estore;
	ESP8266WebServer* _webserver;

public:
	WebInterface(Estore* estore, ESP8266WebServer* web)
	{
		this->_estore = estore;
		this->_webserver = web;
	}

	void HandleSetupRoot()
	{
		char setupoutputbuffer[sizeof(HTML_HEADER_SETUP) + sizeof(HTML_SSID) + 5];
		strcpy_P(setupoutputbuffer, HTML_HEADER_SETUP);
		strcat_P(setupoutputbuffer, HTML_SSID);
		this->_webserver->send(200, "text/html", setupoutputbuffer);
	}

	void HandleFormat()
	{

	}

	void handleSetupSSID()
	{
		char setupoutputbuffer[sizeof(HTML_HEADER_SETUP) + sizeof(HTML_SSIDOK) + 5];
		String a = this->_webserver->arg("ssid");
		String b = this->_webserver->arg("password");
		String c = this->_webserver->arg("format");
		String d = this->_webserver->arg("homebridgeusername");
		String e = this->_webserver->arg("homebridgepassword");
		bool format = false;
		if (c.compareTo("format") == 0)
		{
			format = true;
		}

		strcpy(this->_estore->ssid, a.c_str());
		strcpy(this->_estore->password, b.c_str());
		strcpy(this->_estore->homebridgeUsername, d.c_str());
		strcpy(this->_estore->homebrdigePassword, e.c_str());
		
		this->_estore->wifiSave(format);

		strcpy_P(setupoutputbuffer, HTML_HEADER_SETUP);
		strcat_P(setupoutputbuffer, HTML_SSIDOK);
		this->_webserver->send(200, "text/html", setupoutputbuffer);
	}
};
