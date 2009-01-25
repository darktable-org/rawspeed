// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once


#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <stdio.h>
#ifndef __unix__
#include <tchar.h>
#include <io.h>
#include <Windows.h>
#include <crtdbg.h>
#endif // __unix__
#include <malloc.h>
#include <math.h>
// STL
#include <string>
#include <vector>
#include <map>
#include <list>
using namespace std;

//My own
#include "TiffTag.h"
#include "Common.h"
#include "Point.h"


