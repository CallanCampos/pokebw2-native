#include "types.h"
#include "constants/sound.h"
#include "gfl/bg_sys.h"
#include "gfl/bmp.h"
#include "gfl/bmpwin.h"
#include "gfl/heap.h"
#include "gfl/key.h"
#include "gfl/sound.h"
#include "gfl/ui.h"
#include "system/bmp_cursor.h"
#include "system/bmp_menulist.h"
#include "system/bmp_menuwork.h"
#include "system/printsys.h"

// Scrolling lists of options in a window, with a cursor

struct BmpMenuList {
    BmpMenuListHeader header;
    // Colors that BmpMenuList_SetOverrideColors sets, used instead of the header's while enabled
    struct {
        u8 fgColor : 4;
        u8 bgColor : 4;
        u8 shadowColor : 4;
        u8 letterSpacing : 6;
        u8 unk37 : 6;
        u8 fontId : 7;
        u8 enabled : 1;
    } override;
    BmpCursor *cursor;
    // The option in the first row shown, and the cursor's row in the window
    u16 listTop;
    u16 cursorRow;
    u8 unk44;
    u8 unk45;
    u8 unk46;
    // The direction the cursor last moved, 1 to 4 for up, down, a page up and a page down, or 0
    u8 moveDir;
    HeapID heapId;
    u8 cancelDisabled;
    // Frames until the cursor is drawn
    u8 wait;
    u32 unk4C;
};

static void BmpMenuList_PrintOption(BmpMenuList *list, const StrBuf *text, u8 x, u8 y, BOOL useQueue);
static void BmpMenuList_PrintOptions(BmpMenuList *list, u16 index, u16 row, u16 count, BOOL withPrevious);
static void BmpMenuList_DrawCursor(BmpMenuList *list);
static void BmpMenuList_EraseCursor(BmpMenuList *list, u16 row);
static u8 BmpMenuList_Scroll(BmpMenuList *list, BOOL movingDown);
static void BmpMenuList_ScrollOptions(BmpMenuList *list, u8 count, BOOL movingDown);
static BOOL BmpMenuList_CycleCursor(BmpMenuList *list, u8 redraw, u8 count, u8 movingDown);
static void BmpMenuList_CallCursorCallback(BmpMenuList *list, u8 init);
static void Bitmap_Scroll16(GFLBitmap *bitmap, u8 dir, u8 dist, u8 fill, u16 size);
static void Bitmap_Scroll256(GFLBitmap *bitmap, u8 dir, u8 dist, u8 fill, u16 size);
static void BmpWin_ScrollChars(BmpWin *window, u8 dir, u8 dist, u8 fill, u16 tiles);

// The y of a row of the window
static inline u16 RowY(BmpMenuList *list, u16 row) {
    return list->header.y + (u8)(list->header.fontSizeY + list->header.lineSpacing) * row;
}

// The directions Bitmap_Scroll16 and Bitmap_Scroll256 scroll in
enum {
    SCROLL_UP,
    SCROLL_DOWN,
    SCROLL_LEFT,
    SCROLL_RIGHT,
};

BmpMenuList *BmpMenuList_Create(const BmpMenuListHeader *header, u16 listTop, u16 cursorRow, HeapID heapId) {
    BmpMenuList *list = GFL_HeapAllocate(heapId, sizeof(BmpMenuList), TRUE, "bmp_menulist.c", 107);

    list->header = *header;
    list->cursor = BmpCursor_Create(heapId);
    list->listTop = listTop;
    list->cursorRow = cursorRow;
    list->unk44 = 0;
    list->unk45 = 0;
    list->unk46 = 0xff;
    list->moveDir = 0;
    list->heapId = heapId;
    if (list->header.wait == 0) {
        list->wait = 2;
    } else {
        list->wait = list->header.wait;
        if (GCTX_HIDGetUpdateRate() == 30) {
            list->wait /= 2;
        }
    }
    list->override.fgColor = list->header.fgColor;
    list->override.bgColor = list->header.bgColor;
    list->override.shadowColor = list->header.shadowColor;
    list->override.letterSpacing = list->header.letterSpacing;
    list->override.fontId = list->header.fontId;
    list->override.enabled = FALSE;
    if (list->header.count < list->header.maxShown) {
        list->header.maxShown = list->header.count;
    }
    while (list->header.options[list->listTop + list->cursorRow].value == BMPMENULIST_LABEL) {
        list->cursorRow++;
    }
    GFL_BitmapFill(BmpWin_GetBitmap(list->header.printWindow->window), list->header.bgColor);
    BmpMenuList_PrintOptions(list, list->listTop, 0, list->header.maxShown, TRUE);
    BmpMenuList_CallCursorCallback(list, TRUE);
    return list;
}

