#include "Logger.h"

static inline bool fchg(float a, float b, float eps) {
  return fabsf(a - b) >= eps;
}

void Logger::emit() {
  if (millis() - _lastEmitMS < 200) return;
  _lastEmitMS = millis();

  // Snapshot.
  char  ph  = phaseCode(_m.state);
  float Tg  = _m.currentTargetDeg;
  float sp  = _m.currentTargetDeg;        // for spin/diag we use the live integrated/eased setpoint
  // (MoveOperator and Calibrator advance currentTargetDeg directly during their phases,
  //  so reading it back gives the current setpoint without the operator having to
  //  expose its internal interpolation state.)
  float p   = _m.readEncoderDeg();
  float er  = sp - p;
  float a   = MotorIO::actualRPM();
  float cur = MotorIO::currentMA();
  int   tmp = static_cast<int>(MotorIO::tempC());
  float v   = MotorIO::vinV();

  String d;
  d.reserve(140);
  bool first    = !_prev.initialized;
  bool outputOn = (_m.state != ST_RELEASED);

  if (first || _prev.ph  != ph)            { d += ",ph="; d += ph;          _prev.ph = ph; }
  if (first || fchg(_prev.Tg,  Tg,  0.05f)){ d += ",Tg="; d += String(Tg, 2); _prev.Tg  = Tg;  }
  if (first || fchg(_prev.sp,  sp,  0.05f)){ d += ",sp="; d += String(sp, 2); _prev.sp  = sp;  }
  if (first || fchg(_prev.p,   p,   0.05f)){ d += ",p=";  d += String(p,  2); _prev.p   = p;   }
  if (first || fchg(_prev.er,  er,  0.05f)){ d += ",er="; d += String(er, 2); _prev.er  = er;  }
  if (first || fchg(_prev.a,   a,   0.1f)) { d += ",a=";  d += String(a,  1); _prev.a   = a;   }
  if (outputOn && (first || fchg(_prev.cur, cur, 5.0f))) {
    d += ",cur="; d += String(cur, 0);     _prev.cur = cur;
  }
  if (outputOn && (first || _prev.tmp != tmp)) {
    d += ",tmp="; d += String(tmp);        _prev.tmp = tmp;
  }
  if (first || fchg(_prev.v,   v,   0.05f)){ d += ",v=";  d += String(v,  2); _prev.v   = v;   }

  _prev.initialized = true;
  if (d.length() > 0) {
    Serial.print("t="); Serial.print(millis());
    Serial.println(d);
  }
}
