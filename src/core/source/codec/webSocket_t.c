

#include "codec/webSocket_t.h"

#include <assert.h>
#include <stdlib.h>

static inline void unMask(char* pBuffer, size_t nLength, char szMask[4])
{
    for (size_t i = 0; i < nLength; ++i) {
        pBuffer[i] ^= szMask[i % 4];
    }
}

int32_t webSocket_decodeHead(byteQueue_tt* pInBytes, webSocketHead_tt* pOutHead)
{
    assert(pOutHead && pInBytes);

    size_t  nBytesWritten = byteQueue_getBytesReadable(pInBytes);
    uint8_t szFrameBuffer[4];
    pOutHead->uiOffset = 0;

    if (nBytesWritten >= 2) {
        byteQueue_readBytes(pInBytes, szFrameBuffer, 2, true);
        switch (szFrameBuffer[0] & 0x0f) {
        case 0:
        {
            pOutHead->uiFlag = 0;
        } break;
        case 1:
        {
            pOutHead->uiFlag = DEF_WS_FRAME_TEXT;
        } break;
        case 2:
        {
            pOutHead->uiFlag = DEF_WS_FRAME_BINARY;
        } break;
        case 8:
        {
            pOutHead->uiFlag = DEF_WS_FRAME_CLOSE;
        } break;
        case 9:
        {
            pOutHead->uiFlag = DEF_WS_FRAME_PING;
        } break;
        case 10:
        {
            pOutHead->uiFlag = DEF_WS_FRAME_PONG;
        } break;
        default:
        {
            return -1;
        }
        }

        if ((szFrameBuffer[0] & 0x80) == 0x80) {
            pOutHead->uiFlag |= DEF_WS_FRAME_FINAL;
        }

        pOutHead->uiPayloadLen = szFrameBuffer[1] & 0x7F;
        if (pOutHead->uiPayloadLen < 126) {
            if ((szFrameBuffer[1] & 0x80) == 0x80) {
                if (pOutHead->uiPayloadLen + 6 > nBytesWritten) {
                    return 0;
                }
                pOutHead->uiOffset = 6;
            }
            else {
                if (pOutHead->uiPayloadLen + 2 > nBytesWritten) {
                    return 0;
                }
                pOutHead->uiOffset = 2;
            }
        }
        else if (pOutHead->uiPayloadLen == 126) {
            if (nBytesWritten < 4) {
                return 0;
            }
            byteQueue_readBytes(pInBytes, szFrameBuffer, 4, true);
            pOutHead->uiPayloadLen = ((szFrameBuffer[2]) | ((uint16_t)(szFrameBuffer[3]) << 8));
            if ((szFrameBuffer[1] & 0x80) == 0x80) {
                if (pOutHead->uiPayloadLen + 8 > nBytesWritten) {
                    return 0;
                }
                pOutHead->uiOffset = 8;
            }
            else {
                if (pOutHead->uiPayloadLen + 4 > nBytesWritten) {
                    return 0;
                }
                pOutHead->uiOffset = 4;
            }
        }
        else {
            return -1;
        }
        return 1;
    }
    return 0;
}

int32_t webSocket_decode(byteQueue_tt* pInBytes, cbuf_tt* pOutBuf, uint8_t* pOutFlag)
{
    assert(pOutBuf && pInBytes && pOutFlag);

    char*            pPackBuffer = NULL;
    char             szMask[4];
    webSocketHead_tt head;
    int32_t          r = webSocket_decodeHead(pInBytes, &head);
    if (r == 1) {
        if (head.uiOffset > 4) {
            byteQueue_readOffset(pInBytes, head.uiOffset - 4);
            byteQueue_readBytes(pInBytes, szMask, 4, false);
        }
        else {
            byteQueue_readOffset(pInBytes, head.uiOffset);
        }

        if (head.uiPayloadLen > 0) {
            pPackBuffer = mem_malloc(head.uiPayloadLen);
            byteQueue_readBytes(pInBytes, pPackBuffer, head.uiPayloadLen, false);
            if (head.uiOffset > 4) {
                unMask(pPackBuffer, head.uiPayloadLen, szMask);
            }
        }

        size_t nLength = head.uiPayloadLen;
        *pOutFlag      = head.uiFlag;
        cbuf_swap(pOutBuf, &pPackBuffer, &nLength);
    }
    return r;
}

