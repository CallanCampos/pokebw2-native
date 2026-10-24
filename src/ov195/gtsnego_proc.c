// gtsnego_proc.c, in overlay 195: the process of GTS Negotiations. The player picks a trade with anyone, with the
// search's settings, or a rendezvous with someone of the Pal Pad; it searches for a partner with matching keys, and
// the two machines exchange their settings, player info, greeting and Join Avenue person before the trade's process
// takes over. The ROM has no file name for it, so it is named after its process, as pokemontrade_proc.c is (a guess).
// It draws with gtsnego_disp.c and prints with gtsnego_message.c. Our names

#include "types.h"
#include "app/gtsnego_local.h"
#include "constants/arc.h"
#include "constants/sound.h"
#include "constants/text_banks.h"
#include "constants/version.h"
#include "dwc/dwc.h"
#include "gfl/bg_sys.h"
#include "gfl/dwc_rap.h"
#include "gfl/dwc_rapcommon.h"
#include "gfl/gx_layers.h"
#include "gfl/heap.h"
#include "gfl/key.h"
#include "gfl/msg.h"
#include "gfl/net.h"
#include "gfl/net_command.h"
#include "gfl/net_handle.h"
#include "gfl/net_state.h"
#include "gfl/net_system.h"
#include "gfl/proc.h"
#include "gfl/sound.h"
#include "gfl/std.h"
#include "gfl/str.h"
#include "gfl/touchpanel.h"
#include "gfl/ui.h"
#include "nitro/gx.h"
#include "nitro/os.h"
#include "pml/poke_party.h"
#include "save/join_avenue.h"
#include "save/player_info.h"
#include "save/save_control.h"
#include "save/wifi_list.h"
#include "system/game_data.h"
#include "system/pms.h"
#include "system/pms_data.h"
#include "system/wipe.h"
#include "text/system/ability_handlers_btl_main.h"
#include "text/system/gtsnego_proc.h"

// The network commands, and the timings the machines synchronize on
#define GTSNEGO_NET_CMD_BASE 0x2200
enum {
    GTSNEGO_NET_CMD_REQUEST = GTSNEGO_NET_CMD_BASE,
    GTSNEGO_NET_CMD_PLAYER_INFO,
    GTSNEGO_NET_CMD_PHRASE,
    GTSNEGO_NET_CMD_PERSON,
};
#define GTSNEGO_TIMING_NO 0x22

// The name that replaces a partner's name with bad words
#ifdef BLACK2
#define GTSNEGO_DEFAULT_NAME_MSG AbilityHandlersBtlMain_Text_Kuro
#else
#define GTSNEGO_DEFAULT_NAME_MSG AbilityHandlersBtlMain_Text_Shiro
#endif

// The search's kinds
#define GTSNEGO_MODE_ANYONE 0
#define GTSNEGO_MODE_RENDEZVOUS 1

// The level range a rendezvous with anyone of the Pal Pad searches with
#define GTSNEGO_LEVEL_RENDEZVOUS 4

// The search's matching keys
enum {
    GTSNEGO_KEY_MY_ID,
    GTSNEGO_KEY_MODE,
    GTSNEGO_KEY_LEVEL,
    GTSNEGO_KEY_MY_KIND,
    GTSNEGO_KEY_WANT_KIND,
    GTSNEGO_KEY_ROM,
    GTSNEGO_KEY_DEBUG,
    GTSNEGO_KEY_FRIEND_ID,
    GTSNEGO_KEY_COUNT,
};

typedef struct GtsNegoWork GtsNegoWork;
typedef void (*GtsNegoStateFunc)(GtsNegoWork *wk);

typedef struct {
    int value;
    u8 keyId;
    u8 unk5;
    char key[6];
} GtsNegoMatchKey;

// What the machines show of each other: the greeting and how many trades the player has made
typedef struct {
    PMSData phrase;
    u32 tradeCount;
} GtsNegoGreeting;

// The partner's name, for the server's check for bad words
typedef struct {
    u16 name[12];
    const u16 *words;
    char result;
    int badCount;
    u16 unk24;
} GtsNegoNameCheck;

struct GtsNegoWork {
    GtsNegoStateFunc state;
    // What a touched button of the lower screen does
    BOOL (*buttonCallback)(int button, GtsNegoWork *wk);
    u32 unk8;
    HeapID heapId;
    u32 unk10;
    int timer;
    // The person to meet
    u32 friendId;
    u8 unk1C[0x10];
    GameData *gameData;
    SaveControl *save;
    AppTaskMenuWin *button;
    AppTaskMenu *menu;
    GtsNegoDisp *disp;
    GtsNegoMessage *msg;
    WifiList *wifiList;
    GtsNegoParam *param;
    u8 unk4C[0xc];
    GtsNegoMatchKey keys[GTSNEGO_KEY_COUNT];
    char query[0x80];
    int mode;
    // The search's settings: 5 kinds of Pokémon each, and 4 level ranges
    int myKind;
    int wantKind;
    int level;
    // The person of the Pal Pad to meet, or -1 for any of them
    int friendIndex;
    u32 myId;
    GtsNegoScroll scroll;
    // The GTSNEGO_CURSOR_* of each menu
    int mainCursor;
    int searchCursor;
    int friendCursor;
    GtsNegoGreeting send;
    GtsNegoGreeting recv;
    u32 unk180;
    // With fewer than two Pokémon there is no trading
    BOOL notEnoughPokemon;
    GtsNegoNameCheck nameCheck;
    // The error screen was shown, and the frames since
    BOOL errorShown;
    u32 errorFrames;
    JoinAvenuePerson myPerson;
    JoinAvenuePerson recvPerson;
};

static void GtsNego_SetState(GtsNegoWork *wk, GtsNegoStateFunc state);
static void GtsNego_ChangeState(GtsNegoWork *wk, GtsNegoStateFunc state, int line);
static void GtsNego_OnRecvRequest(int netId, int size, const void *data, void *work, NetHandle *handle);
static void GtsNego_OnRecvPlayerInfo(int netId, int size, const void *data, void *work, NetHandle *handle);
static void GtsNego_OnRecvPhrase(int netId, int size, const void *data, void *work, NetHandle *handle);
static void GtsNego_OnRecvPerson(int netId, int size, const void *data, void *work, NetHandle *handle);
static void *GtsNego_GetPersonBuffer(int netId, void *work, int size);
static BOOL GtsNego_OnMainButton(int button, GtsNegoWork *wk);
static void GtsNego_AddMyKind(int delta, GtsNegoWork *wk);
static void GtsNego_AddWantKind(int delta, GtsNegoWork *wk);
static void GtsNego_AddLevel(int delta, GtsNegoWork *wk);
static int GtsNego_PressArrow(int arrow, GtsNegoWork *wk);
static BOOL GtsNego_OnSearchButton(int button, GtsNegoWork *wk);
static void GtsNego_PressArrowOfCursor(int dir, GtsNegoWork *wk);
static void GtsNego_StateWaitFadeOut(GtsNegoWork *wk);
static void GtsNego_StateWaitThenFadeOut(GtsNegoWork *wk);
static void GtsNego_StateShowPhrase(GtsNegoWork *wk);
static void GtsNego_StateWaitPartnerShown(GtsNegoWork *wk);
static void GtsNego_StateShowPartner(GtsNegoWork *wk);
static void GtsNego_StateWaitPersonSync(GtsNegoWork *wk);
static void GtsNego_StateSendPerson(GtsNegoWork *wk);
static void GtsNego_StateWaitNameCheck(GtsNegoWork *wk);
static void GtsNego_StateStartNameCheck(GtsNegoWork *wk);
static void GtsNego_StateWaitInfoSync(GtsNegoWork *wk);
static void GtsNego_StateStartInfoSync(GtsNegoWork *wk);
static void GtsNego_StateSendPlayerInfo(GtsNegoWork *wk);
static void GtsNego_StateSendPhrase(GtsNegoWork *wk);
static void GtsNego_StateSendRequest(GtsNegoWork *wk);
static void GtsNego_StateStartExchange(GtsNegoWork *wk);
static void GtsNego_StateWaitConnect(GtsNegoWork *wk);
static void GtsNego_StateWaitMatched(GtsNegoWork *wk);
static void GtsNego_StateSearching(GtsNegoWork *wk);
static int GtsNego_EvalMatch(int index, void *work);
static void GtsNego_StateStartSearch(GtsNegoWork *wk);
static void GtsNego_OnButton(u32 button, u32 event, void *work);
static void GtsNego_StateWaitSearchPressed(GtsNegoWork *wk);
static void GtsNego_StateSearchMenu(GtsNegoWork *wk);
static void GtsNego_StateOpenSearchMenu(GtsNegoWork *wk);
static PlayerInfo *GtsNego_GetFriendClamped(GameData *gameData, int index);
static void GtsNego_StateReturnToMainMenu(GtsNegoWork *wk);
static void GtsNego_StateResetScreens(GtsNegoWork *wk);
static void GtsNego_StateWaitCancelPressed(GtsNegoWork *wk);
static void GtsNego_OnCancelButton(u32 button, u32 event, void *work);
static void GtsNego_StateRecreateFriendButton(GtsNegoWork *wk);
static void GtsNego_StateWaitFriendConfirm(GtsNegoWork *wk);
static void GtsNego_StateOpenFriendConfirm(GtsNegoWork *wk);
static void GtsNego_StatePrintFriendChosen(GtsNegoWork *wk);
static void GtsNego_StateLeaveFriendList(GtsNegoWork *wk);
static int GtsNego_FriendListUp(GtsNegoWork *wk);
static int GtsNego_FriendListDown(GtsNegoWork *wk);
static void GtsNego_StateWaitAnyFriendPressed(GtsNegoWork *wk);
static void GtsNego_FreeButton(GtsNegoWork *wk);
static void GtsNego_StateFriendList(GtsNegoWork *wk);
static void GtsNego_StateWaitNoFriends(GtsNegoWork *wk);
static void GtsNego_StateOpenFriendList(GtsNegoWork *wk);
static void GtsNego_StateOpenMainMenu(GtsNegoWork *wk);
static int GtsNego_PressCursor(GtsNegoWork *wk, int cursor);
static int GtsNego_GetPressedCursor(GtsNegoWork *wk, int cursor);
static void GtsNego_StateWaitMainPressed(GtsNegoWork *wk);
static void GtsNego_StateWaitExitConfirm(GtsNegoWork *wk);
static void GtsNego_StateOpenExitConfirm(GtsNegoWork *wk);
static void GtsNego_StateMainMenu(GtsNegoWork *wk);
static void GtsNego_HandleDisconnect(GtsNegoWork *wk);
static BOOL GtsNegoProc_Init(GameProc *proc, u32 *state, void *param, void *work);
static BOOL GtsNegoProc_Main(GameProc *proc, u32 *state, void *param, void *work);
static BOOL GtsNegoProc_Exit(GameProc *proc, u32 *state, void *param, void *work);

