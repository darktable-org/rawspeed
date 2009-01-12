// RawSpeed.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "FileReader.h"
#include "TiffParser.h"
#include "RawDecompressor.h"
#include "libgfl.h"

void BitBlt(BYTE* dstp, int dst_pitch, const BYTE* srcp, int src_pitch, int row_size, int height) {
  for (int y=height; y>0; --y) {
    memcpy(dstp, srcp, row_size);
    dstp += dst_pitch;
    srcp += src_pitch;
  }
}

int wmain(int argc, _TCHAR* argv[])
{
  GFL_ERROR err;
  err = gflLibraryInit();
  if (err) {
    string errSt = string("Could not initialize GFL library. Library returned: ") + string(gflGetErrorString(err));
    return 1;
  }

  FileReader f(L"..\\testimg\\Pentax_K20D.pef");
  FileMap* m = f.readFile();
  int startTime;
  try {
    TiffParser t(m);
    t.parseData();
    RawDecompressor *d = t.getDecompressor();

    startTime = GetTickCount();
    d->decodeRaw();

    wchar_t buf[200];
    swprintf(buf,200,L"Load took: %u ms\n", GetTickCount()-startTime);
    
    RawImage r = d->mRaw;

    GFL_BITMAP* b;
    if (r->isCFA)
      b = gflAllockBitmapEx(GFL_GREY,d->mRaw->dim.x, d->mRaw->dim.y,16,16,NULL);
    else
      b = gflAllockBitmapEx(GFL_RGB,d->mRaw->dim.x, d->mRaw->dim.y,16,8,NULL);

    BitBlt(b->Data,b->BytesPerLine, r->getData(),r->pitch, r->dim.x*r->bpp, r->dim.y );

    GFL_SAVE_PARAMS s;
    gflGetDefaultSaveParams(&s);
    s.FormatIndex = gflGetFormatIndexByName("tiff");
    gflSaveBitmap("Output.tif",b,&s);
    MessageBox(0,buf,L"Jubii!",MB_OK);

  } catch (TiffParserException e) {
//    MessageBox(0, , "Tiff Parser error",0);
    return 1;
  }

  gflLibraryExit();
	return 0;
}

