/*
 * lzma zlib simplified wrapper
 *
 * Copyright (c) 2005-2006 Oleg I. Vdovikin <oleg@cs.msu.su>
 *
 * This library is free software; you can redistribute 
 * it and/or modify it under the terms of the GNU Lesser 
 * General Public License as published by the Free Software 
 * Foundation; either version 2.1 of the License, or 
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be 
 * useful, but WITHOUT ANY WARRANTY; without even the implied 
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
 * PURPOSE. See the GNU Lesser General Public License 
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General 
 * Public License along with this library; if not, write to 
 * the Free Software Foundation, Inc., 59 Temple Place, 
 * Suite 330, Boston, MA 02111-1307 USA 
 */

/*
 * default values for encoder/decoder used by wrapper
 */

#include <stdio.h>
#include <zlib.h>

// CJH: For Gentoo zlib compatibility.
#ifdef _Z_OF
#undef OF
#define OF _Z_OF
#endif

#define ZLIB_LC 3
#define ZLIB_LP 0
#define ZLIB_PB 2

// CJH: Taken from E2100 squashfs implementation
#define ZLIB_LC_E2100 0
#define ZLIB_LP_E2100 0
#define ZLIB_PB_E2100 2

#ifdef WIN32
#include <initguid.h>
#else
#define INITGUID
#endif

#include "../../../Common/MyWindows.h"
#include "../LZMA/LZMADecoder.h"
#include "../LZMA/LZMAEncoder.h"

#define STG_E_SEEKERROR                  ((HRESULT)0x80030019L)
#define STG_E_MEDIUMFULL                 ((HRESULT)0x80030070L)

class CInMemoryStream: 
  public IInStream,
  public IStreamGetSize,
  public CMyUnknownImp
{
public:
  CInMemoryStream(const Bytef *data, UInt64 size) : 
	  m_data(data), m_size(size), m_offset(0) {}

  virtual ~CInMemoryStream() {}

  MY_UNKNOWN_IMP2(IInStream, IStreamGetSize)

  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize)
  {
	  if (size > m_size - m_offset) 
		  size = m_size - m_offset;

	  if (size) {
		  memcpy(data, m_data + m_offset, size);
	  }

	  m_offset += size;

	  if (processedSize) 
		  *processedSize = size;

	  return S_OK;
  }

  STDMETHOD(ReadPart)(void *data, UInt32 size, UInt32 *processedSize)
  {
	return Read(data, size, processedSize);
  }

  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition)
  {
	  UInt64 _offset;

	  if (seekOrigin == STREAM_SEEK_SET) _offset = offset;
	  else if (seekOrigin == STREAM_SEEK_CUR) _offset = m_offset + offset; 
	  else if (seekOrigin == STREAM_SEEK_END) _offset = m_size;
	  else return STG_E_INVALIDFUNCTION;

	  if (_offset < 0 || _offset > m_size)
		  return STG_E_SEEKERROR;

	  m_offset = _offset;

	  if (newPosition)
		  *newPosition = m_offset;

	  return S_OK;
  }

  STDMETHOD(GetSize)(UInt64 *size)
  {
	  *size = m_size;
	  return S_OK;
  }
protected:
	const Bytef *m_data;
	UInt64 m_size;
	UInt64 m_offset;
};

class COutMemoryStream: 
  public IOutStream,
  public CMyUnknownImp
{
public:
  COutMemoryStream(Bytef *data, UInt64 maxsize) : 
	  m_data(data), m_size(0), m_maxsize(maxsize), m_offset(0) {}
  virtual ~COutMemoryStream() {}
  
  MY_UNKNOWN_IMP1(IOutStream)

  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize)
  {
	  if (size > m_maxsize - m_offset) 
		  size = m_maxsize - m_offset;

	  if (size) {
		  memcpy(m_data + m_offset, data, size);
	  }

	  m_offset += size;

	  if (m_offset > m_size)
		m_size = m_offset;

	  if (processedSize) 
		  *processedSize = size;

	  return S_OK;
  }
  
  STDMETHOD(WritePart)(const void *data, UInt32 size, UInt32 *processedSize)
  {
	  return Write(data, size, processedSize);
  }

  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition)
  {
	  UInt64 _offset;

	  if (seekOrigin == STREAM_SEEK_SET) _offset = offset;
	  else if (seekOrigin == STREAM_SEEK_CUR) _offset = m_offset + offset; 
	  else if (seekOrigin == STREAM_SEEK_END) _offset = m_size;
	  else return STG_E_INVALIDFUNCTION;

	  if (_offset < 0 || _offset > m_maxsize)
		  return STG_E_SEEKERROR;

	  m_offset = _offset;

	  if (newPosition)
		  *newPosition = m_offset;

	  return S_OK;
  }
  
  STDMETHOD(SetSize)(Int64 newSize)
  {
	  if ((UInt64)newSize > m_maxsize) 
		  return STG_E_MEDIUMFULL;

	  return S_OK;
  }
