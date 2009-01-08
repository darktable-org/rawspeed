//==================================================================
// Copyright 2002, softSurfer (www.softsurfer.com)
// This code may be freely used and modified for any purpose
// providing that this copyright notice is included with it.
// SoftSurfer makes no warranty for this code, and cannot be held
// liable for any real or imagined damage resulting from it's use.
// Users of this code must verify correctness for their application.
//==================================================================

#include "StdAfx.h"


//==================================================================
// Point Class Methods
//==================================================================

//------------------------------------------------------------------
// Constructors (add more as needed)
//------------------------------------------------------------------

// n-dim Point
Point::Point( int n, int a[]) {
	x = y = z = 0;
	switch (dimn = n) {
	case 3: z = a[2];
	case 2: y = a[1];
	case 1: x = a[0];
		break;
	default:
		THROW_MATH("Dimension of Point invalid for operation");
	}
}

Point::Point( int n, double a[]) {
	x = y = z = 0.0;
	switch (dimn = n) {
	case 3: z = a[2];
	case 2: y = a[1];
	case 1: x = a[0];
		break;
	default:
		THROW_MATH("Dimension of Point invalid for operation");
	}
}

//------------------------------------------------------------------
// IO streams
//------------------------------------------------------------------

// Read input Point format: "(%f)", "(%f, %f)", or "(%f, %f, %f)"
istream& operator>>( istream& input, Point& P) {
	char c;
	input >> c;                // skip '('
	input >> P.x;
	input >> c;                
	if (c == ')') {
		P.setdim(1);       // 1D coord
		return input;
	}
	// else                    // skip ','
	input >> P.y;
	input >> c;
	if (c == ')') {
		P.setdim(2);       // 2D coord
		return input;
	}
	// else                    // skip ','
	input >> P.z;
	P.setdim(3);               // 3D coord
	input >> c;                // skip ')'
	return input;
}

// Write output Point in format: "(%f)", "(%f, %f)", or "(%f, %f, %f)"
ostream& operator<<( ostream& output, Point P) {
	switch (P.dim()) {
	case 1:
		output << "(" << P.x << ")";
		break;
	case 2:
		output << "(" << P.x << ", " << P.y << ")";
		break;
	case 3:
		output << "(" << P.x << ", " << P.y << ", " << P.z << ")";
		break;
	default:
		THROW_MATH("Dimension of Point invalid for operation");
	}
	return output;
}

//------------------------------------------------------------------
// Assign (set) dimension
//------------------------------------------------------------------

int Point::setdim( int n) {
	switch (n) {
	case 1: y = 0;
	case 2: z = 0;
	case 3:
		return dimn = n;
	default:                      // out of range value
		THROW_MATH("Dimension of Point invalid for operation");
		return ERROR;
	}
}

//------------------------------------------------------------------
// Comparison (note: dimension must compare)
//------------------------------------------------------------------

int Point::operator==( Point Q)
{
  int mindimn = min(dimn, Q.dim());
	switch (mindimn) {
	case 1:
		return (x==Q.x);
	case 2:
		return (x==Q.x && y==Q.y);
	case 3:
	default:
		return (x==Q.x && y==Q.y && z==Q.z);
	}
}

int Point::operator!=( Point Q)
{
	if (dimn != Q.dim()) return TRUE;
	switch (dimn) {
	case 1:
		return (x!=Q.x);
	case 2:
		return (x!=Q.x || y!=Q.y);
	case 3:
	default:
		return (x!=Q.x || y!=Q.y || z!=Q.z);
	}
}

//------------------------------------------------------------------
// Point Vector Operations
//------------------------------------------------------------------

Vec Point::operator-( Point Q)        // Vec diff of Points
{
	Vec v;
	v.x = x - Q.x;
	v.y = y - Q.y;
	v.z = z - Q.z;
	v.dimn = max( dimn, Q.dim());
	return v;
}

Point Point::operator+( Vec v)        // +ve translation
{
	Point P;
	P.x = x + v.x;
	P.y = y + v.y;
	P.z = z + v.z;
	P.dimn = max( dimn, v.dim());
	return P;
}

