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
private:
//	VOID CALLBACK FileIOCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);
  LPCWSTR mFilename;
};