s32 BmpMenuList_Update(BmpMenuList *list) {
    u32 keys = GCTX_HIDGetPressedKeys();
    u32 repeat = GCTX_HIDGetTypedKeys();
    u16 pageUp, pageDown;

    list->moveDir = 0;
    if (list->wait != 0) {
        list->wait--;
        if (list->wait == 0) {
            BmpMenuList_DrawCursor(list);
        }
        return BMPMENULIST_NULL;
    }
    if (keys & PAD_BUTTON_A) {
        GFL_SndSEPlay(SEQ_SE_DECIDE1);
        return list->header.options[list->listTop + list->cursorRow].value;
    }
    if (list->cancelDisabled == FALSE && (keys & PAD_BUTTON_B)) {
        GFL_SndSEPlay(SEQ_SE_CANCEL1);
        return BMPMENULIST_CANCEL;
    }
    if (repeat & PAD_KEY_UP) {
        if (BmpMenuList_CycleCursor(list, TRUE, 1, FALSE) == FALSE) {
            GFL_SndSEPlay(SEQ_SE_SELECT1);
            list->moveDir = 1;
        }
        return BMPMENULIST_NULL;
    }
    if (repeat & PAD_KEY_DOWN) {
        if (BmpMenuList_CycleCursor(list, TRUE, 1, TRUE) == FALSE) {
            GFL_SndSEPlay(SEQ_SE_SELECT1);
            list->moveDir = 2;
        }
        return BMPMENULIST_NULL;
    }
    switch (list->header.pageSkip) {
    case BMPMENULIST_SKIP_NONE:
    default:
        pageUp = 0;
        pageDown = 0;
        break;
    case BMPMENULIST_SKIP_LR_KEY:
        pageUp = repeat & PAD_KEY_LEFT;
        pageDown = repeat & PAD_KEY_RIGHT;
        break;
    case BMPMENULIST_SKIP_LR_BUTTON:
        pageUp = repeat & PAD_BUTTON_L;
        pageDown = repeat & PAD_BUTTON_R;
        break;
    }
    if (pageUp) {
        if (BmpMenuList_CycleCursor(list, TRUE, list->header.maxShown, FALSE) == FALSE) {
            GFL_SndSEPlay(SEQ_SE_SELECT1);
            list->moveDir = 3;
        }
        return BMPMENULIST_NULL;
    }
    if (pageDown) {
        if (BmpMenuList_CycleCursor(list, TRUE, list->header.maxShown, TRUE) == FALSE) {
            GFL_SndSEPlay(SEQ_SE_SELECT1);
            list->moveDir = 4;
        }
        return BMPMENULIST_NULL;
    }
    return BMPMENULIST_NULL;
}

void BmpMenuList_Free(BmpMenuList *list, u16 *listTop, u16 *cursorRow) {
    if (listTop != NULL) {
        *listTop = list->listTop;
    }
    if (cursorRow != NULL) {
        *cursorRow = list->cursorRow;
    }
    BmpCursor_Free(list->cursor);
    GFL_HeapFree(list);
}

void BmpMenuList_Redraw(BmpMenuList *list) {
    GFL_BitmapFill(BmpWin_GetBitmap(list->header.printWindow->window), list->header.bgColor);
    BmpMenuList_PrintOptions(list, list->listTop, 0, list->header.maxShown, TRUE);
    BmpMenuList_DrawCursor(list);
}

void BmpMenuList_SetColors(BmpMenuList *list, u8 fgColor, u8 bgColor, u8 shadowColor) {
    list->header.fgColor = fgColor;
    list->header.bgColor = bgColor;
    list->header.shadowColor = shadowColor;
}

void BmpMenuList_SetOverrideColors(BmpMenuList *list, u8 fgColor, u8 bgColor, u8 shadowColor) {
    list->override.fgColor = fgColor;
    list->override.bgColor = bgColor;
    list->override.shadowColor = shadowColor;
    list->override.enabled = TRUE;
}

void BmpMenuList_GetCursorIndex(BmpMenuList *list, u16 *index) {
    *index = list->listTop + list->cursorRow;
}

