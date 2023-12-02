//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC

#include "vgui_surfacelib/stbfont.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#include <tier0/dbg.h>
#include <vgui/ISurface.h>
#include <utlbuffer.h>
#include <fontconfig/fontconfig.h>
#include "materialsystem/imaterialsystem.h"

#include "vgui_surfacelib/fontmanager.h"
#include "FontEffects.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

bool CSTBFont::ms_bSetFriendlyNameCacheLessFunc = false;
CUtlRBTree< CSTBFont::font_name_entry > CSTBFont::m_FriendlyNameCache;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSTBFont::CSTBFont() : m_ExtendedABCWidthsCache(256, 0, &ExtendedABCWidthsCacheLessFunc),
m_ExtendedKernedABCWidthsCache( 256, 0, &ExtendedKernedABCWidthsCacheLessFunc )
{
	m_iTall = 0;
	m_iWeight = 0;
	m_iFlags = 0;
	m_iMaxCharWidth = 0;
	m_bAntiAliased = false;
	m_bUnderlined = false;
	m_iBlur = 0;
	m_pGaussianDistribution = NULL;
	m_iScanLines = 0;
	m_bRotary = false;
	m_bAdditive = false;
	if ( !ms_bSetFriendlyNameCacheLessFunc )
	{
		ms_bSetFriendlyNameCacheLessFunc = true;
		SetDefLessFunc( m_FriendlyNameCache );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSTBFont::~CSTBFont()
{
}


//-----------------------------------------------------------------------------
// Purpose: creates the font from windows.  returns false if font does not exist in the OS.
//-----------------------------------------------------------------------------
bool CSTBFont::Create(const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags)
{
	// setup font properties
	m_szName = windowsFontName;
	m_iTall = tall;
	m_iWeight = weight;
	m_iFlags = flags;
	m_bAntiAliased = flags & FONTFLAG_ANTIALIAS;
	m_bUnderlined = flags & FONTFLAG_UNDERLINE;
	m_iDropShadowOffset = (flags & FONTFLAG_DROPSHADOW) ? 1 : 0;
	m_iOutlineSize = (flags & FONTFLAG_OUTLINE) ? 1 : 0;
	m_iBlur = blur;
	m_iScanLines = scanlines;
	m_bRotary = flags & FONTFLAG_ROTARY;
	m_bAdditive = flags & FONTFLAG_ADDITIVE;

	FILE *fd = fopen( "platform/vgui/fonts/tahoma.ttf", "rb" );
	if( !fd )
		return false;

	fseek( fd, 0, SEEK_END );
	size_t len = ftell( fd );
	fseek( fd, 0, SEEK_SET );

	m_pFontData = new unsigned char[len+1];
	size_t red = fread( m_pFontData, 1, len, fd );
	fclose( fd );
	if( red != len )
		return false;

	if( !stbtt_InitFont( &m_fontInfo, m_pFontData, 0 ) )
	{
		m_szName[0] = 0;
		return false;
	}

	// HACKHACK: for some reason size scales between ft2 and stbtt are different
	scale = stbtt_ScaleForPixelHeight(&m_fontInfo, tall + 2);
	int x0, y0, x1, y1;

	stbtt_GetFontVMetrics(&m_fontInfo, &m_iAscent, NULL, NULL );
	m_iAscent *= scale;

	stbtt_GetFontBoundingBox( &m_fontInfo, &x0, &y0, &x1, &y1 );
	m_iHeight = (( y1 - y0 ) * scale); // maybe wrong!
	m_iMaxCharWidth = (( x1 - x0 ) * scale); // maybe wrong!

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: writes the char into the specified 32bpp texture
//-----------------------------------------------------------------------------
void CSTBFont::GetCharRGBA(wchar_t ch, int rgbaWide, int rgbaTall, unsigned char *prgba )
{
	unsigned char *buf;
	int a, b, c;

	GetCharABCWidths( ch, a, b, c );

	int bm_top, bm_left, bm_rows, bm_width;

	buf = stbtt_GetCodepointBitmap( &m_fontInfo, scale, scale, ch, &bm_width, &bm_rows, &bm_left, &bm_top );

	unsigned char *rgba = prgba + ( ( m_iAscent - bm_top ) * rgbaWide * 4 );

	/* now draw to our target surface */
	for ( int y = 0; y < MIN( bm_rows, rgbaTall ); y++ )
	{
		for ( int x = 0; x < bm_width; x++ )
		{
			int rgbaOffset = 4*(x + m_iBlur); // +(rgbaTall-y-1)*rgbaWide*4
			rgba[ rgbaOffset   ] =  255;
			rgba[ rgbaOffset+1 ] =  255;
			rgba[ rgbaOffset+2 ] =  255;
			rgba[ rgbaOffset+3 ] =  buf[ x + y * bm_width ];
		}
		rgba += ( rgbaWide*4 );
	}

	// apply requested effects in specified order
	ApplyDropShadowToTexture( rgbaWide, rgbaTall, prgba, m_iDropShadowOffset );
	ApplyOutlineToTexture( rgbaWide, rgbaTall, prgba, m_iOutlineSize );
	ApplyGaussianBlurToTexture( rgbaWide, rgbaTall, prgba, m_iBlur );
	ApplyScanlineEffectToTexture( rgbaWide, rgbaTall, prgba, m_iScanLines );
	ApplyRotaryEffectToTexture( rgbaWide, rgbaTall, prgba, m_bRotary );
}

void CSTBFont::GetKernedCharWidth( wchar_t ch, wchar_t chBefore, wchar_t chAfter, float &wide, float &abcA, float &abcC )
{
	int a, b, c;
	GetCharABCWidths( ch, a, b, c );
	wide = a+b+c;
	abcA = a;
	abcC = c;
}

//-----------------------------------------------------------------------------
// Purpose: gets the abc widths for a character
//-----------------------------------------------------------------------------
void CSTBFont::GetCharABCWidths(int ch, int &a, int &b, int &c)
{
	Assert(IsValid());

	// look for it in the cache
	abc_cache_t finder = { (wchar_t)ch };

	unsigned short i = m_ExtendedABCWidthsCache.Find(finder);
	if (m_ExtendedABCWidthsCache.IsValidIndex(i))
	{
		a = m_ExtendedABCWidthsCache[i].abc.a;
		b = m_ExtendedABCWidthsCache[i].abc.b;
		c = m_ExtendedABCWidthsCache[i].abc.c;
		return;
	}

	int glyphId = stbtt_FindGlyphIndex( &m_fontInfo, ch );

	int x0, x1;
	int width, horiBearingX, horiAdvance;

	stbtt_GetGlyphBox( &m_fontInfo, glyphId, &x0, NULL, &x1, NULL );
	stbtt_GetCodepointHMetrics( &m_fontInfo, ch, &horiAdvance, &horiBearingX );
	width = x1 - x0;

	a = horiBearingX * scale;

	// HACKHACK: stbtt does not support hinting,
	// so we add 1 pixel margin here and stbtt
	// won't look bad on too small screen resolutions
	b = width * scale + 1;

	c = (horiAdvance - horiBearingX - width) * scale;

	finder.abc.a = a;
	finder.abc.b = b;
	finder.abc.c = c;

	m_ExtendedABCWidthsCache.Insert(finder);
}
							   

//-----------------------------------------------------------------------------
// Purpose: returns true if the font is equivalent to that specified
//-----------------------------------------------------------------------------
bool CSTBFont::IsEqualTo(const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags)
{
	if (!Q_stricmp(windowsFontName, m_szName.String() ) 
		&& m_iTall == tall
		&& m_iWeight == weight
		&& m_iBlur == blur
		&& m_iFlags == flags)
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: returns true only if this font is valid for use
//-----------------------------------------------------------------------------
bool CSTBFont::IsValid()
{
	if ( !m_szName.IsEmpty() )
		return true;

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: returns the height of the font, in pixels
//-----------------------------------------------------------------------------
int CSTBFont::GetHeight()
{
	assert(IsValid());
	return m_iHeight;
}

//-----------------------------------------------------------------------------
// Purpose: returns the ascent of the font, in pixels (ascent=units above the base line)
//-----------------------------------------------------------------------------
int CSTBFont::GetAscent()
{
	assert(IsValid());
	return m_iAscent;
}

//-----------------------------------------------------------------------------
// Purpose: returns the maximum width of a character, in pixels
//-----------------------------------------------------------------------------
int CSTBFont::GetMaxCharWidth()
{
	assert(IsValid());
	return m_iMaxCharWidth;
}

//-----------------------------------------------------------------------------
// Purpose: returns the flags used to make this font, used by the dynamic resizing code
//-----------------------------------------------------------------------------
int CSTBFont::GetFlags()
{
	return m_iFlags;
}

//-----------------------------------------------------------------------------
// Purpose: Comparison function for abc widths storage
//-----------------------------------------------------------------------------
bool CSTBFont::ExtendedABCWidthsCacheLessFunc(const abc_cache_t &lhs, const abc_cache_t &rhs)
{
	return lhs.wch < rhs.wch;
}

//-----------------------------------------------------------------------------
// Purpose: Comparison function for abc widths storage
//-----------------------------------------------------------------------------
bool CSTBFont::ExtendedKernedABCWidthsCacheLessFunc(const kerned_abc_cache_t &lhs, const kerned_abc_cache_t &rhs)
{
	return lhs.wch < rhs.wch || ( lhs.wch == rhs.wch && lhs.wchBefore < rhs.wchBefore ) 
	|| ( lhs.wch == rhs.wch && lhs.wchBefore == rhs.wchBefore && lhs.wchAfter < rhs.wchAfter );
}

void *CSTBFont::SetAsActiveFont( void *cglContext )
{
	Assert( false );
	return NULL;
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Ensure that all of our internal structures are consistent, and
//			account for all memory that we've allocated.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
void CSTBFont::Validate( CValidator &validator, const char *pchName )
{
	validator.Push( "CSTBFont", this, pchName );

	m_ExtendedABCWidthsCache.Validate( validator, "m_ExtendedABCWidthsCache" );
	m_ExtendedKernedABCWidthsCache.Validate( validator, "m_ExtendedKernedABCWidthsCache" );
	validator.ClaimMemory( m_pGaussianDistribution );

	validator.Pop();
}
#endif // DBGFLAG_VALIDATE
