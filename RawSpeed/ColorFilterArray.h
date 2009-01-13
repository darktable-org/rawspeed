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
} CFAColor;


class ColorFilterArray
{
public:
  ColorFilterArray();
  ColorFilterArray(iPoint2D size);
  ColorFilterArray(const ColorFilterArray& obj);
  virtual ~ColorFilterArray(void);

  CFAColor* mCFAColors;
  CFAColor getColorAt(const iPoint2D &pos);
  void setColorAt(const iPoint2D &pos, CFAColor c);
  vector<CFAColor> getUniqueColors();
  const iPoint2D& getSize() {return size;}
  int getColorNum();                            // Gets the number of unique colors.
  int getColorNumInRow(int n, CFAColor c);      // Get the number of this color in row n
  int getColorNumInCol(int n, CFAColor c);      // Get the number of this color in col n
  int getWidthMultiplier(CFAColor c);        // This will return the maximum number of this color in one row.
  int getHeightMultiplier(CFAColor c);       // This will get the accumulated number of rows where the color is present.
  ColorFilterArray& ColorFilterArray::operator= (const ColorFilterArray& f);
protected:
  iPoint2D size;
};
