/*
 * Copyright (C) 2009 The Android Open Source Project
 * Modified for use by h264bsd standalone library
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*------------------------------------------------------------------------------

    Table of contents

     1. Include headers
     2. External compiler flags
     3. Module defines
     4. Local function prototypes
     5. Functions
          ExtractNalUnit

------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "h264bsd_byte_stream.h"
#include "h264bsd_util.h"

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

#define BYTE_STREAM_ERROR  0xFFFFFFFF

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Function name: ExtractNalUnit

        Functional description:
            Extracts one NAL unit from the byte stream buffer. Removes
            emulation prevention bytes if present. The original stream buffer
            is used directly and is therefore modified if emulation prevention
            bytes are present in the stream.

            Stream buffer is assumed to contain either exactly one NAL unit
            and nothing else, or one or more NAL units embedded in byte
            stream format described in the Annex B of the standard. Function
            detects which one is used based on the first bytes in the buffer.

        Inputs:
            pByteStream     pointer to byte stream buffer
            len             length of the stream buffer (in bytes)

        Outputs:
            pStrmData       stream information is stored here
            readBytes       number of bytes "consumed" from the stream buffer

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      error in byte stream

------------------------------------------------------------------------------*/

void printData(u8 *data, u32 len) {
    int i = 0;

    for (i=0; i<len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

u32 h264bsdExtractNalUnit(u8 *pByteStream, u32 len, strmData_t *pStrmData, u32 *readBytes)
{
//    EPRINT("1");
/* Variables */

    u32 i, tmp;
    u32 byteCount,initByteCount;
    u32 zeroCount;
    u8  byte;
    u32 hasEmulation = HANTRO_FALSE;
    u32 invalidStream = HANTRO_FALSE;
    u8 *readPtr, *writePtr;

/* Code */

//    EPRINT("2");
    ASSERT(pByteStream);
    ASSERT(len);
    ASSERT(len < BYTE_STREAM_ERROR);
    ASSERT(pStrmData);
//    EPRINT("3");

    /* byte stream format if starts with 0x000001 or 0x000000 */
    if (len > 3 && pByteStream[0] == 0x00 && pByteStream[1] == 0x00 &&
        (pByteStream[2]&0xFE) == 0x00)
    {
//        EPRINT("4");
        /* search for NAL unit start point, i.e. point after first start code
         * prefix in the stream */
        zeroCount = byteCount = 2;
        readPtr = pByteStream + 2;
        /*lint -e(716) while(1) used consciously */
        while (1)
        {
//            EPRINT("41");
            byte = *readPtr++;
            byteCount++;

            if (byteCount == len)
            {
                /* no start code prefix found -> error */
                *readBytes = len;
                EPRINT("err1");
                return(HANTRO_NOK);
            }

            if (!byte)
                zeroCount++;
            else if ((byte == 0x01) && (zeroCount >= 2))
                break;
            else
                zeroCount = 0;
        }

        initByteCount = byteCount;

        /* determine size of the NAL unit. Search for next start code prefix
         * or end of stream and ignore possible trailing zero bytes */
        zeroCount = 0;
        /*lint -e(716) while(1) used consciously */
//        EPRINT("5");
        while (1)
        {
//            EPRINT("51");
            byte = *readPtr++;
            byteCount++;
            if (!byte)
                zeroCount++;

            if ( (byte == 0x03) && (zeroCount == 2) )
            {
                printf("00 00 03\n");
//                printData(pByteStream, len);
                hasEmulation = HANTRO_TRUE;
            }

            if ( (byte == 0x01) && (zeroCount >= 2 ) )
            {
                pStrmData->strmBuffSize =
                    byteCount - initByteCount - zeroCount - 1;
                zeroCount -= MIN(zeroCount, 3);
                break;
            }
            else if (byte)
            {
                if (zeroCount >= 3)
                    invalidStream = HANTRO_TRUE;
                zeroCount = 0;
            }

            if (byteCount == len)
            {
                pStrmData->strmBuffSize = byteCount - initByteCount - zeroCount;
                break;
            }

        }
//        EPRINT("6");
    }
    /* separate NAL units as input -> just set stream params */
    else
    {
//        EPRINT("7");
        initByteCount = 0;
        zeroCount = 0;
        pStrmData->strmBuffSize = len;
        hasEmulation = HANTRO_TRUE;
    }

//    EPRINT("8");
    pStrmData->pStrmBuffStart    = pByteStream + initByteCount;
    pStrmData->pStrmCurrPos      = pStrmData->pStrmBuffStart;
    pStrmData->bitPosInWord      = 0;
    pStrmData->strmBuffReadBits  = 0;

    /* return number of bytes "consumed" */
    *readBytes = pStrmData->strmBuffSize + initByteCount + zeroCount;

    if (invalidStream)
    {
        EPRINT("err2");
        printData(pByteStream, len);
        return(HANTRO_NOK);
    }

    /* remove emulation prevention bytes before rbsp processing */
    if (hasEmulation)
    {
//        EPRINT("A");
        tmp = pStrmData->strmBuffSize;
        readPtr = writePtr = pStrmData->pStrmBuffStart;
        zeroCount = 0;
        for (i = tmp; i--;)
        {
//            EPRINT("B");
            if ((zeroCount == 2) && (*readPtr == 0x03))
            {
//                EPRINT("C");
                /* emulation prevention byte shall be followed by one of the
                 * following bytes: 0x00, 0x01, 0x02, 0x03. This implies that
                 * emulation prevention 0x03 byte shall not be the last byte
                 * of the stream. */
                if ( (i == 0) || (*(readPtr+1) > 0x03) ) {
                    EPRINT("err3");
                    return(HANTRO_NOK);
                }

                /* do not write emulation prevention byte */
                readPtr++;
                zeroCount = 0;
            }
            else
            {
                /* NAL unit shall not contain byte sequences 0x000000,
                 * 0x000001 or 0x000002 */
                if ( (zeroCount == 2) && (*readPtr <= 0x02) ) {
                    EPRINT("err4");
                    return(HANTRO_NOK);
                }

                if (*readPtr == 0)
                    zeroCount++;
                else
                    zeroCount = 0;

                *writePtr++ = *readPtr++;
            }
        }

        /* (readPtr - writePtr) indicates number of "removed" emulation
         * prevention bytes -> subtract from stream buffer size */
        pStrmData->strmBuffSize -= (u32)(readPtr - writePtr);
    }

    return(HANTRO_OK);

}

