

#include "codec/tpack_t.h"

#include <assert.h>
#include <stdlib.h>

int32_t tpack_decodeHead(byteQueue_tt* pInBytes, tpackHead_tt* pOutHead)
{
    assert(pOutHead && pInBytes);
    size_t  nBytesWritten = byteQueue_getBytesReadable(pInBytes);
    uint8_t szFrameBuffer[4];
    pOutHead->uiOffset = 0;
    while (nBytesWritten >= 2) {
        byteQueue_readBytes(pInBytes, szFrameBuffer, 2, true);

        switch (szFrameBuffer[0] & 0x0f) {
        case 0:
        {
            pOutHead->uiFlag = 0;
        } break;
        case 1:
        {
            pOutHead->uiFlag = DEF_TP_FRAME_TEXT;
        } break;
        case 2:
        {
            pOutHead->uiFlag = DEF_TP_FRAME_BINARY;
        } break;
        case 3:
        {
            pOutHead->uiFlag = DEF_TP_FRAME_CLOSE;
        } break;
        case 4:
        {
            pOutHead->uiFlag = DEF_TP_FRAME_PONG;
        } break;
        case 5:
        {
            pOutHead->uiFlag = DEF_TP_FRAME_PING;
        } break;
        case 6:
        {
            pOutHead->uiFlag = DEF_TP_FRAME_CALL;
        } break;
        case 7:
        {
            pOutHead->uiFlag = DEF_TP_FRAME_REPLY;
        } break;
        default:
        {
            return -1;
        }
        }

        pOutHead->uiPayloadLen = szFrameBuffer[1];
        if (pOutHead->uiPayloadLen < 0xff)   // 0xff = 255
        {
            switch (pOutHead->uiFlag) {
            case DEF_TP_FRAME_CLOSE:
            case DEF_TP_FRAME_PONG:
            case DEF_TP_FRAME_PING:
            case DEF_TP_FRAME_CALL:
            case DEF_TP_FRAME_REPLY:
            {
                if (pOutHead->uiPayloadLen + 6 > nBytesWritten) {
                    return 0;
                }
                pOutHead->uiOffset = 6;
            } break;
            default:
            {
                if (pOutHead->uiPayloadLen + 2 > nBytesWritten) {
                    return 0;
                }
                pOutHead->uiOffset = 2;
            } break;
            }
        }
        else {
            if (nBytesWritten < 4) {
                return 0;
            }
            byteQueue_readBytes(pInBytes, szFrameBuffer, 4, true);
            pOutHead->uiPayloadLen = ((uint16_t)szFrameBuffer[2] << 8) | szFrameBuffer[3];

            switch (pOutHead->uiFlag) {
            case DEF_TP_FRAME_CLOSE:
            case DEF_TP_FRAME_PONG:
            case DEF_TP_FRAME_PING:
            case DEF_TP_FRAME_CALL:
            case DEF_TP_FRAME_REPLY:
            {
                if (pOutHead->uiPayloadLen + 8 > nBytesWritten) {
                    return 0;
                }
                pOutHead->uiOffset = 8;
            } break;
            default:
            {
                if (pOutHead->uiPayloadLen + 4 > nBytesWritten) {
                    return 0;
                }
                pOutHead->uiOffset = 4;
            } break;
            }
        }

        if ((szFrameBuffer[0] & 0x80) == 0x80) {
            pOutHead->uiFlag |= DEF_TP_FRAME_FINAL;
        }
        return 1;
    }
    return 0;
}

int32_t tpack_decode(byteQueue_tt* pInBytes, cbuf_tt* pOutBuf, uint8_t* pOutFlag,
                     uint32_t* pOutToken)
{
    assert(pOutBuf && pInBytes);
    uint8_t      szToken[4];
    char*        pPackBuffer = NULL;
    tpackHead_tt head;
    int32_t      r = tpack_decodeHead(pInBytes, &head);
    if (r == 1) {
        if (head.uiOffset > 4) {
            byteQueue_readOffset(pInBytes, head.uiOffset - 4);
            byteQueue_readBytes(pInBytes, szToken, 4, false);
            *pOutToken = ((uint32_t)szToken[0] << 24) | ((uint32_t)szToken[1] << 16) |
                         ((uint32_t)szToken[2] << 8) | szToken[3];
        }
        else {
            byteQueue_readOffset(pInBytes, head.uiOffset);
            *pOutToken = 0;
        }

        if (head.uiPayloadLen > 0) {
            pPackBuffer = mem_malloc(head.uiPayloadLen);
            byteQueue_readBytes(pInBytes, pPackBuffer, head.uiPayloadLen, false);
        }

        size_t nLength = head.uiPayloadLen;
        *pOutFlag      = head.uiFlag;
        cbuf_swap(pOutBuf, &pPackBuffer, &nLength);
    }
    return r;
}