Point Point::operator-( Vec v)        // -ve translation
{
	Point P;
	P.x = x - v.x;
	P.y = y - v.y;
	P.z = z - v.z;
	P.dimn = max( dimn, v.dim());
	return P;
}

Point& Point::operator+=( Vec v)        // +ve translation
{
	x += v.x;
	y += v.y;
	z += v.z;
	dimn = max( dimn, v.dim());
	return *this;
}

Point& Point::operator-=( Vec v)        // -ve translation
{
	x -= v.x;
	y -= v.y;
	z -= v.z;
	dimn = max( dimn, v.dim());
	return *this;
}

//------------------------------------------------------------------
// Point Scalar Operations (convenient but often illegal)
//        are not valid for points in general,
//        unless they are 'affine' as coeffs of 
//        a sum in which all the coeffs add to 1,
//        such as: the sum (a*P + b*Q) with (a+b == 1).
//        The programmer must enforce this (if they want to).
//------------------------------------------------------------------

Point operator*( int c, Point Q) {
	Point P;
	P.x = c * Q.x;
	P.y = c * Q.y;
	P.z = c * Q.z;
	P.dimn = Q.dim();
	return P;
}

Point operator*( double c, Point Q) {
	Point P;
	P.x = c * Q.x;
	P.y = c * Q.y;
	P.z = c * Q.z;
	P.dimn = Q.dim();
	return P;
}

Point operator*( Point Q, int c) {
	Point P;
	P.x = c * Q.x;
	P.y = c * Q.y;
	P.z = c * Q.z;
	P.dimn = Q.dim();
	return P;
}

Point operator*( Point Q, double c) {
	Point P;
	P.x = c * Q.x;
	P.y = c * Q.y;
	P.z = c * Q.z;
	P.dimn = Q.dim();
	return P;
}

Point operator/( Point Q, int c) {
	Point P;
	P.x = Q.x / c;
	P.y = Q.y / c;
	P.z = Q.z / c;
	P.dimn = Q.dim();
	return P;
}

Point operator/( Point Q, double c) {
	Point P;
	P.x = Q.x / c;
	P.y = Q.y / c;
	P.z = Q.z / c;
	P.dimn = Q.dim();
	return P;
}

//------------------------------------------------------------------
// Point Addition (also convenient but often illegal)
//    is not valid unless part of an affine sum.
//    The programmer must enforce this (if they want to).
//------------------------------------------------------------------

Point operator+( Point Q, Point R)
{
	Point P;
	P.x = Q.x + R.x;
	P.y = Q.y + R.y;
	P.z = Q.z + R.z;
	P.dimn = max( Q.dim(), R.dim());
	return P;
}

//------------------------------------------------------------------
// Affine Sums
// Returns weighted sum, even when not affine, but...
// Tests if coeffs add to 1.  If not, sets: err = Esum.
//------------------------------------------------------------------

Point asum( int n, int c[], Point Q[])
{
	int        maxd = 0;
	int        cs = 0;
	Point      P;

	for (int i=0; i<n; i++) {
		cs += c[i];
		if (Q[i].dim() > maxd)
			maxd = Q[i].dim();
	}
	if (cs != 1)                 // not an affine sum
    THROW_MATH("Sum not affine (cooefs add to 1)");        

	for (int i=0; i<n; i++) {
		P.x += c[i] * Q[i].x;
		P.y += c[i] * Q[i].y;
		P.z += c[i] * Q[i].z;
	}
	P.dimn = maxd;
	return P;
}

Point asum( int n, double c[], Point Q[])
{
	int        maxd = 0;
	double     cs = 0.0;
	Point      P;

	for (int i=0; i<n; i++) {
		cs += c[i];
		if (Q[i].dim() > maxd)
			maxd = Q[i].dim();
	}
	if (cs != 1)                 // not an affine sum
    THROW_MATH("Sum not affine (cooefs add to 1)");        

	for (int i=0; i<n; i++) {
		P.x += c[i] * Q[i].x;
		P.y += c[i] * Q[i].y;
		P.z += c[i] * Q[i].z;
	}
	P.dimn = maxd;
	return P;
}