protected:
	Bytef *m_data;
	UInt64 m_size;
	UInt64 m_maxsize;
	UInt64 m_offset;
};

ZEXTERN int ZEXPORT compress2 OF((Bytef *dest,   uLongf *destLen,
                                  const Bytef *source, uLong sourceLen,
                                  int level))
{
	CInMemoryStream *inStreamSpec = new CInMemoryStream(source, sourceLen);
	CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
	
	COutMemoryStream *outStreamSpec = new COutMemoryStream(dest, *destLen);
	CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
	
	NCompress::NLZMA::CEncoder *encoderSpec = 
		new NCompress::NLZMA::CEncoder;
	CMyComPtr<ICompressCoder> encoder = encoderSpec;
	
	PROPID propIDs[] = 
	{
		NCoderPropID::kDictionarySize,
		NCoderPropID::kPosStateBits,
		NCoderPropID::kLitContextBits,
		NCoderPropID::kLitPosBits,
		NCoderPropID::kAlgorithm,
		NCoderPropID::kNumFastBytes,
		NCoderPropID::kMatchFinder,
		NCoderPropID::kEndMarker
	};
	const int kNumProps = sizeof(propIDs) / sizeof(propIDs[0]);
	
	PROPVARIANT properties[kNumProps];
	for (int p = 0; p < 6; p++)
		properties[p].vt = VT_UI4;
	properties[0].ulVal = UInt32(1 << (level + 14));
	properties[1].ulVal = UInt32(ZLIB_PB);
	properties[2].ulVal = UInt32(ZLIB_LC); // for normal files
	properties[3].ulVal = UInt32(ZLIB_LP); // for normal files
	properties[4].ulVal = UInt32(2);
	properties[5].ulVal = UInt32(128);
	
	properties[6].vt = VT_BSTR;
	properties[6].bstrVal = (BSTR)(const wchar_t *)L"BT4";
	
	properties[7].vt = VT_BOOL;
	properties[7].boolVal = VARIANT_TRUE;
	
	if (encoderSpec->SetCoderProperties(propIDs, properties, kNumProps) != S_OK)
		return Z_MEM_ERROR; // should not happen
	
	HRESULT result = encoder->Code(inStream, outStream, 0, 0, 0);
	if (result == E_OUTOFMEMORY)
	{
		return Z_MEM_ERROR;
	}   
	else if (result != S_OK)
	{
		return Z_BUF_ERROR;	// should not happen
	}   
	
	UInt64 fileSize;
	outStreamSpec->Seek(0, STREAM_SEEK_END, &fileSize);
	*destLen = fileSize;
	
	return Z_OK;
}

/*
 * CJH: These are depreciated, and left here merely for future reference.
 *
// CJH: s/uncompress/lzmalib_uncompress/
extern "C" int lzmalib_uncompress OF((Bytef *dest,   uLongf *destLen,
                                   const Bytef *source, uLong sourceLen))
{
	// CJH: 7zip ID implemented by some LZMA implementations
	if(strncmp((char *) source, "7zip", 4) == 0)
	{
		source += 4;
		sourceLen -= 4;
	}

	CInMemoryStream *inStreamSpec = new CInMemoryStream(source, sourceLen);
	CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
	
	COutMemoryStream *outStreamSpec = new COutMemoryStream(dest, *destLen);
	CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
	
	NCompress::NLZMA::CDecoder *decoderSpec = 
		new NCompress::NLZMA::CDecoder;
	CMyComPtr<ICompressCoder> decoder = decoderSpec;
	
	if (decoderSpec->SetDecoderPropertiesRaw(ZLIB_LC, 
		ZLIB_LP, ZLIB_PB, (1 << 23)) != S_OK) return Z_DATA_ERROR;
	
	UInt64 fileSize = *destLen;
	
	if (decoder->Code(inStream, outStream, 0, &fileSize, 0) != S_OK)
	{
        return Z_DATA_ERROR;
	}
	
	outStreamSpec->Seek(0, STREAM_SEEK_END, &fileSize);
	*destLen = fileSize;
	
	return Z_OK;
}

// CJH: A decompressor used by some Linksys SquashFS images
extern "C" int lzmalinksys_uncompress OF((Bytef *dest,   uLongf *destLen,
                                   const Bytef *source, uLong sourceLen))
{
	CInMemoryStream *inStreamSpec = new CInMemoryStream(source, sourceLen);
	CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
	
	COutMemoryStream *outStreamSpec = new COutMemoryStream(dest, *destLen);
	CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
	
	NCompress::NLZMA::CDecoder *decoderSpec = 
		new NCompress::NLZMA::CDecoder;
	CMyComPtr<ICompressCoder> decoder = decoderSpec;
	
	if (decoderSpec->SetDecoderPropertiesRaw(ZLIB_LC_E2100, 
		ZLIB_LP_E2100, ZLIB_PB_E2100, (1 << 23)) != S_OK) return Z_DATA_ERROR;
	
	UInt64 fileSize = *destLen;
	
	if (decoder->Code(inStream, outStream, 0, &fileSize, 0) != S_OK)
	{
        return Z_DATA_ERROR;
	}
	
	outStreamSpec->Seek(0, STREAM_SEEK_END, &fileSize);
	*destLen = fileSize;
	
	return Z_OK;
}

// CJH: A decompressor for "squashfs7z" images
extern "C" int lzma7z_uncompress OF((Bytef *dest,   uLongf *destLen,
                                   const Bytef *source, uLong sourceLen))
{
    // CJH: This variation encodes the properties values + size into the first nine bytes
	CInMemoryStream *inStreamSpec = new CInMemoryStream(source+9, sourceLen-9);
	CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
	
	COutMemoryStream *outStreamSpec = new COutMemoryStream(dest, *destLen);
	CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
	
	NCompress::NLZMA::CDecoder *decoderSpec = 
		new NCompress::NLZMA::CDecoder;
	CMyComPtr<ICompressCoder> decoder = decoderSpec;

    // CJH: This variation uses SetDecoderProperties2
	//if (decoderSpec->SetDecoderPropertiesRaw(source[1], 
	//	source[2], source[0], (1 << 23)) != S_OK) return Z_DATA_ERROR;
    if (decoderSpec->SetDecoderProperties2(source+4, 5) != S_OK)
                return Z_DATA_ERROR;

	UInt64 fileSize = *destLen;
	
	if (decoder->Code(inStream, outStream, 0, &fileSize, 0) != S_OK)
	{
		return Z_DATA_ERROR;
	}
	
	outStreamSpec->Seek(0, STREAM_SEEK_END, &fileSize);
	*destLen = fileSize;
	
	return Z_OK;
}

*/