// The names of the matching keys, two letters each
static char sGtsNegoKeyNames[GTSNEGO_KEY_COUNT][2] = {
    { 'p', '1' }, { 't', 'y' }, { 'l', 'v' }, { 'm', 'y' }, { 'f', 'r' }, { 'r', 'm' }, { 'd', 'b' }, { 'p', '2' },
};

// The search button
static const TouchRect sGtsNegoSearchRect[] = {
    { 160, 192, 80, 176 },
    { TOUCH_RECT_END, 0, 0, 0 },
};

// The scroll bar of the list of people
static const TouchRect sGtsNegoScrollBarRect[] = {
    { 26, 158, 236, 0 },
    { TOUCH_RECT_END, 0, 0, 0 },
};

// The rows of the list of people, and the button under them
// clang-format off
static const TouchRect sGtsNegoFriendRects[] = {
    { 24, 72, 0, 236 },
    { 72, 120, 0, 236 },
    { 120, 168, 0, 236 },
    { 168, 192, 64, 192 },
    { TOUCH_RECT_END, 0, 0, 0 },
};

// clang-format on

static const NetCommand sGtsNegoCommands[] = {
    { GtsNego_OnRecvRequest, NULL },
    { GtsNego_OnRecvPlayerInfo, NULL },
    { GtsNego_OnRecvPhrase, NULL },
    { GtsNego_OnRecvPerson, GtsNego_GetPersonBuffer },
};

static void GtsNego_SetState(GtsNegoWork *wk, GtsNegoStateFunc state) {
    wk->state = state;
}

// Every state change gives the line it is on
static void GtsNego_ChangeState(GtsNegoWork *wk, GtsNegoStateFunc state, int line) {
    GtsNego_SetState(wk, state);
}

// The partner's settings
static void GtsNego_OnRecvRequest(int netId, int size, const void *data, void *work, NetHandle *handle) {
    GtsNegoWork *wk = work;
    GtsNegoParam *param = wk->param;

    if (handle != NetHandle_GetOwn()) {
        return;
    }
    if (netId == GFL_NetGetHandleNetId(NetHandle_GetOwn())) {
        return;
    }
    MI_CpuCopy8(data, &param->unk4[1], sizeof(GtsNegoUnk4));
}

static void GtsNego_OnRecvPlayerInfo(int netId, int size, const void *data, void *work, NetHandle *handle) {
    GtsNegoWork *wk = work;
    GtsNegoParam *param = wk->param;

    if (handle != NetHandle_GetOwn()) {
        return;
    }
    if (netId == GFL_NetGetHandleNetId(NetHandle_GetOwn())) {
        return;
    }
    MI_CpuCopy8(data, param->playerInfo2, PlayerInfo_GetSize());
}

static void GtsNego_OnRecvPhrase(int netId, int size, const void *data, void *work, NetHandle *handle) {
    GtsNegoWork *wk = work;

    if (handle != NetHandle_GetOwn()) {
        return;
    }
    if (netId == GFL_NetGetHandleNetId(NetHandle_GetOwn())) {
        return;
    }
    MI_CpuCopy8(data, &wk->recv, sizeof(GtsNegoGreeting));
}

// The partner's Join Avenue person, who becomes a visitor of the avenue
static void GtsNego_OnRecvPerson(int netId, int size, const void *data, void *work, NetHandle *handle) {
    GtsNegoWork *wk = work;
    GtsNegoParam *param = wk->param;
    JoinAvenuePerson *person = (JoinAvenuePerson *)data;
    JoinAvenueSave *joinAvenue = SaveControl_GetJoinAvenue(GameData_GetSaveControl(param->gameData));

    if (handle != NetHandle_GetOwn()) {
        return;
    }
    if (netId == GFL_NetGetHandleNetId(NetHandle_GetOwn())) {
        return;
    }
    JoinAvenuePerson_SetParam(person, JOIN_AVE_PARAM_UNK_26, 2);
    JoinAvenuePerson_SetParam(person, JOIN_AVE_PARAM_NAME, (uptr)GetPlayerName(param->playerInfo2));
    JoinAvenue_Add(joinAvenue, param->gameData, person, 1);
}

static void *GtsNego_GetPersonBuffer(int netId, void *work, int size) {
    GtsNegoWork *wk = work;

    if (netId == GFL_NetGetHandleNetId(NetHandle_GetOwn())) {
        GFL_DebugAssertFail("", 0, "0");
        return NULL;
    }
    return &wk->recvPerson;
}

// A button of the main menu was touched
static BOOL GtsNego_OnMainButton(int button, GtsNegoWork *wk) {
    wk->mode = button;
    GCTX_HIDSetUsingTouch(TRUE);
    if (wk->notEnoughPokemon == TRUE) {
        GFL_SndSEPlay(SEQ_SE_CANCEL1);
        return TRUE;
    }
    if (button == GTSNEGO_MODE_ANYONE) {
        wk->mainCursor = GTSNEGO_CURSOR_ANYONE;
        GtsNegoDisp_SetCursor(wk->disp, wk->button, GTSNEGO_CURSOR_ANYONE);
        GtsNego_PressCursor(wk, wk->mainCursor);
        GtsNego_ChangeState(wk, GtsNego_StateWaitMainPressed, 437);
    } else {
        wk->mainCursor = GTSNEGO_CURSOR_RENDEZVOUS;
        GtsNegoDisp_SetCursor(wk->disp, wk->button, GTSNEGO_CURSOR_RENDEZVOUS);
        GtsNego_PressCursor(wk, wk->mainCursor);
        GtsNego_ChangeState(wk, GtsNego_StateWaitMainPressed, 444);
    }
    return TRUE;
}

static void GtsNego_AddMyKind(int delta, GtsNegoWork *wk) {
    wk->myKind += delta;
    if (wk->myKind >= 5) {
        wk->myKind = 0;
    } else if (wk->myKind < 0) {
        wk->myKind = 4;
    }
}

static void GtsNego_AddWantKind(int delta, GtsNegoWork *wk) {
    wk->wantKind += delta;
    if (wk->wantKind >= 5) {
        wk->wantKind = 0;
    } else if (wk->wantKind < 0) {
        wk->wantKind = 4;
    }
}

static void GtsNego_AddLevel(int delta, GtsNegoWork *wk) {
    wk->level += delta;
    if (wk->level >= 4) {
        wk->level = 0;
    } else if (wk->level < 0) {
        wk->level = 3;
    }
}

// Changes the setting of one of the six arrows, left then right of each, and returns the setting's cursor
static int GtsNego_PressArrow(int arrow, GtsNegoWork *wk) {
    int cursor = 0;

    switch (arrow) {
    case 0:
        GtsNego_AddLevel(-1, wk);
        cursor = GTSNEGO_CURSOR_LEVEL;
        GtsNegoMessage_SetLevel(wk->msg, wk->level);
        break;
    case 1:
        GtsNego_AddLevel(1, wk);
        cursor = GTSNEGO_CURSOR_LEVEL;
        GtsNegoMessage_SetLevel(wk->msg, wk->level);
        break;
    case 2:
        GtsNego_AddMyKind(-1, wk);
        cursor = GTSNEGO_CURSOR_MY_KIND;
        GtsNegoMessage_SetMyKind(wk->msg, wk->myKind);
        break;
    case 3:
        GtsNego_AddMyKind(1, wk);
        cursor = GTSNEGO_CURSOR_MY_KIND;
        GtsNegoMessage_SetMyKind(wk->msg, wk->myKind);
        break;
    case 4:
        GtsNego_AddWantKind(-1, wk);
        cursor = GTSNEGO_CURSOR_WANT_KIND;
        GtsNegoMessage_SetWantKind(wk->msg, wk->wantKind);
        break;
    case 5:
        GtsNego_AddWantKind(1, wk);
        cursor = GTSNEGO_CURSOR_WANT_KIND;
        GtsNegoMessage_SetWantKind(wk->msg, wk->wantKind);
        break;
    }
    if (arrow >= 0 && arrow <= 5) {
        GFL_SndSEPlay(SEQ_SE_SELECT1);
        GtsNegoDisp_PressArrow(wk->disp, arrow);
    }
    return cursor;
}

// An arrow of the search's settings was touched
static BOOL GtsNego_OnSearchButton(int button, GtsNegoWork *wk) {
    int cursor;

    if (wk->button != NULL) {
        AppTaskMenuWin_SetActive(wk->button, FALSE);
    }
    cursor = GtsNego_PressArrow(button, wk);
    if (cursor != 0) {
        wk->searchCursor = cursor;
    }
    return TRUE;
}

