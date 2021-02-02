/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <limero.h>
#include <stdio.h>

#include "LedBlinker.h"
#include "esp_spi_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

//____________________________________________________________________________________
//
class Poller : public Actor {
  TimerSource _pollInterval;
  std::vector<Requestable *> _requestables;
  uint32_t _idx = 0;

 public:
  ValueFlow<bool> connected;
  ValueFlow<uint32_t> interval = 500;
  Poller(Thread &t) : Actor(t), _pollInterval(t, 500, true) {
    _pollInterval >> [&](const TimerMsg tm) {
      if (_requestables.size() && connected())
        _requestables[_idx++ % _requestables.size()]->request();
    };
    interval >> [&](const uint32_t iv) { _pollInterval.interval(iv); };
  };

  template <class T>
  LambdaSource<T> &operator>>(LambdaSource<T> &source) {
    _requestables.push_back(&source);
    return source;
  }

  template <class T>
  ValueSource<T> &operator>>(ValueSource<T> &source) {
    _requestables.push_back(&source);
    return source;
  }

  template <class T>
  ValueFlow<T> &operator>>(ValueFlow<T> &source) {
    _requestables.push_back(&source);
    return source;
  }

  template <class T>
  RefSource<T> &operator>>(RefSource<T> &source) {
    _requestables.push_back(&source);
    return source;
  }
};

Log logger(1024);
// ---------------------------------------------- THREAD
Thread thisThread("main");
Thread ledThread("led");
Thread mqttThread("mqtt");
Thread workerThread("worker");
#define PIN_LED 2

LedBlinker led(ledThread, PIN_LED, 301);

#ifdef MQTT_SERIAL
#include <MqttSerial.h>
MqttSerial mqtt(mqttThread);
#else
#include <MqttWifi.h>
#include <Wifi.h>
Wifi wifi(mqttThread);
MqttWifi mqtt(mqttThread);
MqttOta mqttOta;
#endif

// ---------------------------------------------- system properties
ValueSource<std::string> systemBuild("NOT SET");
ValueSource<std::string> systemHostname("NOT SET");
ValueSource<bool> systemAlive = true;
LambdaSource<uint32_t> systemHeap([]() { return Sys::getFreeHeap(); });
LambdaSource<uint64_t> systemUptime([]() { return Sys::millis(); });
Poller poller(mqttThread);

extern "C" void app_main(void) {
#ifdef HOSTNAME
  Sys::hostname(S(HOSTNAME));
#else
  std::string hn;
  union {
    uint8_t macBytes[6];
    uint64_t macInt;
  };
  macInt = 0L;
  if (esp_read_mac(macBytes, ESP_MAC_WIFI_STA) != ESP_OK)
    WARN(" esp_base_mac_addr_get() failed.");
  string_format(hn, "ESP32-%d", macInt & 0xFFFF);
  Sys::hostname(hn.c_str());
#endif
  systemHostname = Sys::hostname();
  systemBuild = __DATE__ " " __TIME__;
  INFO("%s : %s ", Sys::hostname(), systemBuild().c_str());

#ifdef MQTT_SERIAL
  mqtt.init();
#ifdef ECHOTEST
  echoTest.init();
#endif

#else
  wifi.init();
  mqtt.init();
  wifi.connected >> mqtt.wifiConnected;
  //---------------------------------------------------------------  WIFI props
  poller >> wifi.macAddress >> mqtt.toTopic<std::string>("wifi/mac");
  poller >> wifi.ipAddress >> mqtt.toTopic<std::string>("wifi/ip");
  poller >> wifi.ssid >> mqtt.toTopic<std::string>("wifi/ssid");
  poller >> wifi.rssi >> mqtt.toTopic<int>("wifi/rssi");
  mqtt.blocks >> mqttOta.blocks;
#endif
  led.init();
  mqtt.connected >> led.blinkSlow;
  mqtt.connected >> poller.connected;
  //-----------------------------------------------------------------  SYS props
  poller >> systemUptime >> mqtt.toTopic<uint64_t>("system/upTime");
  poller >> systemHeap >> mqtt.toTopic<uint32_t>("system/heap");
  poller >> systemHostname >> mqtt.toTopic<std::string>("system/hostname");
  poller >> systemBuild >> mqtt.toTopic<std::string>("system/build");
  poller >> systemAlive >> mqtt.toTopic<bool>("system/alive");

  ledThread.start();
  mqttThread.start();
  workerThread.start();
  thisThread.run();  // DON'T EXIT , local variable will be destroyed
}