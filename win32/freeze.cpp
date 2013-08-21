/*
 * A library for storing parameters in the registry.
 *
 * Jonathan Westhues, 2002
 */
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

/*
 * store a window's position in the registry, or fail silently if the registry calls don't work
 */
void FreezeWindowPosF(HWND hwnd, char *subKey, char *name)
{
    RECT r;
    GetWindowRect(hwnd, &r);

    HKEY software;
    if(RegOpenKeyEx(HKEY_CURRENT_USER, "Software", 0, KEY_ALL_ACCESS, &software) != ERROR_SUCCESS)
        return;

    char *keyName = (char *)malloc(strlen(name) + 30);
    if(!keyName)
        return;

    HKEY sub;
    if(RegCreateKeyEx(software, subKey, 0, "", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &sub, NULL) != ERROR_SUCCESS)
        return;

    sprintf(keyName, "%s_left", name);
    if(RegSetValueEx(sub, keyName, 0, REG_DWORD, (BYTE *)&(r.left), sizeof(DWORD)) != ERROR_SUCCESS)
        return;

    sprintf(keyName, "%s_right", name);
    if(RegSetValueEx(sub, keyName, 0, REG_DWORD, (BYTE *)&(r.right), sizeof(DWORD)) != ERROR_SUCCESS)
        return;

    sprintf(keyName, "%s_top", name);
    if(RegSetValueEx(sub, keyName, 0, REG_DWORD, (BYTE *)&(r.top), sizeof(DWORD)) != ERROR_SUCCESS)
        return;

    sprintf(keyName, "%s_bottom", name);
    if(RegSetValueEx(sub, keyName, 0, REG_DWORD, (BYTE *)&(r.bottom), sizeof(DWORD)) != ERROR_SUCCESS)
        return;

    sprintf(keyName, "%s_maximized", name);
    DWORD v = IsZoomed(hwnd);
    if(RegSetValueEx(sub, keyName, 0, REG_DWORD, (BYTE *)&(v), sizeof(DWORD)) != ERROR_SUCCESS)
        return;

    free(keyName);
}

static void Clamp(LONG *v, LONG min, LONG max)
{
    if(*v < min) *v = min;
    if(*v > max) *v = max;
}

/*
 * retrieve a window's position from the registry, or do nothing if there is no info saved
 */
void ThawWindowPosF(HWND hwnd, char *subKey, char *name)
{
    HKEY software;
    if(RegOpenKeyEx(HKEY_CURRENT_USER, "Software", 0, KEY_ALL_ACCESS, &software) != ERROR_SUCCESS)
        return;

    HKEY sub;
    if(RegOpenKeyEx(software, subKey, 0, KEY_ALL_ACCESS, &sub) != ERROR_SUCCESS)
        return;

    char *keyName = (char *)malloc(strlen(name) + 30);
    if(!keyName)
        return;

    DWORD l;
    RECT  r;

    sprintf(keyName, "%s_left", name);
    l = sizeof(DWORD);
    if(RegQueryValueEx(sub, keyName, NULL, NULL, (BYTE *)&(r.left), &l) != ERROR_SUCCESS)
        return;

    sprintf(keyName, "%s_right", name);
    l = sizeof(DWORD);
    if(RegQueryValueEx(sub, keyName, NULL, NULL, (BYTE *)&(r.right), &l) != ERROR_SUCCESS)
        return;

    sprintf(keyName, "%s_top", name);
    l = sizeof(DWORD);
    if(RegQueryValueEx(sub, keyName, NULL, NULL, (BYTE *)&(r.top), &l) != ERROR_SUCCESS)
        return;

    sprintf(keyName, "%s_bottom", name);
    l = sizeof(DWORD);
    if(RegQueryValueEx(sub, keyName, NULL, NULL, (BYTE *)&(r.bottom), &l) != ERROR_SUCCESS)
        return;

    sprintf(keyName, "%s_maximized", name);
    DWORD v;
    l = sizeof(DWORD);
    if(RegQueryValueEx(sub, keyName, NULL, NULL, (BYTE *)&v, &l) != ERROR_SUCCESS)
        return;
    if(v)
        ShowWindow(hwnd, SW_MAXIMIZE);

    RECT dr;
    GetWindowRect(GetDesktopWindow(), &dr);

    // If it somehow ended up off-screen, then put it back.
    Clamp(&(r.left),   dr.left, dr.right);
    Clamp(&(r.right),  dr.left, dr.right);
    Clamp(&(r.top),    dr.top,  dr.bottom);
    Clamp(&(r.bottom), dr.top,  dr.bottom);
    if(r.right - r.left < 100) {
        r.left -= 300; r.right += 300;
    }
    if(r.bottom - r.top < 100) {
        r.top -= 300; r.bottom += 300;
    }
    Clamp(&(r.left),   dr.left, dr.right);
    Clamp(&(r.right),  dr.left, dr.right);
    Clamp(&(r.top),    dr.top,  dr.bottom);
    Clamp(&(r.bottom), dr.top,  dr.bottom);

    MoveWindow(hwnd, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);

    free(keyName);
}

