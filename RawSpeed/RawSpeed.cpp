// RawSpeed.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "FileReader.h"
#include "TiffParser.h"
#include "RawDecoder.h"
#include "libgfl.h"

void BitBlt(BYTE* dstp, int dst_pitch, const BYTE* srcp, int src_pitch, int row_size, int height) {
  for (int y=height; y>0; --y) {
    memcpy(dstp, srcp, row_size);
    dstp += dst_pitch;
    srcp += src_pitch;
  }
}

int startTime;

void OpenFile(FileReader f) {
  RawDecoder *d = 0;
  FileMap* m = 0;
  try {
    wprintf(L"Opening:%s\n",f.Filename());
    m = f.readFile();
    TiffParser t(m);
    t.parseData();
    d = t.getDecompressor();

    startTime = GetTickCount();
    try {
      d->decodeRaw();
      wprintf(L"Decoding %s took: %u ms\n", f.Filename(), GetTickCount()-startTime);

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

      char ascii[1024];
      WideCharToMultiByte(CP_ACP, 0, f.Filename(), -1, ascii, 1024, NULL, NULL);
      string savename(ascii);
      size_t index = savename.rfind('.');
      savename = savename.substr(0,index).append(".tiff");

      gflSaveBitmap((char*)savename.c_str(),b,&s);
      gflFreeBitmap(b);
    } catch (RawDecoderException e) {
      wchar_t uni[1024];
      MultiByteToWideChar(CP_ACP, 0, e.what(), -1, uni, 1024);
      //    MessageBox(0,uni, L"RawDecoder Exception",0);
      wprintf(L"Raw Decoder Exception:%s\n",uni);
    }
  } catch (TiffParserException e) {
    wchar_t uni[1024];
    MultiByteToWideChar(CP_ACP, 0, e.what(), -1, uni, 1024);
    //    MessageBox(0,uni, L"Tiff Parser error",0);
    wprintf(L"Tiff Exception:%s\n",uni);
  }
  if (d) delete d;
  if (m) delete m;

}

int wmain(int argc, _TCHAR* argv[])
{
  GFL_ERROR err;
  err = gflLibraryInit();
  if (err) {
    string errSt = string("Could not initialize GFL library. Library returned: ") + string(gflGetErrorString(err));
    return 1;
  }

/*  OpenFile(FileReader(L"..\\testimg\\5d.CR2"));
  OpenFile(FileReader(L"..\\testimg\\5d-raw.dng"));
  OpenFile(FileReader(L"..\\testimg\\Canon_EOS_1Ds_Mk3-2.cr2"));
  OpenFile(FileReader(L"..\\testimg\\Canon_EOS_20D-demosaic.cr2"));
  OpenFile(FileReader(L"..\\testimg\\Canon_EOS_30D.cr2"));
  OpenFile(FileReader(L"..\\testimg\\Canon_EOS_450D.cr2"));
  OpenFile(FileReader(L"..\\testimg\\Canon_EOS_350d.cr2"));
  OpenFile(FileReader(L"..\\testimg\\Canon_EOS_40D.cr2"));
  OpenFile(FileReader(L"..\\testimg\\Canon_EOS_50D.cr2"));
  OpenFile(FileReader(L"..\\testimg\\Canon_EOS_450D-2.cr2"));
  OpenFile(FileReader(L"..\\testimg\\Canon_Powershot_G10.cr2"));
  OpenFile(FileReader(L"..\\testimg\\Canon_PowerShot_G9.cr2"));
  OpenFile(FileReader(L"..\\testimg\\Canon_EOS_1D_Mk2.cr2"));
  OpenFile(FileReader(L"..\\testimg\\Canon_EOS_1000D.cr2"));
  OpenFile(FileReader(L"..\\testimg\\Canon_EOS_1D_Mk3.cr2"));
  OpenFile(FileReader(L"..\\testimg\\Canon_EOS_1Ds_Mk2.cr2"));
  OpenFile(FileReader(L"..\\testimg\\Canon_EOS_1Ds_Mk3.cr2"));
  OpenFile(FileReader(L"..\\testimg\\Canon_EOS_400D.cr2"));
  
    
  OpenFile(FileReader(L"..\\testimg\\Pentax_K100D.pef"));
  OpenFile(FileReader(L"..\\testimg\\Pentax_K10D.pef"));
  OpenFile(FileReader(L"..\\testimg\\Pentax_K20D.pef"));
  OpenFile(FileReader(L"..\\testimg\\Pentax_optio_33wr.pef"));
  
  
  OpenFile(FileReader(L"..\\testimg\\SONY-DSLR-A700.arw"));
  OpenFile(FileReader(L"..\\testimg\\SONY_A200.ARW"));
  OpenFile(FileReader(L"..\\testimg\\Sony_A300.arw"));
  OpenFile(FileReader(L"..\\testimg\\Sony_DSLR-A100-1.arw"));
  OpenFile(FileReader(L"..\\testimg\\Sony_DSLR-A350.arw"));
  OpenFile(FileReader(L"..\\testimg\\Sony_DSLR-A900-2.arw"));
  OpenFile(FileReader(L"..\\testimg\\Sony_DSLR-A900.arw"));
*/
  OpenFile(FileReader(L"..\\testimg\\Nikon_D200_compressed-1.nef"));
  OpenFile(FileReader(L"..\\testimg\\Nikon_D200-1.nef"));
  OpenFile(FileReader(L"..\\testimg\\NikonCoolPix8800.nef"));
  OpenFile(FileReader(L"..\\testimg\\Nikon_D100-1.nef"));
  OpenFile(FileReader(L"..\\testimg\\Nikon_D1H.nef"));
  OpenFile(FileReader(L"..\\testimg\\Nikon_D1X.nef"));
  OpenFile(FileReader(L"..\\testimg\\Nikon_D2H.nef"));
  OpenFile(FileReader(L"..\\testimg\\Nikon_D2X_sRGB.nef"));
  OpenFile(FileReader(L"..\\testimg\\Nikon_D3.nef"));
  OpenFile(FileReader(L"..\\testimg\\Nikon_D300.nef"));
  OpenFile(FileReader(L"..\\testimg\\Nikon_D40X.nef"));
  OpenFile(FileReader(L"..\\testimg\\Nikon_D40_(sRGB).nef"));
  OpenFile(FileReader(L"..\\testimg\\Nikon_D60-2.nef"));
  OpenFile(FileReader(L"..\\testimg\\Nikon_D60.nef"));
  OpenFile(FileReader(L"..\\testimg\\Nikon_D70.nef"));
  OpenFile(FileReader(L"..\\testimg\\Nikon_D700.nef"));
  OpenFile(FileReader(L"..\\testimg\\Nikon_D70s-3.nef"));
  OpenFile(FileReader(L"..\\testimg\\Nikon_D80_(sRGB).nef"));
  OpenFile(FileReader(L"..\\testimg\\Nikon_D90.nef"));
  OpenFile(FileReader(L"..\\testimg\\Nikon_E5400.nef"));
  OpenFile(FileReader(L"..\\testimg\\Nikon_E5700.nef"));
  OpenFile(FileReader(L"..\\testimg\\Nikon_E5700_(sRGB).nef"));

  MessageBox(0,L"Finished", L"Finished",0);
  gflLibraryExit();
  _CrtDumpMemoryLeaks();
	return 0;
}

