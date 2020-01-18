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
*    AuxMath.cpp - Galeria de funciones auxiliares geométricas	*
*				   Implementación.								*	
*																*
*	 By Cuervo (1999) 											*
*	 diego_tartara@ciudad.com.ar								*
****************************************************************/

#if defined(USE_GLIDE) || defined(USE_OPENGL)

//headers 
#include "auxmath.h"

#pragma warning (disable : 4244)	//Conversión de double a float

//----------------------------------------------------------------------/
//////////////////////////////////////////////////////
//Implementación de los operadores de la clase vect	//
//////////////////////////////////////////////////////

//////////////////
//Constructores //
//////////////////
vect::vect(const float x1, const float y1, const float z1)
{
	x = x1; 
	y = y1; 
	z = z1;
}

vect::vect(const vect& v)
{ 
	x = v.x; 
	y = v.y; 
	z = v.z; 
}

/////////////////////////////
//Operadores de asignación //
/////////////////////////////
vect& vect::operator =  (const vect v)
{
	x = v.x; 
	y = v.y; 
	z = v.z; 
	return *this; 
}	

vect& vect::operator += (const vect v)
{
	x += v.x;
	y += v.y;
	z += v.z;
	return *this;
}

vect& vect::operator -= (const vect v)
{
	x -= v.x;
	y -= v.y;
	z -= v.z;
	return *this;
}

vect& vect::operator *= (const float num)
{
	x *= num;
	y *= num;
	z *= num;
	return *this;
}

vect& vect::operator *= (const vect v)
{
	x *= v.x;
	y *= v.y;
	z *= v.z;
	return *this;
}

vect& vect::operator /= (const float num)
{
	x /= num;
	y /= num;
	z /= num;
	return *this;
}

vect& vect::operator /= (const vect v)
{
	x /= v.x;
	y /= v.y;
	z /= v.z;
	return *this;
}

/////////////////////////////////
// Operador de acceso indexado //
/////////////////////////////////
const float& vect::operator[](int i) const
{
	return n[i];
}

////////////////////////////////////////////////////////////////
//															   /		
// Implementaciones de operadores friends para la clase vect   /	
//															   /	
////////////////////////////////////////////////////////////////

/////////////////////////
// Operadores binarios //
/////////////////////////
vect operator+ (const vect& vect1, const vect& vect2)
{
	vect result ( vect1.x + vect2.x,
		          vect1.y + vect2.y,
		          vect1.z + vect2.z );
	return result;

}

vect operator- (const vect& vect1, const vect& vect2)
{
	vect result ( vect1.x - vect2.x,
			      vect1.y - vect2.y,
				  vect1.z - vect2.z );
	return result;
}

vect operator* (const vect& vect1,const vect& vect2)
{
	vect result ( vect1.x * vect2.x,
				  vect1.y * vect2.y,
				  vect1.z * vect2.z );	
	return result;
}

vect operator* (const vect& vect1, const float num)
{
	vect result ( num * vect1.x,
				  num * vect1.y,
				  num * vect1.z ); 
	return result;
}

vect operator* (const float num, const vect& vect1)
{
	vect result ( num * vect1.x,
			      num * vect1.y,
				  num * vect1.z ); 
	return result;
}

vect operator/ (const vect& vect1, const vect& vect2)
{
	vect result ( vect1.x / vect2.x,
				  vect1.y / vect2.y,
				  vect1.z / vect2.z);
	return result;
}

vect operator/ (const vect& vect1, const float num)
{
	vect result ( vect1.x / num,
				  vect1.y / num,
				  vect1.z / num ); 
	return result;
}

//////////////////////////
// Operadores unarios   //
//////////////////////////
vect operator- (const vect& vect1)
{
	vect result ( -vect1.x,
				  -vect1.y,
				  -vect1.z);
	return result;
}

	
///////////////////////////////
// Operadores de comparación //
///////////////////////////////
int operator> (const vect& vect1, const vect& vect2)
{
	return Mod(vect1) > Mod(vect2);
}

int operator>= (const vect& vect1, const vect& vect2)
{
	return Mod(vect1) >= Mod(vect2);
}

int operator< (const vect& vect1, const vect& vect2)
{
	return Mod(vect1) < Mod(vect2);
}

int operator<= (const vect& vect1, const vect& vect2)
{
	return Mod(vect1) >= Mod(vect2);
}

int operator== (const vect& vect1,const vect& vect2)
{
	return (vect1.x == vect2.x &&
			vect1.y == vect2.y &&
			vect1.z == vect2.z);
}

int operator!= (const vect& vect1, const vect& vect2)
{
	return !(vect1.x == vect2.x &&
			 vect1.y == vect2.y &&
			 vect1.z == vect2.z);
}

void vect::Normalize(void)
{
	float mod = GetMod();
	x /= mod;
	y /= mod;
	z /= mod;
}

//a partir de dos vectores, genera el ortonormal (perpendicular)
//a ambos para completar una base
void vect::MakeOrthonormal(vect &base1, vect &base2)
{
	base1.Normalize();
	base2.Normalize();
	
	*this = VectXProd(base1,base2);

	Normalize();

	base1 = VectXProd(base2,*this);

	base1.Normalize();
}



//----------------------------------------------------------------------/

//////////////////////////////////////////////////////
//Implementación de los operadores de la clase quat	//
//////////////////////////////////////////////////////

//////////////////
//Constructores //
//////////////////
quat::quat(const quat& q)
{ 
	x = q.x;
	y = q.y; 
	z = q.z; 
	w = q.w;
}

quat::quat(const float x1, const float y1, const float z1, const float angle)
{
	x = x1;
	y = y1;
	z = z1;
	w = angle;
}
	
inline quat::quat(const vect& v, const float angle)	
{
	x = v.x;
	y = v.y;
	z = v.z;
	w = angle;
}