void BmpMenuList_GetPos(BmpMenuList *list, u16 *listTop, u16 *cursorRow) {
    if (listTop != NULL) {
        *listTop = list->listTop;
    }
    if (cursorRow != NULL) {
        *cursorRow = list->cursorRow;
    }
}

u16 BmpMenuList_GetCursorY(BmpMenuList *list) {
    return BmpMenuList_GetRowY(list, list->cursorRow);
}

u16 BmpMenuList_GetRowY(BmpMenuList *list, u16 row) {
    return RowY(list, row);
}

s32 BmpMenuList_GetOptionValue(BmpMenuList *list, u16 index) {
    return list->header.options[index].value;
}

sptr BmpMenuList_GetParam(BmpMenuList *list, u8 param) {
    switch (param) {
    case BMPMENULIST_PARAM_CURSOR_CALLBACK:
        return (sptr)list->header.cursorCallback;
    case BMPMENULIST_PARAM_PRINT_CALLBACK:
        return (sptr)list->header.printCallback;
    case BMPMENULIST_PARAM_COUNT:
        return list->header.count;
    case BMPMENULIST_PARAM_MAX_SHOWN:
        return list->header.maxShown;
    case BMPMENULIST_PARAM_4:
        break;
    case BMPMENULIST_PARAM_LABEL_X:
        return list->header.labelX;
    case BMPMENULIST_PARAM_ITEM_X:
        return list->header.itemX;
    case BMPMENULIST_PARAM_CURSOR_X:
        return list->header.cursorX;
    case BMPMENULIST_PARAM_Y:
        return list->header.y;
    case BMPMENULIST_PARAM_ROW_HEIGHT:
        return list->header.fontSizeY + list->header.lineSpacing;
    case BMPMENULIST_PARAM_FG_COLOR:
        return list->header.fgColor;
    case BMPMENULIST_PARAM_BG_COLOR:
        return list->header.bgColor;
    case BMPMENULIST_PARAM_SHADOW_COLOR:
        return list->header.shadowColor;
    case BMPMENULIST_PARAM_LETTER_SPACING:
        return list->header.letterSpacing;
    case BMPMENULIST_PARAM_LINE_SPACING:
        return list->header.lineSpacing;
    case BMPMENULIST_PARAM_PAGE_SKIP:
        return list->header.pageSkip;
    case BMPMENULIST_PARAM_FONT_ID:
        return list->header.fontId;
    case BMPMENULIST_PARAM_CURSOR_DISPLAY:
        return list->header.cursorDisplay;
    case BMPMENULIST_PARAM_18:
        break;
    case BMPMENULIST_PARAM_WORK:
        return (sptr)list->header.work;
    }
    return -1;
}

static void BmpMenuList_PrintOption(BmpMenuList *list, const StrBuf *text, u8 x, u8 y, BOOL useQueue) {
    if (text == NULL) {
        return;
    }
    if (list->override.enabled) {
        if (useQueue) {
            PrintWindow_Print(list->header.printWindow, list->header.queue, x, y, text, list->header.font,
                              PRINT_COLOR(list->override.fgColor, list->override.shadowColor, list->override.bgColor));
        } else {
            GFL_TextRendererDrawToBitmapEx(
                BmpWin_GetBitmap(list->header.printWindow->window), x, y, text, list->header.font,
                PRINT_COLOR(list->override.fgColor, list->override.shadowColor, list->override.bgColor));
        }
    } else {
        if (useQueue) {
            PrintWindow_Print(list->header.printWindow, list->header.queue, x, y, text, list->header.font,
                              PRINT_COLOR(list->header.fgColor, list->header.shadowColor, list->header.bgColor));
        } else {
            // BUG: Printing without the queue uses the override's colors, which still hold the header's colors as
            // BmpMenuList_Create copied them, so it ignores any later BmpMenuList_SetColors
#ifdef BUGFIX
            GFL_TextRendererDrawToBitmapEx(
                BmpWin_GetBitmap(list->header.printWindow->window), x, y, text, list->header.font,
                PRINT_COLOR(list->header.fgColor, list->header.shadowColor, list->header.bgColor));
#else
            GFL_TextRendererDrawToBitmapEx(
                BmpWin_GetBitmap(list->header.printWindow->window), x, y, text, list->header.font,
                PRINT_COLOR(list->override.fgColor, list->override.shadowColor, list->override.bgColor));
#endif
        }
    }
}

