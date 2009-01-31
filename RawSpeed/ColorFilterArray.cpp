#include "StdAfx.h"
#include "ColorFilterArray.h"

ColorFilterArray::ColorFilterArray(void) {
  setCFA(CFA_UNKNOWN, CFA_UNKNOWN, CFA_UNKNOWN, CFA_UNKNOWN);
}

ColorFilterArray::ColorFilterArray( CFAColor up_left, CFAColor up_right, CFAColor down_left, CFAColor down_right )
{
  cfa[0] = up_left;
  cfa[1] = up_right;
  cfa[2] = down_left;
  cfa[3] = down_right;
}

ColorFilterArray::~ColorFilterArray(void)
{
}

void ColorFilterArray::setCFA( CFAColor up_left, CFAColor up_right, CFAColor down_left, CFAColor down_right )
{
  cfa[0] = up_left;
  cfa[1] = up_right;
  cfa[2] = down_left;
  cfa[3] = down_right;
}

void ColorFilterArray::setCFA( guint dcrawCode )
{
  cfa[0] = (CFAColor)(dcrawCode&0x3);
  cfa[1] = (CFAColor)((dcrawCode>>2)&0x3);
  cfa[2] = (CFAColor)((dcrawCode>>4)&0x3);
  cfa[3] = (CFAColor)((dcrawCode>>6)&0x3);
}

guint ColorFilterArray::getDcrawFilter() {
  if (cfa[0]>3 || cfa[1]>3 || cfa[2]>3 || cfa[3]>3)
    ThrowRDE("getDcrawFilter: Invalid colors defined.");
  guint v =  cfa[0] | cfa[1]<<2 | cfa[2]<<4 | cfa[3]<<6;
  return v | (v<<8) | (v<<16) | (v<<24);
}

