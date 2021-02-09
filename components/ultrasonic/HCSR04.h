#ifndef H_HCSR04
#define H_HCSR04

#include <Hardware.h>

class HCSR04 {
 private:
  DigitalOut& _trigger;
  DigitalIn& _echo;
  uint64_t _trigger_usec;
  uint64_t _echo_usec;
  uint64_t _delta_usec;

 public:
  HCSR04(Uext&);
  HCSR04(DigitalOut&, DigitalIn&);
  bool init();
  bool trigger();
  uint32_t getCentimeters();
  uint64_t getTime();
  static void onEcho(void* obj);
};

#endif
