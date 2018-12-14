#pragma once

enum State { initialzed = 0, waitingForConnect, waitingForConnectACK, doingnil, waitingformore, waitingforack, readytosend };
struct structJsonFiles {
	char *discoverydevice;
	char *discoveryfooter;
	char *discoveryheader;
	char *state;
	char *switchjson;
};

class MiniMqttClientSender
{
private:
	AsyncClient *_client  = 0;
	char _sendbuffer[8192];
	static MiniMqttClientSender* _myinstance;
	int _currentState;
	static volatile size_t _oldlen;
	static volatile size_t _remain;
	char *_clientId;
	char *_username;
	char *_password;
	char *_topic;

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

	int addToSendbuffer(char *txt, int len)
	{
		int l = strlen(txt);
		_sendbuffer[len++] = 0;
		_sendbuffer[len++] = l;
		memcpy(_sendbuffer + len, txt, l);
		len += l;
		return len;
	}

	void sendPing()
	{
		if (this->_currentState != waitingforack && this->_currentState != readytosend)
		{
			int len = 0;
			_sendbuffer[len++] = 0x0c0;
			_sendbuffer[len++] = 0;
			this->_client->write((char *)_sendbuffer, len);
			this->pingmillis = millis() + (1000 * 30);
		}
	}

	int addResponseTopic(char *buf)
	{
		int lenTopic = sprintf(buf + 2, "response/%s/1", this->_clientId);
		buf[0] = 0;
		buf[1] = lenTopic;
		return lenTopic + 2;
	}

	int addDiscoveryHeader(char *buf, char *format, char *msgId)
	{
		return sprintf(buf, format, msgId);
	}

	int addDiscoveryDevice(char *buf, char *format, char *deviceName, int id)
	{
		return sprintf(buf, format, id, deviceName, deviceName);
	}

	int addDiscoveryFooter(char *buf, char *format)
	{
		return sprintf(buf, format);
	}


	int addSwitchResponse(char* buf, char* format, char *correlation, bool onoff, int deviceId)
	{
		return sprintf(buf, format, onoff ? "ON" : "OFF", correlation, deviceId);
	}

	char* loadFileFromSpiffs(char *filename)
	{
		File fs = SPIFFS.open(filename, "r");
		if (!fs)
		{
			Serial.println("ERRROR: File not found!");
			Serial.println(filename);
			return 0;
		}

		int size = fs.size();
		char *result = (char *)malloc(size + 1);
		fs.readBytes(result, size);
		fs.close();
		result[size] = 0;
		return result;
	}

	char * loadFileFromMonarchDe(char *filename)
	{
		WiFiClientSecure client;
		if (!client.connect("www.monarch.de", 443))
		{
			return 0;
		}

		client.print("GET /remote.php/webdav/microhomebridge");
		client.print(filename);
		client.print(" HTTP/1.1\r\n");
		client.print("Host: www.monarch.de\r\n");
		client.print("Connection: close\r\n");
		client.print("User-Agent: Mozilla/4.0 (compatible; esp8266;arduino sdk;)\r\n");
		client.print("\r\n");
		size_t filelen=0;
		char * result =0;
		while(client.available())
		{
			String line = client.readStringUntil('\r');
			Serial.print(line);
			if (line.startsWith("Content-Length:"))
			{
				filelen = atoi(line.c_str()+16);
				result = (char*)malloc(filelen+1);
				result[filelen] = 0;
				if (!result)
				{
					Serial.println("ERROR: Not enough memory");
					while(1);
				}
			}

			if (line == "")
			{
				Serial.println("Begin download");
				size_t readlen = client.readBytes(result, filelen);
				Serial.print("Read: ");
				Serial.print(readlen);
				Serial.print(" / ");
				Serial.println(filelen);
			}
		}

		return result;
	}

	void stageSender(int len)
	{
		if (_myinstance->_currentState != waitingforack)
		{
			_myinstance->_currentState = readytosend;
		}

		MiniMqttClientSender::_oldlen = len;
	}



public:
	unsigned long pingmillis = 0x0fffffff;
	structJsonFiles* _allFiles;
	bool allFilesLoaded = false;

