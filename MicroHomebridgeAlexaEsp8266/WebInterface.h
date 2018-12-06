#pragma once
#include <ESP8266WebServer.h>
#include "Progmem.h"
#include "Estore.h"
#include "WcFnRequestHandler.h"

using namespace std;
class WebInterface
{
private:
	Estore* _estore;
	ESP8266WebServer* _webserver;
	volatile char *_urlToCall = NULL;
	std::function<void(dipswitch*,int,bool)> _callBackSwitch;

	int addStringToMemory(char *buf, char *txt)
	{
		int pos2=0;
		memcpy(buf, txt, (pos2=strlen(txt)+1));
		return pos2;
	}

	void parseStringNumbers(String& data, uint16_t*numbers)
	{
		dipswitch dp;

		Serial.println("parsing " + data);

		int currentNumber=0;
		char *myData = (char *)data.c_str();
		char *currentBlock = strtok(myData, ",");
		while (currentBlock != NULL && currentNumber<(int)((sizeof(dp.irDataOn)/sizeof(uint16_t))-1))
		{
			ulong number = atol(currentBlock);
			if (number >= 0 && number < 65536)
			{
				Serial.print(number);
				numbers[currentNumber++] = number;
			}

			currentBlock = strtok(NULL, ",");
		}
		numbers[currentNumber] = 0xc1a0;
		Serial.println(" done");
	}

public:
	WebInterface(Estore* estore, ESP8266WebServer* web, std::function<void(dipswitch*,int,bool)> callbackSwitch)
	{
		this->_estore = estore;
		this->_webserver = web;
		this->_urlToCall = 0;
		this->_callBackSwitch = callbackSwitch;
	}

	void HandleNotFound()
	{
		String uri = this->_webserver->uri();
		String filename = "";
		char *type = (char *)"text/javascript";
		Serial.println("Handle not found");
		Serial.println(uri);
		_webserver->sendHeader("Cache-Control", " max-age=120");
		_webserver->sendHeader("Last-Modified", "Wed, 21 Oct 2019 07:28:00 GMT");

		if (_webserver->hasHeader("If-Modified-Since"))
		{
			_webserver->send(304, "");
			return;
		}

		if (uri.startsWith("/styles")) 
		{
			filename = "/styles.css";
			type = (char *)"text/css";
		}
		else if (uri.startsWith("/inline"))
		{
			filename = "/inline.js";
		}
		else if (uri.startsWith("/poly"))
		{
			filename = "/polyfills.js";
		}
		else if (uri.startsWith("/main"))
		{
			filename = "/main.js";
		}
		else
		{
			this->_webserver->send(404, "text/plain", "404: Not found");
			return;
		}

		String pathWithGz = filename + ".gz";
		if (SPIFFS.exists(pathWithGz)) filename = pathWithGz;
		File fs = SPIFFS.open(filename, "r");
		if (!fs)
		{
			this->_webserver->send(404, "text/plain", "404: Not found");

		}
		else
		{
			_webserver->streamFile(fs, type);
		}

		fs.close();
	}

	void HandleAngular(WcFnRequestHandler *handler, String requestUri, HTTPMethod method) 
	{

		Serial.println(requestUri);
		_webserver->sendHeader("Cache-Control", " max-age=120");
		_webserver->sendHeader("Last-Modified", "Wed, 21 Oct 2030 07:28:00 GMT");

		if (_webserver->hasHeader("If-Modified-Since"))
		{
			_webserver->send(304, "");
			return;
		}

		if (requestUri.endsWith("/")) requestUri += "index.html";
		String pathWithGz = requestUri + ".gz";
		if (SPIFFS.exists(pathWithGz)) requestUri = pathWithGz;
		Serial.println(requestUri);
		File fs = SPIFFS.open(requestUri.c_str(), "r");
		if (!fs)
		{
			this->_webserver->send(404, "text/plain", "404: Not found");
		}
		else
		{
			Serial.println("serving spiffs");
			//_webserver->sendHeader("Content-Encoding", "gzip");
			_webserver->streamFile(fs, "text/html");
			fs.close();
		}

		//_webserver->streamFile()
	}

	void HandleJsonList()
	{
		typedef struct dipswitches_struct dipswitch;
		dipswitch dp;
		char *responseContent = (char*)malloc(1024);
		if (responseContent == NULL)
		{
			Serial.println("out of memory");
			return;
		}

		strcpy(responseContent, "[ ");

		bool entry = false;
		for (int i = 0; i < N_DIPSWITCHES; i++)
		{
			this->_estore->dipSwitchLoad(i, &dp);
			if (dp.name[0] != 0)
			{
				char buf[49];
				entry = true;
				sprintf(buf, " { \"i\": %d, \"n\": \"%s\" },", i, dp.name);
				strcat(responseContent, buf);
			}
		}
		if (entry)
		{
			responseContent[strlen(responseContent) - 1] = 0;
		}

		strcat(responseContent, " ]");
		_webserver->sendHeader("Access-Control-Allow-Origin", "*");
		_webserver->send(200, "application/json", responseContent);
		free(responseContent);
	}

