/****************************************************************************************************************
 * COPYRIGHT � 2014 PixeLINK CORPORATION.  ALL RIGHTS RESERVED.                                                 *
 *                                                                                                              *
 * Copyright Notice and Disclaimer of Liability:                                                                *
 *                                                                                                              *
 *                                                                                                              *
 * PixeLINK Corporation is henceforth referred to as PixeLINK or PixeLINK Corporation.                          *
 * Purchaser henceforth refers to the original purchaser(s) of the equipment, and/or any legitimate user(s).    *
 *                                                                                                              *
 * PixeLINK hereby explicitly prohibits any form of reproduction (with the express strict exception for backup  *
 * and archival purposes, which are allowed as stipulated within the License Agreement for PixeLINK Corporation *
 * Software), modification, and/or distribution of this software and/or its associated documentation unless     *
 * explicitly specified in a written agreement signed by both parties.                                          *
 *                                                                                                              *
 * To the extent permitted by law, PixeLINK disclaims all other warranties or conditions of any kind, either    *
 * express or implied, including but not limited to all warranties or conditions of merchantability and         *
 * fitness for a particular purpose and those arising by statute or otherwise in law or from a course of        *
 * dealing or usage of trade. Other written or oral statements by PixeLINK, its representatives, or others do   *
 * not constitute warranties or conditions of PixeLINK.                                                         *
 *                                                                                                              *
 * PixeLINK makes no guarantees or representations, including but not limited to: the use of, or the result(s)  *
 * of the use of: the software and associated documentation in terms of correctness, accuracy, reliability,     *
 * being current, or otherwise. The Purchaser hereby agree to rely on the software, associated hardware and     *
 * documentation and results stemming from the use thereof solely at their own risk.                            *
 *                                                                                                              *
 * By using the products(s) associated with the software, and/or the software, the Purchaser and/or user(s)     *
 * agree(s) to abide by the terms and conditions set forth within this document, as well as, respectively,      *
 * any and all other documents issued by PixeLINK in relation to the product(s).                                *
 *                                                                                                              *
 * PixeLINK is hereby absolved of any and all liability to the Purchaser, and/or a third party, financial or    *
 * otherwise, arising from any subsequent loss, direct and indirect, and damage, direct and indirect,           *
 * resulting from intended and/or unintended usage of its software, product(s) and documentation, as well       *
 * as additional service(s) rendered by PixeLINK, such as technical support, unless otherwise explicitly        *
 * specified in a written agreement signed by both parties. Under no circumstances shall the terms and          *
 * conditions of such an agreement apply retroactively.                                                         *
 *                                                                                                              *
 ****************************************************************************************************************/

/*******************************************************************************
    File: LinuxTypes.h
    
    Description: Simple types that are defined on Windows, but not Linux
    
    Revisions:
        2014-11-13, Paul Carroll: Created
        
*******************************************************************************/

#if !defined(LINUX_TYPES_INCLUDED_)
#define LINUX_TYPES_INCLUDED_

// TEMP PEC ++++
//    Eclipse does not know that I this is PIXELINK_LINUX, so the editor shows errors and warnings
//    for all types based on these ones.  Remove the conditional definitions to over-ride this
//    annoying behavior (should not be necessary, but no harm).
//#ifdef PIXELINK_LINUX
#include <stddef.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <errno.h>
#include <byteswap.h>

#define MAX_PATH          260
#define CW_USEDEFAULT     0x80000000 // user controllable parameters, that the user has not specified

#ifndef _TBOOLEAN_DEFINED
typedef bool BOOL;
typedef BYTE BOOLEAN;
#define _TBOOLEAN_DEFINED
#endif // BOOLEAN

typedef char CHAR;
typedef long LONG;
typedef unsigned long  DWORD;
typedef unsigned short WORD;

#ifndef _TCHAR_DEFINED
typedef char TCHAR, *PTCHAR;
typedef unsigned char TBYTE , *PTBYTE ;
#define _TCHAR_DEFINED
#endif /* !_TCHAR_DEFINED */

#ifndef ZeroMemory
#define ZeroMemory(dest,len) (memset((dest),0,(len)))
#endif //ZeroMemory
#ifndef CopyMemory
#define CopyMemory(dest,src,len) (memcpy((dest),(src),(len)))
#endif //CopyMemory

#define FIELD_OFFSET(type, field) (offsetof(type, field))

// some networking typedefs that Windows provides that Linux does not
typedef struct sockaddr_in     SOCKADDR_IN, *PSOCKADDR_IN;
typedef int                    SOCKET;
typedef struct sockaddr        SOCKADDR, *PSOCKADDR;

#define SD_RECEIVE      0x00
#define SD_SEND         0x01
#define SD_BOTH         0x02

#define INVALID_SOCKET  (SOCKET)(~0)
#define SOCKET_ERROR            (-1)

#define WSAGetLastError()        errno

#define closesocket(socketId)    close(socketId)

#define htonll(x) ((1==htonl(1)) ? (x) : ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))

// 2024-01-30 -- Additions to support compression
//       aligned memory -- provide Linux definitions for the functions names used in Windows
#ifndef _aligned_malloc
#define _aligned_malloc(size,alignment) aligned_alloc(size,alignment)
#define _aligned_free(bytePtr) free(bytePtr)
#endif
//       endian conversions                                                                 
#ifndef _byteswap_ulong
#define _byteswap_ulong(X) bswap_32(X)
#endif


//#endif // PIXELINK_LINUX

#endif // !defined(LINUX_TYPES_INCLUDED_)
