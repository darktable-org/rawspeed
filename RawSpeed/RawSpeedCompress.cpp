#include "StdAfx.h"
#include "RawSpeed-API.h"
#include "FileWriter.h"
/*#include "rice.h"
#include "bitops.h"
#include "aricoder.h"*/
/* 
RawSpeed - RAW file decoder.

Copyright (C) 2013 Klaus Post

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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

http://www.klauspost.com
*/

using namespace RawSpeed;

int startTime;

static void shuffleShorts(ushort16 *in_array, uint32 elements)
{
  uchar8* a = (uchar8*)in_array;
  uchar8* temp = new uchar8[elements*2];
  for (uint32 i = 0; i < elements; i++)
  {
    temp[i] = a[i*2];
    temp[i+elements] = a[i*2+1];
  }
  memcpy(a, temp, elements*2);
  delete[] temp;
}

static __inline ushort16 ZigZag(short word) {
  return (word >> 15) ^ (word << 1);
}

class LineInfo
{
public:
  int line_no;
  uint32 delta_total;
  ushort16* start;
  LineInfo(int _line_no, uint32 _delta_total, ushort16* _start) {
    line_no = _line_no;
    delta_total = _delta_total;
    start = _start;
  }
};

// comparison, not case sensitive.
bool compare_lines (LineInfo first, LineInfo second)
{
  return (first.delta_total < second.delta_total);
}

