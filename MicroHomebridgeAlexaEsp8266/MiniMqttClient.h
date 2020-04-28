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
#if ASYNC_TCP_SSL_ENABLED
#include <tcp_axtls.h>
#define SHA1_SIZE 20
const uint8_t fingerprint[] = { 0x53, 0x65, 0x25, 0x00, 0x27, 0x9c, 0x10, 0xbc, 0xeb, 0xa8, 0x66, 0xdd, 0x91, 0x52, 0x4a, 0xb6, 0xc1, 0x91, 0x39, 0x2e };
#endif
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
	std::function<void(char*,size_t)> _onMessage;
	static MiniMqttClient* _myinstance;
	static char* _rcvbuffer;
	static volatile size_t _writepointer;
	static volatile size_t _messagelen;
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
		Serial.println("Connected");
		if (MiniMqttClient::_myinstance->_sender != 0)
		{
#if ASYNC_TCP_SSL_ENABLED
			SSL* clientSsl = client->getSSL();

			if (ssl_match_fingerprint(clientSsl, fingerprint) != SSL_OK) 
			{
				Serial.println("fingerprint check failed.");
				client->close(true);

			}
#endif
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
			memcpy(MiniMqttClient::_rcvbuffer + MiniMqttClient::_writepointer, rcvbuffer, len);
			MiniMqttClient::_writepointer += len;
			return;
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
			size_t remain = _myinstance->getRemainLength(rcvbuffer + 1);
			if (len - 3 < remain)
			{
				_myinstance->ensureAllocRcvBuffer(remain);

				memcpy(MiniMqttClient::_rcvbuffer, rcvbuffer, len);
				MiniMqttClient::_writepointer = len;
				MiniMqttClient::_messagelen = remain;
				_myinstance->_currentState = waitingformore;
				return;
			}
			if (_myinstance->_onMessage != 0 && _myinstance->_sender->allFilesLoaded)
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

	void setCallback(std::function<void(char*,size_t)> callback)
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

	bool getAllFilesLoaded()
	{
		return this->_sender->allFilesLoaded;
	}

	void connect()
	{
		if (this->_currentState != waitingForConnect)
		{
			Serial.print("Connecting to ");
			Serial.print(this->_server);
			Serial.print(":");
			Serial.println(this->_port);
			this->_client = new AsyncClient();
			this->_client->onConnect(&handleOnConnect, this->_client);
			this->_client->onData(&handleData);
			this->_client->onDisconnect(&handleDisconnect);
			this->_client->onAck(&MiniMqttClientSender::handleAck);
#if ASYNC_TCP_SSL_ENABLED
			Serial.println("via ssl");
			this->_client->connect(this->_server, this->_port, true);
#else
			this->_client->connect(this->_server, this->_port);

#endif
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
				if (MiniMqttClient::_writepointer >= MiniMqttClient::_messagelen + 3)
				{
					// message received
					if (this->_onMessage != 0)
					{
						if (this->_sender->allFilesLoaded)
						{
							this->_onMessage(MiniMqttClient::_rcvbuffer, MiniMqttClient::_messagelen + 3);
						}

						this->_currentState = doingnil;
						if (MiniMqttClient::_writepointer > MiniMqttClient::_messagelen + 3)
						{
							this->handleData(0, 0, MiniMqttClient::_rcvbuffer + MiniMqttClient::_messagelen + 3, MiniMqttClient::_writepointer - (MiniMqttClient::_messagelen + 3));
							// there is more to handle
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
};

MiniMqttClient* MiniMqttClient::_myinstance;
char* MiniMqttClient::_rcvbuffer;
volatile size_t MiniMqttClient::_writepointer;
volatile size_t MiniMqttClient::_messagelen;
size_t MiniMqttClient::_rcvbufferlen;