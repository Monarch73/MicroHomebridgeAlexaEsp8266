/*
 Name:		HomebridgeAlexaEsp8266.ino
 Created:	11/17/2018 12:43:58 PM
 Author:	niels
*/

// the setup function runs once when you press reset or power the board
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

const char* mqtt_server = "homebridge.cloudwatch.net";
//const char* mqtt_server = "192.168.1.193";
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
	web = new ESP8266WebServer(80);
	ui = new WebInterface(estore, web);
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

void message(String& message)
{
	char*liste[] = {
		(char*)"Waffel",
		(char*)"Hamburger",
		(char *)0};
	int pos;
	char *discovery = (char*)"\"name\":\"Discover\",\"payloadVersion\":\"3\",\"messageId\":\"";//(char*)"\"messageId\":\"";
	char *turn = (char*)":\"Alexa.PowerController\",\"name\":\"Turn";
	Serial.println("Message arrived:");
	Serial.println(message.c_str());
	if ((pos = message.indexOf(discovery)) > 0)
	{
		Serial.println("discover response");
		String msgId = message.substring(pos + strlen(discovery), pos + strlen(discovery) + 36);
		Serial.println(msgId);
		mqtt->sendDiscoveryResponse((char*)msgId.c_str(), liste);
	}
	else if ((pos = message.indexOf(turn)) > 0)
	{
		Serial.println("turn xy response");
	}
}

void setup() {
	Serial.begin(115200);
	int zahl = analogRead(A0);
	Serial.println(zahl);
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
	//mqtt = new MiniMqttClient((char *)mqtt_server, mqtt_port, (char *)mqtt_clientId, (char *)mqtt_username, (char *)mqtt_password);
	mqtt = new MiniMqttClient((char *)mqtt_server, mqtt_port, (char *)estore->homebridgeUsername, (char *)estore->homebridgeUsername, estore->homebrdigePassword);
	mqtt->setCallback(message);
}

void loop() {
	delay(10);
	if (mqtt != 0)
	{
		mqtt->loop();
	}

	if (web != 0)
	{
		web->handleClient();
	}
}
