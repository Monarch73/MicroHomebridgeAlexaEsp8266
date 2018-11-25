/*
 Name:		HomebridgeAlexaEsp8266.ino
 Created:	11/17/2018 12:43:58 PM
 Author:	niels
*/

// the setup function runs once when you press reset or power the board
#include <ESP8266HTTPClient.h>
#include <ir_Trotec.h>
#include <ir_Toshiba.h>
#include <ir_Samsung.h>
#include <ir_Panasonic.h>
#include <ir_NEC.h>
#include <ir_Mitsubishi.h>
#include <ir_Midea.h>
#include <ir_Magiquest.h>
#include <ir_LG.h>
#include <ir_Kelvinator.h>
#include <ir_Haier.h>
#include <ir_Gree.h>
#include <ir_Fujitsu.h>
#include <ir_Daikin.h>
#include <ir_Coolix.h>
#include <ir_Argo.h>
#include <IRutils.h>
#include <IRtimer.h>
#include <IRsend.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <RCSwitch.h>
#include <tcp_axtls.h>
#include <SyncClient.h>
#include <ESPAsyncTCPbuffer.h>
#include <ESPAsyncTCP.h>
#include <async_config.h>
#include <AsyncPrinter.h>
#include <EEPROM.h>
#include <ESP8266WebServerSecureBearSSL.h>
#include <ESP8266WebServerSecureAxTLS.h>
#include <ESP8266WebServerSecure.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include "MiniMqttClient.h"
#include "WebInterface.h"
#include "Estore.h"
#include "WcFnRequestHandler.h"
#include "RemoteControl.h"
#include "FtpServ.h"
#include "StrFunc.h"

//const char* mqtt_server = "homebridge.cloudwatch.net";
const char* mqtt_server = "192.168.1.136";
const int   mqtt_port = 1883;
const char* mqtt_topic = "#";

//MQTTClient client;
long lastMsg = 0;
char msg[50];
int value = 0;
MiniMqttClient* mqtt=0;
Estore* estore;
ESP8266WebServer* web = 0;
WebInterface* ui = 0;
RemoteControl* remote = 0;
RCSwitch mySwitch = RCSwitch();
IRsend *myIr;
FtpServ *ftp;


typedef std::function<void(WcFnRequestHandler *, String, HTTPMethod)> HandlerFunction;
unsigned int lastDiscoveryResponse =0;

void on(HandlerFunction fn, const String &wcUri, HTTPMethod method, char wildcard = '*') {
	web->addHandler(new WcFnRequestHandler(fn, wcUri, method, wildcard));
}

void switchCallBack(dipswitch dp, int number, bool on)
{
	if (remote !=0 )
	{
		remote->Send(&dp,number, on);
	}
}

void WrapperHandleAngular(WcFnRequestHandler *handler, String requestUri, HTTPMethod method)
{
	if (ui != 0)
	{
		ui->HandleAngular(handler, requestUri, method);
	}
}

void WrapperSetupRoot()
{
	if (ui!=0)
	{
		ui->HandleSetupRoot();
	}
}

void WrapperSetupSSID()
{
	if (ui != 0)
	{
		ui->handleSetupSSID();
	}
}

void WrapperHandleJsonList()
{
	if (ui != 0)
	{
		ui->HandleJsonList();
	}
}

void WrapperHandleEStore()
{
	if (ui != 0)
	{
		ui->HandleEStore();
	}
}

void WrapperHandleEDelete()
{
	if (ui != 0)
	{
		ui->HandleEDelete();
	}
}

void WarpperHandleESocket()
{
	if (ui != 0)
	{
		ui->HandleESocket();
	}
}

void WrapperNotFound()
{
	if (ui != 0)
	{
		ui->HandleNotFound();
	}
}