//------------------------------------------------------------------
// Distance between Points
//------------------------------------------------------------------

double d( Point P, Point Q) {      // Euclidean distance
	double dx = P.x - Q.x;
	double dy = P.y - Q.y;
	double dz = P.z - Q.z;
	return sqrt(dx*dx + dy*dy + dz*dz);
}

double d2( Point P, Point Q) {     // squared distance (more efficient)
	double dx = P.x - Q.x;
	double dy = P.y - Q.y;
	double dz = P.z - Q.z;
	return (dx*dx + dy*dy + dz*dz);
}

//------------------------------------------------------------------
// Sidedness of a Point wrt a directed line P1->P2
//        - makes sense in 2D only
//------------------------------------------------------------------

double Point::isLeft( Point P1, Point P2) {
	if (dimn != 2 || P1.dim() != 2 || P2.dim() != 2) {
    THROW_MATH("error: invalid dimension for operation");        
	}
	return ((P1.x - x) * (P2.y - y) - (P2.x - x) * (P1.y - y));
}

//------------------------------------------------------------------
// Error Routines
//------------------------------------------------------------------
/*
char* Point::errstr() {            // return error string
	switch (err) {
	case Enot:
		return "no error";
	case Edim:
		return "error: invalid dimension for operation";
	case Esum:
		return "error: Point sum is not affine";
	default:
		return "error: unknown err value";
	}
}
*/



//==================================================================
// iPoint Class Methods
//==================================================================

//------------------------------------------------------------------
// Constructors (add more as needed)
//------------------------------------------------------------------

// n-dim iPoint
iPoint::iPoint( int n, int a[]) {
	x = y = z = 0;
	switch (dimn = n) {
	case 3: z = a[2];
	case 2: y = a[1];
	case 1: x = a[0];
		break;
	default:
		THROW_MATH("Dimension of iPoint invalid for operation");
	}
}

iPoint::iPoint( int n, double a[]) {
	x = y = z = 0.0;
	switch (dimn = n) {
	case 3: z = a[2];
	case 2: y = a[1];
	case 1: x = a[0];
		break;
	default:
		THROW_MATH("Dimension of iPoint invalid for operation");
	}
}

//------------------------------------------------------------------
// IO streams
//------------------------------------------------------------------

// Read input iPoint format: "(%f)", "(%f, %f)", or "(%f, %f, %f)"
istream& operator>>( istream& input, iPoint& P) {
	char c;
	input >> c;                // skip '('
	input >> P.x;
	input >> c;                
	if (c == ')') {
		P.setdim(1);       // 1D coord
		return input;
	}
	// else                    // skip ','
	input >> P.y;
	input >> c;
	if (c == ')') {
		P.setdim(2);       // 2D coord
		return input;
	}
	// else                    // skip ','
	input >> P.z;
	P.setdim(3);               // 3D coord
	input >> c;                // skip ')'
	return input;
}

// Write output iPoint in format: "(%f)", "(%f, %f)", or "(%f, %f, %f)"
ostream& operator<<( ostream& output, iPoint P) {
	switch (P.dim()) {
	case 1:
		output << "(" << P.x << ")";
		break;
	case 2:
		output << "(" << P.x << ", " << P.y << ")";
		break;
	case 3:
		output << "(" << P.x << ", " << P.y << ", " << P.z << ")";
		break;
	default:
		THROW_MATH("Dimension of iPoint invalid for operation");
	}
	return output;
}

//------------------------------------------------------------------
// Assign (set) dimension
//------------------------------------------------------------------

int iPoint::setdim( int n) {
	switch (n) {
	case 1: y = 0;
	case 2: z = 0;
	case 3:
		return dimn = n;
	default:                      // out of range value
		THROW_MATH("Dimension of iPoint invalid for operation");
		return ERROR;
	}
}