// Prints count options and one more from index at row and down, stopping at the last option. withPrevious prints the
// option before index above the first row too, scrolling it up out of sight
static void BmpMenuList_PrintOptions(BmpMenuList *list, u16 index, u16 row, u16 count, BOOL withPrevious) {
    u8 rowHeight = list->header.fontSizeY + list->header.lineSpacing;
    s32 i;
    u8 x, y;

    if (index != 0 && withPrevious) {
        GFLBitmap *bitmap;
        u8 fill;

        if (list->header.options[index - 1].value != BMPMENULIST_LABEL) {
            x = list->header.itemX;
        } else {
            x = list->header.labelX;
        }
        bitmap = BmpWin_GetBitmap(list->header.printWindow->window);
        GFL_BitmapFillArea(bitmap, 0, 0, BmpWin_GetSizeX(list->header.printWindow->window) * 8,
                           list->header.y + (row + 1) * rowHeight * 2, list->header.bgColor);
        y = list->header.y + row * rowHeight;
        if (list->header.printCallback != NULL) {
            list->header.printCallback(list, list->header.options[index - 1].value, y);
        }
        BmpMenuList_PrintOption(list, list->header.options[index - 1].text, x, y, FALSE);
        fill = list->header.bgColor;
        BmpWin_ScrollChars(list->header.printWindow->window, SCROLL_UP, rowHeight, (fill << 4) | fill,
                           GFL_BitmapGetWidth(BmpWin_GetBitmap(list->header.printWindow->window)) / 8 *
                               (rowHeight * 2 / 8));
    }
    count++;
    for (i = 0; i < count; i++) {
        if (list->header.options[index].value != BMPMENULIST_LABEL) {
            x = list->header.itemX;
        } else {
            x = list->header.labelX;
        }
        y = list->header.y + (i + row) * rowHeight;
        if (list->header.printCallback != NULL) {
            list->header.printCallback(list, list->header.options[index].value, y);
        }
        BmpMenuList_PrintOption(list, list->header.options[index].text, x, y, TRUE);
        index++;
        if (index >= list->header.count) {
            break;
        }
    }
}

static void BmpMenuList_DrawCursor(BmpMenuList *list) {
    u8 x = list->header.cursorX;
    u8 y = list->header.y + (u8)(list->header.fontSizeY + list->header.lineSpacing) * list->cursorRow;

    switch (list->header.cursorDisplay) {
    case BMPMENULIST_CURSOR_SHOW:
        BmpCursor_Print(list->cursor, x, y, list->header.printWindow, list->header.queue, list->header.font);
        break;
    case BMPMENULIST_CURSOR_HIDE:
    case 2:
    case 3:
        break;
    }
}

static void BmpMenuList_EraseCursor(BmpMenuList *list, u16 row) {
    switch (list->header.cursorDisplay) {
    case BMPMENULIST_CURSOR_SHOW:
        GFL_BitmapFillArea(BmpWin_GetBitmap(list->header.printWindow->window), list->header.cursorX, RowY(list, row),
                           list->header.fontSizeX, list->header.fontSizeY, list->header.bgColor);
        break;
    case BMPMENULIST_CURSOR_HIDE:
    case 2:
    case 3:
        break;
    }
}

// Moves the cursor a row, or the list when the cursor is past the middle of the window: 0 if it can't move, 1 if the
// cursor moved and 2 if the list scrolled
static u8 BmpMenuList_Scroll(BmpMenuList *list, BOOL movingDown) {
    u32 listTop;
    u16 cursorRow;
    u16 newRow;
    u32 newTop;

    cursorRow = list->cursorRow;
    listTop = list->listTop;

    if (!movingDown) {
        if (list->header.maxShown == 1) {
            newRow = 0;
        } else {
            newRow = list->header.maxShown - (list->header.maxShown / 2 + list->header.maxShown % 2) - 1;
        }
        if (listTop == 0) {
            while (cursorRow != 0) {
                cursorRow--;
                if (list->header.options[listTop + cursorRow].value != BMPMENULIST_LABEL) {
                    list->cursorRow = cursorRow;
                    return 1;
                }
            }
            return 0;
        }
        while (cursorRow > newRow) {
            cursorRow--;
            if (list->header.options[listTop + cursorRow].value != BMPMENULIST_LABEL) {
                list->cursorRow = cursorRow;
                return 1;
            }
        }
        list->cursorRow = newRow;
        newTop = listTop - 1;
    } else {
        if (list->header.maxShown == 1) {
            newRow = 0;
        } else {
            newRow = list->header.maxShown / 2 + list->header.maxShown % 2;
        }
        if (listTop == list->header.count - list->header.maxShown) {
            while (cursorRow < list->header.maxShown - 1) {
                cursorRow++;
                if (list->header.options[listTop + cursorRow].value != BMPMENULIST_LABEL) {
                    list->cursorRow = cursorRow;
                    return 1;
                }
            }
            return 0;
        }
        while (cursorRow < newRow) {
            cursorRow++;
            if (list->header.options[listTop + cursorRow].value != BMPMENULIST_LABEL) {
                list->cursorRow = cursorRow;
                return 1;
            }
        }
        list->cursorRow = newRow;
        newTop = listTop + 1;
    }
    list->listTop = newTop;
    return 2;
}