int32_t tpack_encode(const char* pBuffer, size_t nLength, uint8_t uiFlag, uint32_t uiToken,
                     ioBufVec_tt* pOutBufVec)
{
    assert(pBuffer && pOutBufVec);

    uint32_t uiTokenByte = 0;
    uint8_t  uiOpcode    = 0;
    switch (uiFlag) {
    case DEF_TP_FRAME_TEXT:
    {
        uiOpcode = 1;
    } break;
    case DEF_TP_FRAME_BINARY:
    {
        uiOpcode = 2;
    } break;
    case DEF_TP_FRAME_CLOSE:
    {
        uiOpcode    = 3;
        uiTokenByte = 4;
    } break;
    case DEF_TP_FRAME_PONG:
    {
        uiOpcode    = 4;
        uiTokenByte = 4;
    } break;
    case DEF_TP_FRAME_PING:
    {
        uiOpcode    = 5;
        uiTokenByte = 4;
    } break;
    case DEF_TP_FRAME_CALL:
    {
        uiOpcode    = 6;
        uiTokenByte = 4;
    } break;
    case DEF_TP_FRAME_REPLY:
    {
        uiOpcode    = 7;
        uiTokenByte = 4;
    } break;
    }

    int32_t  iEncodeBufCount = 0;
    uint8_t* pPackBuffer     = NULL;

    int32_t iFragmentCount = nLength / 0xffff + 1;   // 0xffff 65536
    if (iFragmentCount == 1) {
        if (nLength < 0xff) {
            if (uiTokenByte != 0) {
                pPackBuffer                         = mem_malloc(6 + nLength);
                pPackBuffer[2]                      = (uiToken >> 24 & 0xff);
                pPackBuffer[3]                      = (uiToken >> 16 & 0xff);
                pPackBuffer[4]                      = (uiToken >> 8 & 0xff);
                pPackBuffer[5]                      = (uiToken & 0xff);
                pOutBufVec[iEncodeBufCount].iLength = 6 + nLength;
            }
            else {
                pPackBuffer                         = mem_malloc(2 + nLength);
                pOutBufVec[iEncodeBufCount].iLength = 2 + nLength;
            }
            pPackBuffer[1] = (nLength & 0xff);
        }
        else {
            if (uiTokenByte != 0) {
                pPackBuffer                         = mem_malloc(8 + nLength);
                pPackBuffer[4]                      = (uiToken >> 24 & 0xff);
                pPackBuffer[5]                      = (uiToken >> 16 & 0xff);
                pPackBuffer[6]                      = (uiToken >> 8 & 0xff);
                pPackBuffer[7]                      = (uiToken & 0xff);
                pOutBufVec[iEncodeBufCount].iLength = 8 + nLength;
            }
            else {
                pPackBuffer                         = mem_malloc(4 + nLength);
                pOutBufVec[iEncodeBufCount].iLength = 4 + nLength;
            }
            pPackBuffer[1] = 0xff;
            pPackBuffer[2] = ((nLength >> 8) & 0xff);
            pPackBuffer[3] = (nLength & 0xff);
        }

        pPackBuffer[0] = uiOpcode | DEF_TP_FRAME_FINAL;
        memcpy(pPackBuffer + (pOutBufVec[iEncodeBufCount].iLength - nLength), pBuffer, nLength);
        pOutBufVec[iEncodeBufCount].pBuf = (char*)pPackBuffer;
        ++iEncodeBufCount;
    }
    else {
        for (int32_t j = 0; j < iFragmentCount; j++) {
            if (j == iFragmentCount - 1) {
                size_t nPackLength = nLength - j * 0xffff;
                if (nPackLength < 0xff) {
                    if (uiTokenByte != 0) {
                        pPackBuffer                         = mem_malloc(6 + nPackLength);
                        pPackBuffer[2]                      = (uiToken >> 24 & 0xff);
                        pPackBuffer[3]                      = (uiToken >> 16 & 0xff);
                        pPackBuffer[4]                      = (uiToken >> 8 & 0xff);
                        pPackBuffer[5]                      = (uiToken & 0xff);
                        pOutBufVec[iEncodeBufCount].iLength = 6 + nPackLength;
                    }
                    else {
                        pPackBuffer                         = mem_malloc(2);
                        pOutBufVec[iEncodeBufCount].iLength = 2 + nPackLength;
                    }
                    pPackBuffer[1] = (nPackLength & 0xff);
                }
                else {
                    if (uiTokenByte != 0) {
                        pPackBuffer                         = mem_malloc(8 + nPackLength);
                        pPackBuffer[4]                      = (uiToken >> 24 & 0xff);
                        pPackBuffer[5]                      = (uiToken >> 16 & 0xff);
                        pPackBuffer[6]                      = (uiToken >> 8 & 0xff);
                        pPackBuffer[7]                      = (uiToken & 0xff);
                        pOutBufVec[iEncodeBufCount].iLength = 8 + nPackLength;
                    }
                    else {
                        pPackBuffer                         = mem_malloc(4 + nPackLength);
                        pOutBufVec[iEncodeBufCount].iLength = 4 + nPackLength;
                    }
                    pPackBuffer[1] = 0xff;
                    pPackBuffer[2] = ((nPackLength >> 8) & 0xff);
                    pPackBuffer[3] = (nPackLength & 0xff);
                }
                pPackBuffer[0] = DEF_TP_FRAME_FINAL;

                memcpy(pPackBuffer + pOutBufVec[iEncodeBufCount].iLength - nPackLength,
                       pBuffer + j * 0xffff,
                       nPackLength);

                pOutBufVec[iEncodeBufCount].pBuf = (char*)pPackBuffer;
                ++iEncodeBufCount;
            }
            else {
                size_t nPackLength = 0xffff;
                if (uiTokenByte != 0) {
                    pPackBuffer                         = mem_malloc(8);
                    pPackBuffer[4]                      = (uiToken >> 24 & 0xff);
                    pPackBuffer[5]                      = (uiToken >> 16 & 0xff);
                    pPackBuffer[6]                      = (uiToken >> 8 & 0xff);
                    pPackBuffer[7]                      = (uiToken & 0xff);
                    pOutBufVec[iEncodeBufCount].iLength = 8 + nPackLength;
                }
                else {
                    pPackBuffer                         = mem_malloc(4 + nPackLength);
                    pOutBufVec[iEncodeBufCount].iLength = 4 + nPackLength;
                }
                pPackBuffer[1] = 0xff;
                pPackBuffer[2] = 0xff;
                pPackBuffer[3] = 0xff;

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

int32_t tpack_encodeVec(ioBufVec_tt* pInBufVec, int32_t iCount, uint8_t uiFlag, uint32_t uiToken,
                        ioBufVec_tt* pOutBufVec)
{
    assert(pInBufVec && pOutBufVec);

    uint32_t uiTokenByte = 0;
    uint8_t  uiOpcode    = 0;

    switch (uiFlag) {
    case DEF_TP_FRAME_TEXT:
    {
        uiOpcode = 1;
    } break;
    case DEF_TP_FRAME_BINARY:
    {
        uiOpcode = 2;
    } break;
    case DEF_TP_FRAME_CLOSE:
    {
        uiOpcode    = 3;
        uiTokenByte = 4;
    } break;
    case DEF_TP_FRAME_PONG:
    {
        uiOpcode    = 4;
        uiTokenByte = 4;
    } break;
    case DEF_TP_FRAME_PING:
    {
        uiOpcode    = 5;
        uiTokenByte = 4;
    } break;
    case DEF_TP_FRAME_CALL:
    {
        uiOpcode    = 6;
        uiTokenByte = 4;
    } break;
    case DEF_TP_FRAME_REPLY:
    {
        uiOpcode    = 7;
        uiTokenByte = 4;
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
        if (nLength < 0xff) {
            if (uiTokenByte != 0) {
                pPackBuffer                         = mem_malloc(6 + nLength);
                pPackBuffer[2]                      = (uiToken >> 24 & 0xff);
                pPackBuffer[3]                      = (uiToken >> 16 & 0xff);
                pPackBuffer[4]                      = (uiToken >> 8 & 0xff);
                pPackBuffer[5]                      = (uiToken & 0xff);
                pOutBufVec[iEncodeBufCount].iLength = 6 + nLength;
            }
            else {
                pPackBuffer                         = mem_malloc(2 + nLength);
                pOutBufVec[iEncodeBufCount].iLength = 2 + nLength;
            }
            pPackBuffer[1] = (nLength & 0xff);
        }
        else {
            if (uiTokenByte != 0) {
                pPackBuffer                         = mem_malloc(8 + nLength);
                pPackBuffer[4]                      = (uiToken >> 24 & 0xff);
                pPackBuffer[5]                      = (uiToken >> 16 & 0xff);
                pPackBuffer[6]                      = (uiToken >> 8 & 0xff);
                pPackBuffer[7]                      = (uiToken & 0xff);
                pOutBufVec[iEncodeBufCount].iLength = 8 + nLength;
            }
            else {
                pPackBuffer                         = mem_malloc(4 + nLength);
                pOutBufVec[iEncodeBufCount].iLength = 4 + nLength;
            }
            pPackBuffer[1] = 0xff;
            pPackBuffer[2] = ((nLength >> 8) & 0xff);
            pPackBuffer[3] = (nLength & 0xff);
        }

        pPackBuffer[0] = uiOpcode | DEF_TP_FRAME_FINAL;

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
                if (nPackLength < 0xff) {
                    if (uiTokenByte != 0) {
                        pPackBuffer                         = mem_malloc(6 + nPackLength);
                        pPackBuffer[2]                      = (uiToken >> 24 & 0xff);
                        pPackBuffer[3]                      = (uiToken >> 16 & 0xff);
                        pPackBuffer[4]                      = (uiToken >> 8 & 0xff);
                        pPackBuffer[5]                      = (uiToken & 0xff);
                        pOutBufVec[iEncodeBufCount].iLength = 6 + nPackLength;
                    }
                    else {
                        pPackBuffer                         = mem_malloc(2 + nPackLength);
                        pOutBufVec[iEncodeBufCount].iLength = 2 + nPackLength;
                    }
                    pPackBuffer[1] = (nPackLength & 0xff);
                }
                else {
                    if (uiTokenByte != 0) {
                        pPackBuffer                         = mem_malloc(8 + nPackLength);
                        pPackBuffer[4]                      = (uiToken >> 24 & 0xff);
                        pPackBuffer[5]                      = (uiToken >> 16 & 0xff);
                        pPackBuffer[6]                      = (uiToken >> 8 & 0xff);
                        pPackBuffer[7]                      = (uiToken & 0xff);
                        pOutBufVec[iEncodeBufCount].iLength = 8 + nPackLength;
                    }
                    else {
                        pPackBuffer                         = mem_malloc(4 + nPackLength);
                        pOutBufVec[iEncodeBufCount].iLength = 4 + nPackLength;
                    }
                    pPackBuffer[1] = 0xff;
                    pPackBuffer[2] = ((nPackLength >> 8) & 0xff);
                    pPackBuffer[3] = (nPackLength & 0xff);
                }
                pPackBuffer[0] = DEF_TP_FRAME_FINAL;

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
                size_t nPackLength = 0xffff;
                if (uiTokenByte != 0) {
                    pPackBuffer                         = mem_malloc(8 + nPackLength);
                    pPackBuffer[4]                      = (uiToken >> 24 & 0xff);
                    pPackBuffer[5]                      = (uiToken >> 16 & 0xff);
                    pPackBuffer[6]                      = (uiToken >> 8 & 0xff);
                    pPackBuffer[7]                      = (uiToken & 0xff);
                    pOutBufVec[iEncodeBufCount].iLength = 8 + nPackLength;
                }
                else {
                    pPackBuffer                         = mem_malloc(4 + nPackLength);
                    pOutBufVec[iEncodeBufCount].iLength = 4 + nPackLength;
                }
                pPackBuffer[1] = 0xff;
                pPackBuffer[2] = 0xff;
                pPackBuffer[3] = 0xff;

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
