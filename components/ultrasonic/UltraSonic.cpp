#include "UltraSonic.h"

UltraSonic::UltraSonic(Thread& thr, Connector* connector)
    : Actor(thr), Sink(3), _pollTimer(thr, 100, true) {
  _connector = connector;
  _hcsr = new HCSR04(*_connector);
  distance = 0;
  delay = 0;
  _pollTimer >> this;
}

UltraSonic::~UltraSonic() { delete _hcsr; }

void UltraSonic::init() { _hcsr->init(); }

void UltraSonic::on(const TimerMsg& tm) {
  int cm = _hcsr->getCentimeters();
  if (cm < 400 && cm > 0) {
    distance = distance() + (cm - distance()) / 2;
    delay = delay() + (_hcsr->getTime() - delay()) / 2;
  }
  _hcsr->trigger();
}
