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

//#include "Vec.h"

class iPoint;
//==================================================================
//  Point Class Definition
//==================================================================

class Point {
friend class Vec;
protected:
	int dimn;            // # coords (1, 2, or 3 max here)
public:
	double x, y, z;      // z=0 for 2D, y=z=0 for 1D

	//----------------------------------------------------------
	// Lots of Constructors (add more as needed)
	Point() { dimn=3; x=y=z=0; }
	// 1D Point
	Point( int a) {
		dimn=1; x=a; y=z=0;  }
	Point( double a) {
		dimn=1; x=a; y=z=0;  }
	// 2D Point
	Point( int a, int b) {
		dimn=2; x=a; y=b; z=0;  }
	Point( double a, double b) {
		dimn=2; x=a; y=b; z=0;  }
	// 3D Point
	Point( int a, int b, int c) {
		dimn=3; x=a; y=b; z=c;  }
	Point( double a, double b, double c) {
		dimn=3; x=a; y=b; z=c;  }
	// n-dim Point
	Point( int n, int a[]);
	Point( int n, double a[]);
	// Destructor
	~Point() {};

	//----------------------------------------------------------
	// Input/Output streams
	friend istream& operator>>( istream&, Point&);
	friend ostream& operator<<( ostream&, Point);

	//----------------------------------------------------------
	// Assignment "=": use the default to copy all members
	int dim() { return dimn; }      // get dimension
	int setdim( int);               // set new dimension

	//----------------------------------------------------------
	// Comparison (dimension must match, or not)
	int operator==( Point);
	int operator!=( Point);

	//----------------------------------------------------------
	// Point and Vec Operations (always valid) 
	Vec operator-( Point);       // Vec difference
	Point  operator+( Vec);      // +translate
	Point  operator-( Vec);      // -translate
	Point& operator+=( Vec);     // inc translate
	Point& operator-=( Vec);     // dec translate

	//----------------------------------------------------------
	// Point Scalar Operations (convenient but often illegal)
	// using any type of scalar (int, float, or double)
	//    are not valid for points in general,
	//    unless they are 'affine' as coeffs of 
	//    a sum in which all the coeffs add to 1,
	//    such as: the sum (a*P + b*Q) with (a+b == 1).
	//    The programmer must enforce this (if they want to).

	// Scalar Multiplication
	friend Point operator*( int, Point);
	friend Point operator*( double, Point);
	friend Point operator*( Point, int);
	friend Point operator*( Point, double);
	// Scalar Division
	friend Point operator/( Point, int);
	friend Point operator/( Point, double);

	//----------------------------------------------------------
	// Point Addition (also convenient but often illegal)
	//    is not valid unless part of an affine sum.
	//    The programmer must enforce this (if they want to).
	friend Point operator+( Point, Point);     // add points

	// Affine Sum
	// Returns weighted sum, even when not affine, but...
	// Tests if coeffs add to 1.  If not, sets: err = Esum.
	friend Point asum( int, int[], Point[]);
	friend Point asum( int, double[], Point[]);

	//----------------------------------------------------------
	// Point Relations
	friend double d( Point, Point);         // Distance
	friend double d2( Point, Point);        // Distance^2
	double isLeft( Point, Point);           // 2D only
	double Area( Point, Point); 		// any dim for triangle PPP

	// Collinearity Conditions (any dim n)
	/*bool isOnLine( Point, Point, char);  // is On line (char= flag)
	bool isOnLine( Point, Point);        // is On line (flag= all)
	bool isBefore( Point, Point);        // is On line (flag= before)
	bool isBetween( Point, Point);       // is On line (flag= between)
	bool isAfter( Point, Point);         // is On line (flag= after)
	bool isOnRay( Point, Point);         // is On line (flag= between|after)*/

	//----------------------------------------------------------
};

class iPoint {
friend class iVec;
protected:
	int dimn;            // # coords (1, 2, or 3 max here)
public:
	int x, y, z;      // z=0 for 2D, y=z=0 for 1D

