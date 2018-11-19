#pragma once
#define N_DIPSWITCHES 30
#define N_CHAR_SSID   36
#define N_CHAR_PASSWORD 80
#define N_CHAR_HOMEBRIDGE 40
#define N_CHAR_HOMEBRIDGE_PASSWORD 80
#define USE_SPIFFS 1

#include <EEPROM.h>
#include <FS.h>

struct dipswitches_struct
{
	char name[20];
	char housecode[6];
	char code[6];
	char tri1[16];
	char tri2[16];
	char urlOn[160];
	char urlOff[160];
	uint16_t irhz;
	uint16_t irDataOn[100];
	uint16_t irDataOff[100];
};

class Estore
{
public:
	char ssid[N_CHAR_SSID];
	char password[N_CHAR_PASSWORD];
	char homebridgeUsername[N_CHAR_HOMEBRIDGE];
	char homebrdigePassword[N_CHAR_HOMEBRIDGE_PASSWORD];

	Estore()
	{

	}

	void setupEeprom(bool doit = false)
	{
		size_t memorysize = (sizeof(dipswitches_struct)*N_DIPSWITCHES) + 4 + N_CHAR_PASSWORD + N_CHAR_SSID + N_CHAR_HOMEBRIDGE+ N_CHAR_HOMEBRIDGE_PASSWORD;
		uint8_t dipswitchsize = (uint8_t)((uint)sizeof(dipswitches_struct));
		if (doit)
		{
			SPIFFS.format();
		}

		if (SPIFFS.begin() == false)
		{
			Serial.println("mounting SPIFFS failed...formatting");
			if (SPIFFS.format() == false)
			{
				Serial.println("formatting SPIFFS failed...");
				return;
			}
			else
			{
				if (SPIFFS.begin() == false)
				{
					Serial.println("SPIFFS failed...giving up");
					return;
				}
			}
		}

		FSInfo fsinfo;
		if (SPIFFS.info(fsinfo) == false)
		{
			Serial.println("Error. NO SPIFFS Info.");
			return;
		}

		if (!SPIFFS.exists("/EEPROM.TXT") && memorysize > fsinfo.totalBytes)
		{
			Serial.println("Not enough SPIFFS memory");
			return;
		}
		else
		{
			Serial.print("Selected FlashChipSize: "); Serial.println(ESP.getFlashChipSize());
			Serial.print("FlashChipRealSize: "); Serial.println(ESP.getFlashChipRealSize());
			Serial.print("SketchSize:"); Serial.println(ESP.getSketchSize());
			Serial.print("FreeSketchSpace:"); Serial.println(ESP.getFreeSketchSpace());
			Serial.print("FreeHeap:"); Serial.println(ESP.getFreeHeap());
			Serial.print("SPIFFS Bytes Total ");
			Serial.println(fsinfo.totalBytes);
			Serial.print("SPIFFS Bytes Used  ");
			Serial.println(fsinfo.usedBytes);
			Serial.print("SPIFFS Bytes Requ. ");
			Serial.println(memorysize);
		}

		File fs = SPIFFS.open("/EEPROM.TXT", "r+");
		Serial.println(fs.size());
		if (!fs)
		{
				Serial.println("SPIFFS unable to open storage");
				return;
		}

		if (fs.size() != memorysize)
		{
			Serial.print("Filesize failed...formatting spiffs ");
			Serial.print(fs.size());
			Serial.print(" / ");
			Serial.println(memorysize);
			fs.close();
			SPIFFS.remove("/EEPROM.TXT");
			goto InitializeSpiffs;
		}


		if (fs.seek(memorysize - 1, SeekSet) == false)
		{
			Serial.println("Seek memsize failed.");
			fs.close();
			goto InitializeSpiffs;
		}


		if (fs.seek(0, SeekSet) == false)
		{
			Serial.println("Seek start failed");
			fs.close();
			goto InitializeSpiffs;
		}

		if (fs.seek(0, SeekSet) == false)
		{
			Serial.println("Seek start failed");
			fs.close();
			goto InitializeSpiffs;
		}

		if (fs.read() != 'N')
		{
			Serial.println("N check failed");
			fs.close();
			goto InitializeSpiffs;
		}

		if (fs.read() != 'H')
		{
			Serial.println("H check failed");
			fs.close();
			goto InitializeSpiffs;
		}

		if (fs.read() != N_DIPSWITCHES)
		{
			Serial.println("N_DIPSWITCH failed");
			fs.close();
			goto InitializeSpiffs;
		}

		if (fs.read() != dipswitchsize)
		{
			Serial.println("sizeof check failed");
			fs.close();
			goto InitializeSpiffs;
		}

		goto SkipInitializeSpiffs;

	InitializeSpiffs:
		fs = SPIFFS.open("/EEPROM.TXT", "a+");
		Serial.println("Initializing file contents");
		fs.write((uint8_t)'N');
		fs.write((uint8_t)'H');
		fs.write((uint8_t)N_DIPSWITCHES);
		fs.write((uint8_t)dipswitchsize);
		for (size_t cou = 0; cou < memorysize - 4; cou++)
		{
			fs.write((uint8_t)0);
		}
		fs.close();
		fs = SPIFFS.open("/EEPROM.TXT", "a+");
		if (fs.size() != memorysize)
		{
			Serial.println("fuck spiffs!");
			return;
		}
	SkipInitializeSpiffs:

		if (!doit)
		{
			int eeprompos = 4 + (N_DIPSWITCHES * sizeof(dipswitches_struct));
			fs.seek(eeprompos, SeekSet);

			uint8_t *bytepointer;
			bytepointer = (uint8_t*)ssid;
			for (int cou = 0; cou < N_CHAR_SSID; cou++)
			{
				*bytepointer = fs.read();
				bytepointer++;
			}

			eeprompos = 4 + (N_DIPSWITCHES * sizeof(dipswitches_struct)) + N_CHAR_SSID;
			fs.seek(eeprompos, SeekMode::SeekSet);
			bytepointer = (uint8_t*)password;
			for (int cou = 0; cou < N_CHAR_PASSWORD; cou++)
			{
				*bytepointer = fs.read();
				bytepointer++;
			}

			eeprompos = 4 + (N_DIPSWITCHES * sizeof(dipswitches_struct)) + N_CHAR_SSID + N_CHAR_PASSWORD;
			fs.seek(eeprompos, SeekMode::SeekSet);
			bytepointer = (uint8_t*)homebridgeUsername;
			for (int cou = 0; cou < N_CHAR_HOMEBRIDGE; cou++)
			{
				*bytepointer = fs.read();
				bytepointer++;
			}

			eeprompos = 4 + (N_DIPSWITCHES * sizeof(dipswitches_struct)) + N_CHAR_SSID + N_CHAR_PASSWORD + N_CHAR_HOMEBRIDGE;
			fs.seek(eeprompos, SeekMode::SeekSet);
			bytepointer = (uint8_t*)homebrdigePassword;
			for (int cou = 0; cou < N_CHAR_HOMEBRIDGE_PASSWORD; cou++)
			{
				*bytepointer = fs.read();
				bytepointer++;
			}


		}
		fs.close();
	}