//------------------------------------------------------------------
// Comparison (note: dimension must compare)
//------------------------------------------------------------------

int iPoint::operator==( iPoint Q)
{
  int mindimn = min(dimn, Q.dim());
	switch (mindimn) {
	case 1:
		return (x==Q.x);
	case 2:
		return (x==Q.x && y==Q.y);
	case 3:
	default:
		return (x==Q.x && y==Q.y && z==Q.z);
	}
}

int iPoint::operator!=( iPoint Q)
{
	if (dimn != Q.dim()) return TRUE;
	switch (dimn) {
	case 1:
		return (x!=Q.x);
	case 2:
		return (x!=Q.x || y!=Q.y);
	case 3:
	default:
		return (x!=Q.x || y!=Q.y || z!=Q.z);
	}
}

//------------------------------------------------------------------
// iPoint iVector Operations
//------------------------------------------------------------------

iVec iPoint::operator-( iPoint Q)        // iVec diff of iPoints
{
	iVec v;
	v.x = x - Q.x;
	v.y = y - Q.y;
	v.z = z - Q.z;
	v.dimn = max( dimn, Q.dim());
	return v;
}

iPoint iPoint::operator+( iVec v)        // +ve translation
{
	iPoint P;
	P.x = x + v.x;
	P.y = y + v.y;
	P.z = z + v.z;
	P.dimn = max( dimn, v.dim());
	return P;
}

iPoint iPoint::operator-( iVec v)        // -ve translation
{
	iPoint P;
	P.x = x - v.x;
	P.y = y - v.y;
	P.z = z - v.z;
	P.dimn = max( dimn, v.dim());
	return P;
}

iPoint& iPoint::operator+=( iVec v)        // +ve translation
{
	x += v.x;
	y += v.y;
	z += v.z;
	dimn = max( dimn, v.dim());
	return *this;
}

iPoint& iPoint::operator-=( iVec v)        // -ve translation
{
	x -= v.x;
	y -= v.y;
	z -= v.z;
	dimn = max( dimn, v.dim());
	return *this;
}

//------------------------------------------------------------------
// iPoint Scalar Operations (convenient but often illegal)
//        are not valid for iPoints in general,
//        unless they are 'affine' as coeffs of 
//        a sum in which all the coeffs add to 1,
//        such as: the sum (a*P + b*Q) with (a+b == 1).
//        The programmer must enforce this (if they want to).
//------------------------------------------------------------------

iPoint operator*( int c, iPoint Q) {
	iPoint P;
	P.x = c * Q.x;
	P.y = c * Q.y;
	P.z = c * Q.z;
	P.dimn = Q.dim();
	return P;
}

iPoint operator*( double c, iPoint Q) {
	iPoint P;
	P.x = c * Q.x;
	P.y = c * Q.y;
	P.z = c * Q.z;
	P.dimn = Q.dim();
	return P;
}

iPoint operator*( iPoint Q, int c) {
	iPoint P;
	P.x = c * Q.x;
	P.y = c * Q.y;
	P.z = c * Q.z;
	P.dimn = Q.dim();
	return P;
}

iPoint operator*( iPoint Q, double c) {
	iPoint P;
	P.x = c * Q.x;
	P.y = c * Q.y;
	P.z = c * Q.z;
	P.dimn = Q.dim();
	return P;
}

iPoint operator/( iPoint Q, int c) {
	iPoint P;
	P.x = Q.x / c;
	P.y = Q.y / c;
	P.z = Q.z / c;
	P.dimn = Q.dim();
	return P;
}

iPoint operator/( iPoint Q, double c) {
	iPoint P;
	P.x = Q.x / c;
	P.y = Q.y / c;
	P.z = Q.z / c;
	P.dimn = Q.dim();
	return P;
}
iPoint operator/( iPoint Q, iPoint c) {
  if (c.dimn != Q.dimn)
    THROW_MATH("iPoint: Number of dimensions doesn't match.");
	iPoint P;
	P.x = Q.x / c.x;
	P.y = Q.y / c.y;
	P.z = Q.z / c.z;
	P.dimn = Q.dim();
	return P;
}
iPoint& iPoint::operator/=(const iPoint &R)
{
  if (dimn != R.dimn)
    THROW_MATH("iPoint: Number of dimensions doesn't match.");
  switch (dimn) {
    case 3:
	    z = z / R.z;
    case 2:
	    y = y / R.y;
    case 1:
	    x = x / R.x;
  }
  return *this;
}


