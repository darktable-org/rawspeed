#pragma once

class BlackArea
{
public:
  BlackArea(int offset, int size, gboolean isVertical);
  virtual ~BlackArea(void);
  guint offset; // Offset in bayer pixels.
  guint size;   // Size in bayer pixels.
  gboolean isVertical;  // Otherwise horizontal
};