	//----------------------------------------------------------
	// Lots of Constructors (add more as needed)
	iPoint() { dimn=3; x=y=z=0; }
	// 1D iPoint
	iPoint( int a) {
		dimn=1; x=a; y=z=0;  }
	iPoint( double a) {
		dimn=1; x=a; y=z=0;  }
	// 2D iPoint
	iPoint( int a, int b) {
		dimn=2; x=a; y=b; z=0;  }
	iPoint( double a, double b) {
		dimn=2; x=a; y=b; z=0;  }
	// 3D iPoint
	iPoint( int a, int b, int c) {
		dimn=3; x=a; y=b; z=c;  }
	iPoint( double a, double b, double c) {
		dimn=3; x=a; y=b; z=c;  }
	// n-dim iPoint
	iPoint( int n, int a[]);
	iPoint( int n, double a[]);
	// Destructor
	~iPoint() {};

	//----------------------------------------------------------
	// Input/Output streams
	friend istream& operator>>( istream&, iPoint&);
	friend ostream& operator<<( ostream&, iPoint);

	//----------------------------------------------------------
	// Assignment "=": use the default to copy all members
	int dim() { return dimn; }      // get dimensions
	int setdim( int);               // set new dimensions

	//----------------------------------------------------------
	// Comparison (dimension must match, or not)
	int operator==( iPoint);
	int operator!=( iPoint);

	//----------------------------------------------------------
	// iPoint and iVec Operations (always valid) 
	iVec operator-( iPoint);       // iVec difference
	iPoint  operator+( iVec);      // +translate
	iPoint  operator-( iVec);      // -translate
	iPoint& operator+=( iVec);     // inc translate
	iPoint& operator-=( iVec);     // dec translate

	//----------------------------------------------------------
	// iPoint Scalar Operations (convenient but often illegal)
	// using any type of scalar (int, float, or double)
	//    are not valid for iPoints in general,
	//    unless they are 'affine' as coeffs of 
	//    a sum in which all the coeffs add to 1,
	//    such as: the sum (a*P + b*Q) with (a+b == 1).
	//    The programmer must enforce this (if they want to).

	// Scalar Multiplication
	friend iPoint operator*( int, iPoint);
	friend iPoint operator*( double, iPoint);
	friend iPoint operator*( iPoint, int);
	friend iPoint operator*( iPoint, double);
	// Scalar Division
	friend iPoint operator/( iPoint, int);
	friend iPoint operator/( iPoint, double);
	friend iPoint operator/( iPoint, iPoint);
  iPoint& operator/=(const iPoint &R);

	//----------------------------------------------------------
	// iPoint Addition (also convenient but often illegal)
	//    is not valid unless part of an affine sum.
	//    The programmer must enforce this (if they want to).
	friend iPoint operator+( const iPoint, const iPoint);     // add iPoints

	// Affine Sum
	// Returns weighted sum, even when not affine, but...
	// Tests if coeffs add to 1.  If not, sets: err = Esum.
	friend iPoint asum( int, int[], iPoint[]);
	friend iPoint asum( int, double[], iPoint[]);

	//----------------------------------------------------------
	// iPoint Relations
	friend double d( iPoint, iPoint);         // Distance
	friend int d2( iPoint, iPoint);        // Distance^2
	double isLeft( iPoint, iPoint);           // 2D only
//	int Area( iPoint, iPoint); 		          // any dim for triangle PPP
	int Area() const; 		                        // area from (0,0,0) to Point in n dimnesions
  bool isInside(const iPoint& Q) const;

	// Collinearity Conditions (any dim n)
/*	bool isOnLine( iPoint, iPoint, char);  // is On line (char= flag)
	bool isOnLine( iPoint, iPoint);        // is On line (flag= all)
	bool isBefore( iPoint, iPoint);        // is On line (flag= before)
	bool isBetween( iPoint, iPoint);       // is On line (flag= between)
	bool isAfter( iPoint, iPoint);         // is On line (flag= after)
	bool isOnRay( iPoint, iPoint);         // is On line (flag= between|after)*/

	//----------------------------------------------------------

};

class iPoint2D : public iPoint {
public:
	//----------------------------------------------------------
	// Lots of Constructors (add more as needed)
	iPoint2D() { dimn=2; }
	// 2D iPoint
	iPoint2D( int a, int b) {
		dimn=2; x=a; y=b; z=0;  }
//	iPoint2D( double a, double b) {
//		dimn=2; x=a; y=b; z=0;  }
//	iPoint2D( int n, int a[]);
//	iPoint2D( int n, double a[]);
	// Destructor
	~iPoint2D() {};
private:
  int z;
};

#endif SS_Point_H