int32_t webSocket_encode(const char* pBuffer, size_t nLength, uint8_t uiFlag,
                         ioBufVec_tt* pOutBufVec)
{
    assert(pBuffer && pOutBufVec);

    uint8_t uiOpcode = 0;
    switch (uiFlag & DEF_WS_FRAME_MASK) {
    case DEF_WS_FRAME_TEXT:
    {
        uiOpcode = 1;
    } break;
    case DEF_WS_FRAME_BINARY:
    {
        uiOpcode = 2;
    } break;
    case DEF_WS_FRAME_CLOSE:
    {
        uiOpcode = 8;
    } break;
    case DEF_WS_FRAME_PING:
    {
        uiOpcode = 9;
    } break;
    case DEF_WS_FRAME_PONG:
    {
        uiOpcode = 10;
    } break;
    default:
    {
        uiOpcode = 1;
    } break;
    }

    uint8_t* pPackBuffer     = NULL;
    int32_t  iEncodeBufCount = 0;
    size_t   nFragmentCount  = nLength / 0xffff + 1;
    if (nFragmentCount == 1) {
        if (nLength < 126) {
            pPackBuffer                         = mem_malloc(2 + nLength);
            pOutBufVec[iEncodeBufCount].iLength = 2 + nLength;
            pPackBuffer[1]                      = nLength & 0xFF;
        }
        else {
            pPackBuffer                         = mem_malloc(nLength + 4);
            pOutBufVec[iEncodeBufCount].iLength = 4 + nLength;
            pPackBuffer[1]                      = 126;
            pPackBuffer[2]                      = ((nLength >> 8) & 0xFF);
            pPackBuffer[3]                      = (nLength & 0xFF);
        }
        pPackBuffer[0] = uiOpcode | DEF_WS_FRAME_FINAL;
        memcpy(pPackBuffer + (pOutBufVec[iEncodeBufCount].iLength - nLength), pBuffer, nLength);
        pOutBufVec[iEncodeBufCount].pBuf = (char*)pPackBuffer;
        ++iEncodeBufCount;
    }
    else {
        for (int32_t j = 0; j < nFragmentCount; j++) {
            if (j == nFragmentCount - 1) {
                size_t nPackLength = nLength - j * 0xffff;
                if (nLength - j * 0xffff < 126) {
                    pPackBuffer                         = mem_malloc(2 + nPackLength);
                    pOutBufVec[iEncodeBufCount].iLength = 2 + nPackLength;
                    pPackBuffer[1]                      = nPackLength & 0xff;
                }
                else {
                    pPackBuffer                         = mem_malloc(4 + nPackLength);
                    pOutBufVec[iEncodeBufCount].iLength = 4 + nPackLength;
                    pPackBuffer[1]                      = 126;
                    pPackBuffer[2]                      = ((nPackLength >> 8) & 0xff);
                    pPackBuffer[3]                      = (nPackLength & 0xff);
                }
                pPackBuffer[0] = DEF_WS_FRAME_FINAL;

                memcpy(pPackBuffer + pOutBufVec[iEncodeBufCount].iLength - nPackLength,
                       pBuffer + j * 0xffff,
                       nPackLength);
                pOutBufVec[iEncodeBufCount].pBuf = (char*)pPackBuffer;
                ++iEncodeBufCount;
            }
            else {
                size_t nPackLength                  = 0xffff;
                pPackBuffer                         = mem_malloc(4 + nPackLength);
                pOutBufVec[iEncodeBufCount].iLength = 4 + nPackLength;
                pPackBuffer[1]                      = 126;
                pPackBuffer[2]                      = 0xff;
                pPackBuffer[3]                      = 0xff;

                if (j == 0) {
                    pPackBuffer[0] = uiOpcode;
                }
                else {
                    pPackBuffer[0] = 0;
                }

                memcpy(pPackBuffer + pOutBufVec[iEncodeBufCount].iLength - nPackLength,
                       pBuffer + j * 0xffff,
                       nPackLength);
                pOutBufVec[iEncodeBufCount].pBuf = (char*)pPackBuffer;
                ++iEncodeBufCount;
            }
        }
    }
    return iEncodeBufCount;
}

