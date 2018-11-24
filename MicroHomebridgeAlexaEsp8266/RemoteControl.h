#pragma once
#include "Estore.h"
#include <RCSwitch.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include "WebInterface.h"

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
		if (onoff == false)
		{
			lightStates[number%N_DIPSWITCHES] = true;
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
			Serial.print("Turn On via ");
			lightStates[number%N_DIPSWITCHES] = false;
			if (strlen(dp->tri1) > 2)
			{
				Serial.println("tristate");
				this->_switch->sendTriState(dp->tri1);
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
