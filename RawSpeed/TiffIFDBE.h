/*
 * Copyright 2008 Klaus Post. All rights reserved.
 */

#pragma once
#include "tiffifd.h"

class TiffIFDBE :
  public TiffIFD
{
public:
  TiffIFDBE();
  TiffIFDBE(FileMap* f, guint offset);
  virtual ~TiffIFDBE(void);
};