// Left or right on the setting the cursor is on
static void GtsNego_PressArrowOfCursor(int dir, GtsNegoWork *wk) {
    int cursor = wk->searchCursor;
    int setting = cursor - GTSNEGO_CURSOR_LEVEL;

    // BUG: the check is always true, so the search button's cursor gives an arrow past the last, which does nothing
#ifdef BUGFIX
    if (cursor >= GTSNEGO_CURSOR_LEVEL && cursor <= GTSNEGO_CURSOR_WANT_KIND) {
#else
    if (cursor >= GTSNEGO_CURSOR_LEVEL || cursor <= GTSNEGO_CURSOR_WANT_KIND) {
#endif
        GtsNego_PressArrow(dir + setting * 2, wk);
    }
}

static void GtsNego_StateWaitFadeOut(GtsNegoWork *wk) {
    if (GFL_WipeIsFinished()) {
        GtsNego_ChangeState(wk, NULL, 593);
    }
}

static void GtsNego_StateWaitThenFadeOut(GtsNegoWork *wk) {
    wk->timer--;
    if (wk->timer <= 0) {
        GFL_WipeSet(WIPE_MODE_BOTH, WIPE_TYPE_FADE_OUT, WIPE_TYPE_FADE_OUT, WIPE_COLOR_BLACK, 6, 1, wk->heapId);
        GtsNego_ChangeState(wk, GtsNego_StateWaitFadeOut, 604);
    }
}

// Shows the partner's greeting
static void GtsNego_StateShowPhrase(GtsNegoWork *wk) {
    PMSData empty = { 1, 1, { 1, 1 } };
    PMSData *phrase = &empty;

    if (GFL_NetIsInitialized()) {
        phrase = &wk->recv.phrase;
    }
    wk->timer = 180;
    GtsNegoMessage_CreatePMSDraw(wk->msg, wk->disp);
    GtsNegoMessage_PrintPhrase(wk->msg, phrase);
    GtsNego_ChangeState(wk, GtsNego_StateWaitThenFadeOut, 624);
}

static void GtsNego_StateWaitPartnerShown(GtsNegoWork *wk) {
    wk->timer--;
    if (wk->timer == 20) {
        GtsNegoMessage_PrintShort(wk->msg, GtsnegoProc_Text_TradeWillStarted);
    }
    if (wk->timer == 0) {
        GtsNegoMessage_PrintHeading(wk->msg, GtsnegoProc_Text_Greetings);
        GtsNegoMessage_ClearLocation(wk->msg);
        GtsNego_ChangeState(wk, GtsNego_StateShowPhrase, 639);
    }
}

// Shows the partner found, who is not to be matched again
static void GtsNego_StateShowPartner(GtsNegoWork *wk) {
    GtsNegoParam *param = wk->param;
    int count = 2;
    PlayerInfo *partner;

    if (!GFL_NetIsInitialized()) {
        partner = GetGameDataPlayerInfo(param->gameData);
    } else {
        partner = param->playerInfo2;
        count = wk->recv.tradeCount;
        param->metIds[param->metIndex] = getIDAsUInt(partner);
        param->metIndex++;
        if (param->metIndex >= GTSNEGO_MET_COUNT) {
            param->metIndex = 0;
        }
    }
    GtsNegoMessage_ShowPartner(wk->msg, partner, count);
    GtsNegoDisp_ShowPartner(wk->disp, PlayerInfo_GetUnionView(partner));
    wk->timer = 180;
    GtsNego_ChangeState(wk, GtsNego_StateWaitPartnerShown, 675);
}

static void GtsNego_StateWaitPersonSync(GtsNegoWork *wk) {
    if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 4, GTSNEGO_TIMING_NO)) {
        GtsNego_ChangeState(wk, GtsNego_StateShowPartner, 690);
    }
}

// Sends the player's Join Avenue person to the partner
static void GtsNego_StateSendPerson(GtsNegoWork *wk) {
    GtsNegoParam *param = wk->param;
    // The other machine of the two
    int target = GFL_NetGetHandleNetId(NetHandle_GetOwn()) == 0;

    JoinAvenuePerson_InitPlayer(&wk->myPerson, param->gameData);
    if (GFL_NetSendCommandToMachine(NetHandle_GetOwn(), target, GTSNEGO_NET_CMD_PERSON, sizeof(JoinAvenuePerson),
                                    &wk->myPerson, TRUE, FALSE, TRUE)) {
        NetHandle_SendTimingPair(NetHandle_GetOwn(), 4, GTSNEGO_TIMING_NO);
        GtsNego_ChangeState(wk, GtsNego_StateWaitPersonSync, 705);
    }
}

// Once the server has checked the partner's name, replaces a bad one, and keeps the partner
static void GtsNego_StateWaitNameCheck(GtsNegoWork *wk) {
    PlayerInfo *partner = wk->param->playerInfo2;
    BOOL bad = FALSE;
    BOOL done = DWC_CheckProfanityProcess() == 2;
    u32 id;

    if (done) {
        bad = wk->nameCheck.badCount > 0;
    }
    if (done) {
        if (bad) {
            MsgData *msgData =
                GFL_MsgSysLoadData(FALSE, ARCID_SYSTEM_MESSAGE, TEXT_BANK_ABILITY_HANDLERS_BTL_MAIN, HEAPID_GTSNEGO);
            StrBuf *name = GFL_MsgDataLoadStrbufNew(msgData, GTSNEGO_DEFAULT_NAME_MSG);

            GFL_MsgDataFree(msgData);
            copyTrainerNameFromStrbuf(partner, name);
            GFL_StrBufFree(name);
        }
        PlayerInfo_Copy(partner, GameData_GetPlayerInfoByNetID(wk->gameData, 1 - NetSystem_GetNetId()));
        id = getIDAsUInt(partner);
        Misc_AddMetTrainer(getHollow_RivalData(wk->save), id);
        if (PlayerInfo_GetVersion(partner) == 22 || PlayerInfo_GetVersion(partner) == 23) {
            GtsNego_ChangeState(wk, GtsNego_StateSendPerson, 740);
        } else {
            GtsNego_ChangeState(wk, GtsNego_StateShowPartner, 745);
        }
    }
}

// Asks the server whether the partner's name has bad words
static void GtsNego_StateStartNameCheck(GtsNegoWork *wk) {
    StrBuf *name = copyTrainerNameToNewStrbuf(wk->param->playerInfo2->name, HEAPID_GTSNEGO);
    const u16 *chars;
    u32 i = 0;

    MI_CpuFill8(&wk->nameCheck, 0, sizeof(GtsNegoNameCheck));
    wk->nameCheck.unk24 = HEAPID_GTSNEGO;
    chars = GFL_StrBufGetStringPtr(name);
    for (; i < GFL_StrBufGetCharCount(name) + 1; i++) {
        u16 c = chars[i];

        if (c == GFL_StrBufGetTerminator()) {
            wk->nameCheck.name[i] = 0;
        } else {
            wk->nameCheck.name[i] = c;
        }
    }
    wk->nameCheck.words = wk->nameCheck.name;
    wk->nameCheck.badCount = 0;
    DWC_CheckProfanityExAsync(&wk->nameCheck.words, 1, NULL, 0, &wk->nameCheck.result, &wk->nameCheck.badCount, 0x80);
    GFL_StrBufFree(name);
    GtsNego_ChangeState(wk, GtsNego_StateWaitNameCheck, 768);
}

static void GtsNego_StateWaitInfoSync(GtsNegoWork *wk) {
    if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 3, GTSNEGO_TIMING_NO)) {
        GtsNego_ChangeState(wk, GtsNego_StateStartNameCheck, 778);
    }
}

static void GtsNego_StateStartInfoSync(GtsNegoWork *wk) {
    NetHandle_SendTimingPair(NetHandle_GetOwn(), 3, GTSNEGO_TIMING_NO);
    GtsNego_ChangeState(wk, GtsNego_StateWaitInfoSync, 794);
}

static void GtsNego_StateSendPlayerInfo(GtsNegoWork *wk) {
    if (GFL_NetSendCommand(NetHandle_GetOwn(), GTSNEGO_NET_CMD_PLAYER_INFO, PlayerInfo_GetSize(),
                           wk->param->playerInfo)) {
        GtsNego_ChangeState(wk, GtsNego_StateStartInfoSync, 811);
    }
}

static void GtsNego_StateSendPhrase(GtsNegoWork *wk) {
    if (GFL_NetSendCommand(NetHandle_GetOwn(), GTSNEGO_NET_CMD_PHRASE, sizeof(GtsNegoGreeting), &wk->send)) {
        GtsNego_ChangeState(wk, GtsNego_StateSendPlayerInfo, 829);
    }
}

static void GtsNego_StateSendRequest(GtsNegoWork *wk) {
    if (!GFL_NetIsInitialized()) {
        GtsNego_ChangeState(wk, GtsNego_StateShowPartner, 845);
        return;
    }
    if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 2, GTSNEGO_TIMING_NO)) {
        if (GFL_NetSendCommand(NetHandle_GetOwn(), GTSNEGO_NET_CMD_REQUEST, sizeof(GtsNegoUnk4), &wk->param->unk4[0])) {
            GtsNego_ChangeState(wk, GtsNego_StateSendPhrase, 853);
        }
    }
}

// With both machines connected, starts exchanging what they show of each other
static void GtsNego_StateStartExchange(GtsNegoWork *wk) {
    if (GtsNegoMessage_IsDone(wk->msg)) {
        if (!GFL_NetIsInitialized()) {
            GtsNego_ChangeState(wk, GtsNego_StateSendRequest, 876);
            return;
        }
        if (wk->timer > 0) {
            wk->timer--;
            return;
        }
        if (NetHandle_CountNegotiated() == 2) {
            GtsNegoParam *param = wk->param;
            void *phrases = CGearPhrases_GetBlock(wk->save);

            param->unk4[0].unk0 = wk->level;
            param->unk4[0].unk4 = wk->myKind;
            CGearPhrases_Get(phrases, 0, &wk->send.phrase);
            wk->send.tradeCount = PalPadFriendList_GetTradeCount(GameData_GetPalPadFriendList(wk->gameData));
            NetHandle_SendTimingPair(NetHandle_GetOwn(), 2, GTSNEGO_TIMING_NO);
            GtsNego_ChangeState(wk, GtsNego_StateSendRequest, 898);
        }
    }
}

