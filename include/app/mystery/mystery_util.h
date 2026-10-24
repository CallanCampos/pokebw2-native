#ifndef POKEBW2_APP_MYSTERY_MYSTERY_UTIL_H
#define POKEBW2_APP_MYSTERY_MYSTERY_UTIL_H

// Mystery Gift's windows, menus and sequences (ov197, mystery_util.c). Our names; swan has none for this overlay

#include "types.h"
#include "gfl/clact.h"
#include "gfl/heap.h"
#include "struct_decls.h"
#include "system/printsys.h"

// A step of a sequence, called each frame with the sequence's state and work
typedef void (*MysterySeqFunc)(MysterySeq *seq, u32 *state, void *work);

// The answers of the menus' updates while they wait
#define MYSTERY_MENU_NONE ((u32) - 1)
#define MYSTERY_MENU_CANCEL ((u32) - 2)

typedef struct {
    MsgData *msgData;
    Font *font;
    PrintQueue *queue;
    u32 msgIds[3];
    u32 count;
    u16 bg;
    u16 palette;
    u16 framePalette;
    u16 frameChar;
} MysteryYesNoSetup;

// A window of MysteryTextWin, which prints a message or a string
typedef struct {
    u8 x;
    u8 y;
    u8 width;
    u8 height;
    u32 msgId;
    // Printed instead of the message when not NULL
    StrBuf *str;
    u32 align;
    s16 textX;
    s16 textY;
    u16 color;
} MysteryTextWinEntry;

// A change of a MysteryTextWin's window, which MysteryTextWinCopy_Print makes when update is set
typedef struct {
    BOOL update;
    u32 msgId;
    StrBuf *str;
    u32 align;
    s16 textX;
    s16 textY;
    u16 color;
} MysteryTextWinUpdate;

// The list menu of MysteryList: items are messages, or strings when msgData is NULL
typedef struct {
    MsgData *msgData;
    Font *font;
    PrintQueue *queue;
    ClActor *cursor;
    uptr items[4];
    u32 count;
    u16 bg;
    u16 palette;
    u16 unk28;
    // The BG palette whose colors 10 to 13 gray out disabled items
    u16 bgPalette;
    s16 offsetY;
    s16 offsetX;
    u16 cursorSequence;
    // Called when the cursor moves, with work
    void (*onMove)(void *work);
    void *work;
    // Where the cursor is kept
    u32 *cursorPos;
} MysteryListSetup;

// How a message window prints: all at once, a character at a time waiting for a key at the end, or all at once
// with the wait icon
enum {
    MYSTERY_PRINT_QUEUE,
    MYSTERY_PRINT_STREAM,
    MYSTERY_PRINT_WAIT_ICON,
    MYSTERY_PRINT_NONE,
};

MysteryMsgWin *MysteryMsgWin_Create(u16 bg, u8 palette, PrintQueue *queue, Font *font, HeapID heapId);
// A message window of one line, which waits for a key after its stream
MysteryMsgWin *MysteryMsgWin_CreateSmall(u16 bg, u8 palette, PrintQueue *queue, Font *font, HeapID heapId);
void MysteryMsgWin_Delete(MysteryMsgWin *win);
void MysteryMsgWin_Update(MysteryMsgWin *win);
// mode is a MYSTERY_PRINT_*
void MysteryMsgWin_Print(MysteryMsgWin *win, MsgData *msgData, u32 msgId, u32 mode);
BOOL MysteryMsgWin_IsDone(MysteryMsgWin *win);
void MysteryMsgWin_DrawFrame(MysteryMsgWin *win, u16 frameChar, u8 framePalette);

MysteryYesNo *MysteryYesNo_Create(const MysteryYesNoSetup *setup, HeapID heapId);
void MysteryYesNo_Delete(MysteryYesNo *menu);
u32 MysteryYesNo_Update(MysteryYesNo *menu);

// A window that prints one string, the line of a MysteryTextWin
MysteryTextLine *MysteryTextLine_Create(BOOL deferFlush, u16 bg, u8 x, u8 y, u8 width, u8 height, u8 palette,
                                        PrintQueue *queue, HeapID heapId);
void MysteryTextLine_Delete(MysteryTextLine *line);
void MysteryTextLine_PrintStr(MysteryTextLine *line, const StrBuf *str, Font *font);
void MysteryTextLine_SetColor(MysteryTextLine *line, u16 color);
void MysteryTextLine_SetPos(MysteryTextLine *line, s32 x, s32 y, u32 align);
BOOL MysteryTextLine_Update(MysteryTextLine *line);

MysteryTextWin *MysteryTextWin_Create(BOOL deferFlush, const MysteryTextWinEntry *entries, u32 count, u16 bg,
                                      u8 palette, PrintQueue *queue, MsgData *msgData, Font *font, HeapID heapId);
void MysteryTextWin_Delete(MysteryTextWin *win);
void MysteryTextWin_Clear(MysteryTextWin *win);
BOOL MysteryTextWin_Update(MysteryTextWin *win);
void MysteryTextWin_ClearLine(MysteryTextWin *win, u32 index);
void MysteryTextWin_Flush(MysteryTextWin *win);
MysteryTextWinCopy *MysteryTextWinCopy_Create(MysteryTextWin *textWin, HeapID heapId);
void MysteryTextWinCopy_Delete(MysteryTextWinCopy *copy);
void MysteryTextWinCopy_Print(MysteryTextWinCopy *copy, const MysteryTextWinUpdate *updates, PrintQueue *queue);
void MysteryTextWinCopy_Apply(MysteryTextWinCopy *copy);

MysteryList *MysteryList_Create(const MysteryListSetup *setup, HeapID heapId);
void MysteryList_Delete(MysteryList *list);
u32 MysteryList_Update(MysteryList *list);
void MysteryList_UpdatePrint(MysteryList *list);
void MysteryList_SetItemGrayed(MysteryList *list, u32 item, BOOL grayed);

MysterySeq *MysterySeq_Create(void *work, MysterySeqFunc func, HeapID heapId);
void MysterySeq_Delete(MysterySeq *seq);
void MysterySeq_Main(MysterySeq *seq);
BOOL MysterySeq_IsEnd(MysterySeq *seq);
void MysterySeq_SetNext(MysterySeq *seq, MysterySeqFunc func);
void MysterySeq_End(MysterySeq *seq);
// Sets the state that MysterySeq_Return goes back to, after a step such as a message
void MysterySeq_SetReturn(MysterySeq *seq, u32 state);
void MysterySeq_Return(MysterySeq *seq);

MysteryOamText *MysteryOamText_Create(const ClActorSetup *setup, u16 width, u16 height, u32 palette, u8 paletteOffset,
                                      u32 surface, BmpOamSys *bmpOam, PrintQueue *queue, HeapID heapId);
void MysteryOamText_Delete(MysteryOamText *oamText);
void MysteryOamText_Clear(MysteryOamText *oamText);
void MysteryOamText_Print(MysteryOamText *oamText, MsgData *msgData, u32 msgId, Font *font);
void MysteryOamText_SetColor(MysteryOamText *oamText, u16 color);
void MysteryOamText_SetAlign(MysteryOamText *oamText, s32 x, s32 y, u32 align);
BOOL MysteryOamText_Update(MysteryOamText *oamText);

void MysteryPal_Blend(u32 type, u16 *dest, u16 angle, u32 palette, const u16 *from, const u16 *to);
void MysteryPal_BlendOne(u32 type, u16 *dest, u16 angle, u8 palette, u8 index, u16 from, u16 to);

#endif // POKEBW2_APP_MYSTERY_MYSTERY_UTIL_H
