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
#include <FS.h>
#include "MiniMqttClientSender.h"

#define MQTTSTATUS_CONNACK 0x20
#define MQTTSTATUS_SUBSCRIBEACK 0x90
#define MQTTSTATUS_PINGACK 0x0D0
#define MQTTSTATUS_MESSAGE 0x30

inline size_t get4kBlockSize(size_t size) { return ((size / 4096) + 1) * 4096; }

//enum State { initialzed = 0 , waitingForConnect, waitingForConnectACK, doingnil, waitingformore, waitingforack, readytosend};
class MiniMqttClient
{
private:
	MiniMqttClientSender * _sender;
	char *_server;
	int _port;
	AsyncClient* _client;
	int _currentState;
	std::function<void(char*,int)> _onMessage;
	static MiniMqttClient* _myinstance;
	static char* _rcvbuffer;
	static volatile size_t _oldlen;
	static volatile size_t _remain;
	static size_t _rcvbufferlen;

	void ensureAllocRcvBuffer(size_t remain)
	{
		
		if (MiniMqttClient::_rcvbuffer != 0)
		{
			if (get4kBlockSize(remain) > MiniMqttClient::_rcvbufferlen)
			{
				Serial.println("Existing block too small. Need realloc");
				free(MiniMqttClient::_rcvbuffer);
				MiniMqttClient::_rcvbuffer = 0;
			}
		}

		if (MiniMqttClient::_rcvbuffer == 0)
		{
			MiniMqttClient::_rcvbufferlen = get4kBlockSize(remain + 3);
			Serial.print("Allocating receiverbuffer bytes: ");
			Serial.println(MiniMqttClient::_rcvbufferlen);
			MiniMqttClient::_rcvbuffer = (char *)malloc(MiniMqttClient::_rcvbufferlen);
			if (!MiniMqttClient::_rcvbuffer)
			{
				Serial.println("FEHLER! Nicht genug speicher.");
				while (true);
			}
		}
	}

	int getRemainLength(char *buffer)
	{
		int result = 0;
		int shift = 0;
		int cou = 0;
		int x;
		while ((x=buffer[cou]) >128)
		{
			result += ((x & 0x7fu) << shift);
			shift += 7;
			cou++;
		}
	
		result += ((x & 0x7fu) << shift);
		return result;
	}


	static void handleOnConnect(void *arg, AsyncClient* client)
	{
		if (MiniMqttClient::_myinstance->_sender != 0)
		{
			MiniMqttClient::_myinstance->_sender->sendConnect();
			MiniMqttClient::_myinstance->_currentState = waitingForConnectACK;
		}
	}

	static void handleData(void* arg, AsyncClient* client, void *data, size_t len)
	{
		char *rcvbuffer = (char *)data;
		Serial.print("Received ");
		Serial.println(len);
		if (_myinstance->_currentState == waitingformore)
		{
			Serial.println("Buffering");
			memcpy(MiniMqttClient::_rcvbuffer + MiniMqttClient::_oldlen, rcvbuffer, len);
			MiniMqttClient::_oldlen += len;
			return;
			////if (len + MiniMqttClient::_oldlen == MiniMqttClient::_remain + 3)
			////{
			////	Serial.println("Frame finished");
			////	_myinstance->_currentState = doingnil;
			////	memcpy(MiniMqttClient::_rcvbuffer + MiniMqttClient::_oldlen, rcvbuffer, len);
			////	if (_myinstance->_onMessage != 0)
			////	{
			////		_myinstance->_onMessage(MiniMqttClient::_rcvbuffer, MiniMqttClient::_remain+3);
			////	}

			////	return;
			////}
			////else
			////{
			////	Serial.print("so far: ");
			////	Serial.print(len + MiniMqttClient::_oldlen);
			////	Serial.print(" / ");
			////	Serial.println(MiniMqttClient::_remain);
			////	if (len + MiniMqttClient::_oldlen < MiniMqttClient::_remain)
			////	{
			////		Serial.println("Appending incomplete frame");
			////		memcpy(MiniMqttClient::_rcvbuffer + MiniMqttClient::_oldlen, rcvbuffer, len);
			////		MiniMqttClient::_oldlen += len;
			////		return;
			////	}
			////	else
			////	{
			////		Serial.println("FEHLER im waiting!");
			////		_myinstance->_currentState = doingnil;
			////		return;
			////	}
			////}
		}
		if (len > 3 && rcvbuffer[0] == MQTTSTATUS_CONNACK && rcvbuffer[3] == 0)
		{
			Serial.println("sending subscription request");
			_myinstance->_sender->subscribe((char *)"command/#");
		}

		if (len > 4 && rcvbuffer[0] == MQTTSTATUS_SUBSCRIBEACK && rcvbuffer[4] != 0x80)
		{
			Serial.println("subscription ack");
			_myinstance->_sender->pingmillis = millis() + (1000 * 30);
		}

		if (len > 1 && rcvbuffer[0] == MQTTSTATUS_PINGACK)
		{
			Serial.println("PONG");
		}

		if (len > 4  && rcvbuffer[0] == MQTTSTATUS_MESSAGE)
		{
			Serial.println("Message incoming");
			size_t remain = _myinstance->getRemainLength(rcvbuffer + 1);
			if (len - 3 < remain)
			{
				_myinstance->ensureAllocRcvBuffer(remain);

				memcpy(MiniMqttClient::_rcvbuffer, rcvbuffer, len);
				MiniMqttClient::_oldlen = len;
				MiniMqttClient::_remain = remain;
				_myinstance->_currentState = waitingformore;
				Serial.println("WAITING FOR MORE. Starting FRAME.");
				Serial.print("len: ");
				Serial.print(len);
				Serial.print(" remaining: ");
				Serial.println(remain);
				return;
			}
			if (_myinstance->_onMessage != 0)
			{
				_myinstance->_onMessage(rcvbuffer,len);
			}
			Serial.println();
		}

	}

