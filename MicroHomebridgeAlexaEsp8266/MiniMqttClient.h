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

#define MQTTSTATUS_CONNACK 0x20
#define MQTTSTATUS_SUBSCRIBEACK 0x90
#define MQTTSTATUS_PINGACK 0x0D0
#define MQTTSTATUS_MESSAGE 0x30

enum State { initialzed = 0 , waitingForConnect, waitingForConnectACK, doingnil, waitingformore, waitingforack};
class MiniMqttClient
{
private:
	char *_server;
	int _port;
	char *_clientId;
	char *_username;
	char *_password;
	char *_topic;
	AsyncClient* _client;
	char _sendbuffer[2048];
	int _currentState;
	unsigned long pingmillis=0x0fffffff;
	std::function<void(char*,int)> _onMessage;
	static MiniMqttClient* _myinstance;
	static char* _rcvbuffer;
	static size_t _oldlen;
	static size_t _remain;

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
		return pos;
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

	int addToSendbuffer(char *txt, int len)
	{
		int l = strlen(txt);
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
		this->_client->write((char *)_sendbuffer, len);
		this->pingmillis = millis() + (1000 * 30);
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
		this->_client->write((char *)_sendbuffer, len);
		this->_currentState = waitingForConnectACK;
	}

	int addResponseTopic(char *buf)
	{
		int lenTopic = sprintf(buf +2, "response/%s/1", this->_clientId);
		buf[0] = 0;
		buf[1] = lenTopic;
		return lenTopic+2;
	}

	int addDiscoveryHeader(char *buf,char *format, char *msgId)
	{
		return sprintf(buf, format, msgId);
	}

	int addDiscoveryDevice(char *buf, char *format, char *deviceName, int id)
	{
		Serial.print("adding ");
		Serial.println(id);
		Serial.println(deviceName);
		return sprintf(buf, format, id, deviceName, deviceName);
	}

	int addDiscoveryFooter(char *buf,char *format)
	{
		return sprintf(buf, format);
	}


	int addSwitchResponse(char* buf, char* format,char *correlation, bool onoff, int deviceId)
	{
		return sprintf(buf, format, onoff ? "ON" : "OFF", correlation, deviceId);

	}

	char* loadFile(char *filename)
	{
		File fs = SPIFFS.open(filename, "r");
		if (!fs)
		{
			Serial.println("ERRROR: File not found!");
			Serial.println(filename);
			return 0;
		}

		int size = fs.size();
		char *result = (char *)malloc(size+1);
		fs.readBytes(result, size);
		fs.close();
		result[size] = 0;
		return result;
	}

	static void onAck(void *arg, AsyncClient* client, size_t len, uint32_t time)
	{
		Serial.print("ACK:");
		Serial.println(len);
		Serial.println(_myinstance->_currentState);

		if (_myinstance->_currentState == waitingforack)
		{
			size_t sent = client->write(_myinstance->_sendbuffer + len, MiniMqttClient::_oldlen - len);
			if (sent == MiniMqttClient::_oldlen - len)
			{
				Serial.println("SENT!");
				_myinstance->_currentState = doingnil;
			}
			else
			{
				Serial.println("Error in sent");
			}
		}

	}

	void sendFileResponse(char *filename, char* correlation, bool onoff, int deviceId)
	{
		char* formatSwitch = this->loadFile(filename);
		int lenTopic = addResponseTopic(_sendbuffer);
		int lenPayload = addSwitchResponse(_sendbuffer, formatSwitch, correlation, onoff, deviceId);

		int len = 0;
		_sendbuffer[len++] = 0x30;
		len += setRemainLenth(_sendbuffer + len, lenPayload + lenTopic);
		len += addResponseTopic(_sendbuffer + len);
		len += addSwitchResponse(_sendbuffer+len, formatSwitch, correlation, onoff, deviceId);
		int sent=this->_client->write((char *)_sendbuffer, len);
		if (sent < len)
		{
			_myinstance->_currentState = waitingforack;
			Serial.println("waiting for ack");
			MiniMqttClient::_oldlen = len;
			MiniMqttClient::_remain = sent;
		}
		free(formatSwitch);
	}

	static void handleOnConnect(void *arg, AsyncClient* client)
	{
		_myinstance->sendConnect();
	}

