/******************************Module*Header*******************************\
*
*                           *******************
*                           * D3D SAMPLE CODE *
*                           *******************
* Module Name: d3dmath.h
*
*  Content:    Header for Direct3D Math definitions
*
* Copyright (C) 1996-1998 Microsoft Corporation.  All Rights Reserved.
\**************************************************************************/

#ifndef __D3DMATH__
#define __D3DMATH__

// Arrays used to convert floating point values to the
// hw fixed point format
extern double dTexValtoInt[];
extern double dTexValtoIntPerspective[];

// Fast positive float to int
static float _myfltemp;
static double _mydtemp;

#define TWOPOW32 ((double)(65536.0 * 65536.0))

#define TWOPOW(N) (((N) < 32) ? ((double)(1UL << (N))) : \
                        ((double)(1UL << (N - 32)) * TWOPOW32))
static const double _myfl0const = TWOPOW(52) + TWOPOW(51);
static const double _myfl7const = TWOPOW(45) + TWOPOW(44);
static const double _myfl8const = TWOPOW(44) + TWOPOW(43);
static const double _myfl12const = TWOPOW(40) + TWOPOW(39);
static const double _myfl15const = TWOPOW(37) + TWOPOW(36);
static const double _myfl16const = TWOPOW(36) + TWOPOW(35);
static const double _myfl18const = TWOPOW(34) + TWOPOW(33);
static const double _myfl19const = TWOPOW(33) + TWOPOW(32);
static const double _myfl20const = TWOPOW(32) + TWOPOW(31);
static const double _myfl24const = TWOPOW(28) + TWOPOW(27);
static const double _myfl27const = TWOPOW(25) + TWOPOW(24);
static const double _myfl28const = TWOPOW(24) + TWOPOW(23);
static const double _myfl31const = TWOPOW(21) + TWOPOW(20);
static const float _myflconst = (float)TWOPOW(23);

#ifdef WINNT

#if defined(_X86_)

_inline int _stdcall MYFLINT( float f )
{
    _asm {
            fld     [f]
            fistp   [f]
    }
    return *(int*)&f;
}

#else

static __inline int MYFLINT( float f )
{
#if 0
    float fTemp;

    fTemp = f + _myflconst;
    return( (int)(*(WORD*)&fTemp) );
#else
    return ((int)(f));
#endif
}
#endif //_X86_

static __inline BYTE MYFLINTUCHAR( double d )
{
    double dTemp;

    dTemp = d + _myfl0const;
    return( (BYTE)(*(BYTE*)&dTemp) );
}

#define GEN_MYFLINT(x,y,z)                                \
    static __inline z MYFLINT##x##( double d )            \
    {                                                     \
        double dTemp;                                     \
                                                          \
        dTemp = d + _myfl##x##const;                      \
        return( (z)(*(y*)&dTemp) );                   \
    }

GEN_MYFLINT(0,int,int)
GEN_MYFLINT(7,int,int)
GEN_MYFLINT(12,int,int)
GEN_MYFLINT(15,int,int)
GEN_MYFLINT(16,int,int)
GEN_MYFLINT(18,int,int)
GEN_MYFLINT(19,int,int)
GEN_MYFLINT(20,int,int)
GEN_MYFLINT(24,int,int)
GEN_MYFLINT(27,int,int)
GEN_MYFLINT(28,int,int)
GEN_MYFLINT(31,int,int)
extern double dTexValtoInt[];

static __inline int FLOAT_TO_TEXPOINT( LPS3_CONTEXT c, double d )
{
    double dTemp;

    dTemp = d + c->dTexValtoInt;
    return( (int)(*(int*)&dTemp) );
}

#else
#define MYFLINT(x) (_myfltemp = (float)(x) + _myflconst, (int)(*(WORD *)&_myfltemp))
#define MYFLINTUCHAR(x) (_mydtemp = (double)(x) + _myfl0const, (BYTE)(*(BYTE *)&_mydtemp))
#define MYFLINT0(x) (_mydtemp = (double)(x) + _myfl0const, (int)(*(int *)&_mydtemp))
#define MYFLINT7(x) (_mydtemp = (double)(x) + _myfl7const, (int)(*(int *)&_mydtemp))
#define MYFLINT12(x) (_mydtemp = (double)(x) + _myfl12const, (int)(*(int *)&_mydtemp))
#define MYFLINT15(x) (_mydtemp = (double)(x) + _myfl15const, (int)(*(int *)&_mydtemp))
#define MYFLINT16(x) (_mydtemp = (double)(x) + _myfl16const, (int)(*(int *)&_mydtemp))
#define MYFLINT18(x) (_mydtemp = (double)(x) + _myfl18const, (int)(*(int *)&_mydtemp))
#define MYFLINT19(x) (_mydtemp = (double)(x) + _myfl19const, (int)(*(int *)&_mydtemp))
#define MYFLINT20(x) (_mydtemp = (double)(x) + _myfl20const, (int)(*(int *)&_mydtemp))
#define MYFLINT24(x) (_mydtemp = (double)(x) + _myfl24const, (int)(*(int *)&_mydtemp))
#define MYFLINT28(x) (_mydtemp = (double)(x) + _myfl28const, (int)(*(int *)&_mydtemp))
#define MYFLINT31(x) (_mydtemp = (double)(x) + _myfl31const, (int)(*(int *)&_mydtemp))