static void GtsNego_StateWaitConnect(GtsNegoWork *wk) {
    if (!GFL_NetIsInitialized()) {
        GtsNego_ChangeState(wk, GtsNego_StateStartExchange, 907);
        return;
    }
    if (NetHandle_CountNegotiated() != 0) {
        if (NetHandle_SendNegotiation()) {
            GtsNego_ChangeState(wk, GtsNego_StateStartExchange, 912);
        }
    }
}

static void GtsNego_StateWaitMatched(GtsNegoWork *wk) {
    if (GtsNegoMessage_IsDone(wk->msg)) {
        if (wk->timer > 0) {
            wk->timer--;
            return;
        }
        if (NetSystem_GetNetId() != 0) {
            if (NetHandle_SendNegotiation()) {
                wk->timer = 180;
                GtsNego_ChangeState(wk, GtsNego_StateStartExchange, 941);
            }
        } else {
            wk->timer = 180;
            GtsNego_ChangeState(wk, GtsNego_StateWaitConnect, 946);
        }
    }
}

// Waits for a partner, or for the search to be cancelled. Without a connection, A finds one
static void GtsNego_StateSearching(GtsNegoWork *wk) {
    GtsNegoMessage_UpdateButtons(wk->msg);
    if (GCTX_HIDGetPressedKeys() == PAD_BUTTON_B) {
        GtsNegoMessage_PressCancelButton(wk->msg);
        if (GFL_NetIsInitialized()) {
            NetState_WifiDisconnect(TRUE);
        }
        GCTX_HIDSetUsingTouch(FALSE);
        GFL_SndSEPlay(SEQ_SE_CANCEL1);
        GtsNego_ChangeState(wk, GtsNego_StateWaitCancelPressed, 974);
    }
    if (!GFL_NetIsInitialized()) {
        if (GCTX_HIDGetPressedKeys() != PAD_BUTTON_A) {
            return;
        }
    } else {
        if (NetState_GetWifiStatus() != 1) {
            return;
        }
        func_02042e94(1);
        GFL_NetSetDisconnectError(TRUE);
    }
    GtsNegoMessage_ClearWindow(wk->msg);
    GtsNegoMessage_PrintShort(wk->msg, GtsnegoProc_Text_GettingInformationFromOther);
    wk->timer = 180;
    GtsNegoMessage_FreeCancelButton(wk->msg);
    GtsNego_ChangeState(wk, GtsNego_StateWaitMatched, 997);
}

// How well the machine at index fits the search: a person to meet or not, or by how the settings agree; someone
// found before does not fit
static int GtsNego_EvalMatch(int index, void *work) {
    GtsNegoWork *wk = work;
    GtsNegoParam *param = wk->param;
    int score = -1;
    int level = DWC_GetMatchIntValue(index, wk->keys[GTSNEGO_KEY_LEVEL].key, -1);
    int wantKind = DWC_GetMatchIntValue(index, wk->keys[GTSNEGO_KEY_WANT_KIND].key, -1);
    int myKind = DWC_GetMatchIntValue(index, wk->keys[GTSNEGO_KEY_MY_KIND].key, -1);
    u32 id = DWC_GetMatchIntValue(index, wk->keys[GTSNEGO_KEY_MY_ID].key, 0);
    u32 friendId = DWC_GetMatchIntValue(index, wk->keys[GTSNEGO_KEY_FRIEND_ID].key, 0);
    int i;

    if (wk->mode == GTSNEGO_MODE_RENDEZVOUS) {
        if (level == GTSNEGO_LEVEL_RENDEZVOUS) {
            if (wk->friendIndex == -1) {
                if (PalPadFriendList_HasPlayer(getPalPadFriendListAddress(wk->save), id)) {
                    score = 100;
                }
            } else if (wk->friendId == id) {
                score = 100;
            }
        } else if (friendId == getIDAsUInt(GetGameDataPlayerInfo(param->gameData))) {
            if (wk->friendIndex == -1) {
                if (PalPadFriendList_HasPlayer(getPalPadFriendListAddress(wk->save), id)) {
                    score = 100;
                }
            } else if (wk->friendId == id) {
                score = 100;
            }
        }
    } else if (wk->level == level) {
        if (wk->myKind == wantKind) {
            score += 20;
        } else if (wk->myKind == 0 || wantKind == 0) {
            score += 10;
        } else {
            score -= 40;
        }
        if (wk->wantKind == myKind) {
            score += 20;
        } else if (myKind == 0 || wk->wantKind == 0) {
            score += 10;
        } else {
            score -= 40;
        }
        for (i = 0; i < GTSNEGO_MET_COUNT; i++) {
            if (id == param->metIds[i] && id != 0) {
                score = 0;
                break;
            }
        }
    }
    return score;
}

// Sets the matching keys and starts the search
static void GtsNego_StateStartSearch(GtsNegoWork *wk) {
    if (GtsNegoMessage_IsDone(wk->msg)) {
        int values[GTSNEGO_KEY_COUNT];
        PlayerInfo *friend;
        int i;

        if (!GFL_NetIsInitialized()) {
            GtsNegoMessage_CreateCancelButton(wk->msg, GtsNego_OnCancelButton, wk);
            GtsNego_ChangeState(wk, GtsNego_StateSearching, 1112);
            return;
        }
        friend = NULL;
        GFL_NetSetMPSendEnabled(FALSE);
        // The token is made in the values' buffer, which they then overwrite
        WifiList_CreateExchangeTokenFromUserData(&wk->wifiList->myUserData, (DWCFriendData *)values);
        values[GTSNEGO_KEY_MY_ID] = wk->myId;
        values[GTSNEGO_KEY_MODE] = wk->mode;
        values[GTSNEGO_KEY_LEVEL] = wk->level;
        values[GTSNEGO_KEY_MY_KIND] = wk->myKind;
        values[GTSNEGO_KEY_WANT_KIND] = wk->wantKind;
        values[GTSNEGO_KEY_ROM] = 0x20000 | GAME_VERSION;
        values[GTSNEGO_KEY_DEBUG] = 1;
        values[GTSNEGO_KEY_FRIEND_ID] = 0;
        if (wk->mode == GTSNEGO_MODE_RENDEZVOUS) {
            if (wk->friendIndex == -1) {
                values[GTSNEGO_KEY_LEVEL] = GTSNEGO_LEVEL_RENDEZVOUS;
            } else {
                friend = GtsNego_GetFriend(wk->gameData, wk->friendIndex);
                values[GTSNEGO_KEY_FRIEND_ID] = getIDAsUInt(friend);
                wk->friendId = getIDAsUInt(friend);
            }
        }
        for (i = 0; i < GTSNEGO_KEY_COUNT; i++) {
            GtsNegoMatchKey *key = &wk->keys[i];

            MI_CpuCopy8(sGtsNegoKeyNames[i], key->key, 2);
            key->value = values[i];
            key->keyId = DWC_AddMatchKeyInt(key->keyId, key->key, &key->value);
        }
        STD_TSPrintf(wk->query, "ty=%d And db=%d", wk->mode, 1);
        if (DWCRap_StartMatchQuery(wk->query, 2, GtsNego_EvalMatch, wk)) {
            NetState_WaitGtsMatch();
            DWCRap_SetVoiceChatEnabled(0);
            GtsNegoMessage_CreateCancelButton(wk->msg, GtsNego_OnCancelButton, wk);
            GtsNego_ChangeState(wk, GtsNego_StateSearching, 1163);
        } else {
            NetState_RecordError(0, 0, 0, 1009);
        }
    }
}

// A button of the lower screen's menu was touched
static void GtsNego_OnButton(u32 button, u32 event, void *work) {
    GtsNegoWork *wk = work;

    if (event == 0) {
        if (wk->buttonCallback != NULL) {
            GtsNegoDisp_SetCursor(wk->disp, wk->button, GTSNEGO_CURSOR_NONE);
            GCTX_HIDSetUsingTouch(TRUE);
            if (wk->buttonCallback(button, wk)) {
                return;
            }
        }
    }
}

// Once the search button has flashed, shows the search
static void GtsNego_StateWaitSearchPressed(GtsNegoWork *wk) {
    if (AppTaskMenuWin_IsFlashFinished(wk->button)) {
        AppTaskMenuWin_Free(wk->button);
        wk->button = NULL;
        GFL_BGSysQueueScrLoad(5);
        GtsNegoMessage_Print(wk->msg, GtsnegoProc_Text_SearchingRightPersonTrade);
        GtsNegoDisp_ShowSearching(wk->disp);
        GtsNegoMessage_PrintTitle(wk->msg, GtsnegoProc_Text_TradeSearch);
        GtsNegoDisp_SetDim(wk->disp, TRUE, GTSNEGO_DIM_SEARCH);
        wk->param->result = GTSNEGO_RESULT_ANYONE;
        GtsNegoDisp_SetCursor(wk->disp, NULL, GTSNEGO_CURSOR_NONE);
        GtsNego_ChangeState(wk, GtsNego_StateStartSearch, 1230);
    }
}