//------------------------------------------------------------------
// iPoint Addition (also convenient but often illegal)
//    is not valid unless part of an affine sum.
//    The programmer must enforce this (if they want to).
//------------------------------------------------------------------

iPoint operator+( iPoint Q, iPoint R)
{
	iPoint P;
	P.x = Q.x + R.x;
	P.y = Q.y + R.y;
	P.z = Q.z + R.z;
	P.dimn = max( Q.dim(), R.dim());
	return P;
}

//------------------------------------------------------------------
// Affine Sums
// Returns weighted sum, even when not affine, but...
// Tests if coeffs add to 1.  If not, sets: err = Esum.
//------------------------------------------------------------------

iPoint asum( int n, int c[], iPoint Q[])
{
	int        maxd = 0;
	int        cs = 0;
	iPoint      P;

	for (int i=0; i<n; i++) {
		cs += c[i];
		if (Q[i].dim() > maxd)
			maxd = Q[i].dim();
	}
	if (cs != 1)                 // not an affine sum
    THROW_MATH("Sum not affine (cooefs add to 1)");        

	for (int i=0; i<n; i++) {
		P.x += c[i] * Q[i].x;
		P.y += c[i] * Q[i].y;
		P.z += c[i] * Q[i].z;
	}
	P.dimn = maxd;
	return P;
}

bool iPoint::isInside(const iPoint& Q) const {
  if (dimn != Q.dimn)
    THROW_MATH("iPoint: Number of dimensions doesn't match.");
  switch (dimn) {
    case 3:
      return x > Q.x && y > Q.y && z < Q.z;
    case 2:
      return x > Q.x && y > Q.y;
  }
  return x > Q.x;
}

iPoint asum( int n, double c[], iPoint Q[])
{
	int        maxd = 0;
	double     cs = 0.0;
	iPoint      P;

	for (int i=0; i<n; i++) {
		cs += c[i];
		if (Q[i].dim() > maxd)
			maxd = Q[i].dim();
	}
	if (cs != 1)                 // not an affine sum
    THROW_MATH("Sum not affine (cooefs add to 1)");        

	for (int i=0; i<n; i++) {
		P.x += c[i] * Q[i].x;
		P.y += c[i] * Q[i].y;
		P.z += c[i] * Q[i].z;
	}
	P.dimn = maxd;
	return P;
}

//------------------------------------------------------------------
// Distance between iPoints
//------------------------------------------------------------------

double d( iPoint P, iPoint Q) {      // Euclidean distance
	double dx = P.x - Q.x;
	double dy = P.y - Q.y;
	double dz = P.z - Q.z;
	return sqrt(dx*dx + dy*dy + dz*dz);
}

int d2( iPoint P, iPoint Q) {     // squared distance (more efficient)
	double dx = P.x - Q.x;
	double dy = P.y - Q.y;
	double dz = P.z - Q.z;
	return (dx*dx + dy*dy + dz*dz);
}

//------------------------------------------------------------------
// Sidedness of a iPoint wrt a directed line P1->P2
//        - makes sense in 2D only
//------------------------------------------------------------------

double iPoint::isLeft( iPoint P1, iPoint P2) {
	if (dimn != 2 || P1.dim() != 2 || P2.dim() != 2) {
    THROW_MATH("error: invalid dimension for operation");        
	}
	return ((P1.x - x) * (P2.y - y) - (P2.x - x) * (P1.y - y));
}

int iPoint::Area() const {
	switch (dimn) {
	case 1:
		return (x);
	case 2:
		return (x * y);
	case 3:
	default:
		return (x*y*z);
	}
}