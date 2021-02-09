#include <limero.h>

#include "driver/mcpwm.h"
#include "driver/pcnt.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"

class Tacho : public Actor {
  int _pinTacho;
  mcpwm_unit_t _mcpwm_num;

  TimerSource _reportTimer;
  uint64_t _isrCounter;
  uint32_t _capture;

 public:
  ValueSource<uint32_t> rpm;
  Tacho(Thread&, int pinPulse);
  bool init();
  static void IRAM_ATTR isrHandler(void* pv);
};