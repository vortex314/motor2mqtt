#include <Triac.h>
/* Connections
SCL     INPUT   Zero detect , low on rising edge
TXD     OUTPUT  triac trigger
SDA     INPUT   current sensor ACS712ELCTR

*/
#include <driver/gpio.h>
#include <driver/mcpwm.h>
/*
ESP32 PWM initialization
- pinPwm : output
- pinSync : pin for MCPWM_SYNC_0 to PWM unit
- pwm
- frequency : periodicity of pwm pulse
*/
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

Triac::Triac(Thread& thread, Uext& uext)
    : Actor(thread),
      _gpioZeroDetect(uext.toPin(LP_SCL)),
      _gpioTriac(uext.toPin(LP_TXD)),
      _adcCurrent(uext.getADC(LP_RXD)),
      _measureTimer(thread, 1000, true),
      _controlTimer(thread, 100, true) {
  // convert phase 0-180 to 1000->0
  phase >> [](const int ph) {
    esp_err_t _rc =
        mcpwm_sync_enable(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_SELECT_SYNC0,
                          1000 - ((ph * 1000 / 180)));
    if (_rc) {
      WARN("mcpwm_sync_enable()=%d", _rc);
    };
  };

  // report measurements
  _measureTimer >> [&](const TimerMsg& tm) {
    current = _adcCurrent.getValue();
    INFO("   current : %d ", current());
  };

  _controlTimer >> [&](const TimerMsg& tm) {
    error = rpmTarget() - rpmMeasured();
    float delta = pid(error());
    phase.on(130.0 + delta);
  };
}

bool Triac::init() {
  // PWM drives directly triac with 100 Hz and 1 %  pulse width
  pwmInit(MCPWM_UNIT_0, MCPWM_TIMER_0, _gpioTriac, _gpioZeroDetect, 1.0, 100);
  phase.on(170);
  return _adcCurrent.init() == 0;
}

float Triac::pid(float err) {
  integral = integral() + (err * _interval);
  derivative = (err - _errorPrev) / _interval;
  float integralPart = KI() * integral();
  if (integralPart > MAX_INTEGRAL) integral = MAX_INTEGRAL / KI();
  if (integralPart < -MAX_INTEGRAL) integral = -MAX_INTEGRAL / KI();
  float output = KP() * err + KI() * integral() + KD() * derivative();
  _errorPrev = err;
  return output;
}