// The search's settings: the keys move the cursor and change a setting, and the button starts the search
static void GtsNego_StateSearchMenu(GtsNegoWork *wk) {
    BOOL moved = FALSE;
    BOOL back = FALSE;
    TouchBar *bar = GtsNegoDisp_GetTouchBar(wk->disp);

    if (!GtsNegoMessage_IsDone(wk->msg)) {
        return;
    }
    if (!TouchBar_IsTouching(bar)) {
        GtsNegoMessage_UpdateButtons(wk->msg);
        if (GCTX_HIDGetPressedKeys() != 0 && GCTX_HIDIsUsingTouch() == TRUE) {
            GtsNegoDisp_SetCursor(wk->disp, wk->button, wk->searchCursor);
            GCTX_HIDSetUsingTouch(FALSE);
            return;
        }
        if (GCTX_HIDGetPressedKeys() == PAD_KEY_LEFT) {
            GtsNego_PressArrowOfCursor(0, wk);
        }
        if (GCTX_HIDGetPressedKeys() == PAD_KEY_RIGHT) {
            GtsNego_PressArrowOfCursor(1, wk);
        }
        if (GCTX_HIDGetPressedKeys() == PAD_KEY_UP) {
            GFL_SndSEPlay(SEQ_SE_SELECT1);
            moved = TRUE;
            if (wk->searchCursor != GTSNEGO_CURSOR_LEVEL) {
                wk->searchCursor--;
            }
        }
        if (GCTX_HIDGetPressedKeys() == PAD_KEY_DOWN) {
            GFL_SndSEPlay(SEQ_SE_SELECT1);
            moved = TRUE;
            if (wk->searchCursor != GTSNEGO_CURSOR_SEARCH) {
                wk->searchCursor++;
            }
        }
        if (GCTX_HIDGetPressedKeys() == PAD_BUTTON_A && wk->searchCursor == GTSNEGO_CURSOR_SEARCH) {
            GFL_SndSEPlay(SEQ_SE_DECIDE1);
            GtsNegoDisp_PressCursor(wk->disp, wk->searchCursor);
            AppTaskMenuWin_SetFlashing(wk->button, TRUE);
            GtsNego_ChangeState(wk, GtsNego_StateWaitSearchPressed, 1336);
            return;
        }
        if (moved) {
            GtsNegoDisp_SetCursor(wk->disp, wk->button, wk->searchCursor);
        }
        if (GFL_HIDGetTouchPressedRect(sGtsNegoSearchRect) == 0) {
            GCTX_HIDSetUsingTouch(TRUE);
            GFL_SndSEPlay(SEQ_SE_DECIDE1);
            AppTaskMenuWin_SetFlashing(wk->button, TRUE);
            GtsNego_ChangeState(wk, GtsNego_StateWaitSearchPressed, 1376);
            return;
        }
    }
    TouchBar_Main(GtsNegoDisp_GetTouchBar(wk->disp));
    if (TouchBar_GetDecided(GtsNegoDisp_GetTouchBar(wk->disp)) == TOUCHBAR_ICON_RETURN) {
        back = TRUE;
    }
    if (back) {
        GFL_BGSysSetBGEnabled(4, FALSE);
        GtsNegoDisp_SetCursor(wk->disp, wk->button, GTSNEGO_CURSOR_NONE);
        GtsNegoMessage_HideMenu(wk->msg);
        GtsNegoDisp_HideSearchMenu(wk->disp);
        AppTaskMenuWin_Free(wk->button);
        wk->button = NULL;
        GFL_BGSysQueueScrLoad(5);
        GtsNego_ChangeState(wk, GtsNego_StateOpenMainMenu, 1401);
    }
}

static void GtsNego_StateOpenSearchMenu(GtsNegoWork *wk) {
    wk->mode = GTSNEGO_MODE_ANYONE;
    TouchBar_SetVisible(GtsNegoDisp_GetTouchBar(wk->disp), TRUE);
    GtsNegoMessage_ClearLabels(wk->msg);
    GtsNegoDisp_ShowSearchMenu(wk->disp);
    wk->buttonCallback = GtsNego_OnSearchButton;
    GtsNegoMessage_ShowSearchMenu(wk->msg, GtsNego_OnButton, wk, wk->level, wk->myKind, wk->wantKind);
    GtsNegoMessage_Print(wk->msg, GtsnegoProc_Text_PleasePressSearchWhen);
    wk->button = GtsNegoMessage_CreateButton(wk->msg, GtsnegoProc_Text_Search);
    if (!GCTX_HIDIsUsingTouch()) {
        GtsNegoDisp_SetCursor(wk->disp, NULL, wk->searchCursor);
        if (wk->searchCursor == GTSNEGO_CURSOR_SEARCH) {
            AppTaskMenuWin_SetActive(wk->button, TRUE);
        }
    } else {
        GtsNegoDisp_SetCursor(wk->disp, NULL, GTSNEGO_CURSOR_NONE);
    }
    GFL_BGSysQueueScrLoad(6);
    GtsNego_ChangeState(wk, GtsNego_StateSearchMenu, 1445);
}

PlayerInfo *GtsNego_GetFriend(GameData *gameData, int index) {
    int count = PalPadFriendList_CountPlayers(GameData_GetPalPadFriendList(gameData));

    if (index >= count || index < 0) {
        return NULL;
    }
    return PalPadFriendList_GetPlayer(GameData_GetPalPadFriendList(gameData), index);
}

// The person at index, or the first or last when it is past the list's ends
static PlayerInfo *GtsNego_GetFriendClamped(GameData *gameData, int index) {
    int count = PalPadFriendList_CountPlayers(GameData_GetPalPadFriendList(gameData));

    if (index < 0) {
        index = 0;
    }
    if (index >= count) {
        index = count - 1;
    }
    return PalPadFriendList_GetPlayer(GameData_GetPalPadFriendList(gameData), index);
}

static void GtsNego_StateReturnToMainMenu(GtsNegoWork *wk) {
    GtsNego_StateOpenMainMenu(wk);
    GFL_BGSysSetEnabledBGsB(GX_PLANEMASK_ALL);
}

static void GtsNego_StateResetScreens(GtsNegoWork *wk) {
    GtsNegoDisp_Reset(wk->disp);
    GtsNegoMessage_Reset(wk->msg);
    GtsNego_ChangeState(wk, GtsNego_StateReturnToMainMenu, 1587);
}

// Once the cancel button has flashed, leaves the search
static void GtsNego_StateWaitCancelPressed(GtsNegoWork *wk) {
    if (GtsNegoMessage_IsCancelPressDone(wk->msg)) {
        GFL_BGSysSetEnabledBGsB(GX_PLANEMASK_BG3);
        GtsNegoMessage_HideMenu(wk->msg);
        GtsNegoDisp_HideSearchMenu(wk->disp);
        GtsNegoDisp_SetDim(wk->disp, FALSE, GTSNEGO_DIM_ALL);
        GtsNegoDisp_SetCursor(wk->disp, NULL, GTSNEGO_CURSOR_NONE);
        GFL_BGSysQueueScrLoad(5);
        if (wk->mode == GTSNEGO_MODE_RENDEZVOUS) {
            GFL_BGSysSetBGEnabled(4, FALSE);
            GtsNegoDisp_HideFriendList(wk->disp);
        }
        GtsNego_ChangeState(wk, GtsNego_StateResetScreens, 1611);
    }
}

// The cancel button was touched
static void GtsNego_OnCancelButton(u32 button, u32 event, void *work) {
    GtsNegoWork *wk = work;

    if (event == 0) {
        if (wk->state == GtsNego_StateSearching) {
            GtsNegoMessage_PressCancelButton(wk->msg);
            NetState_WifiDisconnect(TRUE);
            GCTX_HIDSetUsingTouch(TRUE);
            GFL_SndSEPlay(SEQ_SE_CANCEL1);
            GtsNego_ChangeState(wk, GtsNego_StateWaitCancelPressed, 1635);
        }
    }
}

static void GtsNego_StateRecreateFriendButton(GtsNegoWork *wk) {
    wk->button = GtsNegoMessage_CreateButton(wk->msg, GtsnegoProc_Text_RendezvousAny);
    GtsNego_ChangeState(wk, GtsNego_StateFriendList, 1645);
}

// The answer to meeting the person picked: search, or back to the list
static void GtsNego_StateWaitFriendConfirm(GtsNegoWork *wk) {
    if (AppTaskMenu_IsFlashFinished(wk->menu)) {
        int pos = AppTaskMenu_GetCursorPos(wk->menu);

        GtsNegoMessage_FreeMenu(wk->menu);
        wk->menu = NULL;
        TouchBar_SetIconVisible(GtsNegoDisp_GetTouchBar(wk->disp), TOUCHBAR_ICON_RETURN, TRUE);
        switch (pos) {
        case 0:
            GtsNegoMessage_Print(wk->msg, GtsnegoProc_Text_SearchingRightPersonTrade);
            GtsNegoDisp_ShowSearching(wk->disp);
            GtsNegoMessage_PrintTitle(wk->msg, GtsnegoProc_Text_TradeSearch);
            wk->level = 0;
            wk->myKind = 0;
            wk->wantKind = 0;
            wk->param->result = GTSNEGO_RESULT_RENDEZVOUS;
            GtsNego_ChangeState(wk, GtsNego_StateStartSearch, 1666);
            break;
        case 1:
            GtsNegoDisp_SetDim(wk->disp, FALSE, GTSNEGO_DIM_ALL);
            GtsNegoMessage_Print(wk->msg, GtsnegoProc_Text_PleaseSelectWhoWish);
            if (!GCTX_HIDIsUsingTouch()) {
                GtsNegoDisp_SetCursor(wk->disp, NULL, wk->friendCursor);
            } else {
                GtsNegoDisp_SetCursor(wk->disp, NULL, GTSNEGO_CURSOR_NONE);
            }
            GtsNegoDisp_SetKnobAnimating(wk->disp, TRUE);
            GtsNego_ChangeState(wk, GtsNego_StateRecreateFriendButton, 1678);
            break;
        }
    }
}