quat::quat(const float yaw, const float pitch,
				  const float roll) 
{
	//el orden de las rotaciones es:
	//r' = roll ( pitch ( yaw r)))
	//yaw:   sobre eje Z (también llamado heading)
	//pitch: sobre eje Y
	//roll:  sobre eje X
	float SinYaw   = (float)sin(yaw/2);
    float SinPitch = (float)sin(pitch/2);
    float SinRoll  = (float)sin(roll/2);
    float CosYaw   = (float)cos(yaw/2);
    float CosPitch = (float)cos(pitch/2);
    float CosRoll  = (float)cos(roll/2);

    x = SinRoll * CosPitch * CosYaw - CosRoll * SinPitch * SinYaw;
    y = CosRoll * SinPitch * CosYaw + SinRoll * CosPitch * SinYaw;
    z = CosRoll * CosPitch * SinYaw - SinRoll * SinPitch * CosYaw;
    
	w = CosRoll * CosPitch * CosYaw + SinRoll * SinPitch * SinYaw;
}

/////////////////////////////
//Operadores de asignación //
/////////////////////////////
quat& quat::operator =  ( const quat q )
{
	x = q.x;
	y = q.y;
	z = q.z;
	w = q.w;
	return *this;
}

inline quat& quat::operator += ( const quat q )
{
	x += q.x;
	y += q.y;
	z += q.z;
	w += q.w;
	return *this;
}

inline quat& quat::operator -= (const quat q)
{
	x -= q.x;
	y -= q.y;
	z -= q.z;
	w -= q.w;
	return *this;
}


quat& quat::operator *= (const quat q)
{
	//guardo los valores actuales
	float Angle1 = Angle;
	vect RotAx1(RotAx);

	//Los quaterniones se multiplican:
	//Angulo = ang1*ang2 - (RotAx1 o RotAx2)
	//Eje = ang1*V2 + ang2*V1 + (RotAx1 x RotAx2)
	//siendo 'o' prod escalar y 'x' prod vectorial

	//ángulo
	Angle = Angle1 * q.Angle - (VectDotProd(RotAx1, q.RotAx)); 

	//vector
	RotAx = Angle1 * q.RotAx + q.Angle * RotAx1 + VectXProd(RotAx1, q.RotAx);

	return *this;
}

quat& quat::operator *= ( const float num)
{
	x *= num;
	y *= num;
	z *= num;
	w *= num;
	return *this;
}

void quat::set(const float yaw, const float pitch, const float roll)
{
	//idem al constructor a partir de ángulos Euler
	float SinYaw   = (float)sin(yaw/2);
    float SinPitch = (float)sin(pitch/2);
    float SinRoll  = (float)sin(roll/2);
    float CosYaw   = (float)cos(yaw/2);
    float CosPitch = (float)cos(pitch/2);
    float CosRoll  = (float)cos(roll/2);

    x = SinRoll * CosPitch * CosYaw - CosRoll * SinPitch * SinYaw;
    y = CosRoll * SinPitch * CosYaw + SinRoll * CosPitch * SinYaw;
    z = CosRoll * CosPitch * SinYaw - SinRoll * SinPitch * CosYaw;
    
	w = CosRoll * CosPitch * CosYaw + SinRoll * SinPitch * SinYaw;

}

void quat::set(const float x1, const float y1, const float z1, const float angle)
{
	w = angle;
	x = x1;
	y = y1;
	z = z1;
}


//----------------------------------------------------------------------/

//////////////////////////////////////////////////////
//Implementación de los operadores de la clase mat4	//
//////////////////////////////////////////////////////

//////////////////
//Constructores //
//////////////////

mat4::mat4( float m00, float m01, float m02, float m03,
		float m10, float m11, float m12, float m13,
		float m20, float m21, float m22, float m23,
		float m30, float m31, float m32, float m33)
{
	m[0] = m00;		m[1] = m01;		m[2] = m02;		m[3] = m03;
	m[4] = m10;		m[5] = m11;		m[6] = m12;		m[7] = m13;
	m[8] = m20;		m[9] = m21;		m[10]= m22;		m[11]= m23;
	m[12]= m30;		m[13]= m31;		m[14]= m32;		m[15]= m33;
}


mat4::mat4(const mat4 &m)
{
	for(int a = 0 ; a < 16; a++)
		this->m[a] = m.m[a];
}

/////////////////////////////
//Operadores de asignación //
/////////////////////////////
mat4& mat4::operator  = ( const mat4& m )
{
	//resta miembro a miembro
	for(int a = 0 ; a < 16; a++)
		this->m[a] = m.m[a];
	return *this;
}
  
mat4& mat4::operator += ( const mat4& m )
{
	//suma miembro a miembro
	for(int a = 0 ; a < 16; a++)
		this->m[a] += m.m[a];
	return *this;
}

mat4& mat4::operator -= ( const mat4& m )
{
	for(int a = 0 ; a < 16; a++)
		this->m[a] -= m.m[a];
	return *this;
}

