#pragma once
#include "RawDecoderException.h"

typedef enum {
  CFA_COLOR_MIN = 0,
  CFA_RED = 0,
  CFA_GREEN = 1,
  CFA_BLUE = 2,
  CFA_CYAN = 3,
  CFA_MAGENTA = 4,
  CFA_YELLOW = 5,
  CFA_WHITE = 6,
  CFA_COLOR_MAX = 6,
  CFA_UNKNOWN = 255
} CFAColor;

typedef enum {
  CFA_POS_UPPERLEFT,
  CFA_POS_UPPERRIGHT,
  CFA_POS_LOWERLEFT,
  CFA_POS_LOWERRIGHT
} CFAPos;

class ColorFilterArray
{
public:
  ColorFilterArray(void);
  ColorFilterArray(CFAColor up_left, CFAColor up_right, CFAColor down_left, CFAColor down_right);
  virtual ~ColorFilterArray(void);
  void setCFA(CFAColor up_left, CFAColor up_right, CFAColor down_left, CFAColor down_right);
  void setCFA(guint dcrawCode);
  __inline CFAColor getColorAt(guint x, guint y) {return cfa[(x&1)+((y&1)<<1)];}
  guint toDcrawColor(CFAColor c);
  guint getDcrawFilter();
  void printOrder();
private:
  CFAColor cfa[4];
};
