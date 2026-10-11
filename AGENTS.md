# AGENTS.md – MicroHomebridgeAlexaEsp8266

> Onboarding notes for AI agents and developers. Verified against the source as of commit `6d9352a`.
> User-facing docs (flashing, wiring, usage) live in [README.md](README.md) – don't duplicate them here.

## 1. What this is

An ESP8266 firmware (Arduino framework, C++) that **impersonates the `homebridge-alexa` plugin**.
Alexa's "Homebridge" skill sends Smart Home directives through the cloud MQTT broker
`homebridge.cloudwatch.net`. This firmware connects straight to that broker using a hand-written
minimal MQTT client and answers `Discover`, `TurnOn/TurnOff` and `ReportState`. No Raspberry Pi or
Homebridge server is needed (it's a fauxmoESP alternative).

Each configured "switch" (max **30**) fires **one** of these actions:

| Action | Library / mechanism | Pin |
|---|---|---|
| 433 MHz RF, DIP-switch house/device code | `RCSwitch::switchOn/Off(house, code)` | GPIO2 (D4) |
| 433 MHz RF, tri-state code / `decimal,bits,protocol` | `RCSwitch::sendTriState` / `send` | GPIO2 |
| Infrared raw timings | `IRsend::sendRaw(data, n, hz)` | GPIO3 (RX) |
| HTTP GET to a URL (e.g. ESPEasy) | `HTTPClient`, deferred to `loop()` | – |

## 2. Repository layout

```
/                              repo root (also the Arduino "sketchbook" folder)
├─ MicroHomebridgeAlexaEsp8266/  ← ALL first-party firmware source (PlatformIO src_dir)
│  ├─ MicroHomebridgeAlexaEsp8266.ino  entry point: globals, setup(), loop(), MQTT message dispatch
│  ├─ MiniMqttClient.h          MQTT 3.1.1 receive side + connection state (ESPAsyncTCP)
│  ├─ MiniMqttClientSender.h    MQTT send side, 8 KB send buffer, JSON template loading, ping
│  ├─ Estore.h                  persistent config in SPIFFS file /EEPROM.TXT (fixed binary layout)
│  ├─ WebInterface.h            HTTP handlers (config UI, REST-ish endpoints, static files)
│  ├─ WcFnRequestHandler.h      wildcard URI RequestHandler for ESP8266WebServer
│  ├─ RemoteControl.h           dispatches a switch to RF / IR / URL, tracks on/off state
│  ├─ FtpServ.h                 tiny async FTP server (port 21, active/PORT mode only)
│  ├─ StrFunc.h                 indexOf over non-NUL-terminated buffers, substrdup, commaCount
│  ├─ Progmem.h                 PROGMEM HTML for the AP-mode setup page
│  ├─ getserverkey.py           prints SHA1 TLS fingerprint of the MQTT broker (for SSL builds)
│  ├─ data/                     files to upload to SPIFFS (Angular UI *.gz + Alexa JSON templates)
│  │   └─ bkup/                 backup copy incl. a sample EEPROM.TXT (23 820 bytes)
│  ├─ AlexaDump.txt             captured real homebridge MQTT traffic (reference for message format)
│  ├─ daslog*.txt               old serial debug logs
│  ├─ *.vcxproj*, __vm/         Visual Studio + Visual Micro project files (generated)
├─ hardware/esp8266/esp8266/    submodule: esp8266/Arduino core @ 2.4.2
├─ libraries/                   submodules (pinned release versions – do NOT update casually)
│  ├─ AsyncTCP/   me-no-dev/ESPAsyncTCP @ 7e9ed22
│  ├─ IRRemote/   markszabo/IRremoteESP8266 @ v2.5.2
│  └─ rcswitch/   sui77/rc-switch @ 2.6.2
├─ platformio.ini               PlatformIO build (env `basic`, nodemcuv2, espressif8266@1.8.0 = core 2.4.2)
├─ .travis.yml                  CI using makeEspArduino for d1_mini / generic (1M) / nodemcuv2
├─ MicroHomebridgeAlexaEsp8266.sln
└─ .continue/rules/review.md    code-review checklist (memory, blocking, overflow, credentials)
```

The Angular web UI ships **only as compiled `.gz` bundles**. Its source isn't in this repo.

## 3. Build & flash

Always clone with submodules: `git clone --recursive …` (or `git submodule update --init`).

**PlatformIO** (most convenient):
```sh
pio run                    # build env "basic"
pio run -t upload          # upload_port is hard-coded to COM6 in platformio.ini – adjust
pio run -t uploadfs        # upload MicroHomebridgeAlexaEsp8266/data → SPIFFS
pio device monitor         # 115200 baud
```
- `lib_extra_dirs = libraries` makes PIO use the pinned submodule libs.
- Linker script `eagle.flash.4m1m.ld` (4 MB flash, 1 MB SPIFFS). The `generic` 1 MB build in CI uses `FLASH_DEF=1M256`.
- `env_default` is the legacy key. Newer PlatformIO wants `default_envs`.
- `platform = espressif8266@1.8.0` pins the **2.4.2** core. The code relies on 2.4.x APIs (SPIFFS, axTLS, `ESP8266WebServerSecureAxTLS.h`), so a newer core will probably break the build.

**Arduino IDE 1.8.x / Visual Micro:** set the repo root as the sketchbook folder and run
`python get.py` (Python 2.7) in `hardware/esp8266/esp8266/tools`. Details are in README.md.

**CI / Linux:** `makeEspArduino` (see `.travis.yml` and README). Output goes to `/tmp/mkESP/...`.

No unit tests exist. You verify on real hardware via the serial log (115200).
> No toolchain is installed in this workspace. Don't claim a change compiles unless you actually built it.

## 4. Runtime architecture

### Boot (`setup()` in the .ino)
1. `Serial.begin(115200)`, `IRsend(3)`, `RCSwitch.enableTransmit(2)`.
2. **Factory reset:** if `analogRead(A0) > 350` at boot, `Estore::setupEeprom(true)` formats SPIFFS.
3. `Estore::setupEeprom()` mounts SPIFFS, validates/creates `/EEPROM.TXT`, loads WiFi + homebridge credentials.
4. **Empty SSID → `EnterApMode()`:** open AP `EasyAlexa` (192.168.4.1), FTP server, and a minimal
   setup page (`GET /`, `POST /setup`). No MQTT in this mode.
5. Otherwise `setup_wifi()` (**blocks forever** until WiFi connects), mDNS `hb-alexa.local`,
   ArduinoOTA (no password), then `MiniMqttClient`, FTP, web server + routes.

### Main loop
`delay(10)` → `mqtt->loop()` → `web->handleClient()` → `ArduinoOTA.handle()` → if
`WebInterface::_urlToCall` is set, do a blocking `HTTPClient` GET, then free the string.
URL actions are **deferred** this way because they're triggered from MQTT/web callbacks.

### Object graph (all raw globals in the .ino, created with `new`, never deleted)
```
Estore* estore ──────────────┐
ESP8266WebServer* web        │
WebInterface* ui(estore, web, switchCallBack) ──► holds _urlToCall slot
RemoteControl* remote(&mySwitch, myIr, ui)  ──► lightStates[30]
MiniMqttClient* mqtt ──► MiniMqttClientSender (loads JSON templates)
FtpServ* ftp
```
Free functions `Wrapper*` forward ESP8266WebServer callbacks to `ui`, and `on()` registers
`WcFnRequestHandler`. `switchCallBack` → `remote->Send()`.

## 5. Alexa / MQTT protocol (the core of the project)

- Broker `homebridge.cloudwatch.net:1883` (port 8883 if `ASYNC_TCP_SSL_ENABLED` is set in
  `libraries/AsyncTCP/src/async_config.h`; the SHA1 fingerprint is hard-coded in `MiniMqttClient.h`).
- CONNECT: MQTT 3.1.1, clean session, keep-alive 60 s, **clientId = username = homebridge username**,
  password = homebridge password. PINGREQ every 30 s after SUBACK.
- SUBSCRIBE `command/#` (QoS 0). Responses are PUBLISHed (QoS 0) to `response/<username>/1`.
- Directives are recognized by **substring matching** on the raw buffer
  (`message()` in the .ino). The match depends on the exact JSON key order the skill sends:
  - `"name":"Discover","payloadVersion":"3","messageId":"` → discovery (throttled to 1 per 10 s)
  - `:"Alexa.PowerController","name":"Turn` + `On`/`Off` → switch
  - `"namespace":"Alexa","name":"ReportState"` → report state
- **Endpoint IDs are `Monarch-NN`** (`%02d` = slot index 0–29). `getNumber()` reads the two digits
  right before `","` that follows `{"endpointId":"`.
- Responses are built with `sprintf` from SPIFFS templates in `data/`:
  | File | Used for | printf args |
  |---|---|---|
  | `discoveryheader.json` | Discover.Response start | messageId |
  | `discoverydevice.json` | one per device, comma-joined | id, name, name |
  | `discoveryfooter.json` | closing `]}}}` | – |
  | `switch.json` | TurnOn/Off response | "ON"/"OFF", messageId+correlationToken fragment, id |
  | `state.json` | StateReport | "ON"/"OFF", messageId+correlationToken fragment, id |

  `getMessageStampDup()` copies the raw text from `"messageId":"` to the closing `"}` of the header,
  and that fragment is pasted into the template's `%s`. If a template is missing from SPIFFS, the sender
  falls back to `http://www.monarch.de/microhomebridge/<file>`. If any template is still missing,
  `allFilesLoaded=false` and **all incoming messages are silently ignored**.
- Receive path: `handleData` (async TCP callback) assumes a **2-byte MQTT remaining-length**
  (`len - 3 < remain`). Fragmented PUBLISHes are collected into a static heap buffer
  (4 KB-block sized). They're dispatched from `loop()` once complete (state `waitingformore`).
- Send path: everything is serialized into one `char _sendbuffer[8192]`. Each length is computed by
  rendering twice (first into the buffer as scratch, then for real). `stageSender()` marks
  `readytosend` and `loop()` writes it out. Partial writes continue in `handleAck`.

## 6. Persistence – `/EEPROM.TXT` (Estore.h)

The file has a fixed size of 23 820 bytes. Byte offsets:
```
0      'N'
1      'H'
2      N_DIPSWITCHES (30)
3      (uint8_t)sizeof(dipswitch)  → 786 & 0xFF = 18   (truncated! it's only a sanity marker)
4      30 × dipswitch (786 bytes each)           = 23 580
23584  ssid[36]
23620  password[80]
23700  homebridgeUsername[40]
23740  homebrdigePassword[80]   ← sic, field name typo is used everywhere
```
```c
struct dipswitch { char name[20]; char housecode[6]; char code[6]; char tri1[16]; char tri2[16];
                   char urlOn[160]; char urlOff[160]; uint16_t irhz;
                   uint16_t irDataOn[100]; uint16_t irDataOff[100]; };   // 786 bytes, IR lists end with 0xC1A0
```
- A slot is **free when `name[0]==0`**. Delete only zeroes that byte.
- If the header or size check fails, the file is **wiped and re-initialized**. So changing the
  struct, `N_DIPSWITCHES` or any `N_CHAR_*` constant erases every user's config on upgrade.
- `Estore::deviceList` is a static `std::map<char*,int>` rebuilt by `RefreshList()` (strdup'd names).
  It's keyed by **pointer**, so iteration order is heap-address order, not alphabetical.
- Users can download or replace EEPROM.TXT via FTP, then reboot.

## 7. Local HTTP & FTP surface

STA mode, port 80 (`setup()`):
| Route | Handler | Notes |
|---|---|---|
| `/` + any path (wildcard, any method) | `HandleAngular` | serves SPIFFS file (`.gz` preferred), `/` → `/index.html`; Content-Type always `text/html` |
| `GET /jsonList` | `HandleJsonList` | `[{"i":slot,"n":name},…]`, CORS `*` |
| `POST /estore` | `HandleEStore` | args `name,house,code,tri1,tri2,url1,url2,irhz,iron,iroff` (IR = comma-separated µs) → first free slot (no edit-in-place) |
| `GET /edelete?no=N` | `HandleEDelete` | N = slot index |
| `GET /esocket?no=N&sw=0/1` | `HandleESocket` | N = **N-th non-empty** switch (not the slot index!) |
| not found | `HandleNotFound` | maps `/styles*`, `/inline*`, `/poly*`, `/main*` to the Angular bundles |
Any request carrying `If-Modified-Since` gets a `304` back without further checks (crude caching).

AP mode: `GET /` (PROGMEM form) and `POST /setup` (`ssid,password,homebridgeusername,homebridgepassword,format`).
The success page says "restarting", but **the code doesn't restart**. The user has to reset the board.

FTP (`FtpServ.h`, port 21, both modes): accepts any USER/PASS, single client, static state, **active mode
(PORT) only**, flat root. Supported: USER PASS CWD PWD TYPE PORT LIST RETR STOR DELE.

## 8. RemoteControl dispatch order (`RemoteControl::Send`)

The first match wins, evaluated separately for ON and OFF:
1. `tri1`/`tri2` length > 2 → RF. For **ON only**, a `tri1` with exactly two commas is parsed as
   `decimal,bitcount,protocol` → `setProtocol(p); send(dec,bits); setProtocol(1)`. OFF always uses `sendTriState(tri2)`.
2. `irhz != 0` → `sendRaw` up to the `0xC1A0` sentinel.
3. URL length > 2 → `ui->SetUrlToCall(strdup(url))` (runs later in `loop()`).
4. Fallback → `switchOn/Off(housecode, code)`.
`lightStates[number % 30]` holds the reported power state. It's RAM only and not initialized, so the value is garbage after boot.

## 9. Coding conventions

- **Header-only classes.** Every `.h` defines its methods *and its static members*
  (e.g. `Estore::deviceList`, `MiniMqttClient::_rcvbuffer`, `FtpServ::_port`). They must be included
  from **one translation unit only** (the .ino). Adding a `.cpp` that includes them gives
  duplicate-symbol link errors, so move the static definitions first.
- Singletons via static `_myinstance` pointers so async-TCP C-style callbacks can reach the instance.
- C-style strings, `malloc`/`strdup`/`free`, `sprintf`, fixed buffers. `String` appears only at web-server boundaries.
- Tabs, CRLF line endings, `#pragma once`. Log messages mix English and German ("FEHLER", "nicht gefunden").
- Keep heap use low (~40 KB free heap on ESP8266). `_sendbuffer` alone takes 8 KB of the sender object.
- Don't bump submodules. README explicitly says the pinned versions are release versions.

## 10. Known gotchas / latent bugs (check before you touch nearby code)

- **Discovery overflow:** each device renders ~440 bytes, so more than ~18 devices overflow the 8 KB `_sendbuffer` (max is 30).
- `HandleJsonList` uses a 1024-byte buffer. 30 entries × up to ~45 bytes can overflow it.
- `handleSetupSSID` `strcpy`s user input into fixed fields with no length check. `HandleEStore` `memcpy`s `sizeof(field)` from shorter `c_str()` (over-read).
- `handleData` in `waitingformore` `memcpy`s without checking `_rcvbufferlen`.
- `getRemainLength` loops on `> 128` (should be `>= 0x80`), and the "header = 3 bytes" assumption only holds for 128–16383-byte packets.
- Reconnect does `new AsyncClient()` on every attempt without deleting the old one (leak). If a connect never fires a callback, the state stays `waitingForConnect` forever.
- `getMessageStampDup` does `while(true);` when the header end isn't found, which ends in a watchdog reset.
- `FtpServ` RETR: the filename sanitizing loop uses `strlen(hostname)` on an **uninitialized** buffer.
- `/esocket` index semantics (N-th non-empty) differ from the slot index used by `/jsonList`, `/edelete` and Alexa, so `lightStates` can diverge.
- Only one pending URL slot. A second action before `loop()` runs overwrites the first (and leaks it).
- `pingmillis` compares absolute `millis()` values, which breaks on the ~49-day rollover.
- `setup()` prints the WiFi and homebridge passwords to Serial. `.travis.yml` contains a committed GitHub `api_key`.
- IR uses GPIO3 = UART RX, so serial *input* is unusable (TX logging still works).
- `Estore` members aren't zero-initialized. If SPIFFS mounting fails early, `ssid` is garbage.

## 11. Useful references

- `MicroHomebridgeAlexaEsp8266/AlexaDump.txt` – real directive/response pairs (best spec for message shapes).
- Amazon Smart Home Skill API v3 message reference (link at the end of AlexaDump.txt).
- `.continue/rules/review.md` – review checklist that matches the risk areas above.
