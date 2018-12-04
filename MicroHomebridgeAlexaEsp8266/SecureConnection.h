#pragma once
#include <FS.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

#define HOST_NAME "homebridge.cloudwatch.net"
#define PORT 8883

class SecureConnection{
private:
	WiFiClientSecure* _wifiClient;
	
public:
	SecureConnection()
	{
		_wifiClient = new WiFiClientSecure();
	}

	~SecureConnection()
	{
		_wifiClient->stopAll();
	}

	void connect()
	{
		File CACertFile = SPIFFS.open("/CA.der.crt", "r");
		File certFile = SPIFFS.open("/private.der.crt", "r");
		File pkFile = SPIFFS.open("/private.der.key", "r");

		if (!this->_wifiClient->loadCertificate(certFile)) Serial.println("certFile failed!");
		if (!this->_wifiClient->loadCACert(CACertFile)) Serial.println("CACert failed!");
		if (!this->_wifiClient->loadPrivateKey(pkFile)) Serial.println("pkFile failed!");

		this->_wifiClient->connect(HOST_NAME, PORT);
		if (this->_wifiClient->connected())
		{
			Serial.println("Established secure connection to host.");
		}
		else {
			Serial.println("Secure connection to host failed!");
		}
	}
};