// Scrolls the printed options count rows and prints the ones that come into view
static void BmpMenuList_ScrollOptions(BmpMenuList *list, u8 count, BOOL movingDown) {
    u8 rowHeight;
    u8 fill;

    if (count >= list->header.maxShown) {
        GFL_BitmapFill(BmpWin_GetBitmap(list->header.printWindow->window), list->header.bgColor);
        BmpMenuList_PrintOptions(list, list->listTop, 0, list->header.maxShown, TRUE);
        return;
    }
    rowHeight = list->header.fontSizeY + list->header.lineSpacing;
    if (!movingDown) {
        fill = list->header.bgColor;
        BmpWin_ScrollChars(list->header.printWindow->window, SCROLL_DOWN, count * rowHeight, (fill << 4) | fill, 0);
        BmpMenuList_PrintOptions(list, list->listTop, 0, count, TRUE);
    } else {
        fill = list->header.bgColor;
        BmpWin_ScrollChars(list->header.printWindow->window, SCROLL_UP, count * rowHeight, (fill << 4) | fill, 0);
        BmpMenuList_PrintOptions(list, list->listTop + (list->header.maxShown - count), list->header.maxShown - count,
                                 count, FALSE);
    }
}

static BOOL BmpMenuList_CycleCursor(BmpMenuList *list, u8 redraw, u8 count, u8 movingDown) {
    u16 oldRow = list->cursorRow;
    u8 scrolled = 0;
    u8 result = 0;
    u8 i;

    for (i = 0; i < count; i++) {
        u8 ret;

        do {
            ret = BmpMenuList_Scroll(list, movingDown);
            result |= ret;
            if (ret != 2) {
                break;
            }
            scrolled++;
        } while (list->header.options[list->listTop + list->cursorRow].value == BMPMENULIST_LABEL);
    }
    if (redraw) {
        switch (result) {
        case 0:
        default:
            return TRUE;
        case 1:
            BmpMenuList_EraseCursor(list, oldRow);
            BmpMenuList_DrawCursor(list);
            BmpMenuList_CallCursorCallback(list, FALSE);
            BmpWin_FlushChar(list->header.printWindow->window);
            break;
        case 2:
        case 3:
            BmpMenuList_EraseCursor(list, oldRow);
            BmpMenuList_ScrollOptions(list, scrolled, movingDown);
            BmpMenuList_DrawCursor(list);
            BmpMenuList_CallCursorCallback(list, FALSE);
            break;
        }
    }
    return FALSE;
}

static void BmpMenuList_CallCursorCallback(BmpMenuList *list, u8 init) {
    if (list->header.cursorCallback != NULL) {
        list->header.cursorCallback(list, list->header.options[list->listTop + list->cursorRow].value, init);
    }
}