int32_t webSocket_encodeVec(ioBufVec_tt* pInBufVec, int32_t iCount, uint8_t uiFlag,
                            ioBufVec_tt* pOutBufVec)
{
    assert(pInBufVec && pOutBufVec);

    uint8_t uiOpcode = 0;
    switch (uiFlag & DEF_WS_FRAME_MASK) {
    case DEF_WS_FRAME_TEXT:
    {
        uiOpcode = 1;
    } break;
    case DEF_WS_FRAME_BINARY:
    {
        uiOpcode = 2;
    } break;
    case DEF_WS_FRAME_CLOSE:
    {
        uiOpcode = 8;
    } break;
    case DEF_WS_FRAME_PING:
    {
        uiOpcode = 9;
    } break;
    case DEF_WS_FRAME_PONG:
    {
        uiOpcode = 10;
    } break;
    default:
    {
        uiOpcode = 1;
    } break;
    }

    int32_t iEncodeBufCount = 0;

    size_t nLength = 0;
    for (int32_t i = 0; i < iCount; i++) {
        nLength += pInBufVec[i].iLength;
    }

    uint8_t* pPackBuffer    = NULL;
    int32_t  iFragmentCount = nLength / 0xffff + 1;
    if (iFragmentCount == 1) {
        if (nLength < 126) {
            pPackBuffer                         = mem_malloc(2 + nLength);
            pOutBufVec[iEncodeBufCount].iLength = 2 + nLength;
            pPackBuffer[1]                      = nLength & 0xff;
        }
        else {
            pPackBuffer                         = mem_malloc(4 + nLength);
            pOutBufVec[iEncodeBufCount].iLength = 4 + nLength;
            pPackBuffer[1]                      = 126;
            pPackBuffer[2]                      = (nLength >> 8 & 0xff);
            pPackBuffer[3]                      = (nLength & 0xff);
        }

        pPackBuffer[0] = uiOpcode | DEF_WS_FRAME_FINAL;

        size_t nOffset = pOutBufVec[iEncodeBufCount].iLength - nLength;

        for (int32_t i = 0; i < iCount; i++) {
            memcpy(pPackBuffer + nOffset, pInBufVec[i].pBuf, pInBufVec[i].iLength);
            nOffset += pInBufVec[i].iLength;
        }
        pOutBufVec[iEncodeBufCount].pBuf = (char*)pPackBuffer;
        ++iEncodeBufCount;
    }
    else {
        int32_t iSrcBufIndex  = 0;
        size_t  nSrcBufOffset = 0;
        for (int32_t j = 0; j < iFragmentCount; j++) {
            if (j == iFragmentCount - 1) {
                size_t nPackLength = nLength - j * 0xffff;
                if (nPackLength < 126) {
                    pPackBuffer                         = mem_malloc(2 + nPackLength);
                    pOutBufVec[iEncodeBufCount].iLength = 2 + nPackLength;
                    pPackBuffer[1]                      = nPackLength & 0xff;
                }
                else {
                    pPackBuffer                         = mem_malloc(4 + nPackLength);
                    pOutBufVec[iEncodeBufCount].iLength = 4 + nPackLength;
                    pPackBuffer[1]                      = 126;
                    pPackBuffer[2]                      = ((nPackLength >> 8) & 0xff);
                    pPackBuffer[3]                      = (nPackLength & 0xff);
                }
                pPackBuffer[0] = DEF_WS_FRAME_FINAL;

                size_t nOffset = pOutBufVec[iEncodeBufCount].iLength - nPackLength;
                for (;;) {
                    if (pInBufVec[iSrcBufIndex].iLength - nSrcBufOffset >
                        pOutBufVec[iEncodeBufCount].iLength - nOffset) {
                        memcpy(pPackBuffer + nOffset,
                               pInBufVec[iSrcBufIndex].pBuf + nSrcBufOffset,
                               pOutBufVec[iEncodeBufCount].iLength - nOffset);
                        nSrcBufOffset += (pOutBufVec[iEncodeBufCount].iLength - nOffset);
                        break;
                    }
                    else {
                        memcpy(pPackBuffer + nOffset,
                               pInBufVec[iSrcBufIndex].pBuf + nSrcBufOffset,
                               pInBufVec[iSrcBufIndex].iLength - nSrcBufOffset);
                        nOffset += (pInBufVec[iSrcBufIndex].iLength - nSrcBufOffset);
                        nSrcBufOffset = 0;
                        ++iSrcBufIndex;
                        if (nOffset == pOutBufVec[iEncodeBufCount].iLength) {
                            break;
                        }
                    }
                }
                pOutBufVec[iEncodeBufCount].pBuf = (char*)pPackBuffer;
                ++iEncodeBufCount;
            }
            else {
                size_t nPackLength                  = 0xffff;
                pPackBuffer                         = mem_malloc(4 + nPackLength);
                pOutBufVec[iEncodeBufCount].iLength = 4 + nPackLength;
                pPackBuffer[1]                      = 0xff;
                pPackBuffer[2]                      = 0xff;
                pPackBuffer[3]                      = 0xff;

                if (j == 0) {
                    pPackBuffer[0] = uiOpcode;
                }
                else {
                    pPackBuffer[0] = 0;
                }

                size_t nOffset = pOutBufVec[iEncodeBufCount].iLength - nPackLength;
                for (;;) {
                    if (pInBufVec[iSrcBufIndex].iLength - nSrcBufOffset >
                        pOutBufVec[iEncodeBufCount].iLength - nOffset) {
                        memcpy(pPackBuffer + nOffset,
                               pInBufVec[iSrcBufIndex].pBuf + nSrcBufOffset,
                               pOutBufVec[iEncodeBufCount].iLength - nOffset);
                        nSrcBufOffset += (pOutBufVec[iEncodeBufCount].iLength - nOffset);
                        break;
                    }
                    else {
                        memcpy(pPackBuffer + nOffset,
                               pInBufVec[iSrcBufIndex].pBuf + nSrcBufOffset,
                               pInBufVec[iSrcBufIndex].iLength - nSrcBufOffset);
                        nOffset += (pInBufVec[iSrcBufIndex].iLength - nSrcBufOffset);
                        nSrcBufOffset = 0;
                        ++iSrcBufIndex;
                        if (nOffset == pOutBufVec[iEncodeBufCount].iLength) {
                            break;
                        }
                    }
                }
                pOutBufVec[iEncodeBufCount].pBuf = (char*)pPackBuffer;
                ++iEncodeBufCount;
            }
        }
    }
    return iEncodeBufCount;
}
