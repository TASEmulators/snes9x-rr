/*******************************************************************************
  Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
 
  (c) Copyright 1996 - 2002 Gary Henderson (gary.henderson@ntlworld.com) and
                            Jerremy Koot (jkoot@snes9x.com)

  (c) Copyright 2001 - 2004 John Weidman (jweidman@slip.net)

  (c) Copyright 2002 - 2004 Brad Jorsch (anomie@users.sourceforge.net),
                            funkyass (funkyass@spam.shaw.ca),
                            Joel Yliluoma (http://iki.fi/bisqwit/)
                            Kris Bleakley (codeviolation@hotmail.com),
                            Matthew Kendora,
                            Nach (n-a-c-h@users.sourceforge.net),
                            Peter Bortas (peter@bortas.org) and
                            zones (kasumitokoduck@yahoo.com)

  C4 x86 assembler and some C emulation code
  (c) Copyright 2000 - 2003 zsKnight (zsknight@zsnes.com),
                            _Demo_ (_demo_@zsnes.com), and Nach

  C4 C++ code
  (c) Copyright 2003 Brad Jorsch

  DSP-1 emulator code
  (c) Copyright 1998 - 2004 Ivar (ivar@snes9x.com), _Demo_, Gary Henderson,
                            John Weidman, neviksti (neviksti@hotmail.com),
                            Kris Bleakley, Andreas Naive

  DSP-2 emulator code
  (c) Copyright 2003 Kris Bleakley, John Weidman, neviksti, Matthew Kendora, and
                     Lord Nightmare (lord_nightmare@users.sourceforge.net

  OBC1 emulator code
  (c) Copyright 2001 - 2004 zsKnight, pagefault (pagefault@zsnes.com) and
                            Kris Bleakley
  Ported from x86 assembler to C by sanmaiwashi

  SPC7110 and RTC C++ emulator code
  (c) Copyright 2002 Matthew Kendora with research by
                     zsKnight, John Weidman, and Dark Force

  S-DD1 C emulator code
  (c) Copyright 2003 Brad Jorsch with research by
                     Andreas Naive and John Weidman
 
  S-RTC C emulator code
  (c) Copyright 2001 John Weidman
  
  ST010 C++ emulator code
  (c) Copyright 2003 Feather, Kris Bleakley, John Weidman and Matthew Kendora

  Super FX x86 assembler emulator code 
  (c) Copyright 1998 - 2003 zsKnight, _Demo_, and pagefault 

  Super FX C emulator code 
  (c) Copyright 1997 - 1999 Ivar, Gary Henderson and John Weidman


  SH assembler code partly based on x86 assembler code
  (c) Copyright 2002 - 2004 Marcus Comstedt (marcus@mc.pp.se) 

 
  Specific ports contains the works of other authors. See headers in
  individual files.
 
  Snes9x homepage: http://www.snes9x.com
 
  Permission to use, copy, modify and distribute Snes9x in both binary and
  source form, for non-commercial purposes, is hereby granted without fee,
  providing that this license information and copyright notice appear with
  all copies and any derived work.
 
  This software is provided 'as-is', without any express or implied
  warranty. In no event shall the authors be held liable for any damages
  arising from the use of this software.
 
  Snes9x is freeware for PERSONAL USE only. Commercial users should
  seek permission of the copyright holders first. Commercial use includes
  charging money for Snes9x or software derived from Snes9x.
 
  The copyright holders request that bug fixes and improvements to the code
  should be forwarded to them so everyone can benefit from the modifications
  in future versions.
 
  Super NES and Super Nintendo Entertainment System are trademarks of
  Nintendo Co., Limited and its subsidiary companies.
*******************************************************************************/
/****************************************************************
*																*	
*    AuxMath.h - Galería de funciones auxiliares geométricas	*
*				 Declaraciones de AuxMath.cpp.					*	
*																*
*	 By Cuervo (1999) 											*
*	 diego_tartara@ciudad.com.ar								*	
****************************************************************/

#ifndef __AUXMATH_H_CUERVO__
#define __AUXMATH_H_CUERVO__

#include <math.h>