mat4& mat4::operator *= ( const mat4& m )
{
	//la multiplicacion de una matriz por otra consiste en
	//hacer el producto escalar entre los vectores fila de
	//la primera por los vectores columna de la segunda que
	//donde se cruzan definen el valor a calcular
	//asi, si C = A * B
	//C[i][j] = A[i].B[j]
	//siendo A[i] -> vector fila i de la matriz A (4 componentes)
	//	     B[j] -> vector columna j de la matriz b (4 componentes)		
	//el prod escalar era (x1,y1...).(x2,y2...) = x1*x2 + y1*y2 +..
		
	mat4 c(*this);

	this->m1[0][0] = c.m1[0][0] * m.m1[0][0] + c.m1[1][0] * m.m1[0][1] + c.m1[2][0] * m.m1[0][2] + c.m1[3][0] * m.m1[0][3];
    this->m1[0][1] = c.m1[0][1] * m.m1[0][0] + c.m1[1][1] * m.m1[0][1] + c.m1[2][1] * m.m1[0][2] + c.m1[3][1] * m.m1[0][3];
	this->m1[0][2] = c.m1[0][2] * m.m1[0][0] + c.m1[1][2] * m.m1[0][1] + c.m1[2][2] * m.m1[0][2] + c.m1[3][2] * m.m1[0][3];
	this->m1[0][3] = c.m1[0][3] * m.m1[0][0] + c.m1[1][3] * m.m1[0][1] + c.m1[2][3] * m.m1[0][2] + c.m1[3][3] * m.m1[0][3];
  
	this->m1[1][0] = c.m1[0][0] * m.m1[1][0] + c.m1[1][0] * m.m1[1][1] + c.m1[2][0] * m.m1[1][2] + c.m1[3][0] * m.m1[1][3];
	this->m1[1][1] = c.m1[0][1] * m.m1[1][0] + c.m1[1][1] * m.m1[1][1] + c.m1[2][1] * m.m1[1][2] + c.m1[3][1] * m.m1[1][3];
	this->m1[1][2] = c.m1[0][2] * m.m1[1][0] + c.m1[1][2] * m.m1[1][1] + c.m1[2][2] * m.m1[1][2] + c.m1[3][2] * m.m1[1][3];
	this->m1[1][3] = c.m1[0][3] * m.m1[1][0] + c.m1[1][3] * m.m1[1][1] + c.m1[2][3] * m.m1[1][2] + c.m1[3][3] * m.m1[1][3];
  
	this->m1[2][0] = c.m1[0][0] * m.m1[2][0] + c.m1[1][0] * m.m1[2][1] + c.m1[2][0] * m.m1[2][2] + c.m1[3][0] * m.m1[2][3];
	this->m1[2][1] = c.m1[0][1] * m.m1[2][0] + c.m1[1][1] * m.m1[2][1] + c.m1[2][1] * m.m1[2][2] + c.m1[3][1] * m.m1[2][3];
	this->m1[2][2] = c.m1[0][2] * m.m1[2][0] + c.m1[1][2] * m.m1[2][1] + c.m1[2][2] * m.m1[2][2] + c.m1[3][2] * m.m1[2][3];
	this->m1[2][3] = c.m1[0][3] * m.m1[2][0] + c.m1[1][3] * m.m1[2][1] + c.m1[2][3] * m.m1[2][2] + c.m1[3][3] * m.m1[2][3];
  
	this->m1[3][0] = c.m1[0][0] * m.m1[3][0] + c.m1[1][0] * m.m1[3][1] + c.m1[2][0] * m.m1[3][2] + c.m1[3][0] * m.m1[3][3];
	this->m1[3][1] = c.m1[0][1] * m.m1[3][0] + c.m1[1][1] * m.m1[3][1] + c.m1[2][1] * m.m1[3][2] + c.m1[3][1] * m.m1[3][3];
	this->m1[3][2] = c.m1[0][2] * m.m1[3][0] + c.m1[1][2] * m.m1[3][1] + c.m1[2][2] * m.m1[3][2] + c.m1[3][2] * m.m1[3][3];
	this->m1[3][3] = c.m1[0][3] * m.m1[3][0] + c.m1[1][3] * m.m1[3][1] + c.m1[2][3] * m.m1[3][2] + c.m1[3][3] * m.m1[3][3];                                                                  

	return *this;
}

mat4& mat4::operator *= ( const float d )
{
	for(int a = 0 ; a < 16; a++)
		this->m[a] *= d;
	return *this;
}

mat4& mat4::operator /= ( const float d )      
{
	for(int a = 0 ; a < 16; a++)
		this->m[a] /= d;
	return *this;
}	

////////////////////////////////////////////////////////////////
//															   /		
// Implementaciones de operadores friends para la clase mat4   /	
//															   /	
////////////////////////////////////////////////////////////////

//////////////////////
// Operador unario  //
//////////////////////
mat4 operator - (const mat4& a)
{
	mat4 c;
	
	for (int i = 0; i < 16; i++)
		c.m[i] = -a.m[i];

	return c;
}

/////////////////////////
// Operadores binarios //
/////////////////////////
mat4 operator * (const mat4& a, const mat4& b)
{
	mat4 c(a);

	c *= b;

	return c;
}

mat4 operator * (const mat4& a, const float d)
{
	//multiplicacion miembro a miembro
	mat4 c;
	
	for (int i = 0; i < 16; i++)
		c.m[i] = a.m[i]*d;

	return c;
}


mat4 operator * (const float d, const mat4& a)
{
	//multiplicacion miembro a miembro 
	mat4 c;
	
	for (int i = 0; i < 16; i++)
		c.m[i] = a.m[i]*d;

	return c;
}

mat4 operator / (const mat4& a, const float d)
{
	//es la division miembro a miembro
	mat4 b;
	float u = 1.0f / d;

	for (int i = 0; i < 16; i++)
		b.m[i] = a.m[i]*u;

	return b;
}

mat4 operator + (const mat4& a, const mat4& b)
{
	//es la suma miembro a miembro
	mat4 c;
	
	for (int i = 0; i < 16; i++)
		c.m[i] = a.m[i]+b.m[i];

	return c;
}

mat4 operator - (const mat4& a, const mat4& b) 
{
	//es la resta miembro a miembro
	mat4 c;
	
	for (int i = 0; i < 16; i++)
		c.m[i] = a.m[i]-b.m[i];

	return c;
}

///////////////////////////////
// Operadores de comparación //
///////////////////////////////
int operator == (const mat4& a, const mat4& b)
{
 return (
    (b.m[0] == a.m[0]) &&
    (b.m[1] == a.m[1]) &&
    (b.m[2] == a.m[2]) &&
    (b.m[3] == a.m[3]) &&
    
    (b.m[4] == a.m[4]) &&
    (b.m[5] == a.m[5]) &&
    (b.m[6] == a.m[6]) &&
    (b.m[7] == a.m[7]) &&
    
    (b.m[8] == a.m[8]) &&
    (b.m[9] == a.m[9]) &&
    (b.m[10] == a.m[10]) &&
    (b.m[11] == a.m[11]) &&
    
    (b.m[12] == a.m[12]) &&
    (b.m[13] == a.m[13]) &&
    (b.m[14] == a.m[14]) &&
    (b.m[15] == a.m[15]));
}