// Open file and save as tiff
void OpenFile(FileReader f, CameraMetaData *meta) {
  RawDecoder *d = 0;
  FileMap* m = 0;
  try {
        wprintf(L"Opening:%s\n",f.Filename());
    try {
      m = f.readFile();    	
    } catch (FileIOException &e) {
      printf("Could not open image:%s\n", e.what());
      return;
    }
    RawParser t(m);
    d = t.getDecoder();
//    d->failOnUnknown = true;
    d->checkSupport(meta);
    startTime = GetTickCount();

    d->applyCrop = FALSE;
    d->applyStage1DngOpcodes = FALSE;
    d->uncorrectedRawValues = TRUE;
    d->decodeRaw();
    d->decodeMetaData(meta);
    RawImage r = d->mRaw;

    uint32 time = GetTickCount()-startTime;
    float mpps = (float)r->dim.x * (float)r->dim.y * (float)r->getCpp()  / (1000.0f * (float)time);
    wprintf(L"Decoding %s took: %u ms, %4.2f Mpixel/s\n", f.Filename(), time, mpps);
    for (uint32 i = 0; i < r->errors.size(); i++) {
      printf("Error Encountered:%s", r->errors[i]);
    }
    int width= r->dim.x;
    int height= r->dim.y;
    uint32 cpp = r->getCpp();

    vector<TiffIFD*> data;
    if (d->getRootIFD()) {
      data = d->getRootIFD()->getIFDsWithTag(PANASONIC_STRIPOFFSET);
      bool panasonic = !data.empty();
      bool fuji = d->getRootIFD()->hasEntryRecursive(FUJI_STRIPOFFSETS);

      if (data.empty())
        data = d->getRootIFD()->getIFDsWithTag(FUJI_STRIPOFFSETS);
      if (data.empty())
        data = d->getRootIFD()->getIFDsWithTag(TiffTag(0xc5d8));
      if (data.empty())
        data = d->getRootIFD()->getIFDsWithTag(CFAPATTERN);
      if (data.empty())
        data = d->getRootIFD()->getIFDsWithTag(STRIPOFFSETS);
    
      if (data.empty())
        ThrowRDE("Unable to locate probable RAW data");

      TiffIFD* raw = data[0];

      TiffEntry *offsets;
      TiffEntry *counts;
      if (panasonic) {
        offsets = raw->getEntry(PANASONIC_STRIPOFFSET);
        counts = NULL;
      } else if (fuji) {
        offsets = raw->getEntry(FUJI_STRIPOFFSETS);
        counts = raw->getEntry(FUJI_STRIPBYTECOUNTS);
      } else {
        offsets = raw->getEntry(STRIPOFFSETS);
        counts = raw->getEntry(STRIPBYTECOUNTS);
      }
      // Iterate through all slices
      if (!(offsets->isInt() || counts->isInt()))
        ThrowRDE("offset/counts is not int");
      const uint32* off_a = offsets->getIntArray();
      const uint32* count_a = counts ? counts->getIntArray(): NULL;

      for (uint32 s = 0; s < offsets->count; s++) {
        uint32 pw =  width / 14;
        int panasize = (pw * 14 * height * 9 + pw * 2 * height)/8;
        if (panasonic)
          memset(m->getDataWrt(off_a[s]),0, panasize);
        else
          memset(m->getDataWrt(off_a[s]),0, count_a[s]);
      }
    } else {
      FileMap *data = d->getCompressedData();
      if (data) {
        memset(data->getDataWrt(0),0, data->getSize());
      }
    }
    width *=cpp;
    WCHAR filename[65536];
    wsprintf(filename, L"%s-separated", f.Filename());
    CreateDirectoryW(filename,NULL);
    wsprintf(filename, L"%s-separated\\image-shell.dat", f.Filename());
    FileWriter w(filename);      
    w.writeFile(m);
    FileMap imageData(r->dim.area()*2*cpp);
    for (int i = 0; i < height; i++)
      memcpy(imageData.getDataWrt(i*width*2),r->getData(0,i),width*2);
    /*
    wsprintf(filename, L"%s-separated\\image-unmodified.dat", f.Filename());
    w = FileWriter(filename);
    w.writeFile(&imageData);
    */

    // Which prediction mode should be used after upper two lines?
    int other_selected = 2;

    if (r->getCpp() == 3)
      other_selected = 6;
    if (d->getRootIFD()) {
      string make = d->getRootIFD()->getEntryRecursive(MAKE)->getString();
      TrimSpaces(make);
      if (!make.compare("SONY"))
        other_selected = 0; // Predict left
      if (!make.compare("Panasonic"))
        other_selected = 0; // Predict left
    }
    std::list<LineInfo> lineinfo;
    int mostsel[] = {0,0,0};
    FileMap shuffled(r->dim.area()*2*cpp);
    FileMap delta(r->dim.area()*2*cpp);

    // How many lines before we can switch to up prediciton?
    int pred_lines_left = cpp > 1 ? 1 : 2;

    for (int y = 0; y < height; y++) {
      ushort16 *src = (ushort16*)r->getData(0,y);
      ushort16 *dst = (ushort16*)delta.getDataWrt(y*width*2);
      // Save initial values
      uint32 totaldelta = 0;
      ushort16 *src_up2, *src_up;
      int selected_pred = 0;
      // Upper lines cannot be predicted upwards
      if (y < pred_lines_left) {
        if (cpp == 1) {
          dst[0] = src[0];
          dst[1] = src[1];
          // To enable multithreaded, change 'y' to 'y&255' for instance
          selected_pred = 0; //First two lines always left
        } else {
          for (uint32 i = 0; i < cpp; i++)
            dst[i] = src[i];
          selected_pred = 5; // First line always left
        }
      } else { // Prepare when left+up
        if (cpp == 1) {
          src_up = (ushort16*)r->getData(0,y-1);
          src_up2 = (ushort16*)r->getData(0,y-2);
          dst[0] = ZigZag(src[0]-src_up2[0]);
          dst[1] = ZigZag(src[1]-src_up2[1]);
          selected_pred = other_selected;
          if (other_selected == 2 && r->cfa.getColorAt(0,0) != CFA_UNKNOWN) {
            if (r->cfa.getColorAt(0,y) == CFA_GREEN)
              selected_pred = 3;
            else if (r->cfa.getColorAt(1,y) == CFA_GREEN)
              selected_pred = 4;
          }
        } else { // Multiple cpp
          src_up = (ushort16*)r->getData(0,y-1);
          for (uint32 i = 0; i < cpp; i++) {
            dst[i] = ZigZag(src[i]-src_up[i]);
          }
          selected_pred = other_selected;
        }
      }
      for (int x = (cpp == 1) ? 2 : cpp; x < width;) {
        int delta1 = 0;
        int delta2 = 0;
        if (selected_pred == 0) {
          // CFA: Take prediction 2 pixels to the left
          delta1 = src[x] - (int)src[x-2];
          delta2 = src[x+1] - (int)src[x-1];
          dst[x] = ZigZag(delta1);
          dst[x+1] = ZigZag(delta2);
          x+=2;
        } else if (selected_pred == 1) {
          // CFA: Take prediction 2 pixels up
          delta1 = src[x] - (int)src_up2[x];
          delta2 = src[x+1] - (int)src_up2[x+1];
          dst[x] = ZigZag(delta1);
          dst[x+1] = ZigZag(delta2);
          x+=2;
        } else if (selected_pred == 2) {
          // CFA: Take prediction 2 pixels up + left, take average
          delta1 = src[x] - (((int)src_up2[x]+(int)src[x-2] + 1)>>1);
          delta2 = src[x+1] - (((int)src_up2[x+1]+(int)src[x-1] + 1)>>1);
          dst[x] = ZigZag(delta1);
          dst[x+1] = ZigZag(delta2);
          x+=2;
        } else if (selected_pred == 3) {
          // CFA: Take prediction 1 pixels left for even x, up + right_up, take average
          // Use only when cfa has matching pattern.
          // If this is last pixel, use 2 up + 2 left average
          int pred;
          if (x < width - 1)
            pred = (int)src_up[x-1] + (int)src_up[x+1] + 1;
          else
            pred = (int)src_up2[x] + (int)src[x-2] + 1;
          pred >>= 1;
          delta1 = src[x] - pred;
          delta2 = src[x+1] - (((int)src_up2[x+1]+(int)src[x-1]+1)>>1);
          dst[x] = ZigZag(delta1);
          dst[x+1] = ZigZag(delta2);
          x+=2;
        } else if (selected_pred == 4) {
          // CFA: Take prediction 1 pixels left for even x, up + right_up, take average
          // Use only when cfa has matching pattern.
          // If this is last pixel, use 2 up + 2 left average
          delta1 = src[x] - (((int)src_up2[x]+(int)src[x-2]+1)>>1);
          int pred;
          if (x < width-2) 
            pred = (int)src_up[x] + (int)src_up[x+2] + 1;
          else
            pred = (int)src_up2[x+1] + (int)src[x-1] + 1;
          pred >>= 1;
          delta2 = src[x+1] - pred;
          dst[x] = ZigZag(delta1);
          dst[x+1] = ZigZag(delta2);
          x+=2;
        } else if (selected_pred == 5) {
          // Multiple cpp, predict left
          if (cpp == 3)  {
            int dR = src[x] - src[x-3];
            int dG = src[x+1] - src[x-3+1] - dR;
            int dB = src[x+2] - src[x-3+2] - dR;
            dst[x] = ZigZag(dR);
            dst[x+1] = ZigZag(dG);
            dst[x+2] = ZigZag(dB);
          } else {
            for (uint32 i = 0; i < cpp; i++) {
              int delta = src[x+i] - (int)src[x-cpp+i];
              dst[x+i] = ZigZag(delta);
            }
          }            
          x+=cpp;
        } else if (selected_pred == 6) {
          // Multiple cpp, predict up+left
          if (cpp == 3)  {
            // Red
            int dR = src[x] - ((src[x-3] + src_up[x] + 1)>>1);
            int dG = src[x+1] - ((src[x-3+1] + src_up[x+1] + 1)>>1) - dR;
            int dB = src[x+2] - ((src[x-3+2] + src_up[x+2] + 1)>>1) - dR;
            dst[x] = ZigZag(dR);
            dst[x+1] = ZigZag(dG);
            dst[x+2] = ZigZag(dB);
          } else {
            for (uint32 i = 0; i < cpp; i++) {
              int delta = src[x+i] - ((src[x-cpp+i] + src_up[x+i] + 1)>>1);
              dst[x+i] = ZigZag(delta);
            }
          }
          x+=cpp;
        }
      }
    }
    wsprintf(filename, L"%s-separated\\image-delta-zigzag.dat", f.Filename());
    w = FileWriter(filename);
    w.writeFile(&delta);

    memcpy(shuffled.getDataWrt(0),delta.getData(0),delta.getSize());
    shuffleShorts((ushort16*)shuffled.getDataWrt(0), shuffled.getSize()/2);
    wsprintf(filename, L"%s-separated\\image-delta-zigzag-shuffled.dat", f.Filename());
    w = FileWriter(filename);
    w.writeFile(&shuffled);


/*    uint32 rice_size = Rice_Compress(delta.getDataWrt(0), shuffled.getDataWrt(0), shuffled.getSize(), RICE_FMT_UINT16);
    wsprintf(filename, L"%s-separated\\image-delta-zigzag-rice.dat", f.Filename());
    w = FileWriter(filename);
    FileMap riced(shuffled.getDataWrt(0), rice_size);
    w.writeFile(&riced);
*/
#if 0
#define INIT_MODEL_S(a,b,c) new model_s( a, b, c, 255 )
    aiostream out_mem(shuffled.getDataWrt(0), 1 /*mem*/, delta.getSize(), 1 /*write*/);
    aricoder *coder = new aricoder(&out_mem, 1 /*write*/);
#if 0  // canon_eos_5d_mark_iii_05.cr2: 20 453 000
    model_s* model;
    model_s* model_up;
    model = INIT_MODEL_S( 256 + 1, 256, 1 );
    model_up = INIT_MODEL_S( 64 + 1, 64, 1 );
    ushort16 *inputdata = (ushort16*)delta.getDataWrt(0);
    int num = delta.getSize()/2;
    for ( int i = 0; i < num; i++ )
    {
      encode_ari( coder, model, inputdata[ i ] &0xff );
      model->shift_context( inputdata[ i ]&0xff );
      encode_ari( coder, model_up, inputdata[ i ] >>8 );
      model_up->shift_context( inputdata[ i ]>>8 );
    }
#else
/*    model_s* model;
    model_s* model_up;
    model = INIT_MODEL_S( (1<<14) + 1, (1<<14), 1 );
    ushort16 *inputdata = (ushort16*)delta.getDataWrt(0);
    int num = delta.getSize()/2;
    for ( int i = 0; i < num; i++ )
    {
      encode_ari( coder, model, inputdata[ i ] );
      model->shift_context( inputdata[ i ] );
    }*/
#endif
    delete coder;
    wsprintf(filename, L"%s-separated\\image-delta-zigzag-arith.dat", f.Filename());
    w = FileWriter(filename);
    FileMap arithed(shuffled.getDataWrt(0), out_mem.getsize());
    w.writeFile(&arithed);
#endif
  } catch (RawDecoderException &e) {
    wchar_t uni[1024];
    MultiByteToWideChar(CP_ACP, 0, e.what(), -1, uni, 1024);
    //    MessageBox(0,uni, L"RawDecoder Exception",0);
    wprintf(L"Raw Decoder Exception:%s\n",uni);
  }
  if (d) delete d;
  if (m) delete m;

}

