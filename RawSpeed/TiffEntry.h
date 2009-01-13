#pragma once
#include "TiffParserException.h"
#include "FileMap.h"

const int datasizes[] = {0,1,1,2,4,8,1,1,2,4, 8, 4, 8, 4};
                      // 0-1-2-3-4-5-6-7-8-9-10-11-12-13
const int datashifts[] = {0,0,0,1,2,3,0,0,1,2, 3, 2, 3, 2};

#ifdef CHECKSIZE
#undef CHECKSIZE
#endif

#define CHECKSIZE(A) if (A >= f->getSize()) throw TiffParserException("Error reading TIFF structure. File Corrupt")

// 0-1-2-3-4-5-6-7-8-9-10-11-12-13
/*
 * Tag data type information.
 *
 * Note: RATIONALs are the ratio of two 32-bit integer values.
 */
typedef	enum {
	TIFF_NOTYPE	= 0,	/* placeholder */
	TIFF_BYTE	= 1,	/* 8-bit unsigned integer */
	TIFF_ASCII	= 2,	/* 8-bit bytes w/ last byte null */
	TIFF_SHORT	= 3,	/* 16-bit unsigned integer */
	TIFF_LONG	= 4,	/* 32-bit unsigned integer */
	TIFF_RATIONAL	= 5,	/* 64-bit unsigned fraction */
	TIFF_SBYTE	= 6,	/* !8-bit signed integer */
	TIFF_UNDEFINED	= 7,	/* !8-bit untyped data */
	TIFF_SSHORT	= 8,	/* !16-bit signed integer */
	TIFF_SLONG	= 9,	/* !32-bit signed integer */
	TIFF_SRATIONAL	= 10,	/* !64-bit signed fraction */
	TIFF_FLOAT	= 11,	/* !32-bit IEEE floating point */
	TIFF_DOUBLE	= 12	/* !64-bit IEEE floating point */
} TiffDataType;


class TiffEntry
{
public:
  TiffEntry();
  TiffEntry(FileMap* f, guint offset);
  virtual ~TiffEntry(void);
  virtual guint getInt();
  gfloat getFloat();
  virtual gushort getShort();
  virtual const guint* getIntArray();
  virtual const gushort* getShortArray();
  string getString();
  guchar getByte();
  const guchar* getData() {return data;};
  int getElementSize();
  int getElementShift();
// variables:
  TiffTag tag;
  TiffDataType type;
  int count;
protected:
  unsigned char* data;
#ifdef _DEBUG
  int debug_intVal;
  float debug_floatVal;
#endif
};
