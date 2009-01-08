#include "StdAfx.h"
#include "FileReader.h"

FileReader::FileReader(LPCWSTR _filename) : mFilename(_filename)
{
}

FileMap* FileReader::readFile() {
	HANDLE file_h;  // File handle
	file_h = CreateFile(mFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN, NULL);
  if (file_h == INVALID_HANDLE_VALUE) {
		throw new FileIOException("Could not open file.");
  }

	LARGE_INTEGER f_size;
	GetFileSizeEx(file_h , &f_size);

  FileMap *fileData = new FileMap(f_size.LowPart);

	DWORD bytes_read;
  if (! ReadFile(file_h, fileData->getDataWrt(0), fileData->getSize(), &bytes_read, NULL)) {
	  CloseHandle(file_h);
    delete fileData;
		throw new FileIOException("Could not read file.");
	}
	CloseHandle(file_h);
  return fileData;
}

FileReader::~FileReader(void)
{

}