int operator != (const mat4& a, const mat4& b)
{
	return !(a==b);
}

//////////////////////////
//	Funciones			//
//////////////////////////
void mat4::Transpose()
{
	mat4 a(*this);

	/*---------------------------------------------------
	La transpuesta es la que cambia filas por columnas
	o sea m[i][j] = m[j][i] (la diagonal no cambia)
	para una matriz de rotación u ortogonal, la trans-
	puesta es la inversa.
	----------------------------------------------------*/

	m1[0][1] = a.m1[1][0];
	m1[0][2] = a.m1[2][0];
	m1[0][3] = a.m1[3][0];
	m1[1][2] = a.m1[2][1];
	m1[1][3] = a.m1[3][1];
	m1[2][3] = a.m1[3][2];

	m1[1][0] = a.m1[0][1];
	m1[2][0] = a.m1[0][2];
	m1[3][0] = a.m1[0][3];
	m1[2][1] = a.m1[1][2];
	m1[3][1] = a.m1[1][3];
	m1[3][2] = a.m1[2][3];
}

/*-------------------------------------------------------------
Saca la inversa de la matriz. NO ES LA TRANSPUESTA.
siendo A' la inversa de A => A*A' = A'*A = I (mat identidad).
La transformación de la inversa es exactamente contraria a 
la de la matriz. Si tenemos en cuenta que al multiplicar ma-
trices se acumulan sus efectos es fácil ver porque la matriz 
por la inversa da la identidad (transformo y destransformo lo 
que da la identidad que no hace nada).
En este caso se supone una parte rotacional ortogonal, por lo 
que la parte de rotación solo la trasponemos y además que el
último vector columna es (0,0,0,1).
--------------------------------------------------------------*/
void mat4::Invert()
{
	mat4 b = *this;

	b.m1[0][0] = m1[0][0];
  	b.m1[0][1] = m1[1][0];
	b.m1[0][2] = m1[2][0];
	b.m1[1][0] = m1[0][1];
	b.m1[1][1] = m1[1][1];
	b.m1[1][2] = m1[2][1];
	b.m1[2][0] = m1[0][2];
	b.m1[2][1] = m1[1][2];
	b.m1[2][2] = m1[2][2];
  
	/* el nuevo vector desplazamiento es:  d' = -(R^-1) * d */
	b.m1[3][0] = -( m1[3][0] * b.m1[0][0] + m1[3][1] * b.m1[1][0] + m1[3][2] * b.m1[2][0] );
	b.m1[3][1] = -( m1[3][0] * b.m1[0][1] + m1[3][1] * b.m1[1][1] + m1[3][2] * b.m1[2][1] );
	b.m1[3][2] = -( m1[3][0] * b.m1[0][2] + m1[3][1] * b.m1[1][2] + m1[3][2] * b.m1[2][2] );
  
	/* el resto queda igual */
	b.m1[0][3] = b.m1[1][3] = b.m1[2][3] = 0.0f; 
	b.m1[3][3] = 1.0f;

	*this = b;
}

void mat4::MakeGL()
{
	mat4 a(*this);

	//transpone la parte de rotación para hacerla compatible
	//con OpenGL que acomoda los vectores por columna.
	//Si tenemos la matriz en formato matemático (por filas)
	//es necesario antes de usarla para un glMultMatrix().

	m1[0][1] = a.m1[1][0];
	m1[0][2] = a.m1[2][0];
	m1[1][2] = a.m1[2][1];

	m1[1][0] = a.m1[0][1];
	m1[2][0] = a.m1[0][2];
	m1[2][1] = a.m1[1][2];

}

//setea la matriz a una identidad
void mat4::SetId(void)
{
	m[0]  = 1;	m[1]  = 0;	m[2]  = 0;	m[3]  = 0;
	m[4]  = 0;	m[5]  = 1;	m[6]  = 0;	m[7]  = 0;
	m[8]  = 0;	m[9]  = 0;	m[10] = 1;	m[11] = 0;
	m[12] = 0;	m[13] = 0;	m[14] = 0;	m[15] = 1;
}

//crea la matriz de rotación a partir de un quat
void mat4::FromQuat(const quat &q)
{
	float wx, wy, wz, xx, yy, yz, xy, xz, zz, x2, y2, z2;

	// calculo coeficientes
	x2 = q.x + q.x; y2 = q.y + q.y; 
	z2 = q.z + q.z;
	xx = q.x * x2;   xy = q.x * y2;   xz = q.x * z2;
	yy = q.y * y2;   yz = q.y * z2;   zz = q.z * z2;
	wx = q.w * x2;   wy = q.w * y2;   wz = q.w * z2;

	m[0] = 1.0f - (yy + zz); 	m[1] = xy - wz;
	m[2] = xz + wy;			    m[3] = 0.0f;
 
	m[4] = xy + wz;				m[5] = 1.0f - (xx + zz);
	m[6] = yz - wx;				m[7] = 0.0f;

	m[8] = xz - wy;				m[9] = yz + wx;
	m[10] = 1.0f - (xx + yy);	m[11] = 0.0f;

	m[12] = 0.0f;				m[13] = 0.0f;
	m[14] = 0.0f;				m[15] = 1.0f;

}


//Calcula la matriz de rotación a partir de una base (gralmente ortonormal)
void mat4::FromAllVectors(const vect &vpn, const vect &vup, const vect &vr)
{
	m1[0][0] = vr[0];	//->vr = View Rigth 
	m1[1][0] = vr[1];	//(1,0,0) para la identidad
	m1[2][0] = vr[2];
	m1[3][0] = 0;

	m1[0][1] = vup[0];	//->vpn = View Up 
	m1[1][1] = vup[1];	//(0,1,0) para la identidad
	m1[2][1] = vup[2];
	m1[3][1] = 0;

	m1[0][2] = vpn[0];	//->vpn = View Normal 
	m1[1][2] = vpn[1];	//(0,0,1) para la identidad
	m1[2][2] = vpn[2];
	m1[3][2] = 0;

	m1[0][3] = 0;
	m1[1][3] = 0;
	m1[2][3] = 0;
	m1[3][3] = 1;
}

