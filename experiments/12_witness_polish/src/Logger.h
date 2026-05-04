// Logger.h — compressed delta-encoded log over Serial.
// Every 200 ms (called from main loop) emit one line of `key=value` pairs;
// only fields that *changed* since the last printed line are emitted.
// Silence == "still the same."
#pragma once

#include <Arduino.h>

#include "MotorIO.h"
#include "MotorState.h"

class Logger {
 public:
  explicit Logger(MotorState& m) : _m(m) {}
  void emit();              // call every loop iteration; emits at 200 ms cadence

 private:
  MotorState& _m;
  uint32_t _lastEmitMS = 0;

  struct LogState {
    bool initialized = false;
    char ph;
    float Tg;
    float sp;
    float p;
    float er;
    float a;
    float cur;
    int   tmp;
    float v;
  };
  LogState _prev;
};
