#include <Tacho.h>

#include "soc/rtc.h"

/* measure RPM 100->10000
single pole magnet
10000 RPM = 10000/60 RPS = 166 Hz  => +/- 6 ms
100 RPM = 100/60 RPS = 1.66 Hz => 0.6 sec
*/
#define CAPTURE_FREQ 80000000
#define CAPTURE_DIVIDER 1
#define POLES_TACHO 2

#define CAP0_INT_EN BIT(27)  // Capture 0 interrupt bit
#define CAP1_INT_EN BIT(28)  // Capture 1 interrupt bit
#define CAP2_INT_EN BIT(29)  // Capture 2 interrupt bit

static mcpwm_dev_t* MCPWM[2] = {&MCPWM0, &MCPWM1};

Tacho::Tacho(Thread& thread, int pinTacho)
    : Actor(thread), _pinTacho(pinTacho), _reportTimer(thread, 150, true) {
  _mcpwm_num = MCPWM_UNIT_1;
};

bool Tacho::init() {
  INFO(" Tacho init , pin %d ", _pinTacho);
  esp_err_t rc;
  rc = mcpwm_gpio_init(_mcpwm_num, MCPWM_CAP_0, _pinTacho);
  if (rc != ESP_OK) {
    WARN("mcpwm_gpio_init()=%d", rc);
    return false;
  }
  // gpio_pulldown_en(GPIO_CAP0_IN);
  // no prescale == 0
  rc = mcpwm_capture_enable(_mcpwm_num, MCPWM_SELECT_CAP0, MCPWM_POS_EDGE,
                            CAPTURE_DIVIDER);
  if (rc != ESP_OK) {
    WARN("mcpwm_capture_enable()=%d", rc);
    return false;
  }

  MCPWM[_mcpwm_num]->int_ena.val = CAP0_INT_EN;
  // Set ISR Handler
  rc = mcpwm_isr_register(_mcpwm_num, isrHandler, this, ESP_INTR_FLAG_IRAM,
                          NULL);
  if (rc) {
    WARN("mcpwm_isr_register()=%d", rc);
    return false;
  }
  _reportTimer >> [&](const TimerMsg& tm) {
    INFO(" isrCounter : %u capture : %u APB clock : %u", _isrCounter, _capture,
         rtc_clk_apb_freq_get());
    if ((_capture != 0) && (_prevIsrCounter != _isrCounter)) {
      float secPerRotation =
          (_capture * 1.0) /
          (rtc_clk_apb_freq_get() /
           CAPTURE_DIVIDER);  // _capture is ticks per rotation
      float rotationPerSec = 1.0 / secPerRotation;
      _prevIsrCounter = _isrCounter;
      uint32_t _rpm = (rotationPerSec * 60)/POLES_TACHO;
      if ( _rpm < 20000 ) rpm = _rpm;
    } else {
      rpm = 0;
    }
  };
 /* mcpwm_config_t pwm_config;
  BZERO(pwm_config);
  pwm_config.frequency = 10000;  // frequency = 1000Hz,
  pwm_config.cmpr_a = 0;         // duty cycle of PWMxA = 0
  pwm_config.cmpr_b = 0;         // duty cycle of PWMxb = 0
  pwm_config.counter_mode = MCPWM_UP_COUNTER;
  pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
  rc = mcpwm_init(_mcpwm_num, MCPWM_TIMER_0, &pwm_config);
  if (rc != ESP_OK) {
    WARN("mcpwm_init()=%d", rc);
    return true;
  }*/
  return true;
}

void IRAM_ATTR
Tacho::isrHandler(void* pv) {  // ATTENTION !!! no float calculations in ISR
  Tacho* tacho = (Tacho*)pv;
  uint32_t mcpwm_intr_status;

  mcpwm_intr_status = MCPWM[tacho->_mcpwm_num]->int_st.val;  // Read interrupt
  tacho->_isrCounter++;

  // Check for interrupt on rising edge on CAP0 signal
  if (mcpwm_intr_status & CAP0_INT_EN) {
    uint32_t currentCapture =
        mcpwm_capture_signal_get_value(tacho->_mcpwm_num, MCPWM_SELECT_CAP0);
    // get capture signal counter value
    tacho->_capture = (currentCapture - tacho->_prevCapture);
    tacho->_prevCapture = currentCapture;
  }
  MCPWM[tacho->_mcpwm_num]->int_clr.val = mcpwm_intr_status;
}