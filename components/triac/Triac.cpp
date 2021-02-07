#include <Triac.h>
/* Connections
SCL     INPUT   Zero detect , low on rising edge
TXD     OUTPUT  triac trigger
SDA     INPUT   current sensor ACS712ELCTR

*/
#include <driver/gpio.h>
#include <driver/mcpwm.h>

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

void Triac::newZeroDetect() {
  int32_t delta = Sys::micros() - _lastZeroDetect;
  if (delta < 100000) {
    _maxDelta = delta > _maxDelta ? delta : _maxDelta;
    _minDelta = delta < _minDelta ? delta : _minDelta;
  }
  _lastZeroDetect = Sys::micros();
}
/*
IRAM_ATTR void Triac::zeroDetected(void* pv) {
  Triac* pTriac = (Triac*)pv;
  pTriac->_interrupts++;
  pTriac->newZeroDetect();
  // pTriac->_gpioTrigger.write(1);
  Sys::delay(5);
  gpio_set_level((gpio_num_t)pTriac->_gpioTrigger.getPin(), 1);
  Sys::delay(1);
  gpio_set_level((gpio_num_t)pTriac->_gpioTrigger.getPin(), 0);
}
*/
Triac::Triac(Thread& thread, Connector& uext)
    : Actor(thread),
      _gpioZeroDetect(uext.getDigitalIn(LP_SCL)),
      _adcCurrent(uext.getADC(LP_RXD)),
      _gpioTrigger(uext.getDigitalOut(LP_TXD)),  // RXD ?
      _measureTimer(thread, 1000, true) {
  pwmInit(MCPWM_UNIT_0, MCPWM_TIMER_0, _gpioTrigger.getPin(),
          _gpioZeroDetect.getPin(), 1.0, 100);
  /*
    _gpioZeroDetect.onChange(DigitalIn::DIN_RAISE, zeroDetected, this);
    _gpioTrigger.setMode(DigitalOut::DOUT_PULL_DOWN);
  */
  phase >> [](const int ph) {
    esp_err_t _rc =
        mcpwm_sync_enable(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_SELECT_SYNC0,
                          1000 - ((ph * 1000 / 180)));
    if (_rc) {
      WARN("mcpwm_sync_enable()=%d", _rc);
    };
  };
  _measureTimer >> [&](const TimerMsg& tm) {
    interrupts = _interrupts;
    current = _adcCurrent.getValue();
    INFO("  delta zero min %d µsec,max %d µsec , interrupts : %ld", _minDelta,
         _maxDelta, _interrupts);
    _minDelta = INT32_MAX;
    _maxDelta = INT32_MIN;
  };
  phase.on(90);
}

bool Triac::init() {
  return _adcCurrent.init() == 0;
  /*  return _gpioZeroDetect.init() == 0 && _gpioTrigger.init() == 0 &&
           _adcCurrent.init() == 0;*/
}