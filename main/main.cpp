/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <Poller.h>
#include <Tacho.h>
#include <Triac.h>
#include <limero.h>
#include <stdio.h>

#include "LedBlinker.h"
#include "esp_spi_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

Log logger(1024);
// ---------------------------------------------- THREAD
Thread thisThread("main");
Thread ledThread("led");
Thread mqttThread("mqtt");
ThreadProperties props = {.name = "worker",
                          .stackSize = 5000,
                          .queueSize = 20,
                          .priority = 24 | portPRIVILEGE_BIT};
Thread workerThread(props);
#define PIN_LED 2

LedBlinker led(ledThread, PIN_LED, 100);
Uext uextTriac(1);
Triac triac(workerThread, uextTriac);
Uext uextTacho(2);
Tacho tacho(workerThread, uextTacho.toPin(LP_MISO));

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
LambdaSource<std::string> systemBuild([]() { return __DATE__ " " __TIME__; });
LambdaSource<std::string> systemHostname([]() { return Sys::hostname(); });
LambdaSource<uint32_t> systemHeap([]() { return Sys::getFreeHeap(); });
LambdaSource<uint64_t> systemUptime([]() { return Sys::millis(); });
ValueSource<bool> systemAlive = true;

Poller poller(mqttThread);
/*
#include <UltraSonic.h>
Uext uextUs(2);
UltraSonic ultrasonic(thisThread, &uextUs);
*/
extern "C" void app_main(void) {
#ifdef HOSTNAME
  Sys::hostname(S(HOSTNAME));
#endif
#ifdef MQTT_SERIAL
  mqtt.init();
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
  INFO("%s : %s ", systemHostname().c_str(), systemBuild().c_str());
  //-----------------------------------------------------------------  SYS props
  poller >> systemUptime >> mqtt.toTopic<uint64_t>("system/upTime");
  poller >> systemHeap >> mqtt.toTopic<uint32_t>("system/heap");
  poller >> systemHostname >> mqtt.toTopic<std::string>("system/hostname");
  poller >> systemBuild >> mqtt.toTopic<std::string>("system/build");
  poller >> systemAlive >> mqtt.toTopic<bool>("system/alive");

  led.init();
  mqtt.connected >> led.blinkSlow;
  mqtt.connected >> poller.connected;
  //------------------------------------------------------------------- TRIAC
  if (triac.init() && tacho.init()) {
    uint32_t rt=500;
    INFO(" triac and tacho init succesful.");
    tacho.rpm >> triac.rpmMeasured;
  // passive properties
    poller >> triac.rpmTarget == mqtt.topic<uint32_t>("triac/rpmTarget");
    poller >> triac.KP == mqtt.topic<float>("triac/KP");
    poller >> triac.KI == mqtt.topic<float>("triac/KI");
    poller >> triac.KD == mqtt.topic<float>("triac/KD");
  // active properties
    triac.phase >> Cache<float>::nw(mqttThread,rt,1000) >> mqtt.topic<float>("triac/phase");
    triac.integral >> Cache<float>::nw(mqttThread,rt,1000) >> mqtt.topic<float>("triac/integral");
    triac.error >> Cache<int>::nw(mqttThread,rt,1000) >> mqtt.toTopic<int>("triac/error");
    triac.rpmMeasured >> Cache<uint32_t>::nw(mqttThread,rt,1000) >> mqtt.toTopic<uint32_t>("triac/rpmMeasured");
    triac.current >> Cache<int>::nw(mqttThread,rt,1000) >> mqtt.toTopic<int>("triac/current");

  } else {
    WARN(" TRIAC initialization failed. ");
  }
  // ultrasonic.init();
  // ultrasonic.distance >> mqtt.toTopic<int32_t>("us/distance");
  //------------------------------------------------------------------- PWM

  ledThread.start();
  mqttThread.start();
  workerThread.start();
  thisThread.run();  // DON'T EXIT , local variable will be destroyed
}
