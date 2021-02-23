
#include <Hardware.h>
#include <limero.h>

class Triac : public Actor {
  uint32_t _gpioZeroDetect;
  uint32_t _gpioTriac;
  ADC& _adcCurrent;
  TimerSource _measureTimer;
  TimerSource _controlTimer;

  float _errorPrev = 0.0;
  float _interval = 0.1;

  float pid(float e);

 public:
  Triac(Thread&, Uext&);
  bool init();
  ValueSource<int> current;
  ValueFlow<float> phase;
  ValueSource<uint64_t> interrupts;
  ValueFlow<uint32_t> rpmTarget = 100;
  ValueFlow<uint32_t> rpmMeasured;
  ValueFlow<int> error;
  ValueFlow<float> proportional = 0.0, integral = 0.0, derivative = 0.0;
  ValueFlow<float> KP = -0.01;
  ValueFlow<float> KI = -0.01;
  ValueFlow<float> KD = 0.0;
  const float MAX_INTEGRAL = 5;
};