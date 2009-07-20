/*----------------------------------------------------------------------
 *  siSync.h -- 3DxWare GUI Synchronization header
 *
 * Written:     September 2004
 * Author:      Jim Wick
 *
 *----------------------------------------------------------------------
 *
 * (c) 1998-2005 3Dconnexion. All rights reserved. 
 * Permission to use, copy, modify, and distribute this software for all
 * purposes and without fees is hereby grated provided that this copyright
 * notice appears in all copies.  Permission to modify this software is granted
 * and 3Dconnexion will support such modifications only is said modifications are
 * approved by 3Dconnexion.
 *
 */


#ifndef _SISYNC_H_
#define _SISYNC_H_

#ifdef __cplusplus
extern "C" {
#endif


/*
 * Constants
 */
#define SI_SYNC_PACKET_ID 27711
#define SI_SYNC_VERSION_MAJOR 1
#define SI_SYNC_VERSION_MINOR 0


/*
 * Absolute Internal Function Numbers
 * These are function numbers that will never change.
 * For use with Set BUTTON_ASSIGNMENT_ABSOLUTE packets, and some INVOKE items.
 * Some functions (keys) can not be INVOKED because there is a separate
 * press and release and that difference is not exposed.
 */
typedef enum
{
   SI_SYNC_FUNCTION_MENU_TOGGLE		= 12,
   SI_SYNC_FUNCTION_TRANS_TOGGLE	= 13,
   SI_SYNC_FUNCTION_ROT_TOGGLE		= 14,
   SI_SYNC_FUNCTION_HPV_TOGGLE		= 15,
   SI_SYNC_FUNCTION_DEC_SENS		= 16,
   SI_SYNC_FUNCTION_INC_SENS		= 17,
   SI_SYNC_FUNCTION_RESTORE_DEF		= 18,
   SI_SYNC_FUNCTION_PAN				= 19,
   SI_SYNC_FUNCTION_ZOOM			= 20,
   SI_SYNC_FUNCTION_TX				= 21,
   SI_SYNC_FUNCTION_TY				= 22,
   SI_SYNC_FUNCTION_TZ				= 23,
   SI_SYNC_FUNCTION_RX				= 24,
   SI_SYNC_FUNCTION_RY				= 25,
   SI_SYNC_FUNCTION_RZ				= 26,
   SI_SYNC_FUNCTION_REZERO_DEVICE	= 27,
   SI_SYNC_FUNCTION_SAVE			= 33,
   SI_SYNC_FUNCTION_RELOAD			= 57,
   SI_SYNC_FUNCTION_SHIFT_KEY		= 60,
   SI_SYNC_FUNCTION_CTRL_KEY		= 61,
   SI_SYNC_FUNCTION_ALT_KEY			= 62,
   SI_SYNC_FUNCTION_RESTORE_SENS	= 63,
   SI_SYNC_FUNCTION_SPACE_KEY		= 64,
   SI_SYNC_FUNCTION_CTRL_SHIFT_KEY	= 65,
   SI_SYNC_FUNCTION_CTRL_ALT_KEY	= 66,
   SI_SYNC_FUNCTION_SHIFT_ALT_KEY	= 67,
   SI_SYNC_FUNCTION_TAB_KEY			= 68,
   SI_SYNC_FUNCTION_RETURN_KEY		= 69,
   SI_SYNC_FUNCTION_DEC_TRANS_SENS	= 70,
   SI_SYNC_FUNCTION_INC_TRANS_SENS	= 71,
   SI_SYNC_FUNCTION_DEC_ROT_SENS	= 72,
   SI_SYNC_FUNCTION_INC_ROT_SENS	= 73,
   SI_SYNC_FUNCTION_DEC_PAN_SENS	= 74,
   SI_SYNC_FUNCTION_INC_PAN_SENS	= 75,
   SI_SYNC_FUNCTION_DEC_ZOOM_SENS	= 76,
   SI_SYNC_FUNCTION_INC_ZOOM_SENS	= 77,
   SI_SYNC_FUNCTION_ESC_KEY			= 78,
   SI_SYNC_FUNCTION_3DX_HELP        = 94,
   SI_SYNC_FUNCTION_APP_HELP        = 95,
   SI_SYNC_FUNCTION_DIALOG_TOGGLE_FN= 96,
   SI_SYNC_FUNCTION_FIT_FN          = 97
} SiSyncAbsFunctionNumber;


/*
 * Sync Op Codes
 */
typedef enum
   {
   SI_SYNC_OP_COMMAND	= 1,
   SI_SYNC_OP_GET		= 2,
   SI_SYNC_OP_SET		= 3
   } SiSyncOpCode;

/*
 * Sync Item Codes
 */
typedef enum
{
   SI_SYNC_ITEM_VERSION						= 1,
   SI_SYNC_ITEM_QUERY						= 2,
   SI_SYNC_ITEM_SAVE_CONFIG					= 3,
   SI_SYNC_ITEM_NUMBER_OF_FUNCTIONS			= 4,
   SI_SYNC_ITEM_FUNCTION					= 5,
   SI_SYNC_ITEM_BUTTON_ASSIGNMENT			= 6,
   SI_SYNC_ITEM_BUTTON_ASSIGNMENT_ABSOLUTE	= 7,
   SI_SYNC_ITEM_BUTTON_NAME					= 8,
   SI_SYNC_ITEM_AXIS_LABEL					= 9,
   SI_SYNC_ITEM_ORIENTATION					= 10,
   SI_SYNC_ITEM_FILTER						= 11,
   SI_SYNC_ITEM_AXES_STATE					= 12,
   SI_SYNC_ITEM_INFO_LINE					= 13,
   SI_SYNC_ITEM_SCALE_OVERALL				= 14,
   SI_SYNC_ITEM_SCALE_TX					= 15,
   SI_SYNC_ITEM_SCALE_TY					= 16,
   SI_SYNC_ITEM_SCALE_TZ					= 17,
   SI_SYNC_ITEM_SCALE_RX					= 18,
   SI_SYNC_ITEM_SCALE_RY					= 19,
   SI_SYNC_ITEM_SCALE_RZ					= 20,
   SI_SYNC_ITEM_INVOKE_ABSOLUTE_FUNCTION	= 21,
   SI_SYNC_ITEM_BUTTON_STATE				= 22
} SiSyncItemCode;

/*
 * Filters
 */
 typedef enum
 {
   SI_SYNC_FILTER_TRANSLATIONS		= 1,
   SI_SYNC_FILTER_ROTATIONS			= 2,
   SI_SYNC_FILTER_DOMINANT			= 3
 } SiSyncFilter;

 typedef enum
 {
   SI_SYNC_FILTER_OFF		        = 0,
   SI_SYNC_FILTER_ON			    = 1,
   SI_SYNC_FILTER_IN_BETWEEN		= 2
 } SiSyncFilterValue;

/*
 * Axes State
 */
typedef enum
{
	SI_SYNC_AXES_STATE_TX = (1<<0),
	SI_SYNC_AXES_STATE_TY = (1<<1),
	SI_SYNC_AXES_STATE_TZ = (1<<2),
	SI_SYNC_AXES_STATE_RX = (1<<3),
	SI_SYNC_AXES_STATE_RY = (1<<4),
	SI_SYNC_AXES_STATE_RZ = (1<<5)
} SiSyncAxesStateBits;

typedef struct
{
	int state; /* VURZYX (Tx = LSB (& 1<<0) */
} SiSyncAxesState;

/*
 * Button State
 * For indicating the state of whatever the button sets (in the LCD at this point).
 * E.g., to show that Translations are currently OFF for the Translations Toggle button.
 * OFF: reverse video, flag is not set
 * ON: normal video, flag is set
 * DISABLED: (greyed), status of flag is invalid at this time
 */
typedef enum
{
	SI_SYNC_BUTTON_STATE_OFF      =  0,
	SI_SYNC_BUTTON_STATE_ON       =  1,
	SI_SYNC_BUTTON_STATE_DISABLED =  2,
} SiSyncButtonState;


/*
 * Private / implementation structures
 * 
 * We suggest you leave these hidden and use the accessor functions rather than
 * directly accessing the structures.
 */
#include "siSyncPriv.h"


/*
 * Accessor Function headers 
 */
SPWuint32 SiSyncGetSize(SiSyncPacket p);
void      SiSyncSetSize(SiSyncPacket *p, SPWuint32 size);

SPWuint32 SiSyncGetHashCode(SiSyncPacket p);
void      SiSyncSetHashCode(SiSyncPacket *p, SPWuint32 hashCode);

SiSyncOpCode SiSyncGetOpCode(SiSyncPacket p);
void         SiSyncSetOpCode(SiSyncPacket *p, SPWuint32 opCode);

SiSyncItemCode SiSyncGetItemCode(SiSyncPacket p);
void           SiSyncSetItemCode(SiSyncPacket *p, SPWuint32 itemCode);

#ifdef __cplusplus
}
#endif

#endif   /* _SI_SYNC_H_ */
