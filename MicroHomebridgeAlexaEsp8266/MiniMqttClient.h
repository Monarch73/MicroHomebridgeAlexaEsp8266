#pragma once

#include <string.h>
#include <stdio.h>
#include <tcp_axtls.h>
#include <SyncClient.h>
#include <ESPAsyncTCPbuffer.h>
#include <ESPAsyncTCP.h>
#include <async_config.h>
#include <AsyncPrinter.h>
#include <map>

#define MQTTSTATUS_CONNACK 0x20
#define MQTTSTATUS_SUBSCRIBEACK 0x90
#define MQTTSTATUS_PINGACK 0x0D0
#define MQTTSTATUS_MESSAGE 0x30

enum State { initialzed = 0 , waitingForConnect, waitingForConnectACK, doingnil };
class MiniMqttClient
{
private:
	char *_server;
	int _port;
	char *_clientId;
	char *_username;
	char *_password;
	char *_topic;
	SyncClient* _client;
	char _sendbuffer[2048];
	char _rcvbuffer[2048];
	int _currentState;
	unsigned long pingmillis=0x0fffffff;
	std::function<void(String&)> _onMessage;

	void hexdump(char *txt, int len)
	{
		char buf[10];
		for (int i = 0; i < len; i++)
		{
			sprintf(buf, "%2x ", txt[i]);
			Serial.print(buf);
		}
		
		Serial.println("");
		for (int i = 0; i < len; i++)
		{
			sprintf(buf, "%2c ", txt[i]);
			Serial.print(buf);
		}

		Serial.println("");
	}

	int setRemainLenth(char *buffer, uint32_t len)
	{
		int pos = 0;
		while (len >= 0x80u)
		{
			buffer[pos++] = (len & 0x7fu) + 0x80;
			len >>= 7;
		}

		buffer[pos++] = (len & 0x7fu);
		Serial.print("setRemaininLeng");
		Serial.println(pos);
		return pos;
	}

	int addToSendbuffer(char *txt, int len)
	{
		int l = strlen(txt);
		Serial.print(txt);
		Serial.print(" adding:");
		Serial.println(l);
		_sendbuffer[len++] = 0;
		_sendbuffer[len++] = l;
		memcpy(_sendbuffer+len, txt, l);
		len += l;
		return len;
	}

	void sendPing()
	{
		int len = 0;
		_sendbuffer[len++] = 0x0c0;
		_sendbuffer[len++] = 0;
		hexdump(_sendbuffer, len);
		this->_client->write((uint8_t*)_sendbuffer, len);
		this->pingmillis = millis() + (1000 * 20);
	}

	void sendConnect()
	{
		int len = 0;
		_sendbuffer[len++] = 16;  // connect
		_sendbuffer[len++] = 11;  // remaining len
		// protocollname
		_sendbuffer[len++] = 0; // msb
		_sendbuffer[len++] = 4; // lsb
		_sendbuffer[len++] = 'M';
		_sendbuffer[len++] = 'Q';
		_sendbuffer[len++] = 'T';
		_sendbuffer[len++] = 'T';
		// protocolllevel
		_sendbuffer[len++] = 4; // 3.11
		// connectflags
		_sendbuffer[len++] = 128 + 64 + 2; // payload holds username and password...clear session
		// keep alive
		_sendbuffer[len++] = 0; //msb
		_sendbuffer[len++] = 60; //lsb
		// payload:  Client Identifier, Will Topic, Will Message, User Name, Password
		len = this->addToSendbuffer(this->_clientId, len);
		len = this->addToSendbuffer(this->_username, len);
		len = this->addToSendbuffer(this->_password, len);
		_sendbuffer[1] = len - 2;
		hexdump(_sendbuffer, len);
		this->_client->write((uint8_t*)_sendbuffer, len);
		this->_currentState = waitingForConnectACK;
	}

	int addResponseTopic(char *buf)
	{
		int lenTopic = sprintf(buf +2, "response/%s/1", this->_clientId);
		buf[0] = 0;
		buf[1] = lenTopic;
		return lenTopic+2;
	}

	int addDiscoveryHeader(char *buf, char *msgId)
	{
		return sprintf(buf, "{\"event\":{\"header\":{\"namespace\":\"Alexa.Discovery\",\"name\":\"Discover.Response\",\"payloadVersion\":\"3\",\"messageId\":\"%s\"},\"payload\":{\"endpoints\":[", msgId);
	}

	int addDiscoveryDevice(char *buf, char *deviceName, int id)
	{
		return sprintf(buf, "{\"endpointId\":\"Monarch-%02d\",\"friendlyName\":\"%s\",\"description\":\"Homebridge %s Light\",\"manufacturerName\":\"Default-Manufacturer\",\"displayCategories\":[\"LIGHT\"],\"cookie\":{\"TurnOn\":\"{1}\", \"TurnOff\":\"{0}\",\"ReportState\":\"{2}\"},\"capabilities\":[{\"type\":\"AlexaInterface\",\"interface\":\"Alexa\",\"version\":\"3\"},{\"type\":\"AlexaInterface\",\"interface\":\"Alexa.PowerController\",\"version\":\"3\",\"properties\":{\"supported\":[{\"name\":\"powerState\"}],\"proactivelyReported\":false,\"retrievable\":true}}]}", id, deviceName, deviceName);
	}