#define PI		3.14159265358979323846264338327950288419716939937510f
#define PI2		(PI*2.0f)
#define SQRT_2  1.4142135623730950488016887242096980785696718753769f
#define EPS		   1.19e-07f		//epsilon
#define MAX_FLOAT  1.0e+38f			//max 32 bit float

//////////////////////////////////////////////////////////
// Clase vect: vector simple de tres componentes		//
//////////////////////////////////////////////////////////
class vect 
{
public:

	union
	{
		float n[3];
		struct
		{
			float x, y, z;
		};
	};

public:

	// Constructores
	vect() {};
	vect(const float x1, const float y1, const float z1);
	vect(const vect& v);          // copy constructor

  	// Operadores de asignación  
	vect& operator =  ( const vect v );   // asignacion de un vect          
	vect& operator += ( const vect v );   // suma/asignacion
	vect& operator -= ( const vect v );   // resta/asignacion
	vect& operator *= ( const vect v);    // multiplicación/asignación
	vect& operator /= ( const vect v);	  // division/asignación x float
	vect& operator *= ( const float num); // multiplicación/asignación x float
	vect& operator /= ( const float num); // division/asignación
	float& operator [] ( int i) { return n[i]; };   // indexación
	const float& operator[](int i) const;
	
	//float* cast
	operator float*() {return (float*)this;};

	//Operadores "friend", no pertenecen a la clase
	//operadores binarios
	friend vect operator+ (const vect& vect1, const vect& vect2);
	friend vect operator- (const vect& vect1, const vect& vect2);
	friend vect operator* (const vect& vect1,const vect& vect2);
	friend vect operator* (const vect& vect1, const float num);
	friend vect operator* (const float num, const vect& vect1);
	friend vect operator/ (const vect& vect1, const vect& vect2);
	friend vect operator/ (const vect& vect1, const float num);
	//operador unario
	friend vect operator- (const vect& vect1);
	//operadores de comparación
	friend int operator> (const vect& vect1, const vect& vect2);
	friend int operator>= (const vect& vect1, const vect& vect2);
	friend int operator< (const vect& vect1, const vect& vect2);
	friend int operator<= (const vect& vect1, const vect& vect2);
	friend int operator!= (const vect& vect1, const vect& vect2);
	friend int operator== (const vect& vect1, const vect& vect2);
	//funciones
	float GetMod(void) {	return (float)sqrt(x*x+y*y+z*z);}; 
	float GetModSquared(void) {		return x*x+y*y+z*z;};
	void Set(float _x, float _y, float _z)	{ x=_x; y=_y; z=_z;};
	void Normalize(void);
	void MakeOrthonormal(vect &base1, vect &base2);
};	

//----------------------------------------------------------------------/

//////////////////////////////////////////////////////////
// Clase quat: quaternion de rotaciones             	//
//////////////////////////////////////////////////////////
class quat 
{
public:

#ifdef __MINGW32__
/* FIXME: GCC doesn't seem to like anon structs and such.. */
	vect RotAx;
	float Angle;
	float x, y, z, w;
	float n[4];
#else

	union
	{
		struct
		{
			vect RotAx;
			float Angle;		
		};
		struct
		{
			float x, y, z, w;
		};
		float n[4];
	};

#endif

public:

	// Constructores
	quat() {};
	quat(const float x1, const float y1, const float z1, const float angle);
	quat(const quat& q);					// copy constructor
	quat(const vect& v, const float angle);	//de un vector (eje) y un ángulo
	//de tres ángulos respecto de c/eje (ángulos de Euler)
	//yaw:   sobre eje Z (también llamado heading)
	//pitch: sobre eje Y
	//roll:  sobre eje X
	quat(const float yaw, const float pitch, const float roll); 

	// Operadores de asignación  
	quat& operator =  ( const quat q );   // asignacion de un quat          
	quat& operator += ( const quat q );   // composición/asignacion
	quat& operator -= ( const quat q );   // resta/asignacion
	quat& operator *= ( const quat q );   // multiplicación/asignación
	quat& operator *= ( const float num); // multiplicación/asignación x float
	quat& operator /= ( const float num); // division/asignación
	
	//funciones
	void set(const float yaw, const float pitch, const float roll);
	void set(const float x1, const float y1, const float z1, const float angle);
};