//Setea una traslación dada
void mat4::SetTranslation(const vect &pos)
{
	m1[3][0] = pos[0];
	m1[3][1] = pos[1];
	m1[3][2] = pos[2];
}

//devuelve la parte trasladora
vect mat4::GetTranslation(void)
{
	return vect(m1[3][0], m1[3][1], m1[3][2]);
}


//Suma una traslación dada
void mat4::AddTranslation(const vect &pos)
{
	m1[3][0] += pos[0];
	m1[3][1] += pos[1];
	m1[3][2] += pos[2];
}

//devuelve el vector de la base
vect mat4::GetViewUp(void)
{
	return vect(m1[0][1], m1[1][1], m1[2][1]);
}

vect mat4::GetViewRight(void)
{
	return vect(m1[0][0], m1[1][0], m1[2][0]);
}

vect mat4::GetViewNormal(void)
{
	return vect(m1[0][2], m1[1][2], m1[2][2]);
}

//setea el vector de la base
void mat4::SetViewUp(const vect &vup)
{
	m1[0][1] = vup.x;
	m1[1][1] = vup.y;
	m1[2][1] = vup.z;
}

void mat4::SetViewRight(const vect &vr)
{
	m1[0][0] = vr.x;
	m1[1][0] = vr.y;
	m1[2][0] = vr.z;
}

void mat4::SetViewNormal(const vect &vpn)
{
	m1[0][2] = vpn.x;
	m1[1][2] = vpn.y;
	m1[2][2] = vpn.z;
}	



////////////////////////////////////////////////////////////////
//															   /		
// Implementación de funciones matemáticas auxiliares          /	
//															   /	
////////////////////////////////////////////////////////////////

/*****************************************************************
* Function name	: DegToRad  				     				 *
* Description	: Conversión Grados a Radianes en sus dos sabores*
* Return type	: void										     *	
* Argument      : float										     *
*****************************************************************/  

float DegToRad(const float deg)
{
	return deg*PI/180;
}

void DegToRad(float *deg)
{
	*deg = (*deg)*PI/180;
}

/*****************************************************************
* Function name	: RadToDeg	 				     				 *
* Description	: Conversión Radianes a Grados en sus dos sabores*
* Return type	: float										     *	
* Argument      : float										     *
*****************************************************************/  

float RadToDeg(const float rad)
{
	return rad*180/PI;
}

void RadToDeg(float *rad)
{
	*rad = (*rad)*180/PI;
}

/****************************************************************
* Function name	: ClampD-R	   				     				*
* Description	: Keeps angles between 0 and 360/ 0/2PI		    *
* Return type	: void										    *	
* Argument      : float											*
****************************************************************/
void ClampD (float x)
{
	while (x > 360.0f)
		x -= 360.0f;
	while (x < 0.0f)
		x += 360.0f;
}

void ClampR (float x)
{
	while (x > PI2)
		x -= PI2;
	while (x < 0.0f)
		x += PI2;
}

/*****************************************************************
* Function name	: Sgn	 				     				     *
* Description	: Devuelve el signo de un float mediante acceso  *
*				  binario (mas rápido que comparar con 0.		 *	
* Return type	: int (-1 para negativo, 1 para positivo o cero) *	
* Argument      : float										     *
*****************************************************************/  
int Sgn(float num)
{
	//acceso al float como DWORD (32 bits idem al float)
	unsigned long fl = (*(unsigned long *)&(num));
	fl = ((fl &= 0x80000000) >> 31 ) & 0x1;

	if (fl)
		return -1;
	else
		return 1;
}

/****************************************************************
* Function name	: VectDotProd  				     				*
* Description	: Producto escalar de dos vectores.			    *
*				  El orden de los vectores no afecta el			* 
*				  resultado (conmutativo)						*	
* Return type	: float										    *	
* Argument      : vect, vect								    *
****************************************************************/  
	
float VectDotProd(const vect& vect1, const vect& vect2)
{
	return (vect1.x * vect2.x + 
			vect1.y * vect2.y +
			vect1.z * vect2.z);
}

/*****************************************************************
* Function name	: VectXProd  				     			     *
* Description	: Producto vectorial de dos vectores.		     *
*				  El orden de los vectores AFECTA el resultado.  *
*				  El resultado da un vector perpendicular al	 *
*				  plano formado por los dos vectores argumento.	 *
*				  De acuerdo al orden de los vectores pasados	 *
*				  el resultado apunta para un lado o para el otro*
*				  (o sea el orden afecta el sentido del vector   * 
*				  resultado pero no su direccion ni módulo       *
* Return type	: vect										     *	
* Argument      : vect, vect								     *
*****************************************************************/  

vect VectXProd(const vect& vect1, const vect& vect2)
{
	vect result (  vect1.y * vect2.z - vect2.y * vect1.z,
				   vect2.x * vect1.z - vect1.x * vect2.z,
				   vect1.x * vect2.y - vect2.x * vect1.y	);
	return result;

//para entender la direccion del vector resultado se usa la regla
//de la mano derecha. Pone tu indice apuntando al eje x positivo,
//tu largo al z positivo y tu pulgar al y positivo.
//Ahora si multiplicas X=(1,0,0) por Y=(0,1,0) el resultado es
//Z=(0,0,1), perpendicular a X e Y y direccion eje Z positivo.
//Pero si multiplicas Y*X (invertis los argumentos) el resultado
//da Z = (0,0,-1), con sentido contrario al anterior pero misma 
//direccion (eje Z) y módulo.
//Para saber el sentido que va a tener el resultado, con tu mano 
//derecha acomodada como te dije al ppio. pone el indice apuntando 
//al primer vector, el largo al segundo y el pulgar te va a marcar el 
//sentido del vector resultado.
}