	int addDiscoveryFooter(char *buf)
	{
		return sprintf(buf, "]}}}");
	}

public:
	MiniMqttClient(char *server, int port, char *clientId, char *username, char *password) :
		_server(server),
		_port(port),
		_clientId(clientId),
		_username(username),
		_password(password)
	{
		_client = 0;
		_currentState = 0;
		this->_onMessage = 0;
		Serial.print("clientId");
		Serial.println(this->_clientId);
	}

	void setCallback(std::function<void(String&)> callback)
	{
		this->_onMessage = callback;
	}

	bool subscribe(char *topic)
	{
		this->_topic = topic;
		int len = 0;
		_sendbuffer[len++] = 0x82;
		len += setRemainLenth(_sendbuffer + len, strlen(topic) + 5);
		//_sendbuffer[len++] = 0x00;
		// packet identifier
		_sendbuffer[len++] = 0x00;
		_sendbuffer[len++] = 0x01;
		len = addToSendbuffer(topic, len);
		// extra byte (Qos)
		_sendbuffer[len++] = 0;
		hexdump(_sendbuffer, len);
		this->_client->write((uint8_t*)_sendbuffer, len);
		this->_currentState = doingnil;
		return true; 
	}

	void sendDiscoveryResponse(char *msgId, std::map<char *, int> deviceList)
	{
		Serial.printf("Sending response....");
		int lenTopic = addResponseTopic(_sendbuffer);
		int lenPayload = addDiscoveryHeader(_sendbuffer, msgId);
		for (std::map<char *, int>::iterator it=deviceList.begin(); it != deviceList.end(); )
		{
			lenPayload += addDiscoveryDevice(_sendbuffer + lenTopic, it->first, it->second);
			if (++it != deviceList.end())
			{
				_sendbuffer[lenTopic] = ',';
				lenPayload ++;
			}
		}

		lenPayload += addDiscoveryFooter(_sendbuffer + lenTopic);

		int len = 0;
		_sendbuffer[len++] = 0x30;
		len += setRemainLenth(_sendbuffer + len, lenPayload + lenTopic);
		len += addResponseTopic(_sendbuffer + len);
		len += addDiscoveryHeader(_sendbuffer + len, msgId); 
		for (std::map<char *, int>::iterator it = deviceList.begin(); it != deviceList.end(); )
		{
			len += addDiscoveryDevice(_sendbuffer + lenTopic, it->first, it->second);
			if (++it != deviceList.end())
			{
				_sendbuffer[len++] = ',';
			}
		}

		len += addDiscoveryFooter(_sendbuffer + len);
		this->_client->write((uint8_t*)_sendbuffer, len);
		Serial.println(_sendbuffer + 5);
	}

	bool connect()
	{
		_client = new SyncClient();
		int result = _client->connect(this->_server, this->_port);
		Serial.print("Connected to server:");
		Serial.println(result);
		this->_currentState = waitingForConnect;
		return true;
	}

	void loop()
	{
		if (this->_client != 0 && this->_client->connected())
		{
			
			Serial.print("c");
			delay(200);
			if (millis() > this->pingmillis)
			{
				this->sendPing();
			}

			if (this->_currentState == waitingForConnect)
			{
				Serial.println("sending");
				this->sendConnect();
			}

			if (this->_client->available())
			{
				this->pingmillis = millis() + (1000 * 20);
				int bytesRead = this->_client->readBytes(_rcvbuffer, sizeof(_rcvbuffer));
				Serial.print("bytes read: ");
				Serial.println(bytesRead);
				hexdump(_rcvbuffer, bytesRead);
				if (bytesRead > 3 &&_rcvbuffer[0] == MQTTSTATUS_CONNACK && _rcvbuffer[3]==0)
				{
					Serial.println("sending subscription request");
					this->subscribe((char *)"#");
				}

				if (bytesRead > 4 && _rcvbuffer[0] == MQTTSTATUS_SUBSCRIBEACK && _rcvbuffer[4] != 0x80)
				{
					Serial.println("subscription ack");
				}

				if (bytesRead > 1 && _rcvbuffer[0] == MQTTSTATUS_PINGACK)
				{
					Serial.println("PONG");
				}

				if (bytesRead > 4 && bytesRead < 250 && _rcvbuffer[0] == MQTTSTATUS_MESSAGE)
				{
					_rcvbuffer[_rcvbuffer[1] + 3] = 0;
					if (this->_onMessage != 0)
					{
						String mystring = _rcvbuffer+5;
						this->_onMessage(mystring);
					}
					Serial.println();
				}
			}
		}
		else
		{
			Serial.println("connecting");
			this->connect();
		}
		return;
	}
};