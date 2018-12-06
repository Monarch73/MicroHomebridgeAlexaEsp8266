# MicroHomebridgeAlexaEsp8266

Homebridge-server implemantation for alexa on the esp8266

This Project uses an esp8266 board as a hub to control RF-Switches, IFR-Modules via Amazon Echo (Alexa) by leveraging a homebridge skill. You can also send out http-requests. 

# Flashing the release.

The release file is designed to run on D1-Mini-R2, NodeMCU and ESP-01 compatible esp8266 boards. 
Just download the esp8266 flash tool from [github](https://github.com/nodemcu/nodemcu-flasher)
and flash the latest released MicroHomebridge.bin. 


Check [here](https://www.monarch.de/index.php/s/RciDX8qf7x6PZSF?fbclid=IwAR2A837Jk7Xk7Gx5ad6TaKzAOVZIbkrlCuBSjCVWE28rNqn9v6d1KkzC2x0)
Download all files and unzip the tar-bzip2 file. It contains all supported precompiled binaries.

If nodemcu-flasher does not work for you, you can also use ESPTool to upload the bin file.

(https://github.com/igrr/esptool-ck/releases)
```sh
esptool.exe -vv -ce -bz 1M -cd none -cb 921600 -cp "COM7" -ca 0x0 -cz "0x100000" -ca 0x00000 -cf "MicroHomebridgeAlexaEsp8266.ino.bin"
```
# Upload of the resource files via FTP

In order for Microhomebridge to function properly, it requires you to upload the *.json and *.gz files. The FTP-Server build-in in Microhomebridge has a limited, basic functionality. It only supports a single concurrent connection.
It does not support any connection security features (i.e. SSL/TLS encryption). For filetransfer, the PORT-command is supported only. This requires the ESP-Module to be able to connect to your FTP-Client. In case of connectivity issues, please make sure that you
disabled all local firewalls (i.e. Windows-Firewall) and disabled peer-seperation feature in your Wifi Access Point.

On first boot, the esp module functions as a wifi hotspot. You will have to connect to a network called "easyalexa".
The IP-Address is 192.168.4.1. Any username/password will be accepted.

An example configuration in filezilla:
[[https://github.com/Monarch73/MicroHomebridgeAlexaEsp8266/blob/master/wiki_resource/ftp1.PNG|alt=ftp1]]

[[https://github.com/Monarch73/MicroHomebridgeAlexaEsp8266/blob/master/wiki_resource/ftp2.PNG|alt=ftp2]]

# Wiring

Connect a [RF-Transmitter Module](https://www.amazon.com/gp/product/B017AYH5G0) to your esp8266 device.
The data-connector should be connected (GPIO2). For NodeMcu's and D1mini's , this port is labled D4. On Esp01, its pin3. If you are using a different board, you need to look up GPIO2 in the pinout chart of your board.

To use IR-Functionality, connect the annode of an IR-LED to RX (GPIO3) and cathode through a resistor to GND. The value of the resistor is dependent on the forward-voltage of your IR-Led and the output voltage of your GPIOs. In any case 220 Ohms should be working ok.

# Usage

First thing you need is to activate and register a skill called "homebridge" with you alexa device.
Microhomebridge expects some resourcefiles in spiffs. These files need to be uploaded to spiffs using a ftp client. Please refere to the FTP-Section if you haven't done this yet.
The first time Microhomebridge runs on your esp8266, it starts off acting as an open Access Point named "EasyAlexa". Connect to it. The board will have the IP 192.168.4.1. 
Next, open a browser to open http://192.168.4.1. You will be asked to enter a ESSID and Password of your Wifi. You will also have to enter you credentials to the homebridge skill. Optionaly you can also format and reinitialize SPIFFS storage. This is the area where Microhomebridge stores configuration data permanently. Click Save and reset the device.

The esp8266 will now connect to the network. To identify the IP of your device, You can either use a network scanner (ie "Net Scan" for Android Devices) or a usp port monitor.
Use a WebBrowser to the ip of your device via http on port 80.
ie.: http://192.168.1.50

# Add a switch
In the "Switch Configuration" section, specify a name for the switch. This name will be recognized by the Echo later on so it's best to specify a distinct name.

There are 4 ways to control devices that MicroHomebridge supports:

Option 1.) Remote Control Sockets with DIP-Switch configuration

RF-Switches usaly have 10 dip-switches. The first 5 switches specify a "housecode" and the last 5 specify the device Code. The position of the switches need to be translated to 0 (=off position) and 1 (=on postion) IE.

Housecode    00100
Code         10011

Option  2) 
if your RF Switches don't have dip-switches, you can specify so called "TriState Codes". These TriState Codes can be taken from the output of rc-switch/AdvancedReceiveDemo-example. You need a receiver-module for this procedure.

option 3)
if you wan't to trigger other devices on your smarthome network, your can specify URLs that will be fired when alexa triggers a switch.

Option 4) Infrared Control
this option needs a Infrared LED wired up to your esp8266 module as described above. A standard LED might work too. This function leverages the sendRaw()-Method of the IRRemote-Library taken from https://github.com/markszabo/IRremoteESP8266 . 

Please note, you can only use one option at a time! If you configured more than one option, Microhomebridge might not behave as you'd expect.
 
Click Save and start the device recover function in the alexa app. The above configured device should be discovered and should be able to receive voice commands.

# Backup Settings

Microhomebridge stores all information about switches and Wifi-Settings in a SPIFFS-File called "EEPROM.txt". This file is accessable via FTP even during normal operation. You can download and upload this 
config file as you like. Be advised that you should reboot the esp-board after you exchanged this file.

# Setting up development environment.

##  Compile in Arduino Studio
- Requirements
	- Arduino IDE 1.8.x (https://www.arduino.cc/en/Main/Software)
	- Python 2.7!
	- Git (git for windows)
	
- Directions

Clone repository recursivly
```
git clone --recursive https://github.com/Monarch73/MicroHomebridgeAlexaEsp8266.git
```

execute get.py in hardware/esp8266/esp8266/tools

```bash
cd hardware/esp8266/esp8266/tools
python get.py
```
Start Arduino IDE and configure the newly cloned "MicroHomebridge" directory as your sketchbook-folder in settings. (ctrl ,). Also please remove the urls for the boardmanager that you might have added previously. MicroHomebridge doesn't require these settings. Hit ok and ignore popup about availability of updates for libraries. Don't update! The versions of the submodules are release verions.

Now you should be ready open Microhomebridge.ino, compile and flash it.

## Visual Studio Community 2017

Requirements for Windows users

- [Visual Studio 2017](https://www.visualstudio.com/vs/whatsnew/) Any Edtition will do. Visual Studio 2017 Community is for free for individuals.
- Make sure you have the c++ and git extension enabled in the Visual Studio Installer (can be launched any time from the start menu).
- [Visual Micro extension](http://www.visualmicro.com/page/Arduino-Visual-Studio-Downloads.aspx)
- [git for windows](https://git-scm.com/download/win)
- python2.7 for windows. I recommend using [chocolaty](https://chocolatey.org/install) to install the python2-package for windows.

Setup the project

- recursivly clone the project and all submodules to an empty directory (git clone --recursive https://github.com/Monarch73/MicroHomebridgeAlexaEsp8266.git)
- change the directory to MicroHomebridgeAlexaEsp8266\hardware\esp8266\esp8266\tools directory and run the get.py-file

```sh
c:
cd \
mkdir work
cd work
git clone --recursive https://github.com/Monarch73/MicroHomebridgeAlexaEsp8266.git
cd MicroHomeBridgeAlexaEsp8266\hardware\esp8266\esp8266\tools
python get.py
```
- launch Visual Studio 2017
- goto "tools->visualmicro->Configure arduiono ide locations"
- select "Visual Micro(noide)" from drop down
- in "sketchbook-locations" enter the directory where you cloned the repository (c:\work\MicroHomeBridgeAlexaEsp8266);
- click "OK"
- select "tools->visualstudio->rescan toolchains and libraries".

Visual Studio should now be prepared to open the solution file (.sln) and compile the project.

## Jenkins Build Agent

For the deployment files the following buildscript was used. Please note that the script assumes a agent running on Linux OR inside a Windows Linux Subsystem:

```sh
git clone --recursive https://github.com/Monarch73/MicroHomebridgeAlexaEsp8266.git mhb
cd mhb/hardware/esp8266/esp8266/tools
python get.py
cd ../../../../..
git clone https://github.com/plerup/makeEspArduino.git
make -f makeEspArduino/makeEspArduino.mk ESP_ROOT=mhb/hardware/esp8266/esp8266 BOARD=d1_mini FLASH_DEF=4M2M SKETCH=mhb/MicroHomebridgeAlexaEsp8266/MicroHomebridgeAlexaEsp8266.ino LIBS="mhb/hardware/esp8266/esp8266/libraries/ESP8266WiFi/src/ mhb/hardware/esp8266/esp8266/libraries/EEPROM/ mhb/libraries/AsyncTCP/src/ mhb/libraries/IRRemote/src/ mhb/libraries/rcswitch/ mhb/hardware/esp8266/esp8266/libraries/Hash/src/ mhb/hardware/esp8266/esp8266/libraries/ESP8266mDNS/ mhb/hardware/esp8266/esp8266/libraries/" all
make -f makeEspArduino/makeEspArduino.mk ESP_ROOT=mhb/hardware/esp8266/esp8266 BOARD=generic FLASH_DEF=1M256 SKETCH=mhb/MicroHomebridgeAlexaEsp8266/MicroHomebridgeAlexaEsp8266.ino LIBS="mhb/hardware/esp8266/esp8266/libraries/ESP8266WiFi/src/ mhb/hardware/esp8266/esp8266/libraries/EEPROM/ mhb/libraries/AsyncTCP/src/ mhb/libraries/IRRemote/src/ mhb/libraries/rcswitch/ mhb/hardware/esp8266/esp8266/libraries/Hash/src/ mhb/hardware/esp8266/esp8266/libraries/ESP8266mDNS/ mhb/hardware/esp8266/esp8266/libraries/" all
make -f makeEspArduino/makeEspArduino.mk ESP_ROOT=mhb/hardware/esp8266/esp8266 BOARD=nodemcuv2 FLASH_DEF=4M2M SKETCH=mhb/MicroHomebridgeAlexaEsp8266/MicroHomebridgeAlexaEsp8266.ino LIBS="mhb/hardware/esp8266/esp8266/libraries/ESP8266WiFi/src/ mhb/hardware/esp8266/esp8266/libraries/EEPROM/ mhb/libraries/AsyncTCP/src/ mhb/libraries/IRRemote/src/ mhb/libraries/rcswitch/ mhb/hardware/esp8266/esp8266/libraries/Hash/src/ mhb/hardware/esp8266/esp8266/libraries/ESP8266mDNS/ mhb/hardware/esp8266/esp8266/libraries/" all
mv /tmp/mkESP/MicroHomebridgeAlexaEsp8266_d1_mini/MicroHomebridgeAlexaEsp8266.bin MicroHomebridgeAlexaEsp8266_d1_mini.bin
mv /tmp/mkESP/MicroHomebridgeAlexaEsp8266_generic/MicroHomebridgeAlexaEsp8266.bin MicroHomebridgeAlexaEsp8266_generic.bin
mv /tmp/mkESP/MicroHomebridgeAlexaEsp8266_nodemcuv2/MicroHomebridgeAlexaEsp8266.bin MicroHomebridgeAlexaEsp8266_nodemcuv2.bin
tar -cjf MicroHomebridge_$BUILD_NUMBER.tar.bzip2 MicroHomebridgeAlexaEsp8266_*.bin




```

Have fun