void EnterApMode()
{
	WiFi.mode(WIFI_AP);
	Serial.println("WiFi Failed.Entering setup mode.");
	Serial.println("No ssid defined");
	/* You can remove the password parameter if you want the AP to be open. */
	WiFi.softAP("EasyAlexa");
	IPAddress myIP = WiFi.softAPIP();
	Serial.print("AP IP address: ");
	Serial.println(myIP);
	ftp = new FtpServ((char *)"",(char *)"");
	ftp->begin();
	web = new ESP8266WebServer(80);
	ui = new WebInterface(estore, web, switchCallBack);
	remote = new RemoteControl((RCSwitch*)&mySwitch,myIr,(WebInterface *)ui);
	web->on("/", HTTP_GET, WrapperSetupRoot);
	web->on("/setup", HTTP_POST, WrapperSetupSSID);
	web->begin();
}

void messageReceived(String &topic, String &payload) {
	Serial.println("incoming: " + topic + " - " + payload);
}
void setup_wifi() {
	delay(10);
	// We start by connecting to a WiFi network
	Serial.println();
	Serial.print("Connecting to ");
	Serial.println(estore->ssid);
	WiFi.mode(WIFI_STA);
	WiFi.begin(estore->ssid, estore->password);

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}

	Serial.println("");
	Serial.println("WiFi connected");
	Serial.println("IP address: ");
	Serial.println(WiFi.localIP());
}

void connect() {
	Serial.print("checking wifi...");
	while (WiFi.status() != WL_CONNECTED) {
		Serial.print(".");
		delay(1000);
	}
}

char *getMessageStampDup(char *buffer, int len)
{
	char *messageId = (char*)"\"messageId\":\"";
	char *endCorrelationId = (char*)"=\"}";
	char *startMessageId = StrFunc::indexOf(buffer, messageId, len);
	if (!startMessageId)
	{
		Serial.println("FEHLER: Messageid not found");
		return 0;
	}
	Serial.println((int)(startMessageId-buffer));
	Serial.println((buffer + len) - startMessageId);
	char *endCorrelationIdPos = StrFunc::indexOf(startMessageId, endCorrelationId, (buffer + len) - startMessageId);
	if (!endCorrelationIdPos)
	{
		MiniMqttClient::hexdump(buffer, len);
		Serial.printf("FEHLER: ende nicht gefunden");
		while (true);
	}
	Serial.println(system_get_free_heap_size());
	char *messageStamp = StrFunc::substrdup(startMessageId, (endCorrelationIdPos - startMessageId) + 2);
	return messageStamp;
}

int getNumber(char *buffer, int len)
{
	char *endPointNeedle = (char*)"{\"endpointId\":\"";
	char *endPointStart = StrFunc::indexOf(buffer, endPointNeedle, len);
	if (!endPointStart)
	{
		Serial.println("Fehler: endpointId nicht gefunden");
		return 0;
	}
	char *endPointIdEnd = StrFunc::indexOf(endPointStart, (char*)"\",\"", (buffer + len) - endPointStart);
	if (!endPointIdEnd)
	{
		Serial.println("Fehler: ende von endpointId nicht gefunden");
		return 0;
	}
	int number = 0;
	endPointIdEnd -= 2;
	number = (endPointIdEnd[0] - 48) * 10;
	number += endPointIdEnd[1] - 48;
	return number;
}

