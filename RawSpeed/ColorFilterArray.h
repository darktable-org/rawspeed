#pragma once
#include "RawDecoderException.h"

typedef enum {
  CFA_COLOR_MIN = 0,
  CFA_RED = 0,
  CFA_GREEN = 1,
  CFA_BLUE = 2,
  CFA_GREEN2 = 3,
  CFA_CYAN = 4,
  CFA_MAGENTA = 5,
  CFA_YELLOW = 6,
  CFA_WHITE = 7,
  CFA_COLOR_MAX = 8,
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
  void setColorAt(iPoint2D pos, CFAColor c);
  void setCFA(guchar dcrawCode);
  __inline CFAColor getColorAt(guint x, guint y) {return cfa[(x&1)+((y&1)<<1)];}
  guint toDcrawColor(CFAColor c);
  guint getDcrawFilter();
  std::string asString();
private:
  std::string colorToString(CFAColor c);
  CFAColor cfa[4];
};
