/*  This file is part of MS0515BTL.
    MS0515BTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    MS0515BTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
MS0515BTL. If not, see <http://www.gnu.org/licenses/>. */

// BitmapFile.cpp

#include "stdafx.h"
#include "BitmapFile.h"


//////////////////////////////////////////////////////////////////////

bool BmpFile_SaveScreenshot(
    const DWORD* pBits, const DWORD* palette, LPCTSTR sFileName,
    int screenWidth, int screenHeight)
{
    ASSERT(pBits != NULL);
    ASSERT(palette != NULL);
    ASSERT(sFileName != NULL);

    // Create file
    HANDLE hFile = ::CreateFile(sFileName,
            GENERIC_WRITE, FILE_SHARE_READ, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    BITMAPFILEHEADER hdr;
    ::ZeroMemory(&hdr, sizeof(hdr));
    hdr.bfType = 0x4d42;  // "BM"
    BITMAPINFOHEADER bih;
    ::ZeroMemory(&bih, sizeof(bih));
    bih.biSize = sizeof( BITMAPINFOHEADER );
    bih.biWidth = screenWidth;
    bih.biHeight = screenHeight;
    bih.biSizeImage = bih.biWidth * bih.biHeight / 2;
    bih.biPlanes = 1;
    bih.biBitCount = 4;
    bih.biCompression = BI_RGB;
    bih.biXPelsPerMeter = bih.biYPelsPerMeter = 2000;
    hdr.bfSize = (DWORD) sizeof(BITMAPFILEHEADER) + bih.biSize + bih.biSizeImage;
    hdr.bfOffBits = (DWORD) sizeof(BITMAPFILEHEADER) + bih.biSize + sizeof(RGBQUAD) * 16;

    DWORD dwBytesWritten = 0;

    BYTE * pData = (BYTE *) ::malloc(bih.biSizeImage);

    // Prepare the image data
    const DWORD * psrc = pBits;
    BYTE * pdst = pData;
    for (int i = 0; i < screenWidth * screenHeight; i++)
    {
        DWORD rgb = *psrc;
        psrc++;
        BYTE color = 0;
        for (BYTE c = 0; c < 16; c++)
        {
            if (palette[c] == rgb)
            {
                color = c;
                break;
            }
        }
        if ((i & 1) == 0)
            *pdst = (color << 4);
        else
        {
            *pdst = (*pdst) & 0xf0 | color;
            pdst++;
        }
    }

    WriteFile(hFile, &hdr, sizeof(BITMAPFILEHEADER), &dwBytesWritten, NULL);
    if (dwBytesWritten != sizeof(BITMAPFILEHEADER))
    {
        ::free(pData);
        return false;
    }
    WriteFile(hFile, &bih, sizeof(BITMAPINFOHEADER), &dwBytesWritten, NULL);
    if (dwBytesWritten != sizeof(BITMAPINFOHEADER))
    {
        ::free(pData);
        return false;
    }
    WriteFile(hFile, palette, sizeof(RGBQUAD) * 16, &dwBytesWritten, NULL);
    if (dwBytesWritten != sizeof(RGBQUAD) * 16)
    {
        ::free(pData);
        return false;
    }
    WriteFile(hFile, pData, bih.biSizeImage, &dwBytesWritten, NULL);
    ::free(pData);
    if (dwBytesWritten != bih.biSizeImage)
        return false;

    // Close file
    CloseHandle(hFile);

    return true;
}


//////////////////////////////////////////////////////////////////////

// Declaration for CRC calculation function, see the definition below
unsigned long crc(unsigned char *buf, int len);

// Declaration for ADLER32 calculation function, see the definition below
unsigned long update_adler32(unsigned long adler, unsigned char *buf, int len);

DWORD ReadValueMSB(const BYTE * buffer)
{
    DWORD value = *(buffer++);  value <<= 8;
    value |= *(buffer++);  value <<= 8;
    value |= *(buffer++);  value <<= 8;
    value |= *buffer;
    return value;
}

void SaveWordMSB(BYTE * buffer, WORD value)
{
    *(buffer++) = (BYTE)(value >> 8);
    * buffer    = (BYTE)(value);
}
void SaveValueMSB(BYTE * buffer, DWORD value)
{
    *(buffer++) = (BYTE)(value >> 24);
    *(buffer++) = (BYTE)(value >> 16);
    *(buffer++) = (BYTE)(value >> 8);
    * buffer    = (BYTE)(value);
}

void SavePngChunkChecksum(BYTE * chunk)
{
    DWORD datalen = ReadValueMSB(chunk);
    DWORD value = crc(chunk + 4, datalen + 4);
    BYTE* crcplace = (chunk + 8 + datalen);
    SaveValueMSB(crcplace, value);
}

bool PngFile_WriteHeader(FILE * fpFile, BYTE bitdepth, int screenWidth, int screenHeight)
{
    const BYTE pngheader[] = { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
    size_t dwBytesWritten = ::fwrite(pngheader, 1, sizeof(pngheader), fpFile);
    if (dwBytesWritten != sizeof(pngheader))
        return false;

    BYTE IHDRchunk[12 + 13];
    SaveValueMSB(IHDRchunk, 13);
    memcpy(IHDRchunk + 4, "IHDR", 4);
    SaveValueMSB(IHDRchunk + 8, screenWidth);
    SaveValueMSB(IHDRchunk + 12, screenHeight);
    *(IHDRchunk + 16) = bitdepth;  // Bit depth
    *(IHDRchunk + 17) = 3;  // Color type: indexed color
    *(IHDRchunk + 18) = 0;  // No compression
    *(IHDRchunk + 19) = 0;  // No filter
    *(IHDRchunk + 20) = 0;  // No interlace
    SavePngChunkChecksum(IHDRchunk);
    dwBytesWritten = ::fwrite(IHDRchunk, 1, sizeof(IHDRchunk), fpFile);
    if (dwBytesWritten != sizeof(IHDRchunk))
        return false;

    return true;
}

bool PngFile_WriteEnd(FILE * fpFile)
{
    BYTE IENDchunk[12 + 0];
    *((DWORD*)IENDchunk) = 0;
    memcpy(IENDchunk + 4, "IEND", 4);
    SavePngChunkChecksum(IENDchunk);
    size_t dwBytesWritten = ::fwrite(IENDchunk, 1, sizeof(IENDchunk), fpFile);
    if (dwBytesWritten != sizeof(IENDchunk))
        return false;

    return true;
}

bool PngFile_WritePalette(FILE * fpFile, const DWORD* palette, int palsize)
{
    int chunksize = 12 + palsize * 3;
    BYTE PLTEchunk[12 + 16 * 3];
    SaveValueMSB(PLTEchunk, palsize * 3);
    memcpy(PLTEchunk + 4, "PLTE", 4);
    BYTE * p = PLTEchunk + 8;
    for (int i = 0; i < palsize; i++)
    {
        DWORD color = *(palette++);
        *(p++) = (BYTE)(color >> 16);
        *(p++) = (BYTE)(color >> 8);
        *(p++) = (BYTE)(color >> 0);
    }
    SavePngChunkChecksum(PLTEchunk);
    size_t dwBytesWritten = ::fwrite(PLTEchunk, 1, chunksize, fpFile);
    if (dwBytesWritten != chunksize)
        return false;

    return true;
}

bool PngFile_WriteImageData4(FILE * fpFile, DWORD framenum, const DWORD* pBits, const DWORD* palette, int screenWidth, int screenHeight)
{
    // The IDAT chunk data format defined by RFC-1950 "ZLIB Compressed Data Format Specification version 3.3"
    // http://www.ietf.org/rfc/rfc1950.txt
    // We use uncomressed DEFLATE format, see RFC-1951
    // http://tools.ietf.org/html/rfc1951
    DWORD pDataLength = 8 + 2 + (6 + screenWidth / 2) * screenHeight + 4/*adler*/ + 4;
    if (framenum > 1) pDataLength += 4;
    BYTE * pData = (BYTE *) ::malloc(pDataLength);
    SaveValueMSB(pData, pDataLength - 12);
    memcpy(pData + 4, (framenum <= 1) ? "IDAT" : "fdAT", 4);
    if (framenum > 1) SaveValueMSB(pData + 8, framenum);

    BYTE * pDataStart = pData + ((framenum <= 1) ? 8 : 12);
    const BYTE cmf = 8;
    pDataStart[0] = cmf;                           // CM = 8, CMINFO = 0
    pDataStart[1] = (31 - ((cmf << 8) % 31)) % 31; // FCHECK (FDICT/FLEVEL=0)

    BYTE * pdst = pDataStart + 2;
    DWORD adler = 1L;
    for (int line = 0; line < screenHeight; line++)
    {
        const WORD linelen = (screenWidth / 2) + 1; // Each line is 257-byte block of non-compressed data
        *(pdst++) = (line < screenHeight - 1) ? 0 : 1; // Last?
        *(pdst++) = linelen & 0xff;
        *(pdst++) = (linelen >> 8) & 0xff;
        *(pdst++) = (~linelen) & 0xff;
        *(pdst++) = (~linelen >> 8) & 0xff;

        BYTE * pline = pdst;
        *(pdst++) = 0;  // additional "filter-type" byte at the beginning of every scanline
        const DWORD * psrc = pBits + ((screenHeight - 1 - line) * screenWidth);
        for (int i = 0; i < screenWidth; i++)
        {
            DWORD rgb = *(psrc++);
            BYTE color = 0;
            for (BYTE c = 0; c < 16; c++)
            {
                if (palette[c] == rgb)
                {
                    color = c;
                    break;
                }
            }
            if ((i & 1) == 0)
                *pdst = (color << 4);
            else
            {
                *pdst = (*pdst) & 0xf0 | color;
                pdst++;
            }
        }

        adler = update_adler32(adler, pline, linelen);
    }
    // ADLER32 checksum
    SaveValueMSB(pdst, adler);

    SavePngChunkChecksum(pData);

    // Write IDAT chunk
    size_t dwBytesWritten = ::fwrite(pData, 1, pDataLength, fpFile);
    ::free(pData);
    if (dwBytesWritten != pDataLength)
        return false;

    return true;
}

bool PngFile_SaveScreenshot(
    const DWORD* pBits, const DWORD* palette4, LPCTSTR sFileName,
    int screenWidth, int screenHeight)
{
    ASSERT(pBits != NULL);
    ASSERT(palette4 != NULL);
    ASSERT(sFileName != NULL);

    // Create file
    FILE * fpFile = ::_tfopen(sFileName, _T("w+b"));
    if (fpFile == NULL)
        return false;

    // Prepare 16-color palette using the given 4-color palette and all colors from the bitmap
    DWORD palette16[16];
    int palsize = 4;
    {
        memset(palette16, 0, sizeof(palette16));
        memcpy(palette16, palette4, sizeof(DWORD) * 4);
        for (int line = 0; line < screenHeight; line++)
        {
            const DWORD * psrc = pBits + ((screenHeight - 1 - line) * screenWidth);
            for (int i = 0; i < screenWidth; i++)
            {
                DWORD rgb = *(psrc++);
                BYTE color = 255;
                for (BYTE c = 0; c < palsize; c++)
                {
                    if (palette16[c] == rgb)
                    {
                        color = c;
                        break;
                    }
                }
                if (color != 255)
                    continue;
                if (palsize < 16)
                {
                    palette16[palsize] = rgb;
                    palsize++;
                }
            }
        }
    }

    if (!PngFile_WriteHeader(fpFile, 4, screenWidth, screenHeight))
    {
        ::fclose(fpFile);
        return false;
    }

    if (!PngFile_WritePalette(fpFile, palette16, palsize))
    {
        ::fclose(fpFile);
        return false;
    }

    if (!PngFile_WriteImageData4(fpFile, 0, pBits, palette16, screenWidth, screenHeight))
    {
        ::fclose(fpFile);
        return false;
    }

    if (!PngFile_WriteEnd(fpFile))
    {
        ::fclose(fpFile);
        return false;
    }

    ::fclose(fpFile);
    return true;
}


//////////////////////////////////////////////////////////////////////

struct APNGFILE
{
    FILE* fpFile;
    DWORD dwNextFrameNumber;
    fpos_t nActlOffset;       // "acTL" chunk offset
};

bool PngFile_WriteActl(FILE * fpFile, DWORD numframes)
{
    BYTE acTLchunk[12 + 8];
    SaveValueMSB(acTLchunk, 8);
    memcpy(acTLchunk + 4, "acTL", 4);
    SaveValueMSB(acTLchunk + 8, numframes);  // Number of frames
    SaveValueMSB(acTLchunk + 12, 0);  // Number of times to loop this APNG
    SavePngChunkChecksum(acTLchunk);
    size_t bytesWritten = ::fwrite(acTLchunk, 1, sizeof(acTLchunk), fpFile);
    if (bytesWritten != sizeof(acTLchunk))
        return false;

    return true;
}

bool PngFile_WriteFctl(FILE * fpFile, DWORD framenum, int screenWidth, int screenHeight)
{
    BYTE acTLchunk[12 + 26];
    SaveValueMSB(acTLchunk, 26);
    memcpy(acTLchunk + 4, "fcTL", 4);
    SaveValueMSB(acTLchunk + 8 + 0, framenum);  // Sequence number
    SaveValueMSB(acTLchunk + 8 + 4, screenWidth);
    SaveValueMSB(acTLchunk + 8 + 8, screenHeight);
    SaveValueMSB(acTLchunk + 8 + 12, 0);  // X
    SaveValueMSB(acTLchunk + 8 + 16, 0);  // Y
    SaveWordMSB(acTLchunk + 8 + 20,  1);  // Frame delay fraction numerator
    SaveWordMSB(acTLchunk + 8 + 22, 25);  // Frame delay fraction denominator
    acTLchunk[8 + 24] = 0;
    acTLchunk[8 + 25] = 0;
    SavePngChunkChecksum(acTLchunk);
    size_t bytesWritten = ::fwrite(acTLchunk, 1, sizeof(acTLchunk), fpFile);
    if (bytesWritten != sizeof(acTLchunk))
        return false;

    return true;
}


//////////////////////////////////////////////////////////////////////

/* Table of CRCs of all 8-bit messages. */
unsigned long crc_table[256];
/* Flag: has the table been computed? Initially false. */
int crc_table_computed = 0;

/* Make the table for a fast CRC. */
void make_crc_table(void)
{
    unsigned long c;
    int n, k;

    for (n = 0; n < 256; n++)
    {
        c = (unsigned long) n;
        for (k = 0; k < 8; k++)
        {
            if (c & 1)
                c = 0xedb88320L ^ (c >> 1);
            else
                c = c >> 1;
        }
        crc_table[n] = c;
    }
    crc_table_computed = 1;
}

/* Update a running CRC with the bytes buf[0..len-1]--the CRC
   should be initialized to all 1's, and the transmitted value
   is the 1's complement of the final running CRC (see the
   crc() routine below)). */
unsigned long update_crc(unsigned long crc, unsigned char *buf, int len)
{
    unsigned long c = crc;
    int n;

    if (!crc_table_computed)
        make_crc_table();
    for (n = 0; n < len; n++)
    {
        c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
    }
    return c;
}

/* Return the CRC of the bytes buf[0..len-1]. */
unsigned long crc(unsigned char *buf, int len)
{
    return update_crc(0xffffffffL, buf, len) ^ 0xffffffffL;
}


//////////////////////////////////////////////////////////////////////
// Source: http://www.ietf.org/rfc/rfc1950.txt section 9. Appendix: Sample code

unsigned long update_adler32(unsigned long adler, unsigned char *buf, int len)
{
    unsigned long s1 = adler & 0xffff;
    unsigned long s2 = (adler >> 16) & 0xffff;
    int n;

    for (n = 0; n < len; n++)
    {
        s1 = (s1 + buf[n]) % 65521;
        s2 = (s2 + s1)     % 65521;
    }
    return (s2 << 16) + s1;
}


//////////////////////////////////////////////////////////////////////
