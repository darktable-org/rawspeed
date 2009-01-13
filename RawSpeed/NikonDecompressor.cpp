#include "StdAfx.h"
#include "NikonDecompressor.h"

NikonDecompressor::NikonDecompressor(FileMap* file, RawImage img ) :
LJpegDecompressor(file,img)
{

}

NikonDecompressor::~NikonDecompressor(void)
{
}