//----------------------------------------------------------------------/
//////////////////////////////////////////////////////////
// Clase mat4: matriz de 4x4			            	//
//////////////////////////////////////////////////////////
class mat4 
{
public:

	union
	{
		float m[16];
		float m1[4][4];
	};


public:

	// Constructores
	mat4() {};
	mat4(	float m00, float m01, float m02, float m03,
			float m10, float m11, float m12, float m13,
			float m20, float m21, float m22, float m23,
			float m30, float m31, float m32, float m33);
	mat4(	const mat4 &m);

	// Assignment operators
	mat4& operator  = ( const mat4& m );      // asignación de otro mat4
	mat4& operator += ( const mat4& m );      // incrementato por mat4
	mat4& operator -= ( const mat4& m );      // decremento por mat4
	mat4& operator *= ( const mat4& m );      // multiplicacion por matriz
	mat4& operator *= ( const float d );      // multiplicacion por constante
	mat4& operator /= ( const float d );      // division por constante
	
	//indexación
	float & operator () ( const int i, const int j) { return m1[i][j]; } 
	float & operator [] ( int i) { return m[i]; };   

	//operadores friends
	friend mat4 operator - (const mat4& a);					// -m1
	friend mat4 operator + (const mat4& a, const mat4& b);  // m1 + m2
	friend mat4 operator - (const mat4& a, const mat4& b);  // m1 - m2
	friend mat4 operator * (const mat4& a, const mat4& b);  // m1 * m2
	friend mat4 operator * (const mat4& a, const float d);  // m1 * d
	friend mat4 operator * (const float d, const mat4& a);  // d * m1
	friend mat4 operator / (const mat4& a, const float d);  // m1 / d

	//operadores de comparación
	friend int operator == (const mat4& a, const mat4& b);  // m1 == m2 ?
	friend int operator != (const mat4& a, const mat4& b);  // m1 != m2 ?

	//funciones
	void Transpose();
	void Invert();
	void MakeGL();
	void Set(int i, int j, float value) {m1[i][j] = value;};
	void SetId (void);
	void FromQuat(const quat &q);
    void FromAllVectors(const vect &vpn, const vect &vup, const vect &vr);
	void SetTranslation(const vect &pos);
	vect GetTranslation(void);
	void AddTranslation(const vect &pos);
	void SetViewUp(const vect &vup);
	void SetViewRight(const vect &vr);
	void SetViewNormal(const vect &vpn);
	vect GetViewUp(void);
	vect GetViewRight(void);
	vect GetViewNormal(void);

	//float* cast
	operator float*() { return (float*)this; };
};





//////////////////////////////////////////
//Definiciones de funciones contenidas	//
//////////////////////////////////////////


float DegToRad		(const float deg);
void  DegToRad		(float *deg);
float RadToDeg		(const float rad);
void  RadToDeg		(float *rad);
int   Sgn			(float num);
void  ClampD		(float angle);
void  ClampR		(float angle);
float VectDotProd	(const vect& vect1, const vect& vect2);
vect  VectXProd		(const vect& vect1, const vect& vect2);
float Mod			(const vect& vect1);	
float ModSquared	(const vect& vect1);
void  Normalize		(vect *vect1);
vect  Normalize		(const vect& vect1);
float OrigToRect	(const vect& point1, const vect& point2);
float OrigToRect2	(const vect& dir, const vect& point);
float PointToRect	(const vect& point, const vect& rect1, const vect& rect2);
float OrigToPlane	(const vect& point1, const vect& point2, const vect& point3);
float PointToPlane	(const vect& point, const vect& plane1, const vect& plane2, const vect& plane3);
float Distance		(const vect& point1, const vect& point2);	
float GetAngle		(const vect& point1, const vect& point2, const vect& point3);
float GetAngle		(const vect& vect1, const vect& vect2);
void  MatrixfromQuat   (const quat& q, float* m);
void  QuatfromMatrix   (const float *m, quat& q);
void  QuatfromEuler	   (const float yaw, const float pitch,
						 const float roll, quat& q);	
void  QuatSlerp		   (const quat &from, const quat &to,
						float t, quat &res);


#endif //__AUXMATH_H_CUERVO__
