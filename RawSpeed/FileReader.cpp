#include "StdAfx.h"
#include "FileReader.h"
#ifdef __unix__
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#endif // __unix__

FileReader::FileReader(LPCWSTR _filename) : mFilename(_filename)
{
}

FileMap* FileReader::readFile() {
#ifndef __unix__
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
#else // __unix__
	struct stat st;
	int bytes_read = 0;
	int fd;
	char *dest;

	stat(mFilename, &st);
	FileMap *fileData = new FileMap(st.st_size);

	fd = open(mFilename, O_RDONLY);

	while(bytes_read < st.st_size) {
		dest = (char *) fileData->getDataWrt(bytes_read);
		bytes_read += read(fd, dest, st.st_size-bytes_read);
	}#endif // __unix__
  return fileData;
}

FileReader::~FileReader(void)
{

}