/****************************************************************
* Function name	: Mod		  				     				*
* Description	: Módulo de un vector o magnitud.				*
*				  Es el "largo" del vector y se saca por		*
*				  Pitágoras										* 
* Return type	: float										    *	
* Argument      : vect										    *
****************************************************************/

float Mod(const vect& vect1)
{
	return sqrt(vect1.x * vect1.x +
		        vect1.y * vect1.y +
				vect1.z * vect1.z);
}

/****************************************************************
* Function name	: ModSquared  				     				*
* Description	: Módulo de un vector al cuadrado.				*
*				  Sirve para varias funciones					* 
* Return type	: float										    *	
* Argument      : vect										    *
****************************************************************/
float ModSquared(const vect& vect1)
{
	return vect1.x * vect1.x +
		   vect1.y * vect1.y +
		   vect1.z * vect1.z;
}

/****************************************************************
* Function name	: Normalize		  				     			*
* Description	: Normaliza un vector (hace su módulo = 1)	    *
* Return type	: void										    *	
* Argument      : vect*										    *
****************************************************************/

void Normalize(vect *vect1)
{
	float module = Mod(*vect1);
	if(module > EPS)	{
		vect1->x /= module;
		vect1->y /= module;
		vect1->z /= module;
	}
}

/****************************************************************
* Function name	: Normalize		  				     			*
* Description	: Overload de Normalize.						*
*				  Devuelve un vector normalizado sin modificar  * 
*				  argumento.									*
* Return type	: vect.										    *	
* Argument      : vect.										    *
****************************************************************/

vect Normalize(const vect& vect1)
{	
	vect result;
	float module = Mod(vect1);
	if (module > EPS)	{
		result.x = vect1.x/module;
		result.y = vect1.y/module;
		result.z = vect1.z/module;
	}
	else	{
		result.x = result.y = result.z = EPS;
	}

	return result;
}

/****************************************************************
* Function name	: OrigToRect	 				     			*
* Description	: Distancia del origen a una recta.				*
*				  Le pasas dos puntos que definan (pertenezcan) *
*				  a la recta, devuelve la distancia minima ( a	*	
*				  traves de la normal)							*			
* Return type	: float										    *	
* Argument      : vect,vect									    *
****************************************************************/

float OrigToRect(const vect& point1, const vect& point2)
{
	
	//me aseguro que los argumentos no sean iguales
	//(definen infinitas rectas) para no provocar una
	//división por cero.
	if (point1 == point2)
		return 0;
		
	//Saco el vector directriz de la recta
	vect v = point2 - point1;		
	//Saco el módulo cuadrado del directriz
	float modv2 = v.x * v.x + v.y * v.y + v.z * v.z;
	//Saco el prod escalar entre point1 y v
	float p1dotv = VectDotProd(point1,v);
	//Aprovecho la misma variable 
	p1dotv/=modv2;
	//Saco el normal a v, con extremo en la recta point1-point2
	vect n ( -p1dotv * v.x + point1.x,
			 -p1dotv * v.y + point1.y,
			 -p1dotv * v.z + point1.z );

	//Devuelvo el módulo de ese vector
	return Mod(n);
}

/****************************************************************
* Function name	: OrigToRect2	 				     			*
* Description	: Distancia del origen a una recta.				*
*				  Idem OrigToRect pero esta ves la recta se 	*			
*				  define por un vector direccion y un punto		*
*				  perteneciente.								*
* Return type	: float										    *	
* Argument      : vect,vect									    *
****************************************************************/

float OrigToRect2(const vect& dir, const vect& point)
{
	//No hice un overload de la anterior porque tiene
	//los mismos argumentos y return pero los argumentos
	//tienen significados diferentes.

	//Me aseguro que dir no sea el vector nulo para evitar
	//división por cero
	if (dir.x < EPS &&
		dir.y < EPS &&
		dir.z < EPS)
		return 0;

	//Saco el módulo cuadrado del directriz
	float modv2 = dir.x * dir.x + dir.y * dir.y
				+ dir.z * dir.z;
	//Saco el prod escalar entre point y dir
	float p1dotv = VectDotProd(point,dir);
	//Aprovecho la misma variable 
	p1dotv/=modv2;
	//Saco el normal a v, con extremo en la recta point1-point2
	vect n ( -p1dotv * dir.x + point.x,
		     -p1dotv * dir.y + point.y,
		     -p1dotv * dir.z + point.z );

	//Devuelvo el módulo de ese vector
	//Se puede aprovechar para devolver de paso n
	//que es la direccion
	return Mod(n);
}

/******************************************************************
* Function name	: PointToRect	 				     			  *
* Description	: Distancia de un punto a una recta.			  * 
*				  Le pasas el punto y dos puntos mas que definan  *
*				  (pertenezcan) a la recta, devuelve la distancia * 
*				  entre el punto y la recta a traves de la normal *			
*				  O sea la distacia mínima entre ambos.           * 
* Return type	: float										      *	
* Argument      : vect,vect,vect							      *
******************************************************************/

float PointToRect (const vect& point, const vect& rect1, const vect& rect2)
{
	//me aseguro que los puntos que definen la recta 
	//no sea iguales (definan infinitas rectas)
	if (rect1 == rect2)
		return 0;
		
	//Saco el vector directriz de la recta
	//que seria rect2 - rect1
	//Saco la dirección del punto al primer punto que
	//define la recta que sería point - rect1
	//la distancia es el módulo del prod vectorial
	//sobre el módulo de la directriz

	return Mod(VectXProd(rect2 - rect1, point - rect1))/Mod(rect2 - rect1);

}
	
/******************************************************************
* Function name	: OrigToPlane	 				     			  *
* Description	: Distancia del origen a un plano.				  *		
*				  El plano se define por 3 puntos (los argumentos)*
*				  que pertenezcan al mismo						  * 
* Return type	: float										      *	
* Argument      : vect,vect,vect							      *
******************************************************************/	