	void HandleEStore()
	{
		typedef struct dipswitches_struct dipswitch;
		dipswitch dp;
		String a = _webserver->arg("name");
		String b = _webserver->arg("house");
		String c = _webserver->arg("code");
		String c2 = _webserver->arg("tri1");
		String c3 = _webserver->arg("tri2");
		String d1 = _webserver->arg("url1");
		String d2 = _webserver->arg("url2");
		String hz = _webserver->arg("irhz");
		String dataOn = _webserver->arg("iron");
		String dataOff = _webserver->arg("iroff");

		Serial.println("storing");
		Serial.println(a);
		Serial.println(b);
		Serial.println(c);
		Serial.println(c2);
		Serial.println(c3);
		Serial.println(d1);
		Serial.println(d2);

		int no = this->_estore->dipSwitchFindFree();
		if (no >= 0 && _webserver->method() == HTTP_POST)
		{
			memcpy(dp.name, (char *)a.c_str(), sizeof(dp.name));
			memcpy(dp.housecode, (char *)b.c_str(), sizeof(dp.housecode));
			memcpy(dp.code, (char *)c.c_str(), sizeof(dp.code));
			memcpy(dp.tri1, (char*)c2.c_str(), sizeof(dp.tri1));
			memcpy(dp.tri2, (char *)c3.c_str(), sizeof(dp.tri2));
			memcpy(dp.urlOn, (char *)d1.c_str(), sizeof(dp.urlOn));
			memcpy(dp.urlOff, (char *)d2.c_str(), sizeof(dp.urlOff));
			dp.name[sizeof(dp.name) - 1] = 0;
			dp.housecode[sizeof(dp.housecode) - 1] = 0;
			dp.code[sizeof(dp.code) - 1] = 0;
			dp.tri1[sizeof(dp.tri1) - 1] = 0;
			dp.tri2[sizeof(dp.tri2) - 1] = 0;
			dp.urlOff[sizeof(dp.urlOff) - 1] = 0;
			dp.urlOn[sizeof(dp.urlOn) - 1] = 0;
			if (hz == "" || atol(hz.c_str())==0)
			{
				dp.irhz = 0;
				dp.irDataOn[0] = 0xc1a0;
			}
			else
			{
				dp.irhz = (uint16_t)atol(hz.c_str());
				parseStringNumbers(dataOn, (uint16_t *) &dp.irDataOn);
				parseStringNumbers(dataOff, (uint16_t *)&dp.irDataOff);
			}
			
			this->_estore->dipSwitchSave(no, &dp);
		}

		_webserver->send(200, "");
	}


	void HandleSetupRoot()
	{
		char setupoutputbuffer[sizeof(HTML_HEADER_SETUP) + sizeof(HTML_SSID) + 5];
		strcpy_P(setupoutputbuffer, HTML_HEADER_SETUP);
		strcat_P(setupoutputbuffer, HTML_SSID);
		this->_webserver->send(200, "text/html", setupoutputbuffer);
	}

	void HandleEDelete()
	{
		String a = _webserver->arg("no");
		if (a != "")
		{
			int no = atoi(a.c_str());
			if (no < N_DIPSWITCHES)
			{
				Serial.println(no);
				this->_estore->dipSwitchDelete(no);
			}
		}
		_webserver->send(200, "");
	}
	
	void HandleESocket()
	{
		typedef struct dipswitches_struct dipswitch;
		dipswitch dp;

		String argNO = _webserver->arg("no");
		String argOnOff = _webserver->arg("sw");
		if (argNO != "" && argOnOff!="")
		{
			int nummer = atoi(argNO.c_str());
			int onoff = atoi(argOnOff.c_str());
			int currentLight = 0;
			for (int i = 0; i < N_DIPSWITCHES; i++)
			{
				this->_estore->dipSwitchLoad(i, &dp);
				if (dp.name[0] != 0)
				{
					if (currentLight == nummer)
					{
						if (onoff == 1)
						{
							this->_callBackSwitch(&dp, nummer,true);
							break;
						}
						else
						{
							this->_callBackSwitch(&dp, nummer, false);
							break;
						}
					}
					currentLight++;
				}
			}
		}
		_webserver->send(200, "");
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

	void SetUrlToCall(char * urlToCall)
	{
		this->_urlToCall = urlToCall;
	}

	volatile char *GetUrlToCall() 
	{
		return _urlToCall;
	}
};