	void setClient(AsyncClient* client)
	{
		this->_client = client;
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

	void sendFileResponse(char *formatSwitch, char* correlation, bool onoff, int deviceId)
	{
		if (_myinstance->_currentState == waitingforack || _myinstance->_currentState == readytosend)
		{
			Serial.println("Must append");
		}

		int len = (_myinstance->_currentState == waitingforack || _myinstance->_currentState == readytosend) ? MiniMqttClientSender::_oldlen : 0;

		int lenTopic = addResponseTopic(_sendbuffer+len);
		int lenPayload = addSwitchResponse(_sendbuffer+len, formatSwitch, correlation, onoff, deviceId);

		len = (_myinstance->_currentState == waitingforack || _myinstance->_currentState == readytosend) ? MiniMqttClientSender::_oldlen : 0;
		Serial.print("Starting to send from ");
		Serial.println(len);
		_sendbuffer[len++] = 0x30;
		len += setRemainLenth(_sendbuffer + len, lenPayload + lenTopic);
		len += addResponseTopic(_sendbuffer + len);
		len += addSwitchResponse(_sendbuffer + len, formatSwitch, correlation, onoff, deviceId);
		
		this->stageSender(len);

	}
	
	void sendDiscoveryResponse(char *msgId, std::map<char *, int> deviceList)
	{
		if (!this->_client->canSend() || _myinstance->_currentState == waitingforack || _myinstance->_currentState == readytosend)
		{
			Serial.println("ERRROR: Cannot send");
			return;
		}

		char *formatheader = this->_allFiles->discoveryheader; 
		char *formatdevice = this->_allFiles->discoverydevice; 
		char *formatfooter = this->_allFiles->discoveryfooter; 
		int lenTopic = addResponseTopic(_sendbuffer);
		int lenPayload = addDiscoveryHeader(_sendbuffer, formatheader, msgId);
		for (std::map<char *, int>::iterator it = deviceList.begin(); it != deviceList.end(); )
		{
			lenPayload += addDiscoveryDevice(_sendbuffer + lenTopic, formatdevice, it->first, it->second);
			if (++it != deviceList.end())
			{
				_sendbuffer[lenTopic] = ',';
				lenPayload++;
			}
		}
		lenPayload += addDiscoveryFooter(_sendbuffer + lenTopic, formatfooter);

		int len = 0;
		_sendbuffer[len++] = 0x30;
		len += setRemainLenth(_sendbuffer + len, lenPayload + lenTopic);

		len += addResponseTopic(_sendbuffer + len);
		len += addDiscoveryHeader(_sendbuffer + len, formatheader, msgId);

		for (std::map<char *, int>::iterator it = deviceList.begin(); it != deviceList.end(); )
		{
			len += addDiscoveryDevice(_sendbuffer + len, formatdevice, it->first, it->second);
			if (++it != deviceList.end())
			{
				_sendbuffer[len++] = ',';
			}
		}

		len += addDiscoveryFooter(_sendbuffer + len, formatfooter);
		this->stageSender(len);
	}


	MiniMqttClientSender(char *clientId, char *username, char *password) :
		_clientId(clientId),
		_username(username),
		_password(password)
	{
		MiniMqttClientSender::_myinstance = this;
		this->_allFiles = (structJsonFiles*)malloc(sizeof(structJsonFiles));
		this->_allFiles->discoverydevice = this->loadFileFromSpiffs((char*)"/discoverydevice.json");
		this->_allFiles->discoveryfooter = this->loadFileFromSpiffs((char*)"/discoveryfooter.json");
		this->_allFiles->discoveryheader = this->loadFileFromSpiffs((char*)"/discoveryheader.json");
		this->_allFiles->switchjson = this->loadFileFromSpiffs((char*)"/switch.json");
		this->_allFiles->state = this->loadFileFromSpiffs((char*)"/state.json");

		if (!this->_allFiles->discoverydevice)
		{
			this->_allFiles->discoverydevice = this->loadFileFromMonarchDe((char*)"/discoverydevice.json");
		}

		if (!this->_allFiles->discoveryfooter)
		{
			this->_allFiles->discoveryfooter = this->loadFileFromMonarchDe((char*)"/discoveryfooter.json");
		}

		if (!this->_allFiles->discoveryheader)
		{
			this->_allFiles->discoveryheader = this->loadFileFromMonarchDe((char*)"/discoveryheader.json");
		}

		if (!this->_allFiles->switchjson)
		{
			this->_allFiles->switchjson = this->loadFileFromMonarchDe((char*)"/switch.json");
		}

		if (!this->_allFiles->state)
		{
			this->_allFiles->state = this->loadFileFromSpiffs((char*)"/state.json");
		}



		this->allFilesLoaded = (!this->_allFiles->discoverydevice || 
			!this->_allFiles->discoveryfooter || 
			!this->_allFiles->discoveryheader || 
			!this->_allFiles->switchjson ||
			!this->_allFiles->state);
		MiniMqttClientSender::_remain = 0;

	}

	~MiniMqttClientSender()
	{

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
		this->_client->write((char *)_sendbuffer, len);
	}

	static void handleAck(void *arg, AsyncClient* client, size_t len, uint32_t time)
	{
		if (_myinstance->_currentState == waitingforack)
		{
			if (MiniMqttClientSender::_remain + len >= MiniMqttClientSender::_oldlen)
			{
				_myinstance->_currentState = doingnil;
				MiniMqttClientSender::_remain = 0;

			}
			else
			{
				MiniMqttClientSender::_remain += len;
				client->write(_myinstance->_sendbuffer + MiniMqttClientSender::_remain, MiniMqttClientSender::_oldlen - MiniMqttClientSender::_remain);
			}
		}

	}

	void loop()
	{
		if (this->_client != 0 && this->_client->connected())
		{
			if (this->_client->canSend() && this->_currentState == readytosend)
			{
				int len = MiniMqttClientSender::_oldlen;
				int sent = this->_client->write((char *)_sendbuffer, len);
				if (sent < len)
				{
					this->_currentState = waitingforack;
					return;
				}

				this->_currentState = doingnil;
			}

			if (millis() > this->pingmillis)
			{
				if (this->_client->canSend())
				{
					this->sendPing();
				}
				else
				{
					this->pingmillis = millis() + 1000;
				}
			}
		}
	}


};

MiniMqttClientSender* MiniMqttClientSender::_myinstance;
volatile size_t MiniMqttClientSender::_oldlen;
volatile size_t MiniMqttClientSender::_remain;