// Scrolls a 16-color bitmap's pixels dist rows in dir, filling the rows that come into view. size is in bytes, or 0
// for all of the bitmap
static void Bitmap_Scroll16(GFLBitmap *bitmap, u8 dir, u8 dist, u8 fill, u16 size) {
    u8 *pixels;
    u32 widthTiles;
    u32 fill32;
    s32 end;
    u32 y;
    s32 i, j, src, dst;

    pixels = GFL_BitmapGetPixelData(bitmap);
    fill32 = (fill << 24) | (fill << 16) | (fill << 8) | fill;
    widthTiles = GFL_BitmapGetWidth(bitmap) / 8;
    end = size;

    if (end == 0) {
        end = GFL_BitmapGetHeight(bitmap) / 8 * widthTiles * 0x20;
    }
    switch (dir) {
    case SCROLL_UP:
        for (i = 0; i < end; i += 0x20) {
            y = dist;
            for (j = 0; j < 8; j++) {
                dst = i + j * 4;
                src = i + (((y & 7) | widthTiles * (y & ~7)) << 2);
                if (src < end) {
                    *(u32 *)(pixels + dst) = *(u32 *)(pixels + src);
                } else {
                    *(u32 *)(pixels + dst) = fill32;
                }
                y++;
            }
        }
        break;
    case SCROLL_DOWN:
        pixels += end - 4;
        for (i = 0; i < end; i += 0x20) {
            y = dist;
            for (j = 0; j < 8; j++) {
                dst = i + j * 4;
                src = i + (((y & 7) | widthTiles * (y & ~7)) << 2);
                if (src < end) {
                    *(u32 *)(pixels - dst) = *(u32 *)(pixels - src);
                } else {
                    *(u32 *)(pixels - dst) = fill32;
                }
                y++;
            }
        }
        break;
    case SCROLL_LEFT:
    case SCROLL_RIGHT:
        break;
    }
}

// Bitmap_Scroll16 for a 256-color bitmap
static void Bitmap_Scroll256(GFLBitmap *bitmap, u8 dir, u8 dist, u8 fill, u16 size) {
    u8 *pixels;
    u32 widthTiles;
    u32 fill32;
    s32 end;
    s32 i;
    int src;
    int dst;
    u32 y;
    s32 j;

    pixels = GFL_BitmapGetPixelData(bitmap);
    fill32 = (fill << 24) | (fill << 16) | (fill << 8) | fill;
    widthTiles = GFL_BitmapGetWidth(bitmap) / 8;
    end = size;

    if (end == 0) {
        end = GFL_BitmapGetHeight(bitmap) / 8 * widthTiles * 0x40;
    }
    switch (dir) {
    case SCROLL_UP:
        for (i = 0; i < end; i += 0x40) {
            y = dist;
            for (j = 0; j < 8; j++) {
                dst = i + j * 8;
                src = i + (((y & 7) | widthTiles * (y & ~7)) << 3);
                if (src < end) {
                    *(u32 *)(pixels + dst) = *(u32 *)(pixels + src);
                } else {
                    *(u32 *)(pixels + dst) = fill32;
                }
                if (src + 4 < end + 4) {
                    *(u32 *)(pixels + (dst + 4)) = *(u32 *)(pixels + (src + 4));
                } else {
                    *(u32 *)(pixels + (dst + 4)) = fill32;
                }
                y++;
            }
        }
        break;
    case SCROLL_DOWN:
        pixels += end - 8;
        for (i = 0; i < end; i += 0x40) {
            y = dist;
            for (j = 0; j < 8; j++) {
                dst = i + j * 8;
                src = i + (((y & 7) | widthTiles * (y & ~7)) << 3);
                if (src < end) {
                    *(u32 *)(pixels - dst) = *(u32 *)(pixels - src);
                } else {
                    *(u32 *)(pixels - dst) = fill32;
                }
                if (src - 4 < end - 4) {
                    *(u32 *)(pixels - (dst - 4)) = *(u32 *)(pixels - (src - 4));
                } else {
                    *(u32 *)(pixels - (dst - 4)) = fill32;
                }
                y++;
            }
        }
        break;
    case SCROLL_LEFT:
    case SCROLL_RIGHT:
        break;
    }
}

// Scrolls the window's characters, tiles of them or all for 0
static void BmpWin_ScrollChars(BmpWin *window, u8 dir, u8 dist, u8 fill, u16 tiles) {
    u8 bg = BmpWin_GetBGIndex(window);
    GFLBitmap *bitmap = BmpWin_GetBitmap(window);

    if (GFL_BGSysGetBGColorPaletteMode(bg) == GX_BG_COLORMODE_16) {
        Bitmap_Scroll16(bitmap, dir, dist, fill, tiles * 0x20);
    } else {
        Bitmap_Scroll256(bitmap, dir, dist, fill, tiles * 0x40);
    }
}

void BmpMenuList_LoadCursor(BmpMenuList *list, u32 heapId) {
    BmpCursor_LoadBitmap(list->cursor, heapId);
}

void *BmpMenuList_GetWork(BmpMenuList *list) {
    return list->header.work;
}

void BmpMenuList_SetCancelDisabled(BmpMenuList *list, u8 disabled) {
    list->cancelDisabled = disabled;
}
