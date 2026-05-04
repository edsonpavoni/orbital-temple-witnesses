#include "SinusoidFit.h"

void SinusoidFit::reset() {
  _Sxx = _Syy = _Sxy = 0.0f;
  _Sx = _Sy = 0.0f;
  _Sxz = _Syz = _Sz = 0.0f;
  _nSamples = 0;
}

void SinusoidFit::accumulate(float angleDeg, float signal) {
  float a = fmodf(angleDeg, 360.0f);
  if (a < 0) a += 360.0f;
  float r = a * (PI / 180.0f);
  float s = sinf(r);
  float c = cosf(r);
  _Sxx += s * s;
  _Syy += c * c;
  _Sxy += s * c;
  _Sx  += s;
  _Sy  += c;
  _Sxz += s * signal;
  _Syz += c * signal;
  _Sz  += signal;
  _nSamples   += 1;
}

bool SinusoidFit::solve(float &a, float &b, float &dc) const {
  // M · [a, b, dc]^T = r via Gauss elimination.
  // M = [[Sxx,Sxy,Sx],[Sxy,Syy,Sy],[Sx,Sy,N]] ; r = [Sxz,Syz,Sz]
  if (_nSamples < 4) return false;
  float M[3][4] = {
    {_Sxx, _Sxy, _Sx,                _Sxz},
    {_Sxy, _Syy, _Sy,                _Syz},
    {_Sx,  _Sy,  static_cast<float>(_nSamples), _Sz},
  };
  for (int i = 0; i < 3; i++) {
    int max_row = i;
    for (int k = i + 1; k < 3; k++) {
      if (fabsf(M[k][i]) > fabsf(M[max_row][i])) max_row = k;
    }
    if (fabsf(M[max_row][i]) < 1e-9f) return false;
    if (max_row != i) {
      for (int j = 0; j < 4; j++) {
        float tmp = M[i][j]; M[i][j] = M[max_row][j]; M[max_row][j] = tmp;
      }
    }
    for (int k = i + 1; k < 3; k++) {
      float factor = M[k][i] / M[i][i];
      for (int j = i; j < 4; j++) M[k][j] -= factor * M[i][j];
    }
  }
  float sol[3];
  for (int i = 2; i >= 0; i--) {
    float s = M[i][3];
    for (int j = i + 1; j < 3; j++) s -= M[i][j] * sol[j];
    sol[i] = s / M[i][i];
  }
  a = sol[0]; b = sol[1]; dc = sol[2];
  return true;
}

float SinusoidFit::gravityUpDeg() const {
  float a, b, dc;
  if (!solve(a, b, dc)) return NAN;
  float c = atan2f(b, a);
  float upRad = -c;
  float upDeg = upRad * (180.0f / PI);
  upDeg = fmodf(upDeg + 720.0f, 360.0f);
  return upDeg;
}
