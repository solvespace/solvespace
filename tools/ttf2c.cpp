#include <windows.h>
#include <stdio.h>
#include <commctrl.h>

//-----------------------------------------------------------------------------
// Entry point into the program.
//-----------------------------------------------------------------------------
int main(void)
{
    InitCommonControls();

    // A monospaced font
    HFONT font = CreateFont(16, 9, 0, 0, FW_REGULAR, FALSE,
        FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FF_DONTCARE, "Lucida Console");

    HDC hdc = CreateDC("DISPLAY", NULL, NULL, NULL);
    HBITMAP bitmap = CreateCompatibleBitmap(hdc, 30, 30);

    SelectObject(hdc, bitmap);
    SelectObject(hdc, font);

    printf("static const BYTE FontTexture[256*16*16] = {\n");

    int c;
    for(c = 0; c < 128; c++) {

        RECT r;
        r.left = 0; r.top = 0;
        r.right = 30; r.bottom = 30;
        FillRect(hdc, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));

        SetBkColor(hdc, RGB(0, 0, 0));
        SetTextColor(hdc, RGB(255, 255, 255));
        char str[2] = { c, 0 };
        TextOut(hdc, 0, 0, str, 1);
        
        int i, j;
        for(i = 0; i < 16; i++) {
            for(j = 0; j < 16; j++) {
                COLORREF c = GetPixel(hdc, i, j);
                printf("%3d, ", c ? 255 : 0);
            }
            printf("\n");
        }
        printf("\n");
    }
    printf("#include \"bitmapextra.table\"\n");
    printf("};\n");

    return 0;
}