	static void handleDisconnect(void *arg, AsyncClient* client)
	{
		Serial.println("Disconnect and self destruct");
		_myinstance->_currentState = doingnil;
	}


public:

	MiniMqttClient(char *server, int port, char *clientId, char *username, char *password) :
		_server(server),
		_port(port)
	{
		_client = 0;
		_currentState = 0;
		this->_onMessage = 0;
		MiniMqttClient::_myinstance = this;
		MiniMqttClient::_rcvbuffer = 0;
		this->_sender = new MiniMqttClientSender(clientId, username, password);
	}

	void setCallback(std::function<void(char*,int)> callback)
	{
		this->_onMessage = callback;
	}

	void sendSwitchResponse(char *correlation, bool onoff, int deviceId)
	{
		this->_sender->sendFileResponse(this->_sender->_allFiles->switchjson, (char *)correlation, onoff, deviceId);
	}

	void sendPowerStateResponse(char *correlation, bool onoff, int deviceId)
	{
		this->_sender->sendFileResponse(this->_sender->_allFiles->state, (char *)correlation, onoff, deviceId);
	}

	void sendDiscoveryResponse(char *msgId, std::map<char *, int> deviceList)
	{
		Serial.printf("Sending response....");
		this->_sender->sendDiscoveryResponse(msgId, deviceList);
	}

	void connect()
	{
		if (this->_currentState != waitingForConnect)
		{
			Serial.print("Connecting to ");
			Serial.println(this->_server);
			this->_client = new AsyncClient();
			this->_client->onConnect(&handleOnConnect, this->_client);
			this->_client->onData(&handleData);
			this->_client->onDisconnect(&handleDisconnect);
			this->_client->onAck(&MiniMqttClientSender::handleAck);
			this->_client->connect(this->_server, this->_port);
			this->_sender->setClient(this->_client);
			this->_currentState = waitingForConnect;
		}
	}

	void loop()
	{
		if (this->_sender != 0)
		{
			this->_sender->loop();
		}

		if (this->_client != 0 && this->_client->connected())
		{

			if (this->_currentState == waitingformore)
			{
				if (MiniMqttClient::_oldlen >= MiniMqttClient::_remain + 3)
				{
					// message received
					if (this->_onMessage != 0)
					{
						this->_onMessage(MiniMqttClient::_rcvbuffer, MiniMqttClient::_remain + 3);
						this->_currentState = doingnil;
						if (MiniMqttClient::_oldlen > MiniMqttClient::_remain + 3)
						{
							Serial.println("Handle next frame");
							this->handleData(0, 0, MiniMqttClient::_rcvbuffer + MiniMqttClient::_remain + 3, MiniMqttClient::_oldlen - (MiniMqttClient::_remain + 3));
							// there is more to handle
						}
						else
						{
							Serial.println("Frame finished");
						}
					}


				}
			}

		}
		else
		{
			this->connect();
		}
		return;
	}

	static void MNOhexdump(char *txt, int len)
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

};

MiniMqttClient* MiniMqttClient::_myinstance;
char* MiniMqttClient::_rcvbuffer;
volatile size_t MiniMqttClient::_oldlen;
volatile size_t MiniMqttClient::_remain;
size_t MiniMqttClient::_rcvbufferlen;