float OrigToPlane(const vect& point1, const vect& point2, const vect& point3)
{
	//saco dos vectores pertenecientes al plano
	//y la normal al plano haciendo el prod.
	//vectorial de ellos
	vect Norm = VectXProd(point1 - point2, point3 - point2);

	//Haciendo el prod. escalar entre un punto de los tres
	//(cualquiera) y la normal obtengo la proyeccion de ese 
	//vector sobre la normal, multip. por el módulo de la normal.
	float distance = VectDotProd(point1, Norm)/Mod(Norm);

	//la distancia puede dar + o - de acuerdo a la 
	//orientación de la normal respecto del plano
	if (distance < 0)
		distance = -distance;
	
	return distance;
}

/******************************************************************
* Function name	: PointToPlane	 				     			  *
* Description	: Distancia de un punto a un plano.				  *		
*				  El plano se define por 3 puntos (planeX) que	  *
*                 pertenezcan al mismo.							  *
*                 la distancia es a travez de la nornmal al plano * 
*				  o sea la minima distancia.				      *
* Return type	: float										      *	
* Argument      : vect,vect,vect							      *
******************************************************************/	

float PointToPlane(const vect& point, const vect& plane1, const vect& plane2, const vect& plane3)
{
	
	//saco dos vectores pertenecientes al plano y con
	//ellos la normal al plano (haciendo el prod. vectorial)
	vect Norm = VectXProd(plane1 - plane2, plane3 - plane2);

	Normalize(&Norm);

	//Haciendo el prod. escalar entre un punto y la normal
	//obtengo la proyeccion del punto sobre la normal.
	float proj = VectDotProd(point, Norm);

	//Hago prod escalar de la normal por un punto del plano
	//para obtener la dist del origen al plano
	float distplan = VectDotProd(plane2, Norm);

	//la resta de la distancia al plano menos la proyeccion
	//del punto sobre la normal es la distancia buscada
	float distance = distplan - proj;

	//la distancia puede dar + o - de acuerdo a la 
	//orientación de la normal respecto del plano
	if (distance < 0)
		distance = -distance;
	
	return distance;
}

/******************************************************************
* Function name	: Distance		 				     			  *
* Description	: Distancia de un punto a otro.					  *		
* Return type	: float										      *	
* Argument      : vect,vect.								      *
******************************************************************/	

float Distance (const vect& point1, const vect& point2)
{
	return Mod(point2-point1);
}

/******************************************************************
* Function name	: GetAngle		 				     			  *
* Description	: dados 3 puntos devuelve el ángulo formado		  *
*				  por ellos con vértice en point2.				  *
*				  Devuelve en radianes, para converitr a grados   *
*				  usar RadToDeg() con el resultado.               *	
* Return type	: float										      *	
* Argument      : vect,vect,vect								  *
******************************************************************/	

float GetAngle (const vect& point1, const vect& point2, const vect& point3) 
{
	
	//Saco los vectores que van de 2->1 y de 2->3
	vect v21 = Normalize(point1 - point2);
	vect v23 = Normalize(point3 - point2);
	
	//Hallo el prod. escalar de ambos.
	//El prod escalar es también:
	//norma v21 * norma v23 * cos (angulo buscado)
	//como norma de v21 = norma v23 = 1 (porque los normalicé)
	//el resultado es directamente el coseno del ángulo.
	float cos = VectDotProd(v21, v23);

	//Ahora tengo que hacer el coseno inverso (arco coseno)
	float angle = acos(cos);

	return angle;
}

/******************************************************************
* Function name	: GetAngle		 				     			  *
* Description	: dados 2 vectores devuelve el ángulo formado	  *
*				  por ellos.									  *
*				  Devuelve en radianes, para grados usar RadToDeg *	
* Return type	: float										      *	
* Argument      : vect,vect.								      *
******************************************************************/	

float GetAngle (const vect& vect1, const vect& vect2)
{
	//identica a la anterior, pero esta vez ya 
	//tengo los vectores
	float cos = VectDotProd(Normalize(vect1), Normalize(vect2));

	float angle = acos(cos);
		
	return angle;
}

/******************************************************************
* Function name	: MatfromQuat		 				     		  *
* Description	: dados un quaternion de rotación unitario com-   *
*				  pleta la matriz de rotación 4x4 equivalente.    *   
*				  pasar un pointer a m[0][0] si es m[4][4] o      *
*				  m[0] si es m[16]								  *
* Return type	: void										      *	
* Argument      : quat,float*.								      *
******************************************************************/	

void  MatrixfromQuat   (const quat& q, float* m)
{
	float wx, wy, wz, xx, yy, yz, xy, xz, zz, x2, y2, z2;

	// calculo coeficientes
	x2 = q.x + q.x; y2 = q.y + q.y; 
	z2 = q.z + q.z;
	xx = q.x * x2;   xy = q.x * y2;   xz = q.x * z2;
	yy = q.y * y2;   yz = q.y * z2;   zz = q.z * z2;
	wx = q.w * x2;   wy = q.w * y2;   wz = q.w * z2;

	*(m) = 1.0 - (yy + zz); 	*(m+1) = xy - wz;
	*(m+2) = xz + wy;			*(m+3) = 0.0;
 
	*(m+4) = xy + wz;			*(m+5) = 1.0 - (xx + zz);
	*(m+6) = yz - wx;			*(m+7) = 0.0;

	*(m+8) = xz - wy;			*(m+9) = yz + wx;
	*(m+10) = 1.0 - (xx + yy);	*(m+11) = 0.0;

	*(m+12) = 0;				*(m+13) = 0;
	*(m+14) = 0;				*(m+15) = 1;

	//Es asi por decreto =)
}

/******************************************************************
* Function name	: QuatfromMat		 				     		  *
* Description	: dada una matriz de rotación de 4x4 completa el  *
*				  quaternion equivalente.						  *   
*				  La inversa de la anterior.					  *
* Return type	: void										      *	
* Argument      : float* , quat.								      *
******************************************************************/	

