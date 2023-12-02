//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef VGUIFONT_H
#define VGUIFONT_H
#ifdef _WIN32
#pragma once
#endif

// Structure passed to CWin32Font::GetCharsRGBA
struct newChar_t
{
	wchar_t	wch;		// A new character to generate texture data for
	int		fontWide;	// Texel width of the character
	int		fontTall;	// Texel height of the character
	int     offset;		// Offset into the buffer given to GetCharsRGBA
};

#if defined( WIN32 ) 
#include "Win32Font.h"
typedef CWin32Font font_t;
#else
#include "stbfont.h"
typedef CSTBFont font_t;
#endif


#endif //VGUIFONT_H 