	static void handleData(void* arg, AsyncClient* client, void *data, size_t len)
	{
		char *rcvbuffer = (char *)data;
		Serial.print("Received ");
		Serial.println(len);
		if (_myinstance->_currentState == waitingformore)
		{
			if (len + MiniMqttClient::_oldlen == MiniMqttClient::_remain + 3)
			{
				Serial.println("Frame finished");
				_myinstance->_currentState = doingnil;
				memcpy(MiniMqttClient::_rcvbuffer + MiniMqttClient::_oldlen, rcvbuffer, len);
				if (_myinstance->_onMessage != 0)
				{
					_myinstance->_onMessage(MiniMqttClient::_rcvbuffer, MiniMqttClient::_remain+3);
				}

				return;
			}
			else
			{
				if (len + MiniMqttClient::_oldlen < MiniMqttClient::_remain)
				{
					Serial.println("Appending incomplete frame");
					memcpy(MiniMqttClient::_rcvbuffer + MiniMqttClient::_oldlen, rcvbuffer, len);
					MiniMqttClient::_oldlen += len;
					return;
				}
				else
				{
					Serial.println("FEHLER im waiting!");
					_myinstance->_currentState = doingnil;
					return;
				}
			}
		}
		if (len > 3 && rcvbuffer[0] == MQTTSTATUS_CONNACK && rcvbuffer[3] == 0)
		{
			Serial.println("sending subscription request");
			_myinstance->subscribe((char *)"command/#");
		}

		if (len > 4 && rcvbuffer[0] == MQTTSTATUS_SUBSCRIBEACK && rcvbuffer[4] != 0x80)
		{
			Serial.println("subscription ack");
			_myinstance->pingmillis = millis() + (1000 * 30);
		}

		if (len > 1 && rcvbuffer[0] == MQTTSTATUS_PINGACK)
		{
			Serial.println("PONG");
		}

		if (len > 4  && rcvbuffer[0] == MQTTSTATUS_MESSAGE)
		{
			Serial.print("remaining len read: ");
			size_t remain = _myinstance->getRemainLength(rcvbuffer + 1);
			if (len - 3 < remain)
			{
				if (MiniMqttClient::_rcvbuffer != 0)
				{
					free(MiniMqttClient::_rcvbuffer);
				}

				MiniMqttClient::_rcvbuffer =(char *)malloc(remain + 3);
				memcpy(MiniMqttClient::_rcvbuffer, rcvbuffer, len);
				MiniMqttClient::_oldlen = len;
				MiniMqttClient::_remain = remain;
				_myinstance->_currentState = waitingformore;
				Serial.println("WAITING FOR MORE. Starting FRAME.");
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
		MiniMqttClient::_myinstance = this;
		MiniMqttClient::_rcvbuffer = 0;
	}

	void setCallback(std::function<void(char*,int)> callback)
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
		_sendbuffer[len++] = strlen(topic);
		len = addToSendbuffer(topic, len);
		// extra byte (Qos)
		_sendbuffer[len++] = 0;
		this->_client->write((char*)_sendbuffer, len);
		this->_currentState = doingnil;
		return true; 
	}

	void sendSwitchResponse(String& correlation, bool onoff, int deviceId)
	{
		this->sendFileResponse((char *)"/switch.json", (char *)correlation.c_str(), onoff, deviceId);
	}

	void sendPowerStateResponse(String& correlation, bool onoff, int deviceId)
	{
		this->sendFileResponse((char *)"/state.json", (char *)correlation.c_str(), onoff, deviceId);
	}

	void sendDiscoveryResponse(char *msgId, std::map<char *, int> deviceList)
	{
		Serial.printf("Sending response....");
		char *formatheader = this->loadFile((char *)"/discoveryheader.json");
		char *formatdevice = this->loadFile((char *)"/discoverydevice.json");
		char *formatfooter = this->loadFile((char *)"/discoveryfooter.json");
		int lenTopic = addResponseTopic(_sendbuffer);
		int lenPayload = addDiscoveryHeader(_sendbuffer,formatheader, msgId);
		for (std::map<char *, int>::iterator it=deviceList.begin(); it != deviceList.end(); )
		{
			lenPayload += addDiscoveryDevice(_sendbuffer + lenTopic, formatdevice, it->first, it->second);
			if (++it != deviceList.end())
			{
				_sendbuffer[lenTopic] = ',';
				lenPayload ++;
			}
		}
		lenPayload += addDiscoveryFooter(_sendbuffer + lenTopic, formatfooter);

		int len = 0;
		_sendbuffer[len++] = 0x30;
		len += setRemainLenth(_sendbuffer + len, lenPayload + lenTopic);

		len += addResponseTopic(_sendbuffer + len);
		len += addDiscoveryHeader(_sendbuffer + len, formatheader,msgId); 

		for (std::map<char *, int>::iterator it = deviceList.begin(); it != deviceList.end(); )
		{
			len += addDiscoveryDevice(_sendbuffer + len, formatdevice ,it->first, it->second);
			if (++it != deviceList.end())
			{
				_sendbuffer[len++] = ',';
			}
		}

		len += addDiscoveryFooter(_sendbuffer + len, formatfooter);
		this->_client->write((char*)_sendbuffer, len);
		free(formatheader);
		free(formatdevice);
		free(formatfooter);
	}

	void connect()
	{
		if (this->_currentState != waitingForConnect)
		{
			this->_client = new AsyncClient();
			_client->onConnect(&handleOnConnect, this->_client);
			_client->onData(&handleData);
			_client->onDisconnect(&handleDisconnect);
			_client->onAck(&onAck);
			_client->connect(this->_server, this->_port);
			this->_currentState = waitingForConnect;
		}
	}

	void loop()
	{
		if (this->_client != 0 && this->_client->connected())
		{
			if (millis() > this->pingmillis)
			{
				this->sendPing();
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
size_t MiniMqttClient::_oldlen;
size_t MiniMqttClient::_remain;