	void wifiSave(bool format)
	{
		uint32_t eeprompos = 4 + (N_DIPSWITCHES * sizeof(dipswitches_struct));
		uint8_t *bytepointer;
		bytepointer = (uint8_t *)ssid;

		if (format == true)
		{
			SPIFFS.end();
			setupEeprom(true);
		}


		File fs = SPIFFS.open("/EEPROM.TXT", "r+");
		if (!fs)
		{
			Serial.println("SPIFFS unable to open storage");
			return;
		}

		if (fs.seek(eeprompos, SeekSet) == false)
		{
			Serial.println("Positioning failed");
			return;
		}

		for (int cou = 0; cou < N_CHAR_SSID; cou++)
		{
			fs.write(*bytepointer);
			bytepointer++;
		}

		eeprompos = 4 + (N_DIPSWITCHES * sizeof(dipswitches_struct)) + N_CHAR_SSID;
		bytepointer = (uint8_t *)password;

		if (fs.seek(eeprompos, SeekSet) == false)
		{
			Serial.println("Positioning failed");
			return;
		}

		for (int cou = 0; cou < N_CHAR_PASSWORD; cou++)
		{
			fs.write(*bytepointer);
			bytepointer++;
		}

		eeprompos = 4 + (N_DIPSWITCHES * sizeof(dipswitches_struct)) + N_CHAR_SSID + N_CHAR_PASSWORD;
		bytepointer = (uint8_t *)homebridgeUsername;

		if (fs.seek(eeprompos, SeekSet) == false)
		{
			Serial.println("Positioning failed");
			return;
		}

		for (int cou = 0; cou < N_CHAR_HOMEBRIDGE; cou++)
		{
			fs.write(*bytepointer);
			bytepointer++;
		}

		eeprompos = 4 + (N_DIPSWITCHES * sizeof(dipswitches_struct)) + N_CHAR_SSID + N_CHAR_PASSWORD + N_CHAR_HOMEBRIDGE;
		bytepointer = (uint8_t *)homebrdigePassword;

		if (fs.seek(eeprompos, SeekSet) == false)
		{
			Serial.println("Positioning failed");
			return;
		}

		for (int cou = 0; cou < N_CHAR_HOMEBRIDGE_PASSWORD; cou++)
		{
			fs.write(*bytepointer);
			bytepointer++;
		}
		fs.close();
	}

};