/*
 * store a DWORD setting in the registry
 */
void FreezeDWORDF(DWORD val, char *subKey, char *name)
{
    HKEY software;
    if(RegOpenKeyEx(HKEY_CURRENT_USER, "Software", 0, KEY_ALL_ACCESS, &software) != ERROR_SUCCESS)
        return;

    HKEY sub;
    if(RegCreateKeyEx(software, subKey, 0, "", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &sub, NULL) != ERROR_SUCCESS)
        return;
  
    if(RegSetValueEx(sub, name, 0, REG_DWORD, (BYTE *)&val, sizeof(DWORD)) != ERROR_SUCCESS)
        return;
}

/*
 * retrieve a DWORD setting, or return the default if that setting is unavailable
 */
DWORD ThawDWORDF(DWORD val, char *subKey, char *name)
{
    HKEY software;
    if(RegOpenKeyEx(HKEY_CURRENT_USER, "Software", 0, KEY_ALL_ACCESS, &software) != ERROR_SUCCESS)
        return val;

    HKEY sub;
    if(RegOpenKeyEx(software, subKey, 0, KEY_ALL_ACCESS, &sub) != ERROR_SUCCESS)
        return val;

    DWORD l = sizeof(DWORD);
    DWORD v;
    if(RegQueryValueEx(sub, name, NULL, NULL, (BYTE *)&v, &l) != ERROR_SUCCESS)
        return val;

    return v;
}

/*
 * store a string setting in the registry
 */
void FreezeStringF(char *val, char *subKey, char *name)
{
    HKEY software;
    if(RegOpenKeyEx(HKEY_CURRENT_USER, "Software", 0, KEY_ALL_ACCESS, &software) != ERROR_SUCCESS)
        return;

    HKEY sub;
    if(RegCreateKeyEx(software, subKey, 0, "", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &sub, NULL) != ERROR_SUCCESS)
        return;
  
    if(RegSetValueEx(sub, name, 0, REG_SZ, (BYTE *)val, strlen(val)+1) != ERROR_SUCCESS)
        return;
}

/*
 * retrieve a string setting, or return the default if that setting is unavailable
 */
void ThawStringF(char *val, int max, char *subKey, char *name)
{
    HKEY software;
    if(RegOpenKeyEx(HKEY_CURRENT_USER, "Software", 0, KEY_ALL_ACCESS, &software) != ERROR_SUCCESS)
        return;

    HKEY sub;
    if(RegOpenKeyEx(software, subKey, 0, KEY_ALL_ACCESS, &sub) != ERROR_SUCCESS)
        return;

    DWORD l = max;
    if(RegQueryValueEx(sub, name, NULL, NULL, (BYTE *)val, &l) != ERROR_SUCCESS)
        return;
    if(l >= (DWORD)max) return;

    val[l] = '\0';
    return;
}

