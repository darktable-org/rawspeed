#pragma once
#include "FileIOException.h"
#include "FileMap.h"

class FileReader
{
public:
	FileReader(LPCWSTR filename);
public:
	FileMap* readFile();
	virtual ~FileReader();
  LPCWSTR Filename() const { return mFilename; }
//  void Filename(LPCWSTR val) { mFilename = val; }
private:
  LPCWSTR mFilename;
};
