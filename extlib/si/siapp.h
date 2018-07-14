/*-----------------------------------------------------------------------------
 *
 * siapp.h -- Si static library interface header file
 *
 * $Id: siapp.h,v 1.3 2001/01/16 01:18:49 HJin Exp $
 *
 * Contains function headers and type definitions for siapp.c.
 *
 *-----------------------------------------------------------------------------
 *
 * (c) 1998-2005 3Dconnexion. All rights reserved. 
 * Permission to use, copy, modify, and distribute this software for all
 * purposes and without fees is hereby grated provided that this copyright
 * notice appears in all copies.  Permission to modify this software is granted
 * and 3Dconnexion will support such modifications only if said modifications are
 * approved by 3Dconnexion.
 *
 */

#ifndef SIAPP_H
#define SIAPP_H


static char SiAppCvsId[]="(c) 1998-2005 3Dconnexion: $Id: siapp.h,v 1.3 2001/01/16 01:18:49 HJin Exp $";

#ifdef __cplusplus
extern "C" {
#endif


/* some enumerated types used in siapp.c */

enum InitResult 
   {
   NOT_LOADED, 
   FAILED, 
   LOADED
   };

enum ErrorCode 
   {
   NO_DLL_ERROR=0,
   DLL_LOAD_FAILURE,
   DLL_FUNCTION_LOAD_FAILURE,
   DLL_VAR_LOAD_FAILURE
   };


/* externally used functions */

enum SpwRetVal SiInitialize(void);
void           SiTerminate(void);
int            SiGetNumDevices (void);
SiDevID        SiDeviceIndex (int idx);
int            SiDispatch (SiHdl hdl, SiGetEventData *pData,
                           SiSpwEvent *pEvent, SiSpwHandlers *pDHandlers);
void           SiOpenWinInit (SiOpenData *pData, HWND hWnd);
SiHdl          SiOpen (char *pAppName, SiDevID devID, SiTypeMask *pTMask, int mode, 
                       SiOpenData *pData);
enum SpwRetVal SiClose (SiHdl hdl);
void           SiGetEventWinInit (SiGetEventData *pData,
                                  UINT msg, WPARAM wParam, LPARAM lParam);
enum SpwRetVal SiGetEvent (SiHdl hdl, int flags, SiGetEventData *pData, 
                           SiSpwEvent *pEvent);
enum SpwRetVal SiBeep (SiHdl hdl, char *string);
enum SpwRetVal SiRezero (SiHdl hdl);
enum SpwRetVal SiGrabDevice (SiHdl hdl, SPWbool exclusive);
enum SpwRetVal SiReleaseDevice (SiHdl hdl);
int            SiButtonPressed (SiSpwEvent *pEvent);
int            SiButtonReleased (SiSpwEvent *pEvent);
enum SpwRetVal SiSetUiMode (SiHdl hdl, SPWuint32 mode);
enum SpwRetVal SiSetTypeMask (SiTypeMask *pTMask, int type1, ...);
enum           SpwRetVal SiGetDevicePort (SiDevID devID, SiDevPort *pPort);
enum           SpwRetVal SiGetDriverInfo (SiVerInfo *pInfo);
void           SiGetLibraryInfo (SiVerInfo *pInfo);
enum           SpwRetVal SiGetDeviceInfo (SiHdl hdl, SiDevInfo *pInfo);
char * SpwErrorString (enum SpwRetVal val);
enum SpwRetVal SiSyncSendQuery(SiHdl hdl);
enum SpwRetVal SiSyncGetVersion(SiHdl hdl, SPWuint32 *pmajor, SPWuint32 *pminor);
enum SpwRetVal SiSyncGetNumberOfFunctions(SiHdl hdl, SPWuint32 *pnumberOfFunctions);
enum SpwRetVal SiSyncGetFunction(SiHdl hdl, SPWuint32 index, SPWint32 *pabsoluteFunctionNumber, WCHAR name[], SPWuint32 *pmaxNameLen);
enum SpwRetVal SiSyncGetButtonAssignment(SiHdl hdl, SPWuint32 buttonNumber, SPWint32 *passignedFunctionIndex);
enum SpwRetVal SiSyncSetButtonAssignment(SiHdl hdl, SPWuint32 buttonNumber, SPWint32 functionIndex);
enum SpwRetVal SiSyncSetButtonAssignmentAbsolute(SiHdl hdl, SPWuint32 buttonNumber, SPWint32 absoluteFunctionNumber );
enum SpwRetVal SiSyncSetButtonName(SiHdl hdl, SPWuint32 buttonNumber, WCHAR name[]);
enum SpwRetVal SiSyncGetAxisLabel (SiHdl hdl, SPWuint32 axisNumber, WCHAR name[], SPWuint32 *pmaxNameLen );
enum SpwRetVal SiSyncSetAxisLabel (SiHdl hdl, SPWuint32 axisNumber, WCHAR name[] );
enum SpwRetVal SiSyncGetOrientation (SiHdl hdl, SPWint32 axes[6] );
enum SpwRetVal SiSyncSetOrientation (SiHdl hdl, SPWint32 axes[6] );
enum SpwRetVal SiSyncGetFilter (SiHdl hdl, SiSyncFilter i, SiSyncFilterValue *pv );
enum SpwRetVal SiSyncSetFilter (SiHdl hdl, SiSyncFilter i, SiSyncFilterValue v );
enum SpwRetVal SiSyncGetAxesState (SiHdl hdl, SiSyncAxesState *pa );
enum SpwRetVal SiSyncSetAxesState (SiHdl hdl, SiSyncAxesState a );
enum SpwRetVal SiSyncSetInfoLine (SiHdl hdl, SPWint32 duration, WCHAR text[] );
enum SpwRetVal SiSyncGetScaleOverall (SiHdl hdl, SPWfloat32 *pv );
enum SpwRetVal SiSyncSetScaleOverall (SiHdl hdl, SPWfloat32 v );
enum SpwRetVal SiSyncGetScaleTx (SiHdl hdl, SPWfloat32 *pv );
enum SpwRetVal SiSyncSetScaleTx (SiHdl hdl, SPWfloat32 v );
enum SpwRetVal SiSyncGetScaleTy (SiHdl hdl, SPWfloat32 *pv );
enum SpwRetVal SiSyncSetScaleTy (SiHdl hdl, SPWfloat32 v );
enum SpwRetVal SiSyncGetScaleTz (SiHdl hdl, SPWfloat32 *pv );
enum SpwRetVal SiSyncSetScaleTz (SiHdl hdl, SPWfloat32 v );
enum SpwRetVal SiSyncGetScaleRx (SiHdl hdl, SPWfloat32 *pv );
enum SpwRetVal SiSyncSetScaleRx (SiHdl hdl, SPWfloat32 v );
enum SpwRetVal SiSyncGetScaleRy (SiHdl hdl, SPWfloat32 *pv );
enum SpwRetVal SiSyncSetScaleRy (SiHdl hdl, SPWfloat32 v );
enum SpwRetVal SiSyncGetScaleRz (SiHdl hdl, SPWfloat32 *pv );
enum SpwRetVal SiSyncSetScaleRz (SiHdl hdl, SPWfloat32 v );
enum SpwRetVal SiSyncInvokeAbsoluteFunction (SiHdl hdl, SiSyncAbsFunctionNumber i );
enum SpwRetVal SiSyncSetButtonState (SiHdl hdl, SPWuint32 buttonNumber, SiSyncButtonState state );
enum SpwRetVal SiGetButtonName (SiHdl hdl, SPWuint32 buttonNumber, SiButtonName *pname);
enum SpwRetVal SiGetDeviceName (SiHdl hdl, SiDeviceName *pname);
enum SpwRetVal SiGetDeviceImageFileName (SiHdl hdl, char name[], SPWuint32 *pmaxNameLen);
HICON SiGetCompanyIcon(void);
enum SpwRetVal SiGetCompanyLogoFileName (char name[], SPWuint32 *pmaxNameLen);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef SIAPP_H */
