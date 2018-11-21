#pragma once
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <vector>
#include <FS.h>

enum ctlstates { STATE_IDLE, STATE_WELCOME, STATE_UPLOAD, STATE_LIST, STATE_DOWNLOAD };

class FtpServ
{
public:
	FtpServ(char *username, char *password) : _username(username), _password(password)
	{

	}

	void begin()
	{
		this->_cserver = new AsyncServer(21); // start listening on tcp port 7050
		this->_cserver->onClient(&handleNewClient, _cserver);
		this->_cserver->begin();
	}

private:
	char *_username;
	char *_password;
	AsyncServer* _cserver;

	static int _ctlState;
	static int* _port;
	static AsyncClient* _data;
	static AsyncClient* _currentControlClient;
	static File _currentFile;

	static void handleNewClient(void* arg, AsyncClient* client) {
		Serial.printf("\n new client has been connected to server, ip: %s", client->remoteIP().toString().c_str());
		_currentControlClient = client;
		// register events
		client->onData(&handleCtlData, client);
		client->onError(&handleError, client);
		client->onDisconnect(&handleDisconnect, client);
		client->onTimeout(&handleTimeOut, client);
		if (client->space() > 32 && client->canSend())
		{
			client->write("220--- Welcome to FTP for ESP8266 ---\r\n");
			client->write("220---   By Niels Huesken, thanks David Paiva   ---\r\n");
			client->write("220 --   Version 1.0   --\r\n");
			client->send();
		}

		_ctlState = STATE_IDLE;
	}

	static void handleError(void* arg, AsyncClient* client, int8_t error)
	{
		Serial.printf("\n connection error %s from client %s \n", client->errorToString(error), client->remoteIP().toString().c_str());
	}

	static void onDataDisconnect(void * arg, AsyncClient *client)
	{
		if (_ctlState == STATE_DOWNLOAD)
		{
			_currentFile.close();
			client->close();
			_currentControlClient->write("226 file received.\r\n");
		}
	}

	/* event callbacks */
	static void handleDataData(void* arg, AsyncClient* client, void *data, size_t len)
	{
		Serial.printf("\n dataconnection received data from %s \n", client->remoteIP().toString().c_str());
		//Serial.write((uint8_t*)data, len);
		if (_ctlState == STATE_DOWNLOAD)
		{
			_currentFile.write((uint8_t*)data, len);
		}
	}

	static void sendBitOfFile(AsyncClient* client)
	{
		int len;
		char buffer[500];
		len = _currentFile.readBytes(buffer, sizeof(buffer));
		if (len > 0)
		{
			client->write(buffer, len);
		}
		else
		{
			client->send();
			client->close();
			_currentControlClient->write("226 you got it.\r\n");
			Serial.println("Data transfer complete");
			_currentFile.close();
		}
	}

	static void onDataAck(void *arg, AsyncClient * client, size_t len, uint32_t time)
	{
		sendBitOfFile(client);
	}

	static void onDataConnect(void* arg, AsyncClient* client)
	{
		char zeile[120];
		String fn;
		size_t fs;
		int items = 0;

		Serial.printf("\n data-client has been connected\n");
		if (_ctlState == STATE_LIST)
		{
			_currentControlClient->write("150 Connected\r\n");
			_currentControlClient->send();
			Dir dir = SPIFFS.openDir("/");
			while (dir.next())
			{
				fn = dir.fileName();
				fn.remove(0, 1);
				fs = dir.fileSize();
				sprintf(zeile, "-rw-r--r-- 1 owner group %*d Apr 10  1997 %s\r\n", 13, fs, fn.c_str());
				client->write(zeile);
				Serial.print(zeile);
				items++;
			}
			client->send();
			client->close();
			Serial.print(items);
			Serial.println(" files listed.");
			_currentControlClient->write("226 Alles Super.\r\n");
		}
		else if (_ctlState == STATE_UPLOAD)
		{
			_currentControlClient->write("150 Connected\r\n");
			_currentControlClient->send();
			sendBitOfFile(client);
		}

	}