int wmain(int argc, _TCHAR* argv[])
{
  if (1) {  // for memory detection
    try {

#if 0
      CameraMetaData meta("cameras.xml");
      if (argc > 1)
        OpenFile(FileReader(argv[1]),&meta);
      else
        wprintf(L"You must specify file to split\n");

#else 
      CameraMetaData meta("..\\data\\cameras.xml");
      OpenFile(FileReader(L"..\\testimg\\bench\\sigma_dp2.x3f"),&meta);
      OpenFile(FileReader(L"..\\testimg\\bench\\sigma_sd1_merrill_13.x3f"),&meta);
      OpenFile(FileReader(L"..\\testimg\\bench\\fujifilm_finepix_x100_11.raf"),&meta);
        OpenFile(FileReader(L"..\\testimg\\bench\\fujifilm_x_e1_20.raf"),&meta);
        OpenFile(FileReader(L"..\\testimg\\bench\\fujifilm_xf1_08.raf"),&meta);
      OpenFile(FileReader(L"..\\testimg\\bench\\canon_eos_5d_mark_iii_05.cr2"),&meta);
        OpenFile(FileReader(L"..\\testimg\\bench\\canon_eos_6d_14.cr2"),&meta);
        OpenFile(FileReader(L"..\\testimg\\bench\\canon_eos_m_04.cr2"),&meta);
        OpenFile(FileReader(L"..\\testimg\\bench\\nikon_1_v2_17.nef"),&meta);
          OpenFile(FileReader(L"..\\testimg\\bench\\nikon_d4_10.nef"),&meta);
          OpenFile(FileReader(L"..\\testimg\\bench\\nikon_d5200_14.nef"),&meta);
          OpenFile(FileReader(L"..\\testimg\\bench\\olympus_epm2_16.orf"),&meta);
          OpenFile(FileReader(L"..\\testimg\\bench\\olympus_om_d_e_m5_24.orf"),&meta);
          OpenFile(FileReader(L"..\\testimg\\bench\\olympus_xz2_10.orf"),&meta);
          OpenFile(FileReader(L"..\\testimg\\bench\\panasonic_lumix_dmc_gh3_10.rw2"),&meta);
          OpenFile(FileReader(L"..\\testimg\\bench\\panasonic_lumix_g5_15.rw2"),&meta);
          OpenFile(FileReader(L"..\\testimg\\bench\\pentax_k5_ii_12.dng"),&meta);
          OpenFile(FileReader(L"..\\testimg\\bench\\pentax_q10_19.dng"),&meta);
          OpenFile(FileReader(L"..\\testimg\\bench\\samsung_nx1000_19.srw"),&meta);
          OpenFile(FileReader(L"..\\testimg\\bench\\samsung_nx20_01.srw"),&meta);
          OpenFile(FileReader(L"..\\testimg\\bench\\sony_a55.arw"),&meta);
          OpenFile(FileReader(L"..\\testimg\\bench\\sony_a77_08.arw"),&meta);
          OpenFile(FileReader(L"..\\testimg\\bench\\sony_a99_04.arw"),&meta);
         OpenFile(FileReader(L"..\\testimg\\bench\\leica_x1_10.dng"),&meta);
         OpenFile(FileReader(L"..\\testimg\\bench\\leica_m82_05.dng"),&meta);
#endif
    } catch (CameraMetadataException) {

    }
//    MessageBox(0,L"Finished", L"Finished",0);
#ifdef _USE_GFL_
    gflLibraryExit();
#endif
  } // Dump objects
  _CrtDumpMemoryLeaks();
  return 0;
}

