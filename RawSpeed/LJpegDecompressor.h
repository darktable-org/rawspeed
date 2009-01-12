#pragma once
#include "RawDecompressor.h"

/* The following enum is stolen from the IJG JPEG library
 * Comments added by tm
 */ 

typedef enum {		/* JPEG marker codes			*/
  M_STUFF = 0x00,
  M_SOF0  = 0xc0,	/* baseline DCT				*/
  M_SOF1  = 0xc1,	/* extended sequential DCT		*/
  M_SOF2  = 0xc2,	/* progressive DCT			*/
  M_SOF3  = 0xc3,	/* lossless (sequential)		*/
  
  M_SOF5  = 0xc5,	/* differential sequential DCT		*/
  M_SOF6  = 0xc6,	/* differential progressive DCT		*/
  M_SOF7  = 0xc7,	/* differential lossless		*/
  
  M_JPG   = 0xc8,	/* JPEG extensions			*/
  M_SOF9  = 0xc9,	/* extended sequential DCT		*/
  M_SOF10 = 0xca,	/* progressive DCT			*/
  M_SOF11 = 0xcb,	/* lossless (sequential)		*/
  
  M_SOF13 = 0xcd,	/* differential sequential DCT		*/
  M_SOF14 = 0xce,	/* differential progressive DCT		*/
  M_SOF15 = 0xcf,	/* differential lossless		*/
  
  M_DHT   = 0xc4,	/* define Huffman tables		*/
  
  M_DAC   = 0xcc,	/* define arithmetic conditioning table	*/
  
  M_RST0  = 0xd0,	/* restart				*/
  M_RST1  = 0xd1,	/* restart				*/
  M_RST2  = 0xd2,	/* restart				*/
  M_RST3  = 0xd3,	/* restart				*/
  M_RST4  = 0xd4,	/* restart				*/
  M_RST5  = 0xd5,	/* restart				*/
  M_RST6  = 0xd6,	/* restart				*/
  M_RST7  = 0xd7,	/* restart				*/
  
  M_SOI   = 0xd8,	/* start of image			*/
  M_EOI   = 0xd9,	/* end of image				*/
  M_SOS   = 0xda,	/* start of scan			*/
  M_DQT   = 0xdb,	/* define quantization tables		*/
  M_DNL   = 0xdc,	/* define number of lines		*/
  M_DRI   = 0xdd,	/* define restart interval		*/
  M_DHP   = 0xde,	/* define hierarchical progression	*/
  M_EXP   = 0xdf,	/* expand reference image(s)		*/
  
  M_APP0  = 0xe0,	/* application marker, used for JFIF	*/
  M_APP1  = 0xe1,	/* application marker			*/
  M_APP2  = 0xe2,	/* application marker			*/
  M_APP3  = 0xe3,	/* application marker			*/
  M_APP4  = 0xe4,	/* application marker			*/
  M_APP5  = 0xe5,	/* application marker			*/
  M_APP6  = 0xe6,	/* application marker			*/
  M_APP7  = 0xe7,	/* application marker			*/
  M_APP8  = 0xe8,	/* application marker			*/
  M_APP9  = 0xe9,	/* application marker			*/
  M_APP10 = 0xea,	/* application marker			*/
  M_APP11 = 0xeb,	/* application marker			*/
  M_APP12 = 0xec,	/* application marker			*/
  M_APP13 = 0xed,	/* application marker			*/
  M_APP14 = 0xee,	/* application marker, used by Adobe	*/
  M_APP15 = 0xef,	/* application marker			*/
  
  M_JPG0  = 0xf0,	/* reserved for JPEG extensions		*/
  M_JPG13 = 0xfd,	/* reserved for JPEG extensions		*/
  M_COM   = 0xfe,	/* comment				*/
  
  M_TEM   = 0x01,	/* temporary use			*/
  M_FILL  = 0xFF

} JpegMarker;


/*
* The following structure stores basic information about one component.
*/
typedef struct JpegComponentInfo {
  /*
  * These values are fixed over the whole image.
  * They are read from the SOF marker.
  */
  guint componentId;		/* identifier for this component (0..255) */
  guint componentIndex;	/* its index in SOF or cPtr->compInfo[]   */

  /*
  * Huffman table selector (0..3). The value may vary
  * between scans. It is read from the SOS marker.
  */
  guint dcTblNo;
  guint superH; // Horizontal Supersampling
  guint superV; // Vertical Supersampling
} JpegComponentInfo;

/*
* One of the following structures is created for each huffman coding
* table.  We use the same structure for encoding and decoding, so there
* may be some extra fields for encoding that aren't used in the decoding
* and vice-versa.
*/

struct HuffmanTable {
  /*
  * These two fields directly represent the contents of a JPEG DHT
  * marker
  */
  guint bits[17];
  guint huffval[256];

  /*
  * The remaining fields are computed from the above to allow more
  * efficient coding and decoding.  These fields should be considered
  * private to the Huffman compression & decompression modules.
  */

  gushort mincode[17];
  gint maxcode[18];
  gshort valptr[17];
  gint numbits[256];
  gboolean initialized;
};

class SOFInfo {
public:
  SOFInfo() { w = h = cps = prec = 0; initialized = false;};
  ~SOFInfo() {initialized = false;};
  guint w;    // Width
  guint h;    // Height
  guint cps;  // Components
  guint prec; // Precision
  JpegComponentInfo compInfo[4];
  gboolean initialized;  
};

class LJpegDecompressor
{
public:
  LJpegDecompressor(FileMap* file, RawImage img);
  virtual ~LJpegDecompressor(void);
  virtual void startDecoder(guint offset, guint size, guint offsetX, guint offsetY);
  virtual void getSOF(SOFInfo* i, guint offset, guint size);
  gboolean mDNGCompatible;  // DNG v1.0.x compatibility
  virtual void addSlices(vector<int> slices) {slicesW=slices;};  // CR2 slices.
protected:
  virtual void parseSOF(SOFInfo* i);
  virtual void parseSOS();
  virtual void createHuffmanTable(HuffmanTable *htbl);
  virtual void decodeScan() = 0;
  JpegMarker getNextMarker(bool allowskip);
  void parseDHT();
  gint HuffDecode(HuffmanTable *htbl);
  ByteStream* input;
  BitPumpJPEG* bits;
  RawImage mRaw; 
  FileMap *mFile;

  SOFInfo frame;
  vector<int> slicesW;
  guint pred;
  guint Pt;
  guint offX, offY;  // Offset into image where decoding should start
  guint skipX, skipY;   // Tile is larger than output, skip these border pixels
  HuffmanTable huff[4];
};

static guint bitMask[] = {  0xffffffff, 0x7fffffff, 
0x3fffffff, 0x1fffffff,
0x0fffffff, 0x07ffffff, 
0x03ffffff, 0x01ffffff,
0x00ffffff, 0x007fffff, 
0x003fffff, 0x001fffff,
0x000fffff, 0x0007ffff, 
0x0003ffff, 0x0001ffff,
0x0000ffff, 0x00007fff, 
0x00003fff, 0x00001fff,
0x00000fff, 0x000007ff, 
0x000003ff, 0x000001ff,
0x000000ff, 0x0000007f, 
0x0000003f, 0x0000001f,
0x0000000f, 0x00000007, 
0x00000003, 0x00000001};