void  QuatfromMatrix   (const float *m, quat& q)
{

	float qs2, qx2, qy2, qz2;  // squared magniudes of quaternion components
	float tmp;
	int n;

	// primero calculamos los valores al cuadrado del quat.
	//por lo menos uno va a ser > 0 porque el quat es de módulo 1.
	
	qs2 = 0.25f * (*(m+0) + *(m+5) + *(m+10) + 1);
	qx2 = qs2 - 0.5f * (*(m+5) + *(m+10));
	qy2 = qs2 - 0.5f * (*(m+10) + *(m+0));
	qz2 = qs2 - 0.5f * (*(m+0) + *(m+5));

  
	// encontramos el componente de magnitud máxima
	n = (qs2 > qx2 ) ?
		((qs2 > qy2) ? ((qs2 > qz2) ? 0 : 3) : ((qy2 > qz2) ? 2 : 3)) :
		((qx2 > qy2) ? ((qx2 > qz2) ? 1 : 3) : ((qy2 > qz2) ? 2 : 3));

	// calculamos los valores del quat
	switch(n) 
	{
	case 0:
		q.w = sqrt(qs2);
		tmp = 0.25f / q.w;
		q.x = (*(m+9) - *(m+6)) * tmp;
		q.y = (*(m+2) - *(m+8)) * tmp;
		q.z = (*(m+4) - *(m+1)) * tmp;
		break;
	case 1:
		q.x = sqrt(qx2);
		tmp = 0.25f / q.x;
		q.w = (*(m+9) - *(m+6)) * tmp;
		q.y = (*(m+1) + *(m+4)) * tmp;
		q.z = (*(m+2) + *(m+8)) * tmp;
		break;
	case 2:
		q.y = sqrt(qy2);
		tmp = 0.25f / q.y;
		q.w = (*(m+2) - *(m+8)) * tmp;
		q.z = (*(m+6) + *(m+9)) * tmp;
		q.x = (*(m+4) + *(m+1)) * tmp;
		break;
	case 3:
		q.z = sqrt(qz2);
		tmp = 0.25f / q.z;
		q.w = (*(m+4) - *(m+1)) * tmp;
		q.x = (*(m+8) + *(m+2)) * tmp;
		q.y = (*(m+9) + *(m+6)) * tmp;
		break;
	}
	
	// forzamos escalar positivo.
	if (q.w < 0.0f) 
	{
		q.w = -q.w;
		q.x = -q.x;
		q.y = -q.y;
		q.z = -q.z;
	}

	//otro decretazo... es asi.
}

/******************************************************************
* Function name	: QuatfromEuler		 				     		  *
* Description	: dados ángulos de Euler completa el quaternion   *
*				  equivalente. El orden de operación es:          *
*						p' = roll( pitch( yaw(p) ) )			  *   
*																  *
*				  Yaw:	 Respecto del Z							  *
*				  Roll:	 Respecto del X							  *
*				  Pitch: Respecto del Y 						  *
*																  *
* Return type	: void										      *	
* Argument      : float, float, float, quat&.					  *
******************************************************************/
void QuatfromEuler( const float yaw, const float pitch, const float roll, quat& q)	
{
	float cosYaw, sinYaw, cosPitch, sinPitch, cosRoll, sinRoll;
	float half_roll, half_pitch, half_yaw;

	half_yaw = yaw / 2.0;
	half_pitch = pitch / 2.0;
	half_roll = roll / 2.0;

	cosYaw = cos(half_yaw);
	sinYaw = sin(half_yaw);

	cosPitch = cos(half_pitch);
	sinPitch = sin(half_pitch);

	cosRoll = cos(half_roll);
	sinRoll = sin(half_roll);

	q.x = sinRoll * cosPitch * cosYaw - cosRoll * sinPitch * sinYaw;
	q.y = cosRoll * sinPitch * cosYaw + sinRoll * cosPitch * sinYaw;
	q.z = cosRoll * cosPitch * sinYaw - sinRoll * sinPitch * cosYaw;

	q.w = cosRoll * cosPitch * cosYaw + sinRoll * sinPitch * sinYaw;
}

/*****************************************************************
* Function name	: QuatSlerp			 				     		  *
* Description	: Spherical Linear intERPolation entre dos quat.  *
*				  Un promedio ponderado de dos rotaciones.        *   
* Return type	: void										      *	
* Argument      : &quat,&quat,float, &quat.				          *
******************************************************************/	

void QuatSlerp(const quat &from, const quat &to, float t, quat &res)
{
	
	//minlerp define la barrera para interpolar esférico/lineal
	//subir el valor para obtener mayor velocidad/menor calidad
	const float	minlerp = 0.02f;
	quat           to1;
	float        omega, cosom, sinom, scale0, scale1;

	// calc cosine
	cosom = from.x * to.x + 
			from.y * to.y + 
			from.z * to.z +
			from.w * to.w;

	// adjust signs (if necessary)
	if ( cosom <0.0 ) { 
			cosom = -cosom; 
			to1.x = - to.x;
			to1.y = - to.y;
			to1.z = - to.z;
			to1.w = - to.w;
	} else  {
			to1.x = to.x;
			to1.y = to.y;
			to1.z = to.z;
			to1.w = to.w;
	}

	// calculate coefficients
	if ( (1.0 - cosom) > minlerp ) {
          // interpolación esférica (SLERP)
          omega = (float)acos(cosom);
          sinom = (float)sin(omega);
          scale0 = (float)sin((1.0 - t) * omega) / sinom;
          scale1 = (float)sin(t * omega) / sinom;

	} else {        
		// "from" y "to" quaternions están muy cerca 
		//  uso interpolación lineal (LERP)
        scale0 = 1.0f - t;
        scale1 = t;
	}
	// calculate final values
	res.x = scale0 * from.x + scale1 * to1.x;
	res.y = scale0 * from.y + scale1 * to1.y;
	res.z = scale0 * from.z + scale1 * to1.z;
	res.w = scale0 * from.w + scale1 * to1.w;
}
#endif