static void GtsNego_StateOpenFriendConfirm(GtsNegoWork *wk) {
    if (GtsNegoMessage_IsDone(wk->msg)) {
        TouchBar_SetIconVisible(GtsNegoDisp_GetTouchBar(wk->disp), TOUCHBAR_ICON_RETURN, FALSE);
        wk->menu = GtsNegoMessage_CreateConfirm(wk->msg, 1);
        GtsNegoDisp_SetDim(wk->disp, TRUE, GTSNEGO_DIM_LIST_MENU);
        GtsNegoDisp_SetKnobAnimating(wk->disp, FALSE);
        GtsNego_ChangeState(wk, GtsNego_StateWaitFriendConfirm, 1698);
    }
}

static void GtsNego_StatePrintFriendChosen(GtsNegoWork *wk) {
    if (wk->friendIndex == -1) {
        GtsNegoMessage_Print(wk->msg, GtsnegoProc_Text_WillTradeSomeoneHave);
    } else {
        GtsNegoMessage_Print(wk->msg, GtsnegoProc_Text_WillRendezvousPerson);
    }
    GtsNego_ChangeState(wk, GtsNego_StateOpenFriendConfirm, 1716);
}

static void GtsNego_StateLeaveFriendList(GtsNegoWork *wk) {
    GFL_BGSysSetBGEnabled(4, FALSE);
    GtsNegoDisp_HideFriendList(wk->disp);
    GtsNego_ChangeState(wk, GtsNego_StateOpenMainMenu, 1724);
}

// Up in the list of people: moves the cursor, or scrolls the list, the person above coming in
static int GtsNego_FriendListUp(GtsNegoWork *wk) {
    int moved = GtsNegoDisp_ScrollUp(wk->disp, &wk->scroll);

    switch (moved) {
    case GTSNEGO_SCROLL_NONE:
        break;
    case GTSNEGO_SCROLL_CURSOR:
        if (wk->friendCursor != GTSNEGO_CURSOR_FRIEND_1) {
            wk->friendCursor--;
        }
        break;
    case GTSNEGO_SCROLL_LIST:
        GtsNegoDisp_StartListScroll(wk->disp, GTSNEGO_LIST_DOWN);
        if (wk->scroll.top - 1 >= 0) {
            GtsNegoMessage_PrintFirstFriend(wk->msg, wk->gameData, wk->scroll.top - 1);
        }
        break;
    }
    return moved;
}

static int GtsNego_FriendListDown(GtsNegoWork *wk) {
    int moved = GtsNegoDisp_ScrollDown(wk->disp, &wk->scroll);

    switch (moved) {
    case GTSNEGO_SCROLL_NONE:
        break;
    case GTSNEGO_SCROLL_CURSOR:
        if (wk->friendCursor != GTSNEGO_CURSOR_FRIEND_3) {
            wk->friendCursor++;
        }
        break;
    case GTSNEGO_SCROLL_LIST:
        GtsNegoDisp_StartListScroll(wk->disp, GTSNEGO_LIST_UP);
        GtsNegoMessage_PrintLastFriend(wk->msg, wk->gameData, wk->scroll.top + 2);
        break;
    }
    return moved;
}

// Once the button for anyone of the list has flashed, asks to confirm
static void GtsNego_StateWaitAnyFriendPressed(GtsNegoWork *wk) {
    if (AppTaskMenuWin_IsFlashFinished(wk->button)) {
        AppTaskMenuWin_Free(wk->button);
        wk->button = NULL;
        wk->friendIndex = -1;
        GtsNegoDisp_SetCursor(wk->disp, NULL, GTSNEGO_CURSOR_NONE);
        GFL_BGSysQueueScrLoad(5);
        GtsNego_ChangeState(wk, GtsNego_StatePrintFriendChosen, 1832);
    }
}

static void GtsNego_FreeButton(GtsNegoWork *wk) {
    if (wk->button != NULL) {
        AppTaskMenuWin_Free(wk->button);
        GFL_BGSysQueueScrLoad(5);
        wk->button = NULL;
    }
}

// The list of people to meet: the keys and the touch screen move in it and pick a person, L and R move by a page,
// and the scroll bar is dragged
static void GtsNego_StateFriendList(GtsNegoWork *wk) {
    int moved = 0;
    int ended = 0;
    TouchBar *bar = GtsNegoDisp_GetTouchBar(wk->disp);
    u32 x;
    u32 y;
    int i;

    if (!GtsNegoMessage_IsDone(wk->msg)) {
        return;
    }
    if (!TouchBar_IsTouching(bar)) {
        int scrolling = GtsNegoDisp_UpdateListScroll(wk->disp, &ended);
        int touched;

        if (ended == GTSNEGO_LIST_DOWN) {
            GtsNegoDisp_DrawListRows(wk->disp, wk->gameData, wk->scroll.top - 2);
            GtsNegoMessage_ShiftFriendsDown(wk->msg);
            GtsNegoDisp_ShiftFriendIconsDown(wk->disp, GtsNego_GetFriendClamped(wk->gameData, wk->scroll.top - 2));
        } else if (ended == GTSNEGO_LIST_UP) {
            GtsNegoDisp_DrawListRows(wk->disp, wk->gameData, wk->scroll.top - 2);
            GtsNegoMessage_ShiftFriendsUp(wk->msg);
            GtsNegoDisp_ShiftFriendIconsUp(wk->disp, GtsNego_GetFriendClamped(wk->gameData, wk->scroll.top + 3));
        }
        if (scrolling != GTSNEGO_LIST_STILL) {
            return;
        }
        if (GCTX_HIDIsUsingTouch() == TRUE && GCTX_HIDGetPressedKeys() != 0) {
            GtsNegoDisp_SetCursor(wk->disp, NULL, wk->friendCursor);
            GCTX_HIDSetUsingTouch(FALSE);
            return;
        }
        if (GCTX_HIDGetTypedKeys() == PAD_BUTTON_R) {
            moved = 0;
            for (i = 0; i < GTSNEGO_LIST_SHOWN; i++) {
                int result = GtsNego_FriendListDown(wk);

                moved |= result;
                GtsNegoDisp_StopListScroll(wk->disp);
                if (result == GTSNEGO_SCROLL_NONE) {
                    break;
                }
                if (result == GTSNEGO_SCROLL_CURSOR) {
                    i--;
                } else {
                    GtsNegoDisp_MoveFriendIcons(wk->disp, TRUE);
                    GtsNegoDisp_DrawListRows(wk->disp, wk->gameData, wk->scroll.top - 2);
                    GtsNegoMessage_ShiftFriendsUp(wk->msg);
                    GtsNegoDisp_ShiftFriendIconsUp(wk->disp,
                                                   GtsNego_GetFriendClamped(wk->gameData, wk->scroll.top + 3));
                    GtsNegoDisp_SetCursor(wk->disp, NULL, wk->friendCursor);
                }
            }
        }
        if (GCTX_HIDGetTypedKeys() == PAD_BUTTON_L) {
            moved = 0;
            for (i = 0; i < GTSNEGO_LIST_SHOWN; i++) {
                int result = GtsNego_FriendListUp(wk);

                moved |= result;
                GtsNegoDisp_StopListScroll(wk->disp);
                if (result == GTSNEGO_SCROLL_NONE) {
                    break;
                }
                if (result == GTSNEGO_SCROLL_CURSOR) {
                    i--;
                } else {
                    GtsNegoDisp_SetCursor(wk->disp, NULL, wk->friendCursor);
                    GtsNegoDisp_MoveFriendIcons(wk->disp, FALSE);
                    GtsNegoDisp_DrawListRows(wk->disp, wk->gameData, wk->scroll.top - 2);
                    GtsNegoMessage_ShiftFriendsDown(wk->msg);
                    GtsNegoDisp_ShiftFriendIconsDown(wk->disp,
                                                     GtsNego_GetFriendClamped(wk->gameData, wk->scroll.top - 2));
                }
            }
        }
        if (GCTX_HIDGetTypedKeys() == PAD_KEY_UP) {
            moved = GtsNego_FriendListUp(wk);
        }
        if (GCTX_HIDGetTypedKeys() == PAD_KEY_DOWN) {
            moved = GtsNego_FriendListDown(wk);
        }
        if (moved) {
            GFL_SndSEPlay(SEQ_SE_SELECT1);
            GtsNegoDisp_SetKnob(wk->disp, wk->scroll.top + wk->friendCursor - GTSNEGO_CURSOR_FRIEND_1,
                                wk->scroll.count);
            GtsNegoDisp_SetCursor(wk->disp, NULL, wk->friendCursor);
            return;
        }
        if (GCTX_HIDGetPressedKeys() == PAD_BUTTON_A) {
            GFL_SndSEPlay(SEQ_SE_DECIDE1);
            GtsNegoDisp_PressCursor(wk->disp, wk->friendCursor);
            wk->friendIndex = wk->scroll.top + wk->friendCursor - GTSNEGO_CURSOR_FRIEND_1;
            GtsNego_FreeButton(wk);
            GtsNego_ChangeState(wk, GtsNego_StatePrintFriendChosen, 1998);
            return;
        }
        if (GCTX_HIDGetPressedKeys() == PAD_BUTTON_START) {
            GFL_SndSEPlay(SEQ_SE_DECIDE1);
            AppTaskMenuWin_SetFlashing(wk->button, TRUE);
            GtsNego_ChangeState(wk, GtsNego_StateWaitAnyFriendPressed, 2004);
            return;
        }
        touched = GFL_HIDGetTouchPressedRect(sGtsNegoFriendRects);
        switch (touched) {
        case 3:
            GFL_SndSEPlay(SEQ_SE_DECIDE1);
            AppTaskMenuWin_SetFlashing(wk->button, TRUE);
            GtsNego_ChangeState(wk, GtsNego_StateWaitAnyFriendPressed, 2014);
            return;
        case 2:
            if (wk->scroll.count < 3) {
                break;
            }
            // fallthrough
        case 1:
            if (wk->scroll.count < 2) {
                break;
            }
            // fallthrough
        case 0:
            GCTX_HIDSetUsingTouch(TRUE);
            wk->friendCursor = touched + GTSNEGO_CURSOR_FRIEND_1;
            GtsNegoDisp_SetCursor(wk->disp, NULL, wk->friendCursor);
            GtsNegoDisp_PressCursor(wk->disp, wk->friendCursor);
            wk->friendIndex = wk->scroll.top + wk->friendCursor - GTSNEGO_CURSOR_FRIEND_1;
            GtsNegoDisp_SetScrollCursor(wk->disp, &wk->scroll, touched);
            GtsNegoDisp_SetKnob(wk->disp, wk->scroll.top + wk->friendCursor - GTSNEGO_CURSOR_FRIEND_1,
                                wk->scroll.count);
            GFL_SndSEPlay(SEQ_SE_DECIDE1);
            GtsNego_FreeButton(wk);
            GtsNego_ChangeState(wk, GtsNego_StatePrintFriendChosen, 2041);
            return;
        }
        if (GFL_HIDGetTouchHeldRect(sGtsNegoScrollBarRect) == 0) {
            if (GFL_HIDGetTouchHeldPoint(&x, &y) == TRUE) {
                int top = GtsNegoDisp_DragKnob(wk->disp, y, wk->scroll.count);

                if (wk->scroll.top != top) {
                    wk->scroll.top = top;
                    GtsNegoMessage_PrintFriends(wk->msg, wk->gameData, top - 2);
                    GtsNegoDisp_SetFriendIcons(wk->disp, wk->gameData, wk->scroll.top - 2);
                    GtsNegoDisp_DrawListRows(wk->disp, wk->gameData, wk->scroll.top - 2);
                    GFL_SndSEPlay(SEQ_SE_SELECT1);
                }
            }
            return;
        }
    }
    TouchBar_Main(GtsNegoDisp_GetTouchBar(wk->disp));
    if (TouchBar_GetDecided(GtsNegoDisp_GetTouchBar(wk->disp)) == TOUCHBAR_ICON_RETURN) {
        GFL_BGSysSetBGEnabled(4, FALSE);
        GFL_BGSysSetBGEnabled(6, FALSE);
        GtsNego_FreeButton(wk);
        GtsNegoDisp_SetCursor(wk->disp, wk->button, GTSNEGO_CURSOR_NONE);
        GtsNegoDisp_LeaveFriendList(wk->disp);
        GtsNegoMessage_ClearLabels(wk->msg);
        GtsNego_ChangeState(wk, GtsNego_StateLeaveFriendList, 2075);
    }
}