extern double dTexValtoInt[];

#define FLOAT_TO_TEXPOINT( c, x )  (_mydtemp = (double)(x) + c->dTexValtoInt, (int)(*(int *)&_mydtemp))
#define FLOAT_TO_2012DXGX( x )  (_mydtemp = (double)(x) + D3DGLOBALPTR(ctxt)->D3DGlobals.DXGX, (int)(*(int *)&_mydtemp))
#endif

#define FLOAT_TO_2012( x ) MYFLINT12(x)
#define FLOAT_TO_1715( x ) MYFLINT15(x)
#define FLOAT_TO_1616( x ) MYFLINT16(x)
#define FLOAT_TO_1220( x ) MYFLINT20(x)
#define FLOAT_TO_1418( x ) MYFLINT18(x)
#define FLOAT_TO_1319( x ) MYFLINT19(x)
#define FLOAT_TO_824( x ) MYFLINT24(x)
#define FLOAT_TO_427( x ) MYFLINT27(x)
#define FLOAT_TO_428( x ) MYFLINT28(x)
#define FLOAT_TO_87( x ) MYFLINT7(x)
#define COLOR_TO_87( x ) ( (x) << 7 )

_inline float _stdcall DblRound( float f )
{
#if defined(_X86_)
    _asm {
            fld dword ptr    [f]
            frndint
            fstp    [f]
    }
    return *(float*)&f;
#endif
#if defined(_ALPHA_) || (_IA64_)
    int x = (int)(f+0.5f);

    return((float)x);
#endif
}


// FPU Rounding and truncation control
#if defined(_X86_)

#define CHOP_ROUND_ON()                             \
    short OldCW, TruncateCW;                        \
    { _asm fstcw   OldCW };                         \
    TruncateCW = (short)(OldCW | (short)0x0e00);    \
    { _asm fldcw   TruncateCW }

#define CHOP_ROUND_OFF()                            \
    { _asm fldcw OldCW }

#else

#define CHOP_ROUND_ON()
#define CHOP_ROUND_OFF()

#endif // _X86_



// Miscelaneous minimum and maximum functions and definitions
_PRECISION __inline min4(_PRECISION a,_PRECISION b,_PRECISION c,_PRECISION d)
{
    _PRECISION min;

    min = a;
    if (b < min) min = b;
    if (c < min) min = c;
    if (d < min) min = d;
    return min;
}

_PRECISION __inline max4(_PRECISION a,_PRECISION b,_PRECISION c,_PRECISION d)
{
    _PRECISION max;

    max = a;
    if (b > max) max = b;
    if (c > max) max = c;
    if (d > max) max = d;
    return max;
}


_PRECISION __inline min3(_PRECISION a,_PRECISION b,_PRECISION c)
{
    _PRECISION min;

    min = a;
    if (b < min) min = b;
    if (c < min) min = c;
    return min;
}

_PRECISION __inline max3(_PRECISION a,_PRECISION b,_PRECISION c)
{
    _PRECISION max;

    max = a;
    if (b > max) max = b;
    if (c > max) max = c;
    return max;
}

#define max(a,b)    (((a) > (b)) ? (a) : (b))

// Logarithmic functions
unsigned int __inline IntLog2(unsigned int ix)
{
    int i, val = 0;

    for ( i = 0; i < 31; i++ )
    {
        if (  ix & 0x80000000 ) {
            val = 31 - i;
            if ( ix & 0x7FFFFFFF ) {
                val++;
            }
            break;
        }
        ix <<= 1;
    }
    return val;
}

float __inline fltlog2(float x)
{
    float ftemp;
    unsigned int wtemp;
    DWORD *dtemp = (DWORD *)&x;

    if(x>0) {
        wtemp = ((*dtemp >> 23) & 0xff) - 127;
       ftemp = (float)(wtemp) + ((float)((*dtemp>>15) & 0xff))/256.f;
    }
    else
       ftemp = 0.0f;
    return ftemp;
}

#endif // __D3DDRV__