// CJH: A decompressor for LZMA DD-WRT SquashFS images
extern "C" int lzmawrt_uncompress OF((Bytef *dest,   uLongf *destLen,
                                   const Bytef *source, uLong sourceLen))
{
    // CJH: DD-WRT encodes the properties values into the first four bytes
	CInMemoryStream *inStreamSpec = new CInMemoryStream(source+4, sourceLen-4);
	CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
	
	COutMemoryStream *outStreamSpec = new COutMemoryStream(dest, *destLen);
	CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
	
	NCompress::NLZMA::CDecoder *decoderSpec = 
		new NCompress::NLZMA::CDecoder;
	CMyComPtr<ICompressCoder> decoder = decoderSpec;

    /*
     * CJH: DD-WRT encodes the LZMA properties into the beginning of each compressed block.
     *      Sanity check these values to prevent errors in the LZMA library.
     */
     //printf("WRT properties: %d %d %d\n", (unsigned int) source[1],
     //                                     (unsigned int) source[2],
     //                                     (unsigned int) source[0]);
    if((unsigned int) source[1] > 4 ||
       (unsigned int) source[2] > 4 ||
       (unsigned int) source[0] > 4 ||
       ((unsigned int) source[1] + (unsigned int) source[2]) > 4)
    {
        return Z_DATA_ERROR;
    }

	if (decoderSpec->SetDecoderPropertiesRaw(source[1], 
		source[2], source[0], (1 << 23)) != S_OK) return Z_DATA_ERROR;
	
	UInt64 fileSize = *destLen;
	
	if (decoder->Code(inStream, outStream, 0, &fileSize, 0) != S_OK)
	{
		return Z_DATA_ERROR;
	}
	
	outStreamSpec->Seek(0, STREAM_SEEK_END, &fileSize);
	*destLen = fileSize;
	
	return Z_OK;
}

// CJH: A decompressor used for brute forcing commonly modified LZMA fields
extern "C" int lzmaspec_uncompress OF((Bytef *dest, 
                                       uLongf *destLen, 
                                       const Bytef *source, 
                                       uLong sourceLen, 
                                       int lc, 
                                       int lp, 
                                       int pb, 
                                       int dictionary_size,
                                       int offset))
{
	CInMemoryStream *inStreamSpec = new CInMemoryStream(source+offset, sourceLen-offset);
	CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
	
	COutMemoryStream *outStreamSpec = new COutMemoryStream(dest, *destLen);
	CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
	
	NCompress::NLZMA::CDecoder *decoderSpec = new NCompress::NLZMA::CDecoder;
	CMyComPtr<ICompressCoder> decoder = decoderSpec;

    // CJH: Use the default dictionary size if none was specified
    if (dictionary_size <= 0)
    {
            dictionary_size = (1 << 23);
    }

	if (decoderSpec->SetDecoderPropertiesRaw(lc, 
		lp, pb, dictionary_size) != S_OK) return Z_DATA_ERROR;
	
	UInt64 fileSize = *destLen;

	if (decoder->Code(inStream, outStream, 0, &fileSize, 0) != S_OK)
	{
        return Z_DATA_ERROR;
	}
	
	outStreamSpec->Seek(0, STREAM_SEEK_END, &fileSize);
	*destLen = fileSize;
	
	return Z_OK;
}

