//==================================================================
// Copyright 2002, softSurfer (www.softsurfer.com)
// This code may be freely used and modified for any purpose
// providing that this copyright notice is included with it.
// SoftSurfer makes no warranty for this code, and cannot be held
// liable for any real or imagined damage resulting from it's use.
// Users of this code must verify correctness for their application.
//==================================================================

#ifndef SS_Vec_H
#define SS_Vec_H

#include "Point.h"
//==================================================================
//  Vec Class Definition
//==================================================================

class Vec : public Point {
public:
	// Constructors same as Point class
	Vec() : Point() {};
	Vec( int a) : Point(a) {};
	Vec( double a) : Point(a) {};
	Vec( int a, int b) : Point(a,b) {};
	Vec( double a, double b) : Point(a,b) {};
	Vec( int a, int b, int c) : Point(a,b,c) {};
	Vec( double a, double b, double c) : Point(a,b,c) {};
	Vec( int n, int a[]) : Point(n,a) {};
	Vec( int n, double a[]) : Point(n,a) {};
	~Vec() {};

	//----------------------------------------------------------
	// IO streams and Comparisons: inherit from Point class

	//----------------------------------------------------------
	// Vec Unary Operations
	Vec operator-();                // unary minus
	Vec operator~();                // unary 2D perp operator

	//----------------------------------------------------------
	// Scalar Multiplication
	friend Vec operator*( int, Vec);
	friend Vec operator*( double, Vec);
	friend Vec operator*( Vec, int);
	friend Vec operator*( Vec, double);
	// Scalar Division
	friend Vec operator/( Vec, int);
	friend Vec operator/( Vec, double);

	//----------------------------------------------------------
	// Vec Arithmetic Operations
	Vec operator+( Vec);        // Vec add
	Vec operator-( Vec);        // Vec subtract
	double operator*( Vec);        // inner dot product
	double operator|( Vec);        // 2D exterior perp product
	Vec operator^( Vec);        // 3D exterior cross product

	Vec& operator*=( double);      // Vec scalar mult
	Vec& operator/=( double);      // Vec scalar div
	Vec& operator+=( Vec);      // Vec increment
	Vec& operator-=( Vec);      // Vec decrement
	Vec& operator^=( Vec);      // 3D exterior cross product

	//----------------------------------------------------------
	// Vec Properties
	double len() {                    // Vec length
		return sqrt(x*x + y*y + z*z);
	}
	double len2() {                   // Vec length squared (faster)
		return (x*x + y*y + z*z);
	}

	//----------------------------------------------------------
	// Special Operations
	void normalize();                 // convert Vec to unit length
	friend Vec sum( int, int[], Vec[]);     // Vec sum
	friend Vec sum( int, double[], Vec[]);  // Vec sum
};

class iVec : public iPoint {
public:
	// Constructors same as Point class
	iVec() : iPoint() {};
	iVec( int a) : iPoint(a) {};
	iVec( double a) : iPoint(a) {};
	iVec( int a, int b) : iPoint(a,b) {};
	iVec( double a, double b) : iPoint(a,b) {};
	iVec( int a, int b, int c) : iPoint(a,b,c) {};
	iVec( double a, double b, double c) : iPoint(a,b,c) {};
	iVec( int n, int a[]) : iPoint(n,a) {};
	iVec( int n, double a[]) : iPoint(n,a) {};
	~iVec() {};

	//----------------------------------------------------------
	// IO streams and Comparisons: inherit from Point class

	//----------------------------------------------------------
	// iVec Unary Operations
	iVec operator-();                // unary minus
	iVec operator~();                // unary 2D perp operator

	//----------------------------------------------------------
	// Scalar Multiplication
	friend iVec operator*( int, iVec);
	friend iVec operator*( double, iVec);
	friend iVec operator*( iVec, int);
	friend iVec operator*( iVec, double);
	// Scalar Division
	friend iVec operator/( iVec, int);
	friend iVec operator/( iVec, double);

	//----------------------------------------------------------
	// iVec Arithmetic Operations
	iVec operator+( iVec);        // iVec add
	iVec operator-( iVec);        // iVec subtract
	double operator*( iVec);        // inner dot product
	double operator|( iVec);        // 2D exterior perp product
	iVec operator^( iVec);        // 3D exterior cross product

	iVec& operator*=( double);      // iVec scalar mult
	iVec& operator/=( double);      // iVec scalar div
	iVec& operator+=( iVec);      // iVec increment
	iVec& operator-=( iVec);      // iVec decrement
	iVec& operator^=( iVec);      // 3D exterior cross product

	//----------------------------------------------------------
	// iVec Properties
	double len() {                    // iVec length
		return sqrt((double)x*x + y*y + z*z);
	}
	double len2() {                   // iVec length squared (faster)
		return (x*x + y*y + z*z);
	}

	//----------------------------------------------------------
	// Special Operations
	void normalize();                 // convert iVec to unit length
	friend iVec sum( int, int[], iVec[]);     // iVec sum
	friend iVec sum( int, double[], iVec[]);  // iVec sum
};

#endif // SS_Vec_H
