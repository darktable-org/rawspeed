// RawSpeed.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "FileReader.h"
#include "TiffParser.h"
#include "RawDecompressor.h"
#include "libgfl.h"

int wmain(int argc, _TCHAR* argv[])
{
  GFL_ERROR err;
  err = gflLibraryInit();
  if (err) {
    string errSt = string("Could not initialize GFL library. Library returned: ") + string(gflGetErrorString(err));
    return 1;
  }

  FileReader f(L"5d-raw.dng");
  //FileReader f(L"IMG_3304.dng");
  FileMap* m = f.readFile();
//  RgbImage *img;
  int startTime;
  try {
    TiffParser t(m);
    t.parseData();
    RawDecompressor *d = t.getDecompressor();
/*    try {
      img = d->readPreview(TiffParser::PT_smallest);
    } catch (ThumbnailGeneratorException) {
      try {
        img = t.readPreview(TiffParser::PT_smallest);
      } catch (ThumbnailGeneratorException) {
        img = 0;
      }
    }*/
    startTime = GetTickCount();
    d->decodeRaw();
    wchar_t buf[200];
    swprintf(buf,200,L"Load took: %u ms\n", GetTickCount()-startTime);
    GFL_BITMAP* b;
    if (d->mRaw->isCFA)
      b = gflAllockBitmapEx(GFL_GREY,d->mRaw->dim.x, d->mRaw->dim.y,16,16,NULL);
    else
      b = gflAllockBitmapEx(GFL_RGB,d->mRaw->dim.x, d->mRaw->dim.y,16,8,NULL);

    memcpy(b->Data,d->mRaw->getData(),b->BytesPerLine*b->Height);
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