// With nobody in the Pal Pad, goes back to the main menu after the message
static void GtsNego_StateWaitNoFriends(GtsNegoWork *wk) {
    if (GtsNegoMessage_IsDone(wk->msg)) {
        wk->timer--;
        if (wk->timer <= 0) {
            GtsNegoMessage_Print(wk->msg, GtsnegoProc_Text_PleaseSelectHowTrade);
            TouchBar_SetVisible(GtsNegoDisp_GetTouchBar(wk->disp), TRUE);
            GtsNego_ChangeState(wk, GtsNego_StateMainMenu, 2101);
            if (!GCTX_HIDIsUsingTouch()) {
                GtsNegoDisp_SetCursor(wk->disp, NULL, wk->mainCursor);
            }
        }
    }
}

static void GtsNego_StateOpenFriendList(GtsNegoWork *wk) {
    BOOL scrollable = TRUE;
    int count = PalPadFriendList_CountPlayers(GameData_GetPalPadFriendList(wk->gameData));

    wk->mode = GTSNEGO_MODE_RENDEZVOUS;
    if (count == 0) {
        GtsNegoMessage_Print(wk->msg, GtsnegoProc_Text_HaveNoOneRendezvous);
        wk->timer = 120;
        GtsNego_ChangeState(wk, GtsNego_StateWaitNoFriends, 2133);
        return;
    }
    wk->scroll.count = count;
    GtsNegoMessage_Print(wk->msg, GtsnegoProc_Text_PleaseSelectWhoWish);
    if (!GCTX_HIDIsUsingTouch()) {
        GtsNegoDisp_SetCursor(wk->disp, NULL, wk->friendCursor);
    } else {
        GtsNegoDisp_SetCursor(wk->disp, NULL, GTSNEGO_CURSOR_NONE);
    }
    GtsNegoMessage_ClearLabels(wk->msg);
    if (count <= GTSNEGO_LIST_SHOWN) {
        scrollable = FALSE;
    }
    GtsNegoDisp_ShowFriendList(wk->disp, scrollable);
    GtsNegoMessage_ShowFriendList(wk->msg, wk->gameData);
    GtsNegoMessage_PrintFriends(wk->msg, wk->gameData, wk->scroll.top - 2);
    GtsNegoDisp_SetFriendIcons(wk->disp, wk->gameData, wk->scroll.top - 2);
    GtsNegoDisp_DrawListRows(wk->disp, wk->gameData, wk->scroll.top - 2);
    wk->button = GtsNegoMessage_CreateButton(wk->msg, GtsnegoProc_Text_RendezvousAny);
    GtsNegoDisp_SetKnob(wk->disp, wk->scroll.top + wk->friendCursor - GTSNEGO_CURSOR_FRIEND_1, wk->scroll.count);
    GtsNego_ChangeState(wk, GtsNego_StateFriendList, 2179);
}

// Shows the main menu, a few frames after the error screen when it was shown
static void GtsNego_StateOpenMainMenu(GtsNegoWork *wk) {
    GtsNegoDisp_ResetList(wk->disp);
    if (wk->errorShown) {
        if (wk->errorFrames++ < 3) {
            return;
        }
        GFL_NetErrApplyBrightness();
        wk->errorShown = FALSE;
    }
    if (!GCTX_HIDIsUsingTouch()) {
        GtsNegoDisp_SetCursor(wk->disp, NULL, wk->mainCursor);
    } else {
        GtsNegoDisp_SetCursor(wk->disp, NULL, GTSNEGO_CURSOR_NONE);
    }
    GtsNegoMessage_ShowMainMenu(wk->msg, GtsNego_OnButton, wk);
    wk->buttonCallback = GtsNego_OnMainButton;
    if (howManyPartyPokesAreNotEggs(GameData_GetParty(wk->gameData)) < 2) {
        wk->notEnoughPokemon = TRUE;
    }
    if (!wk->notEnoughPokemon) {
        GtsNegoMessage_Print(wk->msg, GtsnegoProc_Text_PleaseSelectHowTrade);
    } else {
        GtsNegoMessage_Print(wk->msg, GtsnegoProc_Text_NeedLeastTwoPokemon);
    }
    GFL_BGSysSetBGEnabled(4, TRUE);
    GFL_BGSysSetBGEnabled(6, TRUE);
    GtsNego_ChangeState(wk, GtsNego_StateMainMenu, 2266);
}

// Plays the cursor's press, returning the cursor, or none when there is no cursor
static int GtsNego_PressCursor(GtsNegoWork *wk, int cursor) {
    if (GtsNegoDisp_PressCursor(wk->disp, cursor)) {
        TouchBar_SetVisible(GtsNegoDisp_GetTouchBar(wk->disp), FALSE);
        GFL_SndSEPlay(SEQ_SE_DECIDE1);
        return cursor;
    }
    return GTSNEGO_CURSOR_NONE;
}

// The cursor once its press has played, none until then
static int GtsNego_GetPressedCursor(GtsNegoWork *wk, int cursor) {
    if (!GtsNegoDisp_IsCursorPressDone(wk->disp)) {
        cursor = GTSNEGO_CURSOR_NONE;
    }
    return cursor;
}

static void GtsNego_StateWaitMainPressed(GtsNegoWork *wk) {
    int cursor = GtsNego_GetPressedCursor(wk, wk->mainCursor);

    if (cursor == GTSNEGO_CURSOR_ANYONE) {
        GtsNegoDisp_SetCursor(wk->disp, wk->button, GTSNEGO_CURSOR_NONE);
        GtsNegoMessage_ClearLabels(wk->msg);
        GFL_BGSysQueueScrLoad(6);
        GtsNego_ChangeState(wk, GtsNego_StateOpenSearchMenu, 2321);
    } else if (cursor == GTSNEGO_CURSOR_RENDEZVOUS) {
        GtsNegoDisp_SetCursor(wk->disp, wk->button, GTSNEGO_CURSOR_NONE);
        GtsNego_ChangeState(wk, GtsNego_StateOpenFriendList, 2325);
    }
}

// The answer to leaving: fade out, or back to the main menu
static void GtsNego_StateWaitExitConfirm(GtsNegoWork *wk) {
    if (AppTaskMenu_IsFlashFinished(wk->menu)) {
        int pos = AppTaskMenu_GetCursorPos(wk->menu);

        GtsNegoMessage_FreeMenu(wk->menu);
        wk->menu = NULL;
        switch (pos) {
        case 0:
            wk->param->result = GTSNEGO_RESULT_EXIT;
            wk->timer = 0;
            GtsNego_ChangeState(wk, GtsNego_StateWaitThenFadeOut, 2339);
            break;
        case 1:
            if (!wk->notEnoughPokemon) {
                GtsNegoMessage_Print(wk->msg, GtsnegoProc_Text_PleaseSelectHowTrade);
            } else {
                GtsNegoMessage_Print(wk->msg, GtsnegoProc_Text_NeedLeastTwoPokemon);
            }
            if (!GCTX_HIDIsUsingTouch()) {
                GtsNegoDisp_SetCursor(wk->disp, NULL, wk->mainCursor);
            }
            TouchBar_SetIconVisible(GtsNegoDisp_GetTouchBar(wk->disp), TOUCHBAR_ICON_RETURN, TRUE);
            GtsNegoDisp_SetDim(wk->disp, FALSE, GTSNEGO_DIM_ALL);
            GtsNego_ChangeState(wk, GtsNego_StateMainMenu, 2353);
            break;
        }
    }
}

