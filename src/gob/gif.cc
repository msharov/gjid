/// This file is part of the fbgl library.
///
/// Copyright (C) 2005 by Mike Sharov <msharov@users.sourceforge.net>
/// This file is free software, distributed under the MIT License.
/// 
/// \file gif.cc
/// \brief Objects for working with the GIF file format.
///

#include "gif.h"
#include "image.h"

namespace gif {

//----------------------------------------------------------------------
// CTable; the LZW string table.
//----------------------------------------------------------------------

const CTable::code_t CTable::c_MaxCodeBits;
const CTable::code_t CTable::c_MaxCodes;
const CTable::code_t CTable::c_Unused;

//----------------------------------------------------------------------

/// Default constructor.
CTable::CTable (void)
: m_Char (),
  m_Prev (),
  m_First (),
  m_nCodes (0),
  m_ResetCode (0),
  m_EndCode (0)
{
}

/// \brief Resets the table to the original code size of \p nRootBits.
///
/// Populates the table with (1 << (nRootBits - 1)) root entries plus
/// the reset and end-of-input codes. All other strings are erased.
/// This table supports codes up to 12 bits long, as in GIF standard.
///
void CTable::Reset (size_t nRootBits)
{
    const int n = 1 << (nRootBits - 1);
    m_CodeBits = nRootBits;
    m_ResetCode = n;
    m_EndCode = m_ResetCode + 1;
    m_nCodes = m_EndCode + 1;
    if (m_nCodes > c_MaxCodes)
	throw runtime_error ("lzw table size is too large");
    fill (m_Char, chr_t(0));
    fill (m_Prev, c_Unused);
    fill (m_First, c_Unused);
    for (code_t i = 0; i < m_nCodes; ++ i)
	m_Char[i] = m_First[i] = i;
}

/// Add a string base+chr to string table
CTable::code_t CTable::AddString (code_t base, chr_t c, bool bWriting)
{
    if (m_nCodes >= c_MaxCodes || base == m_nCodes)
	throw runtime_error ("the compressed data is corrupt");
    m_Char [m_nCodes] = c;
    m_Prev [m_nCodes] = base;
    m_First [m_nCodes] = m_First [base];
    if (++ m_nCodes > BitMask(code_t,m_CodeBits) + bWriting)
	++ m_CodeBits;
    return (m_nCodes - 1);
}

inline bool CTable::CodeIsString (code_t c, const chrciter_t first, chrciter_t last) const
{
    while (!IsUnused(c) && --last >= first && *last == m_Char[c])
	c = BaseOf (c);
    return (IsUnused(c) && last == first);
}

/// Returns the code for the given string [first, last)
CTable::code_t CTable::Find (chrciter_t first, chrciter_t last) const
{
    for (uoff_t i = 0; i < m_nCodes; ++ i)
	if (CodeIsString (i, first, last))
	    return (i);
    return (c_Unused);
}

/// Writes string indexed by \p code into \p os.
void CTable::WriteString (code_t code, ostream& os) const
{
    chrvec_t buf;
    chrvec_t::iterator i = buf.begin();
    while (code < m_nCodes && i != buf.end()) {
	*i++ = m_Char[code];
	code = m_Prev[code];
    }
    const size_t n (distance (buf.begin(), i));
    if (os.remaining() < n)
	throw stream_bounds_exception ("lzw write", "gif", os.pos(), n, os.remaining());
    ostream::iterator io = os.ipos();
    for (uoff_t j = 0; j < n; ++j)
	*io++ = *--i;
    os.skip (n);
}

//----------------------------------------------------------------------
// CDecompressor; the decompression engine.
//----------------------------------------------------------------------

/// Default constructor.
CDecompressor::CDecompressor (void)
: t (),
  m_CurByte (0),
  m_BitsLeft (0),
  m_BlockSize (0)
{
}

/// Decompresses stream \p is into \p os.
void CDecompressor::Run (istream& is, ostream& os)
{
    if (is.remaining() < 2)
	return;
    uint8_t codeBits;
    is >> codeBits;
    t.Reset (++codeBits);

    CTable::code_t oldc = 0, c;
    while ((c = ReadCode (is)) != t.EndCode()) {
	if (c == t.ResetCode())
	    t.Reset (codeBits);
	else {
	    if (oldc != t.ResetCode() && !t.IsUnused(oldc)) {
		CTable::code_t k = t.IsUnused(c) ? oldc : c;
		t.AddString (oldc, t.FirstCharOf(k));
	    }
	    t.WriteString (c, os);
	}
	oldc = c;
    }
    // EOI code should be followed by the usual 0-length terminal block.
    is.skip (min (is.remaining(), m_BlockSize + 1U));
    m_BlockSize = 0;
}

/// Reads t.CodeBits bits from \p is.
CTable::code_t CDecompressor::ReadCode (istream& is)
{
    CTable::code_t c = 0;
    size_t bitsRead = 0;
    while (bitsRead < t.CodeBits()) {
	if (!m_BitsLeft) {
	    if (!m_BlockSize) {
		is >> m_BlockSize;
		if (is.remaining() < size_t(m_BlockSize + !!m_BlockSize))
		    throw stream_bounds_exception ("lzw read", "gif", is.pos(), m_BlockSize + !!m_BlockSize, is.remaining());
	    }
	    if (!m_BlockSize)
		return (t.EndCode());
	    is >> m_CurByte;
	    -- m_BlockSize;
	    m_BitsLeft = 8;
	}
	size_t btr = min (m_BitsLeft, t.CodeBits() - bitsRead);
	c |= ((m_CurByte & BitMask(uint16_t,btr)) << bitsRead);
	m_CurByte >>= btr;
	m_BitsLeft -= btr;
	bitsRead += btr;
    }
    return (c);
}

//----------------------------------------------------------------------
// Compressor.
//----------------------------------------------------------------------

/// Default constructor.
CCompressor::CCompressor (void)
: t (),
  m_BlockSize (0),
  m_CurByte (0),
  m_BitsUsed (0)
{
}

/// Compresses data from \p is into \p os.
void CCompressor::Run (istream& is, ostream& os)
{
    CTable::chrvec_t prefix;
    CTable::chrvec_t::iterator pk = prefix.begin();
    os << uint8_t(m_CodeSize - 1);
    os << (m_BlockSize = 0);
    WriteCode (os, t.ResetCode());
    while (is.remaining()) {
	is >> *pk++;
	if (t.Find (prefix.begin(), pk) != CTable::c_Unused && pk < prefix.end() - 1)
	    continue;
	CTable::code_t base = t.Find (prefix.begin(), --pk);
	WriteCode (os, base);
	CTable::code_t nc = t.AddString (base, *pk, true);
	if (nc == CTable::c_MaxCodes - 1) {
	    WriteCode (os, t.ResetCode());
	    t.Reset (m_CodeSize);
	}
	prefix[0] = *pk;
	pk = prefix.begin() + 1;
    }
    WriteCode (os, t.EndCode());
    FlushCurByte (os);
    StartNextBlock (os);
}

/// Returns the number of output bytes generated by compression of \p is.
size_t CCompressor::EstimateSize (istream& is)
{
    size_t s = 0;
    CTable::chrvec_t prefix;
    CTable::chrvec_t::iterator pk = prefix.begin();
    s += stream_size_of (uint8_t(m_CodeSize - 1));
    s += stream_size_of (m_BlockSize = 0);
    size_t nBits = 0;
    nBits += t.CodeBits();
    while (is.remaining()) {
	is >> *pk++;
	if (t.Find (prefix.begin(), pk) != CTable::c_Unused && pk < prefix.end() - 1)
	    continue;
	CTable::code_t base = t.Find (prefix.begin(), --pk);
	nBits += t.CodeBits();
	CTable::code_t nc = t.AddString (base, *pk, true);
	if (nc == CTable::c_MaxCodes - 1) {
	    nBits += t.CodeBits();
	    t.Reset (m_CodeSize);
	}
	prefix[0] = *pk;
	pk = prefix.begin() + 1;
    }
    nBits += t.CodeBits();
    size_t nBytes = Align(nBits,8U) / 8;
    s += nBytes + nBytes / GIF_MAX_BLOCK_SIZE + 1;
    return (s);
}

/// Writes the current byte to \p os.
inline void CCompressor::FlushCurByte (ostream& os)
{
    os << m_CurByte;
    ++ m_BlockSize;
    m_CurByte = 0;
    m_BitsUsed = 0;
}

/// Updates the size of the current block in its header and starts a new block.
/// The new block has zero size by default.
void CCompressor::StartNextBlock (ostream& os)
{
    const uoff_t cp (os.pos());
    os.seek (cp - m_BlockSize - 1);
    os << m_BlockSize;
    os.seek (cp);
    os << (m_BlockSize = 0);
}

/// Writes code \p c to \p os as CTable::CodeBits bits.
void CCompressor::WriteCode (ostream& os, CTable::code_t c)
{
    size_t btw = t.CodeBits();
    while (btw) {
	size_t bw = min (btw, 8U - m_BitsUsed);
	m_CurByte |= (c & BitMask(CTable::code_t,bw)) << m_BitsUsed;
	m_BitsUsed += bw;
	c >>= bw;
	btw -= bw;
	if (m_BitsUsed == 8)
	    FlushCurByte (os);
	if (m_BlockSize >= GIF_MAX_BLOCK_SIZE)
	    StartNextBlock (os);
    }
}

//----------------------------------------------------------------------
// Blocks headers.
//----------------------------------------------------------------------

const char CFileHeader::s_GifSig[7] = "GIF89a";

//----------------------------------------------------------------------

/// Default constructor.
CFileHeader::CFileHeader (void)
: m_Width (0),
  m_Height (0),
  m_Resolution (0xFF),
  m_Background (0),
  m_AspectRatio (0)
{
}

/// Reads the object from stream \p is.
void CFileHeader::read (istream& is)
{
    char sig [7];
    if (is.remaining() < 13)
	throw runtime_error ("no GIF89 data found in the input stream");
    is.read (sig, 6);
    if (strncmp (sig, s_GifSig, 6))
	throw runtime_error ("no GIF89 data found in the input stream");
    is >> m_Width >> m_Height;
    is >> m_Resolution >> m_Background >> m_AspectRatio;
}

/// Writes the object to stream \p os.
void CFileHeader::write (ostream& os) const
{
    os.write (s_GifSig, 6);
    os << m_Width << m_Height;
    os << m_Resolution << m_Background << m_AspectRatio;
}

//----------------------------------------------------------------------

/// Default constructor.
CImageHeader::CImageHeader (void)
: m_LeftBorder (0),
  m_TopBorder (0),
  m_Width (0),
  m_Height (0),
  m_Flags (0)
{
}

/// Reads the object from stream \p is.
void CImageHeader::read (istream& is)
{
    uint8_t c;
    is >> c; m_LeftBorder = c;
    is >> c; m_LeftBorder |= (c << 8);
    is >> c; m_TopBorder = c;
    is >> c; m_TopBorder |= (c << 8);
    is >> c; m_Width = c;
    is >> c; m_Width |= (c << 8);
    is >> c; m_Height = c;
    is >> c; m_Height |= (c << 8);
    is >> m_Flags;
}

/// Writes the object to stream \p os.
void CImageHeader::write (ostream& os) const
{
    os << uint8_t(m_LeftBorder) << uint8_t(m_LeftBorder >> 8);
    os << uint8_t(m_TopBorder) << uint8_t(m_TopBorder >> 8);
    os << uint8_t(m_Width) << uint8_t(m_Width >> 8);
    os << uint8_t(m_Height) << uint8_t(m_Height >> 8);
    os << m_Flags;
}

//----------------------------------------------------------------------

} // namespace gif

