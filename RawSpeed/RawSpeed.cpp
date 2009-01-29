#include "stdafx.h"
/* 
    RawSpeed - RAW file decoder.

    Copyright (C) 2009 Klaus Post

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA

    http://www.klauspost.com
*/
#include "FileReader.h"
#include "TiffParser.h"
#include "RawDecoder.h"
#define _USE_GFL_
#ifdef _USE_GFL_
#include "libgfl.h"
#pragma comment(lib, "libgfl.lib") 
#endif


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
      for (guint i = 0; i < d->errors.size(); i++) {
        printf("Error Encoutered:%s", d->errors[i]);
      }

      RawImage r = d->mRaw;

#ifdef _USE_GFL_
      GFL_BITMAP* b;
      if (r->getCpp() == 1)
        b = gflAllockBitmapEx(GFL_GREY,d->mRaw->dim.x, d->mRaw->dim.y,16,16,NULL);
      else if (r->getCpp() == 3)
        b = gflAllockBitmapEx(GFL_RGB,d->mRaw->dim.x, d->mRaw->dim.y,16,8,NULL);
      else
        ThrowRDE("Unable to save image.");

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
#endif
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
#ifdef _USE_GFL_
  GFL_ERROR err;
  err = gflLibraryInit();
  if (err) {
    string errSt = string("Could not initialize GFL library. Library returned: ") + string(gflGetErrorString(err));
    return 1;
  }
#endif

  OpenFile(FileReader(L"..\\testimg\\5d.CR2"));
  OpenFile(FileReader(L"..\\testimg\\Canon_EOS_1Ds_Mk3-2.cr2"));
  OpenFile(FileReader(L"..\\testimg\\Canon_EOS_20D-demosaic.cr2"));
  OpenFile(FileReader(L"..\\testimg\\Canon_EOS_30D.cr2"));
  OpenFile(FileReader(L"..\\testimg\\Canon_EOS_450D.cr2"));
  OpenFile(FileReader(L"..\\testimg\\Canon_EOS_350d.cr2"));
  OpenFile(FileReader(L"..\\testimg\\Canon_EOS_40D.cr2"));