static void GtsNego_StateOpenExitConfirm(GtsNegoWork *wk) {
    if (GtsNegoMessage_IsDone(wk->msg)) {
        TouchBar_SetIconVisible(GtsNegoDisp_GetTouchBar(wk->disp), TOUCHBAR_ICON_RETURN, FALSE);
        wk->menu = GtsNegoMessage_CreateYesNo(wk->msg, 1);
        GtsNegoDisp_SetDim(wk->disp, TRUE, GTSNEGO_DIM_MAIN_MENU);
        GtsNegoDisp_SetCursor(wk->disp, NULL, GTSNEGO_CURSOR_NONE);
        GtsNego_ChangeState(wk, GtsNego_StateWaitExitConfirm, 2370);
    }
}

// The main menu: a trade with anyone or a rendezvous, and the touch bar's button to leave
static void GtsNego_StateMainMenu(GtsNegoWork *wk) {
    BOOL moved = FALSE;
    TouchBar *bar = GtsNegoDisp_GetTouchBar(wk->disp);

    if (!GtsNegoMessage_IsDone(wk->msg)) {
        return;
    }
    if (!TouchBar_IsTouching(bar)) {
        if (GCTX_HIDGetPressedKeys() != 0 && GCTX_HIDIsUsingTouch() == TRUE) {
            GtsNegoDisp_SetCursor(wk->disp, NULL, wk->mainCursor);
            GCTX_HIDSetUsingTouch(FALSE);
            return;
        }
        if (GCTX_HIDGetPressedKeys() == PAD_BUTTON_A) {
            if (!wk->notEnoughPokemon) {
                GtsNego_PressCursor(wk, wk->mainCursor);
                GtsNego_ChangeState(wk, GtsNego_StateWaitMainPressed, 2402);
                return;
            }
            GFL_SndSEPlay(SEQ_SE_CANCEL1);
        }
        if (GCTX_HIDGetPressedKeys() == PAD_KEY_UP) {
            moved = TRUE;
            GFL_SndSEPlay(SEQ_SE_SELECT1);
            wk->mainCursor = GTSNEGO_CURSOR_ANYONE;
        }
        if (GCTX_HIDGetPressedKeys() == PAD_KEY_DOWN) {
            moved = TRUE;
            GFL_SndSEPlay(SEQ_SE_SELECT1);
            wk->mainCursor = GTSNEGO_CURSOR_RENDEZVOUS;
        }
        if (moved) {
            GtsNegoDisp_SetCursor(wk->disp, NULL, wk->mainCursor);
        }
        GtsNegoMessage_UpdateButtons(wk->msg);
    }
    TouchBar_Main(GtsNegoDisp_GetTouchBar(wk->disp));
    if (TouchBar_GetDecided(GtsNegoDisp_GetTouchBar(wk->disp)) == TOUCHBAR_ICON_RETURN) {
        wk->timer = 1;
        GtsNegoMessage_Print(wk->msg, GtsnegoProc_Text_OkDisconnectFromNintendo);
        if (GCTX_HIDIsUsingTouch() == TRUE) {
            GtsNegoDisp_SetCursor(wk->disp, NULL, GTSNEGO_CURSOR_NONE);
        }
        GtsNego_ChangeState(wk, GtsNego_StateOpenExitConfirm, 2436);
    }
}

// The partner was lost: back to the main menu, behind the error screen
static void GtsNego_HandleDisconnect(GtsNegoWork *wk) {
    if (wk->param->result != GTSNEGO_RESULT_EXIT) {
        func_02042e94(0);
        GFL_NetSetDisconnectError(FALSE);
        GtsNegoMessage_FreePMSDraw(wk->msg);
        GtsNegoDisp_SetDim(wk->disp, FALSE, GTSNEGO_DIM_ALL);
        GFL_BGSysSetBGEnabled(4, FALSE);
        GtsNegoDisp_Reset(wk->disp);
        GtsNegoMessage_ClearWindow(wk->msg);
        GtsNegoMessage_Reset(wk->msg);
        GtsNegoDisp_SetCursor(wk->disp, NULL, GTSNEGO_CURSOR_NONE);
        wk->errorShown = TRUE;
        wk->errorFrames = 0;
        wk->param->result = GTSNEGO_RESULT_EXIT;
        GtsNego_ChangeState(wk, GtsNego_StateOpenMainMenu, 2469);
    }
}

static BOOL GtsNegoProc_Init(GameProc *proc, u32 *state, void *param, void *work) {
    GtsNegoParam *negoParam = param;
    GtsNegoWork *wk;

    GFL_HeapCreateChild(HEAPID_USER, HEAPID_GTSNEGO, 0x38000);
    G2_BlendNone();
    G2S_BlendNone();
    Wipe_SetScreenCovered(0, WIPE_COLOR_BLACK);
    Wipe_SetScreenCovered(1, WIPE_COLOR_BLACK);
    wk = GFL_ProcInitSubsystem(proc, sizeof(GtsNegoWork), HEAPID_GTSNEGO);
    MI_CpuFill8(wk, 0, sizeof(GtsNegoWork));
    wk->heapId = HEAPID_GTSNEGO;
    wk->param = negoParam;
    wk->disp = GtsNegoDisp_Create(wk->heapId, negoParam->gameData);
    wk->msg = GtsNegoMessage_Create(wk->heapId, TEXT_BANK_GTSNEGO_PROC);
    wk->gameData = negoParam->gameData;
    wk->save = GameData_GetSaveControl(negoParam->gameData);
    wk->wifiList = GameData_GetWifiList(negoParam->gameData);
    wk->param->result = GTSNEGO_RESULT_EXIT;
    wk->mainCursor = GTSNEGO_CURSOR_ANYONE;
    wk->searchCursor = GTSNEGO_CURSOR_LEVEL;
    wk->friendCursor = GTSNEGO_CURSOR_FRIEND_1;
    wk->myId = getIDAsUInt(GetGameDataPlayerInfo(negoParam->gameData));
    if (GFL_NetIsInitialized()) {
        NetCommand_AddTable(GTSNEGO_NET_CMD_BASE, sGtsNegoCommands, NELEMS(sGtsNegoCommands), wk);
    }
    GFL_WipeSet(WIPE_MODE_BOTH, WIPE_TYPE_FADE_IN, WIPE_TYPE_FADE_IN, WIPE_COLOR_BLACK, 6, 1, wk->heapId);
    GFL_SndBGMPlay(SEQ_BGM_GTS, 0xffff);
    GFL_NetSetIconScreen(TRUE, wk->heapId);
    GFL_NetErrSetKeepDark(TRUE);
    GtsNego_ChangeState(wk, GtsNego_StateOpenMainMenu, 2520);
    return TRUE;
}

static BOOL GtsNegoProc_Main(GameProc *proc, u32 *state, void *param, void *work) {
    GtsNegoWork *wk = work;
    GtsNegoStateFunc func = wk->state;
    BOOL done = TRUE;

    if (func != NULL) {
        if (!GFL_NetErrCheck()) {
            func(wk);
        }
        done = FALSE;
    }
    if (wk->button != NULL) {
        TouchBar *bar = GtsNegoDisp_GetTouchBar(wk->disp);

        if (bar == NULL || !TouchBar_IsTouching(bar)) {
            AppTaskMenuWin_Update(wk->button);
        }
    }
    if (wk->menu != NULL) {
        AppTaskMenu_Update(wk->menu);
    }
    GtsNegoDisp_Main(wk->disp);
    GtsNegoMessage_Update(wk->msg);
    if (GFL_NetIsInitialized()) {
        if (GFL_NetErrCheck()) {
            switch (DWCRapCommon_CheckError(TRUE, TRUE)) {
            case DWCRAP_RESULT_DISCONNECTED:
            case DWCRAP_RESULT_TIMEOUT:
                GtsNego_HandleDisconnect(wk);
                break;
            case DWCRAP_RESULT_FAILED:
                wk->param->result = GTSNEGO_RESULT_RETRY_LOGIN;
                done = TRUE;
                Wipe_SetScreenCovered(0, WIPE_COLOR_BLACK);
                Wipe_SetScreenCovered(1, WIPE_COLOR_BLACK);
                break;
            case DWCRAP_RESULT_NET_ERROR:
                GFL_NetErrShowFatal();
                break;
            }
        }
    } else if (GFL_NetErrCheck()) {
        done = TRUE;
    }
    return done;
}

static BOOL GtsNegoProc_Exit(GameProc *proc, u32 *state, void *param, void *work) {
    GtsNegoWork *wk = work;

    GFL_NetErrClearKeepDark();
    if (!GFL_WipeIsFinished()) {
        return FALSE;
    }
    if (GFL_NetIsInitialized()) {
        NetCommand_RemoveTable(GTSNEGO_NET_CMD_BASE);
    }
    if (wk->button != NULL) {
        AppTaskMenuWin_Free(wk->button);
    }
    if (wk->menu != NULL) {
        AppTaskMenu_Free(wk->menu);
    }
    GtsNegoMessage_Free(wk->msg);
    GtsNegoDisp_Free(wk->disp);
    GFL_ProcReleaseSubsystem(proc);
    GFL_HeapDelete(HEAPID_GTSNEGO);
    return TRUE;
}

const GameProcFunctions GTSNEGO_PROC_FUNCTIONS = {
    GtsNegoProc_Init,
    GtsNegoProc_Main,
    GtsNegoProc_Exit,
};
