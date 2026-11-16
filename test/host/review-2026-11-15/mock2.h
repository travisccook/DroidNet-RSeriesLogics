// scratch instrumented mock (review only) — records the selected sequence per unit
#pragma once
#include <stdint.h>
#include <stdio.h>
typedef uint8_t byte;
static uint32_t _mock_millis = 0;
inline uint32_t millis() { return _mock_millis; }
struct CHSV { uint8_t h,s,v; CHSV(int a,int b,int c):h((uint8_t)a),s((uint8_t)b),v((uint8_t)c){} };
struct CRGB { uint8_t r=0,g=0,b=0; CRGB(){} CRGB(int a,int c,int d):r((uint8_t)a),g((uint8_t)c),b((uint8_t)d){} CRGB(const CHSV& h):r(h.v),g(h.v),b(h.v){} };
struct LogicEngineRenderer {
  unsigned _id = 1; int _seq = 0; int _selects = 0; int _resets = 0; int _bri = -1;
  unsigned getID() const { return _id; }
  int width() const { return 8; } int height() const { return 10; }
  CRGB _px[10][8];
  void setPixelRGB(int x,int y,const CRGB& c){ if(x>=0&&x<8&&y>=0&&y<10) _px[y][x]=c; }
  void selectSequence(byte s,int=0,int=0,int=0){ _seq = s; _selects++; }
  void resetEffect(){ _resets++; }
  void setBrightness(byte b){ _bri=b; }
};
static LogicEngineRenderer _fld_obj, _rld_obj;
static LogicEngineRenderer* FLD = &_fld_obj;
static LogicEngineRenderer* RLD = &_rld_obj;
inline void FLD_selectSequence(byte s,int=0,int=0,int=0){ FLD->selectSequence(s); }
inline void RLD_selectSequence(byte s,int=0,int=0,int=0){ RLD->selectSequence(s); }
struct Marcduino { static const char* getCommand(){ return ""; } };
#define MARCDUINO_ACTION(name, marc, p)
