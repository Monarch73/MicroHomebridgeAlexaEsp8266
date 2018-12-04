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
		int len = 0;
		_sendbuffer[len++] = 0x0c0;
		_sendbuffer[len++] = 0;
		hexdump(_sendbuffer, len);
		this->_client->write((char *)_sendbuffer, len);
		this->pingmillis = millis() + (1000 * 30);
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




public:
	unsigned long pingmillis = 0x0fffffff;
	structJsonFiles* _allFiles;

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
		Serial.println("prepaing sending");
		if (!this->_client->canSend() || _myinstance->_currentState == waitingforack || _myinstance->_currentState == readytosend)
		{
			Serial.println("Cannot send");
			return;
		}

		int lenTopic = addResponseTopic(_sendbuffer);
		int lenPayload = addSwitchResponse(_sendbuffer, formatSwitch, correlation, onoff, deviceId);

		int len = 0;
		_sendbuffer[len++] = 0x30;
		len += setRemainLenth(_sendbuffer + len, lenPayload + lenTopic);
		len += addResponseTopic(_sendbuffer + len);
		len += addSwitchResponse(_sendbuffer + len, formatSwitch, correlation, onoff, deviceId);
		_myinstance->_currentState = readytosend;
		MiniMqttClientSender::_oldlen = len;
	}
	
	void sendDiscoveryResponse(char *msgId, std::map<char *, int> deviceList)
	{
		char *formatheader = this->_allFiles->discoveryheader; // this->loadFile((char *)"/discoveryheader.json");
		char *formatdevice = this->_allFiles->discoverydevice; //this->loadFile((char *)"/discoverydevice.json");
		char *formatfooter = this->_allFiles->discoveryfooter; // this->loadFile((char *)"/discoveryfooter.json");
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
		this->_client->write((char*)_sendbuffer, len);
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
		hexdump(_sendbuffer, len);
		this->_client->write((char *)_sendbuffer, len);
		this->_currentState = waitingForConnectACK;
	}

	static void handleAck(void *arg, AsyncClient* client, size_t len, uint32_t time)
	{
		if (_myinstance->_currentState == waitingforack)
		{
			Serial.print("Sending more bytes: ");
			Serial.println(MiniMqttClientSender::_oldlen - len);
			size_t sent = client->write(_myinstance->_sendbuffer + len, MiniMqttClientSender::_oldlen - len);
			if (sent + MiniMqttClientSender::_remain == MiniMqttClientSender::_oldlen)
			{
				Serial.println("SENT!");
				_myinstance->_currentState = doingnil;
			}
			else
			{
				Serial.print("insgesamt: ");
				Serial.println(MiniMqttClientSender::_oldlen);
				Serial.print("schritt 1:");
				Serial.println(len);
				Serial.print("schritt 2:");
				Serial.println(sent);
				Serial.println("Error in sent");
				while (true);
			}
		}

	}

	void loop()
	{
		if (this->_client != 0 && this->_client->connected())
		{
			if (this->_client->canSend() && this->_currentState == readytosend)
			{
				Serial.println("sending");
				int len = MiniMqttClientSender::_oldlen;
				int sent = this->_client->write((char *)_sendbuffer, len);
				if (sent < len)
				{
					this->_currentState = waitingforack;
					Serial.println("waiting for ack");
					MiniMqttClientSender::_remain = sent;
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
