/*
 * A library for storing parameters in the registry.
 *
 * Jonathan Westhues, 2002
 */

#ifndef __FREEZE_H
#define __FREEZE_H

#ifndef FREEZE_SUBKEY
#error must define FREEZE_SUBKEY to a string uniquely identifying the app
#endif

#define FreezeWindowPos(hwnd) FreezeWindowPosF(hwnd, FREEZE_SUBKEY, #hwnd)
void FreezeWindowPosF(HWND hWnd, const char *subKey, const char *name);

#define ThawWindowPos(hwnd) ThawWindowPosF(hwnd, FREEZE_SUBKEY, #hwnd)
void ThawWindowPosF(HWND hWnd, const char *subKey, const char *name);

#define FreezeDWORD(val) FreezeDWORDF(val, FREEZE_SUBKEY, #val)
void FreezeDWORDF(DWORD val, const char *subKey, const char *name);

#define ThawDWORD(val) val = ThawDWORDF(val, FREEZE_SUBKEY, #val)
DWORD ThawDWORDF(DWORD val, const char *subKey, const char *name);

#define FreezeString(val) FreezeStringF(val, FREEZE_SUBKEY, #val)
void FreezeStringF(const char *val, const char *subKey, const char *name);

#define ThawString(val, max) ThawStringF(val, max, FREEZE_SUBKEY, #val)
void ThawStringF(char *val, int max, const char *subKey, const char *name);


#endif