	static void handleCtlData(void* arg, AsyncClient* client, void *data, size_t len)
	{
		Serial.printf("\n data received from client %s \n", client->remoteIP().toString().c_str());
		Serial.write((uint8_t*)data, len);
		if (!strncmp((char *)data, "USER", 4))
		{
			Serial.println("sending user");
			client->write("331 OK. Password required\r\n");
		}
		else if (!strncmp((char *)data, "PASS", 4))
		{
			Serial.println("sending pass");
			client->write("230 OK.\r\n");
		}
		else if (!strncmp((char *)data, "CWD", 3))
		{
			client->write("250 OK.\r\n");
		}
		else if (!strncmp((char *)data, "PWD", 3))
		{
			client->write("257 \"/\" is your current working directory.\r\n");
		}
		else if (!strncmp((char *)data, "TYPE", 4))
		{
			client->write("200 JAAAA MANNN!\r\n");
		}
		else if (!strncmp((char *)data, "PORT", 4))
		{
			size_t pos = 5;
			int numstart = 5;
			int ipcount = 0;

			while (ipcount < 5 && pos < len)
			{
				while (((char *)data)[++pos] != ',' && pos < len)
				{

				}
				_port[ipcount++] = atoi(((char*)(data)) + numstart);
				Serial.println(_port[ipcount - 1]);
				pos++;
				numstart = pos;
			}
			_port[ipcount] = atoi(((char*)(data)) + numstart);
			Serial.println(_port[ipcount]);

			client->write("250 OK.\r\n");

		}
		else if (!strncmp((char *)data, "LIST", 4))
		{
			char hostname[40];
			sprintf(hostname, "%d.%d.%d.%d", _port[0], _port[1], _port[2], _port[3]);
			Serial.print("Connecting to");
			Serial.println(hostname);
			_ctlState = STATE_LIST;
			_data = new AsyncClient();
			_data->onData(&handleDataData, _data);
			_data->onConnect(&onDataConnect, _data);
			_data->connect(hostname, _port[5] + (_port[4] * 256));
		}
		else if (!strncmp((char *)data, "RETR", 4))
		{
			char filename[40];
			char hostname[40];
			memset(filename, 0, sizeof(filename));
			filename[0] = '/';
			memcpy(filename + 1, ((char *)data) + 5, (len - 5) % (sizeof(filename) - 2));

			uint cou;
			for (cou = 0; cou < strlen(hostname) - 1 && filename[cou]>31; cou++);
			if (filename[cou] < 32)
				filename[cou] = 0;

			Serial.print("Retrieve file ");
			Serial.println(filename);
			_currentFile = SPIFFS.open(filename, "r");

			if (!_currentFile)
			{
				client->write("550 File nicht gefunden. Probiers mal mit ner Raspel.\r\n");
				return;
			}

			sprintf(hostname, "%d.%d.%d.%d", _port[0], _port[1], _port[2], _port[3]);
			Serial.print("Connecting to ");
			Serial.print(hostname);
			Serial.print(" to upload ");
			Serial.println(filename);
			_ctlState = STATE_UPLOAD;
			_data = new AsyncClient();
			_data->onData(&handleDataData, _data);
			_data->onConnect(&onDataConnect, _data);
			_data->connect(hostname, _port[5] + (_port[4] * 256));
			_data->onAck(&onDataAck, _data);

		}
		else if (!strncmp((char *)data, "STOR", 4))
		{
			char hostname[40];

			char filename[40];
			memset(filename, 0, sizeof(filename));
			filename[0] = '/';
			memcpy(filename + 1, ((char *)data) + 5, (len - 5) % (sizeof(filename) - 2));

			uint cou;
			for (cou = 0; cou < strlen(filename) - 1 && filename[cou]>31; cou++);
			if (filename[cou] < 32)
				filename[cou] = 0;

			Serial.print("write file ");
			Serial.println(filename);
			_currentFile = SPIFFS.open(filename, "w");

			if (!_currentFile)
			{
				client->write("550 File nicht gefunden. Probiers mal mit ner Raspel.\r\n");
				return;
			}

			sprintf(hostname, "%d.%d.%d.%d", _port[0], _port[1], _port[2], _port[3]);
			Serial.print("Connecting to ");
			Serial.print(hostname);
			Serial.print(" to upload ");
			Serial.println(filename);
			_ctlState = STATE_DOWNLOAD;
			_data = new AsyncClient();
			_data->onData(&handleDataData, _data);
			_data->onConnect(&onDataConnect, _data);
			_data->connect(hostname, _port[5] + (_port[4] * 256));
			_data->onDisconnect(&onDataDisconnect, _data);
		}
		else if (!strncmp((char *)data, "DELE", 4))
		{
			char filename[40];
			memset(filename, 0, sizeof(filename));
			filename[0] = '/';
			memcpy(filename + 1, ((char *)data) + 5, (len - 5) % (sizeof(filename) - 2));

			uint cou;
			for (cou = 0; cou < strlen(filename) - 1 && filename[cou]>31; cou++);
			if (filename[cou] < 32)
				filename[cou] = 0;

			SPIFFS.remove(filename);
			client->write("220 geloescht.\r\n");
		}
		else
		{

			client->write("500 Unknown command.\r\n");
		}
		// reply to client
	}

	static void handleDisconnect(void* arg, AsyncClient* client)
	{
		Serial.printf("\n client %s disconnected \n", client->remoteIP().toString().c_str());
	}

	static void handleTimeOut(void* arg, AsyncClient* client, uint32_t time)
	{
		Serial.printf("\n client ACK timeout ip: %s \n", client->remoteIP().toString().c_str());
	}

};
int* FtpServ::_port = new int[6];
int FtpServ::_ctlState = STATE_IDLE;
AsyncClient* FtpServ::_data;
AsyncClient* FtpServ::_currentControlClient;
File FtpServ::_currentFile;