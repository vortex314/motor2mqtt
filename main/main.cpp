/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <Poller.h>
#include <Triac.h>
#include <driver/mcpwm.h>
#include <limero.h>
#include <stdio.h>

#include "LedBlinker.h"
#include "esp_spi_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
bool pwmInit(mcpwm_unit_t mcpwmUnit, mcpwm_timer_t mcpwmTimer, int pinPwm,
             int pinSync, float pwm, uint32_t frequency) {
  INFO(" MCPWM init PCPWM[%d] TIMER[%d] pin PWM : %d, pin SYNC : %d ",
       mcpwmUnit, mcpwmTimer, pinPwm, pinSync);
  esp_err_t _rc = mcpwm_gpio_init(mcpwmUnit, MCPWM0A, pinPwm);
  if (_rc) {
    WARN("mcpwm_gpio_init()=%d", _rc);
    return false;
  };
  if (pinSync) {
    _rc = mcpwm_gpio_init(mcpwmUnit, MCPWM_SYNC_0, pinSync);
    if (_rc) {
      WARN("mcpwm_gpio_init()=%d", _rc);
      return false;
    };
  }
  mcpwm_config_t mcpwmConfig = {.frequency = frequency,
                                .cmpr_a = pwm,
                                .cmpr_b = 0.0,
                                .duty_mode = MCPWM_DUTY_MODE_0,
                                .counter_mode = MCPWM_UP_COUNTER};
  _rc = mcpwm_init(mcpwmUnit, mcpwmTimer, &mcpwmConfig);
  if (_rc) {
    WARN("mcpwm_init()=%d", _rc);
    return false;
  };
  if (pinSync) {
    _rc = mcpwm_sync_enable(mcpwmUnit, mcpwmTimer, MCPWM_SELECT_SYNC0, 750);
    if (_rc) {
      WARN("mcpwm_sync_enable()=%d", _rc);
      return false;
    };
  }
  return true;
}

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
Connector uextTriac(1);
Triac triac(workerThread, uextTriac);

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

TimerSource ticker(workerThread, 10, true);

extern "C" void app_main(void) {
#ifdef HOSTNAME
  Sys::hostname(S(HOSTNAME));
#endif
  INFO("%s : %s ", systemHostname().c_str(), systemBuild().c_str());
  //-----------------------------------------------------------------  SYS props
  poller >> systemUptime >> mqtt.toTopic<uint64_t>("system/upTime");
  poller >> systemHeap >> mqtt.toTopic<uint32_t>("system/heap");
  poller >> systemHostname >> mqtt.toTopic<std::string>("system/hostname");
  poller >> systemBuild >> mqtt.toTopic<std::string>("system/build");
  poller >> systemAlive >> mqtt.toTopic<bool>("system/alive");

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
  led.init();
  mqtt.connected >> led.blinkSlow;
  mqtt.connected >> poller.connected;
  //------------------------------------------------------------------- TRIAC
  /*if (triac.init()) {
    triac.interrupts >> mqtt.toTopic<uint64_t>("triac/interrupts");
    triac.current >> mqtt.toTopic<int>("triac/current");
  } else {
    WARN(" TRIAC initilaization failed. ");
  }*/
  //------------------------------------------------------------------- PWM
  pwmInit(MCPWM_UNIT_0, MCPWM_TIMER_0, 19, 25, 1.0, 100);
  pwmInit(MCPWM_UNIT_1, MCPWM_TIMER_0, 22, 0, 50.0, 100);
  ticker >> [](const TimerMsg& tm) {
    static int phase = 0;
    mcpwm_sync_enable(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_SELECT_SYNC0, 1000-phase);
    phase += 1;
    if (phase >= 1000) phase = 0;
  };
  ledThread.start();
  mqttThread.start();
  workerThread.start();
  thisThread.run();  // DON'T EXIT , local variable will be destroyed
}
