// SinusoidFit.h — pure math: 3-parameter least-squares fit of
//   signal(angleDeg) = a · sin(angle) + b · cos(angle) + dc
// over accumulated samples. Used by the homing routine to recover the
// gravity-up direction from the motor-current sinusoid.
//
// No I/O, no hardware references. Validated by 05_calibration.
#pragma once

#include <Arduino.h>

class SinusoidFit {
 public:
  void reset();
  void accumulate(float angleDeg, float signal);
  bool solve(float &a, float &b, float &dc) const;
  // Returns gravity-up angle in degrees [0, 360) or NAN on insufficient data.
  float gravityUpDeg() const;
  int   sampleCount() const { return _nSamples; }

 private:
  float _Sxx = 0, _Syy = 0, _Sxy = 0;
  float _Sx  = 0, _Sy  = 0;
  float _Sxz = 0, _Syz = 0, _Sz = 0;
  int   _nSamples   = 0;
};
