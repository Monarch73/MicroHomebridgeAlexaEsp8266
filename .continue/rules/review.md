---
invokable: true
---

Review this code for potential issues, including:

- **Memory Management:** Look for `malloc` without `free`, especially in string handling and buffers (e.g., `MiniMqttClient`). Ensure buffers are sized correctly to prevent overflows.
- **Concurrency & Blocking:** Identify blocking delays (`delay()`) inside the main loop or network callbacks that could disrupt WiFi or MQTT keep-alives.
- **Safety & input validation:** Check for potential buffer overflows when parsing network data (e.g., `strcpy`, `sprintf`, `strtok`). Verify boundaries when accessing arrays or parsing JSON-like structures manually.
- **ESP8266 Specifics:** Ensure code respects ESP8266 watchdog timers (yield/delay where necessary). Check for ISR safety if interrupts are used.
- **Web Server:** Review handlers for security risks (e.g., unvalidated input in `WebInterface.h`).
- **Hardcoded Credentials:** flagging any hardcoded WiFi or MQTT credentials that should be configurable.

Provide specific, actionable feedback for improvements.
