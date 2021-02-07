#include <Triac.h>
/* Connections
SCL     INPUT   Zero detect , low on rising edge
TXD     OUTPUT  triac trigger
SDA     INPUT   current sensor ACS712ELCTR

*/
#include <driver/gpio.h>

void Triac::newZeroDetect() {
  int32_t delta = Sys::micros() - _lastZeroDetect;
  if (delta < 100000) {
    _maxDelta = delta > _maxDelta ? delta : _maxDelta;
    _minDelta = delta < _minDelta ? delta : _minDelta;
  }
  _lastZeroDetect = Sys::micros();
}

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

Triac::Triac(Thread& thread, Connector& uext)
    : Actor(thread),
      _gpioZeroDetect(uext.getDigitalIn(LP_SCL)),
      _adcCurrent(uext.getADC(LP_RXD)),
      _gpioTrigger(uext.getDigitalOut(LP_TXD)),  // RXD ?
      _measureTimer(thread, 1000, true) {
  _gpioZeroDetect.onChange(DigitalIn::DIN_CHANGE, zeroDetected, this);
  _gpioTrigger.setMode(DigitalOut::DOUT_PULL_DOWN);

  _measureTimer >> [&](const TimerMsg& tm) {
    interrupts = _interrupts;
    current = _adcCurrent.getValue();
    INFO("  delta zero min %d µsec,max %d µsec , interrupts : %ld", _minDelta,
         _maxDelta, _interrupts);
    _minDelta = INT32_MAX;
    _maxDelta = INT32_MIN;
  };
}

bool Triac::init() {
  return _gpioZeroDetect.init() == 0 && _gpioTrigger.init() == 0 &&
         _adcCurrent.init() == 0;
}