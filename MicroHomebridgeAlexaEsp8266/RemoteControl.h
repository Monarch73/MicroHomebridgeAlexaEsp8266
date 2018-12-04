#pragma once
#include "Estore.h"
#include <RCSwitch.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include "WebInterface.h"
#include "StrFunc.h"

typedef struct dipswitches_struct dipswitch;

class RemoteControl
{
private:
	RCSwitch *_switch;
	IRsend* _myIr;
	WebInterface* _webui;
	bool lightStates[N_DIPSWITCHES];



public:
	RemoteControl(RCSwitch* rcswitch, IRsend* ir, WebInterface* webui) : _switch(rcswitch), _myIr(ir), _webui(webui)
	{
	}

	bool getPowerState(int number)
	{
		return this->lightStates[number%N_DIPSWITCHES];
	}


	void Send(dipswitch* dp, int number, bool onoff)
	{
		dipswitch measure;
		int tstrlen;
		if (onoff == false)
		{
			lightStates[number%N_DIPSWITCHES] = false;
			Serial.print("Turn Off via ");
			if (strlen(dp->tri2) > 2)
			{
				Serial.println("tristate");
				this->_switch->sendTriState(dp->tri2);
			}
			else if (dp->irhz != 0)
			{
				Serial.println("ir");

				for (int i = 0; i <(int)(sizeof(measure.irDataOff) / sizeof(uint16_t)); i++)
				{
					if (dp->irDataOff[i] == 0xc1a0)
					{
						_myIr->sendRaw(dp->irDataOff, i , dp->irhz);
						break;
					}
				}
			}
			else   if (strlen(dp->urlOff) > 2)
			{
				Serial.println("url");
				this->_webui->SetUrlToCall(strdup(dp->urlOff));
			}
			else 
			{
				Serial.println("house/device code");
				this->_switch->switchOff(dp->housecode, dp->code);
			}
			
		}
		else
		{
			lightStates[number%N_DIPSWITCHES] = true;
			Serial.print("Turn On via ");
			if ((tstrlen = strlen(dp->tri1)) > 2)
			{
				Serial.println(dp->tri1);
				if (StrFunc::commaCount((char *)dp->tri1) == 2)
				{
					char *comma1Pos = StrFunc::indexOf((char *)dp->tri1,(char*) ",", tstrlen);
					char *comma2Pos = StrFunc::indexOf(comma1Pos + 1,(char*) ",", strlen(comma1Pos+1));
					if (comma1Pos > 0 && comma2Pos > 0)
					{
						int dezimal = atoi(dp->tri1);
						int bitcount = atoi(comma1Pos + 1);
						int protocolno = atoi(comma2Pos + 1);
						Serial.print("dezimal: ");
						Serial.println(dezimal);
						Serial.print("bit: ");
						Serial.println(bitcount);
						Serial.print("protocol : ");
						Serial.println(protocolno);
						this->_switch->setProtocol(protocolno);
						this->_switch->send(dezimal, bitcount);
						this->_switch->setProtocol(1);
					}
				}
				else
				{
					Serial.println("tristate");
					this->_switch->sendTriState(dp->tri1);
				}
			}
			else if (dp->irhz != 0)
			{
				Serial.println("ir");
				for (int i = 0; i <(int)(sizeof(measure.irDataOn) / sizeof(uint16_t)); i++)
				{
					if (dp->irDataOn[i] == 0xc1a0)
					{
						_myIr->sendRaw(dp->irDataOn, i, dp->irhz);
						break;
					}
				}
			}
			else if (strlen(dp->urlOn) > 2)
			{
				Serial.println("url");
				this->_webui->SetUrlToCall(strdup(dp->urlOn));
			}
			else
			{
				Serial.println("house/device code");
				this->_switch->switchOn(dp->housecode, dp->code);
			}
		}

	}
};
