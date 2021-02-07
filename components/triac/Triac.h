
#include <Hardware.h>
#include <limero.h>

class Triac : public Actor {
  DigitalIn& _gpioZeroDetect;
  ADC& _adcCurrent;
  DigitalOut& _gpioTrigger;
  TimerSource _measureTimer;
  uint64_t _interrupts;
  int32_t _maxDelta = INT32_MIN;
  int32_t _minDelta = INT32_MAX;
  uint64_t _lastZeroDetect;

  static void zeroDetected(void*);
  void newZeroDetect();

 public:
  Triac(Thread&, Connector&);
  bool init();
  ValueSource<int> current;
  ValueFlow<int> angle;
  ValueSource<uint64_t> interrupts;
};