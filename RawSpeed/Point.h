//==================================================================
// Copyright 2002, softSurfer (www.softsurfer.com)
// This code may be freely used and modified for any purpose
// providing that this copyright notice is included with it.
// SoftSurfer makes no warranty for this code, and cannot be held
// liable for any real or imagined damage resulting from it's use.
// Users of this code must verify correctness for their application.
//==================================================================

#ifndef SS_Point_H
#define SS_Point_H


class iPoint2D {
public:
	iPoint2D() {x = y = 0;  }
	iPoint2D( int a, int b) {
		x=a; y=b;}
	~iPoint2D() {};
  int x,y;
};

#endif // SS_Point_H
