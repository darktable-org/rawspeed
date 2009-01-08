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
// Vec Class Methods
//==================================================================

//------------------------------------------------------------------
//  Unary Ops
//------------------------------------------------------------------

// Unary minus
Vec Vec::operator-() {
	Vec v;
	v.x = -x; v.y = -y; v.z = -z;
	v.dimn = dimn;
	return v;
}

// Unary 2D perp operator
Vec Vec::operator~() {
	if (dimn != 2) 
    THROW_MATH("Error: Invalid dimension for operation");        
	Vec v;
	v.x = -y; v.y = x; v.z = z;
	v.dimn = dimn;
	return v;
}

//------------------------------------------------------------------
//  Scalar Ops
//------------------------------------------------------------------

// Scalar multiplication
Vec operator*( int c, Vec w ) {
	Vec v;
	v.x = c * w.x;
	v.y = c * w.y;
	v.z = c * w.z;
	v.dimn = w.dim();
	return v;
}

Vec operator*( double c, Vec w ) {
	Vec v;
	v.x = c * w.x;
	v.y = c * w.y;
	v.z = c * w.z;
	v.dimn = w.dim();
	return v;
}

Vec operator*( Vec w, int c ) {
	Vec v;
	v.x = c * w.x;
	v.y = c * w.y;
	v.z = c * w.z;
	v.dimn = w.dim();
	return v;
}

Vec operator*( Vec w, double c ) {
	Vec v;
	v.x = c * w.x;
	v.y = c * w.y;
	v.z = c * w.z;
	v.dimn = w.dim();
	return v;
}

// Scalar division
Vec operator/( Vec w, int c ) {
	Vec v;
	v.x = w.x / c;
	v.y = w.y / c;
	v.z = w.z / c;
	v.dimn = w.dim();
	return v;
}

Vec operator/( Vec w, double c ) {
	Vec v;
	v.x = w.x / c;
	v.y = w.y / c;
	v.z = w.z / c;
	v.dimn = w.dim();
	return v;
}

//------------------------------------------------------------------
//  Arithmetic Ops
//------------------------------------------------------------------

Vec Vec::operator+( Vec w ) {
	Vec v;
	v.x = x + w.x;
	v.y = y + w.y;
	v.z = z + w.z;
	v.dimn = max( dimn, w.dim());
	return v;
}

Vec Vec::operator-( Vec w ) {
	Vec v;
	v.x = x - w.x;
	v.y = y - w.y;
	v.z = z - w.z;
	v.dimn = max( dimn, w.dim());
	return v;
}

//------------------------------------------------------------------
//  Products
//------------------------------------------------------------------

// Inner Dot Product
double Vec::operator*( Vec w ) {
	return (x * w.x + y * w.y + z * w.z);
}

// 2D Exterior Perp Product
double Vec::operator|( Vec w ) {
	if (dimn != 2) 
    THROW_MATH("Error: Invalid dimension for operation");        
	return (x * w.y - y * w.x);
}

// 3D Exterior Cross Product
Vec Vec::operator^( Vec w ) {
	Vec v;
	v.x = y * w.z - z * w.y;
	v.y = z * w.x - x * w.z;
	v.z = x * w.y - y * w.x;
	v.dimn = 3;
	return v;
}

//------------------------------------------------------------------
//  Shorthand Ops
//------------------------------------------------------------------

Vec& Vec::operator*=( double c ) {        // Vec scalar mult
	x *= c;
	y *= c;
	z *= c;
	return *this;
}

Vec& Vec::operator/=( double c ) {        // Vec scalar div
	x /= c;
	y /= c;
	z /= c;
	return *this;
}

Vec& Vec::operator+=( Vec w ) {        // Vec increment
	x += w.x;
	y += w.y;
	z += w.z;
	dimn = max(dimn, w.dim());
	return *this;
}

Vec& Vec::operator-=( Vec w ) {        // Vec decrement
	x -= w.x;
	y -= w.y;
	z -= w.z;
	dimn = max(dimn, w.dim());
	return *this;
}

Vec& Vec::operator^=( Vec w ) {        // 3D exterior cross product
	double ox=x, oy=y, oz=z;
	x = oy * w.z - oz * w.y;
	y = oz * w.x - ox * w.z;
	z = ox * w.y - oy * w.x;
	dimn = 3;
	return *this;
}

//------------------------------------------------------------------
//  Special Operations
//------------------------------------------------------------------

void Vec::normalize() {                      // convert to unit length
	double ln = sqrt( x*x + y*y + z*z );
	if (ln == 0) return;                    // do nothing for nothing
	x /= ln;
	y /= ln;
	z /= ln;
}

Vec sum( int n, int c[], Vec w[] ) {     // Vec sum
	int     maxd = 0;
	Vec  v;

	for (int i=0; i<n; i++) {
		if (w[i].dim() > maxd)
			maxd = w[i].dim();
	}
	v.dimn = maxd;

	for (int i=0; i<n; i++) {
		v.x += c[i] * w[i].x;
		v.y += c[i] * w[i].y;
		v.z += c[i] * w[i].z;
	}
	return v;
}

Vec sum( int n, double c[], Vec w[] ) {  // Vec sum
	int     maxd = 0;
	Vec  v;

	for (int i=0; i<n; i++) {
		if (w[i].dim() > maxd)
			maxd = w[i].dim();
	}
	v.dimn = maxd;

	for (int i=0; i<n; i++) {
		v.x += c[i] * w[i].x;
		v.y += c[i] * w[i].y;
		v.z += c[i] * w[i].z;
	}
	return v;
}

