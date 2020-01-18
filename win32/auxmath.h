/**********************************************************************************
  Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.

  (c) Copyright 1996 - 2002  Gary Henderson (gary.henderson@ntlworld.com),
                             Jerremy Koot (jkoot@snes9x.com)

  (c) Copyright 2002 - 2004  Matthew Kendora

  (c) Copyright 2002 - 2005  Peter Bortas (peter@bortas.org)

  (c) Copyright 2004 - 2005  Joel Yliluoma (http://iki.fi/bisqwit/)

  (c) Copyright 2001 - 2006  John Weidman (jweidman@slip.net)

  (c) Copyright 2002 - 2006  funkyass (funkyass@spam.shaw.ca),
                             Kris Bleakley (codeviolation@hotmail.com)

  (c) Copyright 2002 - 2007  Brad Jorsch (anomie@users.sourceforge.net),
                             Nach (n-a-c-h@users.sourceforge.net),
                             zones (kasumitokoduck@yahoo.com)

  (c) Copyright 2006 - 2007  nitsuja


  BS-X C emulator code
  (c) Copyright 2005 - 2006  Dreamer Nom,
                             zones

  C4 x86 assembler and some C emulation code
  (c) Copyright 2000 - 2003  _Demo_ (_demo_@zsnes.com),
                             Nach,
                             zsKnight (zsknight@zsnes.com)

  C4 C++ code
  (c) Copyright 2003 - 2006  Brad Jorsch,
                             Nach

  DSP-1 emulator code
  (c) Copyright 1998 - 2006  _Demo_,
                             Andreas Naive (andreasnaive@gmail.com)
                             Gary Henderson,
                             Ivar (ivar@snes9x.com),
                             John Weidman,
                             Kris Bleakley,
                             Matthew Kendora,
                             Nach,
                             neviksti (neviksti@hotmail.com)

  DSP-2 emulator code
  (c) Copyright 2003         John Weidman,
                             Kris Bleakley,
                             Lord Nightmare (lord_nightmare@users.sourceforge.net),
                             Matthew Kendora,
                             neviksti


  DSP-3 emulator code
  (c) Copyright 2003 - 2006  John Weidman,
                             Kris Bleakley,
                             Lancer,
                             z80 gaiden

  DSP-4 emulator code
  (c) Copyright 2004 - 2006  Dreamer Nom,
                             John Weidman,
                             Kris Bleakley,
                             Nach,
                             z80 gaiden

  OBC1 emulator code
  (c) Copyright 2001 - 2004  zsKnight,
                             pagefault (pagefault@zsnes.com),
                             Kris Bleakley,
                             Ported from x86 assembler to C by sanmaiwashi

  SPC7110 and RTC C++ emulator code
  (c) Copyright 2002         Matthew Kendora with research by
                             zsKnight,
                             John Weidman,
                             Dark Force

  S-DD1 C emulator code
  (c) Copyright 2003         Brad Jorsch with research by
                             Andreas Naive,
                             John Weidman

  S-RTC C emulator code
  (c) Copyright 2001-2006    byuu,
                             John Weidman

  ST010 C++ emulator code
  (c) Copyright 2003         Feather,
                             John Weidman,
                             Kris Bleakley,
                             Matthew Kendora

  Super FX x86 assembler emulator code
  (c) Copyright 1998 - 2003  _Demo_,
                             pagefault,
                             zsKnight,

  Super FX C emulator code
  (c) Copyright 1997 - 1999  Ivar,
                             Gary Henderson,
                             John Weidman

  Sound DSP emulator code is derived from SNEeSe and OpenSPC:
  (c) Copyright 1998 - 2003  Brad Martin
  (c) Copyright 1998 - 2006  Charles Bilyue'

  SH assembler code partly based on x86 assembler code
  (c) Copyright 2002 - 2004  Marcus Comstedt (marcus@mc.pp.se)

  2xSaI filter
  (c) Copyright 1999 - 2001  Derek Liauw Kie Fa

  HQ2x, HQ3x, HQ4x filters
  (c) Copyright 2003         Maxim Stepin (maxim@hiend3d.com)

  Win32 GUI code
  (c) Copyright 2003 - 2006  blip,
                             funkyass,
                             Matthew Kendora,
                             Nach,
                             nitsuja

  Mac OS GUI code
  (c) Copyright 1998 - 2001  John Stiles
  (c) Copyright 2001 - 2007  zones


  Specific ports contains the works of other authors. See headers in
  individual files.


  Snes9x homepage: http://www.snes9x.com

  Permission to use, copy, modify and/or distribute Snes9x in both binary
  and source form, for non-commercial purposes, is hereby granted without
  fee, providing that this license information and copyright notice appear
  with all copies and any derived work.

  This software is provided 'as-is', without any express or implied
  warranty. In no event shall the authors be held liable for any damages
  arising from the use of this software or it's derivatives.

  Snes9x is freeware for PERSONAL USE only. Commercial users should
  seek permission of the copyright holders first. Commercial use includes,
  but is not limited to, charging money for Snes9x or software derived from
  Snes9x, including Snes9x or derivatives in commercial game bundles, and/or
  using Snes9x as a promotion for your commercial product.

  The copyright holders request that bug fixes and improvements to the code
  should be forwarded to them so everyone can benefit from the modifications
  in future versions.

  Super NES and Super Nintendo Entertainment System are trademarks of
  Nintendo Co., Limited and its subsidiary companies.
**********************************************************************************/




/****************************************************************
*																*
*    AuxMath.h - Galer� de funciones auxiliares geom�ricas	*
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

  	// Operadores de asignaci�
	vect& operator =  ( const vect v );   // asignacion de un vect
	vect& operator += ( const vect v );   // suma/asignacion
	vect& operator -= ( const vect v );   // resta/asignacion
	vect& operator *= ( const vect v);    // multiplicaci�/asignaci�
	vect& operator /= ( const vect v);	  // division/asignaci� x float
	vect& operator *= ( const float num); // multiplicaci�/asignaci� x float
	vect& operator /= ( const float num); // division/asignaci�
	float& operator [] ( int i) { return n[i]; };   // indexaci�
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
	//operadores de comparaci�
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
	quat(const vect& v, const float angle);	//de un vector (eje) y un �gulo
	//de tres �gulos respecto de c/eje (�gulos de Euler)
	//yaw:   sobre eje Z (tambi� llamado heading)
	//pitch: sobre eje Y
	//roll:  sobre eje X
	quat(const float yaw, const float pitch, const float roll);

	// Operadores de asignaci�
	quat& operator =  ( const quat q );   // asignacion de un quat
	quat& operator += ( const quat q );   // composici�/asignacion
	quat& operator -= ( const quat q );   // resta/asignacion
	quat& operator *= ( const quat q );   // multiplicaci�/asignaci�
	quat& operator *= ( const float num); // multiplicaci�/asignaci� x float
	quat& operator /= ( const float num); // division/asignaci�

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
	mat4& operator  = ( const mat4& m );      // asignaci� de otro mat4
	mat4& operator += ( const mat4& m );      // incrementato por mat4
	mat4& operator -= ( const mat4& m );      // decremento por mat4
	mat4& operator *= ( const mat4& m );      // multiplicacion por matriz
	mat4& operator *= ( const float d );      // multiplicacion por constante
	mat4& operator /= ( const float d );      // division por constante

	//indexaci�
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

	//operadores de comparaci�
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
