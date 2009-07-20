/*----------------------------------------------------------------------
 *  spwdata.h -- datatypes
 *
 *
 *  $Id: spwdata.h,v 1.4 1996/10/08 23:01:39 chris Exp $
 *
 *  This contains the only acceptable type definitions for 3Dconnexion
 *  products. Needs more work.
 *
 *----------------------------------------------------------------------
 *
 *  (c) 1996-2005 3Dconnexion.  All rights reserved.
 *
 *  The computer codes included in this file, including source code and
 *  object code, constitutes the proprietary and confidential information of
 *  3Dconnexion, and are provided pursuant to a license
 *  agreement.  These computer codes are protected by international, federal
 *  and state law, including United States Copyright Law and international
 *  treaty provisions.  Except as expressly authorized by the license
 *  agreement, or as expressly permitted under applicable laws of member
 *  states of the European Union and then only to the extent so permitted,
 *  no part of these computer codes may be reproduced or transmitted in any
 *  form or by any means, electronic or mechanical, modified, decompiled,
 *  disassembled, reverse engineered, sold, transferred, rented or utilized
 *  for any unauthorized purpose without the express written permission of
 *  3Dconnexion.
 *
 *----------------------------------------------------------------------
 *
 */

#ifndef SPWDATA_H
#define SPWDATA_H

static char spwdataCvsId[]="(C) 1996-2005 3Dconnexion: $Id: spwdata.h,v 1.4 1996/10/08 23:01:39 chris Exp $";

#include <tchar.h>

#define tchar_t				_TCHAR
#define char_t				char
#define uint32_t			unsigned long
#define sint32_t			long
#define boolean_t			unsigned char  
#define void_t				void
#define window_handle_t		HWND


typedef long               SPWint32;
typedef short              SPWint16;
typedef char               SPWint8;
typedef int                SPWbool;
typedef unsigned long      SPWuint32;
typedef unsigned short     SPWuint16;
typedef unsigned char      SPWuint8;
typedef _TCHAR             SPWchar;
typedef _TCHAR*            SPWstring;
typedef float              SPWfloat32;
typedef double             SPWfloat64;



#endif /* SPWDATA_H */