/*******************************************************************
 * Integer version
 *******************************************************************/

// Unary minus
iVec iVec::operator-() {
	iVec v;
	v.x = -x; v.y = -y; v.z = -z;
	v.dimn = dimn;
	return v;
}

// Unary 2D perp operator
iVec iVec::operator~() {
	if (dimn != 2) 
    THROW_MATH("Error: Invalid dimension for operation");        
	iVec v;
	v.x = -y; v.y = x; v.z = z;
	v.dimn = dimn;
	return v;
}

//------------------------------------------------------------------
//  Scalar Ops
//------------------------------------------------------------------

// Scalar multiplication
iVec operator*( int c, iVec w ) {
	iVec v;
	v.x = c * w.x;
	v.y = c * w.y;
	v.z = c * w.z;
	v.dimn = w.dim();
	return v;
}

iVec operator*( double c, iVec w ) {
	iVec v;
	v.x = c * w.x;
	v.y = c * w.y;
	v.z = c * w.z;
	v.dimn = w.dim();
	return v;
}

iVec operator*( iVec w, int c ) {
	iVec v;
	v.x = c * w.x;
	v.y = c * w.y;
	v.z = c * w.z;
	v.dimn = w.dim();
	return v;
}

iVec operator*( iVec w, double c ) {
	iVec v;
	v.x = c * w.x;
	v.y = c * w.y;
	v.z = c * w.z;
	v.dimn = w.dim();
	return v;
}

// Scalar division
iVec operator/( iVec w, int c ) {
	iVec v;
	v.x = w.x / c;
	v.y = w.y / c;
	v.z = w.z / c;
	v.dimn = w.dim();
	return v;
}

iVec operator/( iVec w, double c ) {
	iVec v;
	v.x = w.x / c;
	v.y = w.y / c;
	v.z = w.z / c;
	v.dimn = w.dim();
	return v;
}

//------------------------------------------------------------------
//  Arithmetic Ops
//------------------------------------------------------------------

iVec iVec::operator+( iVec w ) {
	iVec v;
	v.x = x + w.x;
	v.y = y + w.y;
	v.z = z + w.z;
	v.dimn = max( dimn, w.dim());
	return v;
}

iVec iVec::operator-( iVec w ) {
	iVec v;
	v.x = x - w.x;
	v.y = y - w.y;
	v.z = z - w.z;
	v.dimn = max( dimn, w.dim());
	return v;
}

//------------------------------------------------------------------
//  Products
//------------------------------------------------------------------

// Inner Dot Product
double iVec::operator*( iVec w ) {
	return (x * w.x + y * w.y + z * w.z);
}

// 2D Exterior Perp Product
double iVec::operator|( iVec w ) {
	if (dimn != 2) 
    THROW_MATH("Error: Invalid dimension for operation");        
	return (x * w.y - y * w.x);
}

// 3D Exterior Cross Product
iVec iVec::operator^( iVec w ) {
	iVec v;
	v.x = y * w.z - z * w.y;
	v.y = z * w.x - x * w.z;
	v.z = x * w.y - y * w.x;
	v.dimn = 3;
	return v;
}

//------------------------------------------------------------------
//  Shorthand Ops
//------------------------------------------------------------------

iVec& iVec::operator*=( double c ) {        // iVec scalar mult
	x *= c;
	y *= c;
	z *= c;
	return *this;
}

iVec& iVec::operator/=( double c ) {        // iVec scalar div
	x /= c;
	y /= c;
	z /= c;
	return *this;
}

iVec& iVec::operator+=( iVec w ) {        // iVec increment
	x += w.x;
	y += w.y;
	z += w.z;
	dimn = max(dimn, w.dim());
	return *this;
}

iVec& iVec::operator-=( iVec w ) {        // iVec decrement
	x -= w.x;
	y -= w.y;
	z -= w.z;
	dimn = max(dimn, w.dim());
	return *this;
}

iVec& iVec::operator^=( iVec w ) {        // 3D exterior cross product
	double ox=x, oy=y, oz=z;
	x = oy * w.z - oz * w.y;
	y = oz * w.x - ox * w.z;
	z = ox * w.y - oy * w.x;
	dimn = 3;
	return *this;
}

//------------------------------------------------------------------
//  Special Operations
//------------------------------------------------------------------

void iVec::normalize() {                      // convert to unit length
	double ln = sqrt( (double)x*x + y*y + z*z );
	if (ln == 0) return;                    // do nothing for nothing
	x /= (double)ln;
	y /= (double)ln;
	z /= (double)ln;
}

iVec sum( int n, int c[], iVec w[] ) {     // iVec sum
	int     maxd = 0;
	iVec  v;

	for (int i=0; i<n; i++) {
		if (w[i].dim() > maxd)
			maxd = w[i].dim();
	}
	v.dimn = maxd;

	for (int i=0; i<n; i++) {
		v.x += c[i] * w[i].x;
		v.y += c[i] * w[i].y;
		v.z += c[i] * w[i].z;
	}
	return v;
}

iVec sum( int n, double c[], iVec w[] ) {  // iVec sum
	int     maxd = 0;
	iVec  v;

	for (int i=0; i<n; i++) {
		if (w[i].dim() > maxd)
			maxd = w[i].dim();
	}
	v.dimn = maxd;

	for (int i=0; i<n; i++) {
		v.x += c[i] * w[i].x;
		v.y += c[i] * w[i].y;
		v.z += c[i] * w[i].z;
	}
	return v;
}