//  OpenFile(FileReader(L"..\\testimg\\Canon_EOS_50D.cr2"));
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

  OpenFile(FileReader(L"..\\testimg\\Nikon_D200_compressed-1.nef"));
  OpenFile(FileReader(L"..\\testimg\\NikonCoolPix8800.nef"));
  OpenFile(FileReader(L"..\\testimg\\Nikon_D1H.nef"));
  OpenFile(FileReader(L"..\\testimg\\Nikon_D1X.nef"));
  OpenFile(FileReader(L"..\\testimg\\Nikon_D2H.nef"));
  OpenFile(FileReader(L"..\\testimg\\Nikon_D2X_sRGB.nef"));
  OpenFile(FileReader(L"..\\testimg\\Nikon_D100-1.nef"));
  OpenFile(FileReader(L"..\\testimg\\Nikon_D200-1.nef"));
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

  OpenFile(FileReader(L"..\\testimg\\Olympus_500UZ.orf"));
  OpenFile(FileReader(L"..\\testimg\\Olympus_C7070WZ.orf"));
  OpenFile(FileReader(L"..\\testimg\\Olympus_C8080.orf"));
  OpenFile(FileReader(L"..\\testimg\\Olympus_E1.orf"));
  OpenFile(FileReader(L"..\\testimg\\Olympus_E10.orf"));
  OpenFile(FileReader(L"..\\testimg\\Olympus_E20.orf"));
  OpenFile(FileReader(L"..\\testimg\\Olympus_E3-2.orf"));
  OpenFile(FileReader(L"..\\testimg\\Olympus_E3-3.orf"));
  OpenFile(FileReader(L"..\\testimg\\Olympus_E3-4.orf"));
  OpenFile(FileReader(L"..\\testimg\\Olympus_E3.orf"));
  OpenFile(FileReader(L"..\\testimg\\Olympus_E300.orf"));
  OpenFile(FileReader(L"..\\testimg\\Olympus_E330.orf"));
  OpenFile(FileReader(L"..\\testimg\\Olympus_E400.orf"));
  OpenFile(FileReader(L"..\\testimg\\Olympus_E410-2.orf"));
  OpenFile(FileReader(L"..\\testimg\\Olympus_E410.orf"));
  OpenFile(FileReader(L"..\\testimg\\Olympus_E420.orf"));
  OpenFile(FileReader(L"..\\testimg\\Olympus_E500.orf"));
  OpenFile(FileReader(L"..\\testimg\\Olympus_E510-2.orf"));
  OpenFile(FileReader(L"..\\testimg\\Olympus_E510.orf"));
  OpenFile(FileReader(L"..\\testimg\\Olympus_E520-2.orf"));
  OpenFile(FileReader(L"..\\testimg\\Olympus_E520-3.orf"));
  OpenFile(FileReader(L"..\\testimg\\Olympus_E520-4.orf"));
  OpenFile(FileReader(L"..\\testimg\\Olympus_E520-5.orf"));
  OpenFile(FileReader(L"..\\testimg\\Olympus_E520.orf"));
  OpenFile(FileReader(L"..\\testimg\\Olympus_SP350.orf"));

  OpenFile(FileReader(L"..\\testimg\\dng\\5d-raw.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\5d.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\CANON-EOS10-linear.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\CANON-EOS10.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\CANON-EOS20D-linear.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\CANON-EOS20D.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\CANON-EOS300D-linear.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\CANON-EOS300D.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\CANON-POWERSHOTPRO1-linear.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\CANON-POWERSHOTPRO1.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_1000D.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_1Ds_Mk2.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_1Ds_Mk3-2.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_1Ds_Mk3.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_1D_Mk2.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_1D_Mk2_N.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_1D_Mk3.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_20D-demosaic.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_20d.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_30D-uga1.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_30D-uga2.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_30D.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_350d-2.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_350D-3.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_350d.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_400D.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_40D-2.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_40D.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_450D-2.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_450D-3.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_450D-4.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_450D-5.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_450D.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_5D.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_5D_Mk2-ISO100_sRAW1.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_5D_Mk2-ISO12800_sRAW1.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_5D_Mk2-ISO12800_sRAW2.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_EOS_Mk2-ISO100_sRAW2.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_Powershot_G10.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_Powershot_G9-1.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_Powershot_G9-2.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Canon_PowerShot_G9.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\FUJI-FINEPIXS2PRO-linear.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\FUJI-FINEPIXS2PRO.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\KODAK-DCSPRO-linear.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\KODAK-DCSPRO.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\M8-1-linear.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\M8-1.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\MINOLTA-DIMAGE5-linear.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\MINOLTA-DIMAGE5.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\MINOLTA-DIMAGE7HI-linear.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\MINOLTA-DIMAGE7HI.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\MINOLTA-DIMAGEA1-linear.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\MINOLTA-DIMAGEA1.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\MINOLTA-DYNAX7D-01-linear.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\MINOLTA-DYNAX7D-01.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\MINOLTA-DYNAX7D-02-linear.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\MINOLTA-DYNAX7D-02.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\MINOLTA-DYNAX7D-03-linear.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\MINOLTA-DYNAX7D-03.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\MINOLTA-DYNAX7D-04-linear.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\MINOLTA-DYNAX7D-04.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\MINOLTA-DYNAX7D-05-linear.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\MINOLTA-DYNAX7D-05.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\NIKON-COOLPIX5700-linear.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\NIKON-COOLPIX5700.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\NIKON-D100-linear.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\NIKON-D100.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\NIKON-D70-01-linear.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\NIKON-D70-01.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\NIKON-D70-02-linear.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\NIKON-D70-02.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\NikonCoolPix8800.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Nikon_D100-1.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Nikon_D1H.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Nikon_D1X.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Nikon_D200-1.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Nikon_D200_compressed-1.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Nikon_D2H.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Nikon_D2X_sRGB.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Nikon_D3.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Nikon_D300.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Nikon_D40X.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Nikon_D40_(sRGB).dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Nikon_D60-2.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Nikon_D60.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Nikon_D70.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Nikon_D700.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Nikon_D70s-3.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Nikon_D80_(sRGB).dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Nikon_D90.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Nikon_E5400.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Nikon_E5700.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Nikon_E5700_(sRGB).dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\OLYMPUS-C5050Z-linear.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\OLYMPUS-C5050Z.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\OLYMPUS-E10-linear.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\OLYMPUS-E10.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Olympus_500UZ.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Olympus_C7070WZ.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Olympus_C8080.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Olympus_E1.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Olympus_E10.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Olympus_E20.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Olympus_E3-2.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Olympus_E3-3.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Olympus_E3-4.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Olympus_E3.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Olympus_E300.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Olympus_E330.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Olympus_E400.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Olympus_E410-2.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Olympus_E410.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Olympus_E420.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Olympus_E500.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Olympus_E510-2.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Olympus_E510.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Olympus_E520-2.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Olympus_E520-3.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Olympus_E520-4.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Olympus_E520-5.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Olympus_E520.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Olympus_SP350.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\PENTAX-ISD-linear.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\PENTAX-ISD.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Pentax_K100D.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Pentax_K10D.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Pentax_K20D.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\SIGMA-SD10-linear.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\SIGMA-SD10.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\SONY-DSLR-A700.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\SONY_A200.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Sony_A300.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Sony_DSLR-A100-1.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Sony_DSLR-A350.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Sony_DSLR-A900-2.dng"));
  OpenFile(FileReader(L"..\\testimg\\dng\\Sony_DSLR-A900.dng"));
OpenFile(FileReader(L"..\\testimg\\dng\\uncompressed.dng"));
OpenFile(FileReader(L"..\\testimg\\dng\\uncompressed2.dng"));
OpenFile(FileReader(L"..\\testimg\\dng\\uncompressed3.dng"));


  MessageBox(0,L"Finished", L"Finished",0);
#ifdef _USE_GFL_
  gflLibraryExit();
#endif
  _CrtDumpMemoryLeaks();
	return 0;
}