void message(char *buffer, int len)
{

	char *posbuf;
	dipswitch dp;
	char *discovery = (char*)"\"name\":\"Discover\",\"payloadVersion\":\"3\",\"messageId\":\"";
	char *turn = (char*)":\"Alexa.PowerController\",\"name\":\"Turn";
	char *reportState = (char*)"\"namespace\":\"Alexa\",\"name\":\"ReportState\"";

	if ((posbuf = StrFunc::indexOf(buffer, discovery, len)) > 0)
	{
		if (millis() - lastDiscoveryResponse > 10000)
		{
			estore->RefreshList();

			Serial.println("discover response. Number of switches:");
			Serial.println(Estore::deviceList.size());
			char *msgId = StrFunc::substrdup(posbuf + strlen(discovery), 36);
			Serial.println(msgId);
			mqtt->sendDiscoveryResponse(msgId, Estore::deviceList);
			free(msgId);
			lastDiscoveryResponse = millis();
		}
		else
		{
			Serial.println("Ignoring discovery...");
		}

	}
	else if ((posbuf = StrFunc::indexOf(buffer, turn, len)) > 0)
	{
		Serial.println("turn xy response");
		estore->RefreshList();
		char *onoffpos = posbuf + strlen(turn);
		bool onoff = (onoffpos[0] == 'O' && onoffpos[1] == 'n');

		int number = getNumber(buffer,len);
		estore->dipSwitchLoad(number, &dp);
		remote->Send(&dp, number, onoff);
		Serial.print("Number ");
		Serial.print(number);
		Serial.print(" is turned ");
		Serial.println(onoff ? "on" : "off");

		char *messageStamp = getMessageStampDup(buffer, len);
		if (messageStamp)
		{
			mqtt->sendSwitchResponse(messageStamp, onoff, number);
			free(messageStamp);
		}
	}
	else if ((posbuf = StrFunc::indexOf(buffer, reportState, len)) > 0)
	{
		int	number = getNumber(buffer, len);
		bool powerstate = remote->getPowerState(number);
		Serial.print(number);
		Serial.print(" is ");
		Serial.println(powerstate ? "On" : "Off");
		char *messageStamp = getMessageStampDup(buffer, len);
		if (messageStamp)
		{
			mqtt->sendPowerStateResponse(messageStamp, powerstate, number);
			free(messageStamp);
		}
	}
}

void setup() {
	Serial.begin(115200);
	int zahl = analogRead(A0);
	Serial.println(zahl);
	myIr = new IRsend(3);
	myIr->begin();
	mySwitch.enableTransmit(2);
	estore = new Estore(); // &estore2;
	if (zahl > 350)
	{
		estore->setupEeprom(true);
	}
	else
	{
		estore->setupEeprom();
	}
	
	if (estore->ssid[0] == 0)
	{
		EnterApMode();
		return;
	}
	Serial.println("MyData:");
	Serial.println(estore->ssid);
	Serial.println(estore->password);
	Serial.println(estore->homebridgeUsername);
	Serial.println(estore->homebrdigePassword);
	setup_wifi();
	mqtt = new MiniMqttClient((char *)mqtt_server, mqtt_port, (char *)estore->homebridgeUsername, (char *)estore->homebridgeUsername, estore->homebrdigePassword);
	mqtt->setCallback(message);
	ftp = new FtpServ((char *)"", (char *)"");
	ftp->begin();
	web = new ESP8266WebServer(80);
	ui = new WebInterface(estore, web, switchCallBack);
	remote = new RemoteControl((RCSwitch*)&mySwitch,myIr,(WebInterface *)ui);
	on(WrapperHandleAngular,"/", HTTP_ANY);
	web->on("/jsonList", HTTP_GET, WrapperHandleJsonList);
	web->on("/estore", HTTP_POST , WrapperHandleEStore);
	web->on("/edelete", HTTP_GET, WrapperHandleEDelete);
	web->on("/esocket", HTTP_GET, WarpperHandleESocket);
	web->onNotFound(WrapperNotFound);
	const char * headerkeys[] = { "User-Agent","Cookie", "If-Modified-Since" };
	size_t headerkeyssize = sizeof(headerkeys) / sizeof(char*);
	//ask server to track these headers
	web->collectHeaders(headerkeys, headerkeyssize);
	web->begin();
}

void loop() {
	volatile char *urlToCall2;

	delay(10);
	if (mqtt != 0)
	{
		mqtt->loop();
	}

	if (web != 0)
	{
		web->handleClient();
	}

	if ((urlToCall2 = ui->GetUrlToCall()) != NULL)
	{
		Serial.print("Calling ");
		Serial.print((char *)urlToCall2);
		HTTPClient http;
		bool ret = http.begin((char *)urlToCall2);
		Serial.println(ret);
		int ret2 = http.GET();
		Serial.print(" ");
		Serial.println(ret2);
		http.end();
		ui->SetUrlToCall(NULL);
		free((void *)urlToCall2);
	}
}
