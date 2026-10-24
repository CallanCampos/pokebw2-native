#include "types.h"
#include "app/wifi_login.h"
#include "app/wifilogin_message.h"
#include "constants/sound.h"
#include "dpw/dpw_tr.h"
#include "dwc/dwc.h"
#include "gfl/bmp.h"
#include "gfl/bmpwin.h"
#include "gfl/dpw_profile.h"
#include "gfl/heap.h"
#include "gfl/msg.h"
#include "gfl/net.h"
#include "gfl/overlay.h"
#include "gfl/proc.h"
#include "gfl/sound.h"
#include "gfl/std.h"
#include "gfl/str.h"
#include "nitro/gx.h"
#include "save/player_info.h"
#include "save/wifi_list.h"
#include "system/bmp_winframe.h"
#include "system/game_system.h"
#include "system/wordset.h"
#include "worldtrade_local.h"

// The Global Trade Station's connection screen: logging in to Nintendo Wi-Fi Connection and the trade server, and
// logging out, through the Wi-Fi login proc; and the text printing the other screens share. The names are ours,
// guessed

// The connection screen's steps
enum {
    ENTER_START,
    ENTER_END,
    ENTER_WIFI_LOGIN,
    ENTER_WIFI_LOGIN_WAIT,
    ENTER_WIFI_LOGOUT,
    ENTER_WIFI_LOGOUT_WAIT,
};

// The steps of the login that the Wi-Fi login proc calls back for
enum {
    LOGIN_INTERNET_CONNECT,
    LOGIN_INTERNET_CONNECT_WAIT,
    LOGIN_DPW_TR_INIT,
    LOGIN_SERVER_START,
    LOGIN_SERVER_RESULT,
    LOGIN_PROFILE_START,
    LOGIN_PROFILE_RESULT,
    LOGIN_SERVER_ERROR_PRINT,
    LOGIN_SERVER_ERROR_WAIT,
    LOGIN_END,
};

// The library's results are 0 to 2 for the server's state and negative for its errors. -5000 to -5005 are this
// file's, for the profile's errors

// The frames to wait for the server before giving up
#define SERVER_TIMEOUT (30 * 60 * 2)

static void Enter_InitWork(WorldTradeWork *wk);
static void Enter_FreeWork(WorldTradeWork *wk);
static int Enter_Start(WorldTradeWork *wk);
static int Enter_InternetConnect(WorldTradeWork *wk);
static int Enter_InternetConnectWait(WorldTradeWork *wk);
static int Enter_DpwTrInit(WorldTradeWork *wk);
static int Enter_ServerStart(WorldTradeWork *wk);
static int Enter_ServerResult(WorldTradeWork *wk);
static int Enter_ProfileStart(WorldTradeWork *wk);
static int Enter_ProfileResult(WorldTradeWork *wk);
static int Enter_WifiConnectionLogin(WorldTradeWork *wk);
static int Enter_WifiConnectionLoginWait(WorldTradeWork *wk);
static int Enter_WifiConnectionLogout(WorldTradeWork *wk);
static int Enter_WifiConnectionLogoutWait(WorldTradeWork *wk);
static int Enter_LoginCallback(void *msgWork, void *data);
static int Enter_LogoutCallback(void *msgWork, void *data);
static int Enter_End(WorldTradeWork *wk);
static int Enter_ServerErrorPrint(WorldTradeWork *wk, void *msgWork);
static int Enter_ServerErrorWait(WorldTradeWork *wk, void *msgWork);
static void Enter_MessagePrintCore(WorldTradeWork *wk, MsgData *msgManager, int msgNo, int wait, u16 dat, int stream);
static int Enter_PrintCommonFunc(BmpWin *win, StrBuf *str, int x, int flag, u16 color, int font,
                                 WorldTradePrint *print);

static int (*sEnterFuncTable[])(WorldTradeWork *wk) = {
    Enter_Start,
    Enter_End,
    Enter_WifiConnectionLogin,
    Enter_WifiConnectionLoginWait,
    Enter_WifiConnectionLogout,
    Enter_WifiConnectionLogoutWait,
};

int WorldTrade_Enter_Init(WorldTradeWork *wk, int seq) {
    WorldTrade_ExitGraphics(wk);
    Enter_InitWork(wk);
    GX_SetDispSelect(GX_DISP_SELECT_SUB_MAIN);

    switch (wk->subProcessMode) {
    case WT_MODE_LOGIN:
    case WT_MODE_RELOGIN:
        wk->subprocessSeq = ENTER_WIFI_LOGIN;
        break;
    case WT_MODE_LOGOUT:
        wk->subprocessSeq = ENTER_WIFI_LOGOUT;
        break;
    }
    return WT_SEQ_FADEIN;
}

int WorldTrade_Enter_Main(WorldTradeWork *wk, int seq) {
    int oldSeq = wk->subprocessSeq;
    int ret = sEnterFuncTable[oldSeq](wk);

    if (oldSeq != wk->subprocessSeq) {
        wk->localSeq = 0;
        wk->localWait = 0;
    }
    return ret;
}

int WorldTrade_Enter_End(WorldTradeWork *wk, int seq) {
    Enter_FreeWork(wk);
    WorldTrade_InitGraphics(wk);
    WorldTrade_SubProcessUpdate(wk);
    if (wk->subProcess == WORLDTRADE_ENTER) {
        return WT_SEQ_OUT;
    }
    return WT_SEQ_INIT;
}

static void Enter_InitWork(WorldTradeWork *wk) {
}

static void Enter_FreeWork(WorldTradeWork *wk) {
}

static int Enter_Start(WorldTradeWork *wk) {
    GFL_SndBGMPlay(SEQ_BGM_GTS, SND_CHANNEL_MASK_ALL);
    wk->subprocessSeq = ENTER_END;
    wk->boxSearchFlag = 1;
    wk->openingFlag = 0;
    WorldTrade_SubProcessChange(wk, WORLDTRADE_TITLE, 0);
    return WT_SEQ_MAIN;
}

static int Enter_InternetConnect(WorldTradeWork *wk) {
    DWC_NASLoginAsync();
    return TRUE;
}

static int Enter_InternetConnectWait(WorldTradeWork *wk) {
    switch (DWC_NASLoginProcess()) {
    case 3:
        return 1;
    case 0:
    case 4:
    case 5:
        return 2;
    }
    return 0;
}

static int Enter_DpwTrInit(WorldTradeWork *wk) {
    void *userData = WifiList_GetMyUserData(wk->param->wifilist);
    s32 pid;

    PlayerInfo_InitProfileId(wk->param->mystatus, WifiList_GetMyGSID(wk->param->wifilist));
    pid = PlayerInfo_GetProfileId(wk->param->mystatus);
    DpwTr_Init(pid, DWC_CreateFriendKey(userData), 0);
    return TRUE;
}

static int Enter_ServerStart(WorldTradeWork *wk) {
    DpwTr_GetServerState();
    wk->timeoutCount = 0;
    return TRUE;
}

static int Enter_ServerResult(WorldTradeWork *wk) {
    s32 result;

    DpwTr_Process();
    if (DpwTr_IsFinished()) {
        result = DpwTr_GetResult();
        wk->timeoutCount = 0;
        switch (result) {
        case 0:
            return 1;
        case 1:
            WorldTrade_TimeIconDel(wk);
            wk->connectErrorNo = result;
            return 2;
        case 2:
        case -1:
            WorldTrade_TimeIconDel(wk);
            wk->connectErrorNo = result;
            return 2;
        case -12:
        case -15:
            WorldTrade_TimeIconDel(wk);
            wk->connectErrorNo = result;
            return 2;
        case -2:
        case -14:
            WorldTrade_TimeIconDel(wk);
            wk->connectErrorNo = result;
            return 2;
        case -3:
        case -4:
        case -5:
        case -6:
        case -7:
        case -8:
        case -9:
        case -10:
        case -11:
        case -13:
        default:
            WorldTrade_TimeIconDel(wk);
            WorldTrade_ShowFatalError(wk);
            break;
        }
    } else if (++wk->timeoutCount == SERVER_TIMEOUT) {
        WorldTrade_ShowFatalError(wk);
    }
    return 0;
}

static int Enter_ProfileStart(WorldTradeWork *wk) {
    DpwProfile_Fill(&wk->dcProfile, wk->param->mystatus);
    DpwTr_SetProfile(&wk->dcProfile, &wk->dcProfileResult);
    wk->timeoutCount = 0;
    return TRUE;
}

static int Enter_ProfileResult(WorldTradeWork *wk) {
    s32 result;

    DpwTr_Process();
    if (DpwTr_IsFinished()) {
        result = DpwTr_GetResult();
        wk->timeoutCount = 0;
        switch (result) {
        case 0:
            switch (wk->dcProfileResult.code) {
            case 0:
                switch (wk->dcProfileResult.mailAddrAuthResult) {
                case 0:
                    WorldTrade_SubProcessChange(wk, WORLDTRADE_TITLE, 0);
                    return 1;
                case 3:
                    wk->connectErrorNo = -5003;
                    return 2;
                case 1:
                    wk->connectErrorNo = -5000;
                    return 2;
                case 2:
                    wk->connectErrorNo = -5001;
                    return 2;
                default:
                    WorldTrade_ShowFatalError(wk);
                    break;
                }
                break;
            case 1:
                wk->connectErrorNo = -5004;
                return 2;
            case 2:
                wk->connectErrorNo = -5005;
                return 2;
            default:
                WorldTrade_TimeIconDel(wk);
                WorldTrade_ShowFatalError(wk);
                break;
            }
            break;
        case 1:
            WorldTrade_TimeIconDel(wk);
            wk->connectErrorNo = result;
            return 2;
        case 2:
        case -1:
            WorldTrade_TimeIconDel(wk);
            wk->connectErrorNo = result;
            return 2;
        case -12:
        case -15:
            WorldTrade_TimeIconDel(wk);
            wk->connectErrorNo = result;
            return 2;
        case -2:
        case -14:
            WorldTrade_TimeIconDel(wk);
            wk->connectErrorNo = result;
            return 2;
        case -3:
        case -4:
        case -5:
        case -6:
        case -7:
        case -8:
        case -9:
        case -10:
        case -11:
        case -13:
        default:
            WorldTrade_TimeIconDel(wk);
            WorldTrade_ShowFatalError(wk);
            break;
        }
    } else if (++wk->timeoutCount == SERVER_TIMEOUT) {
        WorldTrade_ShowFatalError(wk);
    }
    return 0;
}

static int Enter_WifiConnectionLogin(WorldTradeWork *wk) {
    WifiLoginParam *param;

    wk->subProcParam = GFL_HeapAllocate(HEAPID_WORLDTRADE, sizeof(WifiLoginParam), FALSE, "worldtrade_enter.c", 611);
    MI_CpuFill8(wk->subProcParam, 0, sizeof(WifiLoginParam));
    param = wk->subProcParam;
    param->gameData = GSYS_GetGameData(wk->param->gsys);
    param->unk4 = 0;
    param->unk8 = 1;
    param->buffer = wk->wifiLoginBuffer;
    param->unkC = 30;
    param->unk20 = Enter_LoginCallback;
    param->unk24 = wk;
    wk->loginSeq = LOGIN_INTERNET_CONNECT;
    if (wk->subProcessMode == WT_MODE_RELOGIN) {
        param->unk14 = 1;
    } else {
        param->unk14 = 0;
    }
    QueueGameProc(wk->procManager, OVERLAY_WIFILOGIN, &WIFILOGIN_PROC_FUNCTIONS, wk->subProcParam);
    GFL_NetErrShow(0);
    wk->subprocessSeq = ENTER_WIFI_LOGIN_WAIT;
    return WT_SEQ_MAIN;
}

static int Enter_WifiConnectionLoginWait(WorldTradeWork *wk) {
    WifiLoginParam *param = wk->subProcParam;

    if (!wk->procResult) {
        switch (param->result) {
        case 0:
            wk->subprocessSeq = ENTER_START;
            break;
        case 1:
            WorldTrade_SubProcessChange(wk, WORLDTRADE_ENTER, 0);
            wk->subprocessSeq = ENTER_END;
            break;
        }
        GFL_HeapFree(wk->subProcParam);
    }
    return WT_SEQ_MAIN;
}

static int Enter_WifiConnectionLogout(WorldTradeWork *wk) {
    WifiLogoutParam *param;

    wk->subProcParam = GFL_HeapAllocate(HEAPID_WORLDTRADE, sizeof(WifiLogoutParam), FALSE, "worldtrade_enter.c", 689);
    MI_CpuFill8(wk->subProcParam, 0, sizeof(WifiLogoutParam));
    param = wk->subProcParam;
    param->gameData = GSYS_GetGameData(wk->param->gsys);
    param->unk4 = 0;
    param->unk8 = 1;
    // The header's types for the callback and its data
    param->unk14 = (uptr)Enter_LogoutCallback;
    param->unk18 = (uptr)wk;
    QueueGameProc(wk->procManager, OVERLAY_WIFILOGIN, &WIFILOGOUT_PROC_FUNCTIONS, wk->subProcParam);
    wk->subprocessSeq = ENTER_WIFI_LOGOUT_WAIT;
    GFL_NetErrShow(0);
    return WT_SEQ_MAIN;
}

static int Enter_WifiConnectionLogoutWait(WorldTradeWork *wk) {
    if (!wk->procResult) {
        GFL_HeapFree(wk->subProcParam);
        WorldTrade_SubProcessChange(wk, WORLDTRADE_ENTER, 0);
        wk->subprocessSeq = ENTER_END;
    }
    return WT_SEQ_MAIN;
}

static int Enter_LoginCallback(void *msgWork, void *data) {
    WorldTradeWork *wk = data;

    switch (wk->loginSeq) {
    case LOGIN_INTERNET_CONNECT:
        if (Enter_InternetConnect(wk)) {
            wk->loginSeq++;
        }
        break;
    case LOGIN_INTERNET_CONNECT_WAIT:
        switch (Enter_InternetConnectWait(wk)) {
        case 1:
            wk->loginSeq++;
            break;
        case 2:
            wk->loginSeq = LOGIN_END;
            break;
        }
        break;
    case LOGIN_DPW_TR_INIT:
        if (Enter_DpwTrInit(wk)) {
            wk->loginSeq++;
        }
        break;
    case LOGIN_SERVER_START:
        if (Enter_ServerStart(wk)) {
            wk->loginSeq++;
        }
        break;
    case LOGIN_SERVER_RESULT:
        switch (Enter_ServerResult(wk)) {
        case 1:
            wk->loginSeq++;
            break;
        case 2:
            wk->loginSeq = LOGIN_SERVER_ERROR_PRINT;
            break;
        }
        break;
    case LOGIN_PROFILE_START:
        if (Enter_ProfileStart(wk)) {
            wk->loginSeq++;
        }
        break;
    case LOGIN_PROFILE_RESULT:
        switch (Enter_ProfileResult(wk)) {
        case 1:
            wk->loginSeq = LOGIN_INTERNET_CONNECT;
            return 0;
        case 2:
            wk->loginSeq = LOGIN_SERVER_ERROR_PRINT;
            break;
        }
        break;
    case LOGIN_SERVER_ERROR_PRINT:
        if (Enter_ServerErrorPrint(wk, msgWork)) {
            wk->loginSeq++;
        }
        break;
    case LOGIN_SERVER_ERROR_WAIT:
        if (Enter_ServerErrorWait(wk, msgWork)) {
            wk->loginSeq++;
        }
        break;
    case LOGIN_END:
        wk->loginSeq = LOGIN_INTERNET_CONNECT;
        return 1;
    }
    return 2;
}

static int Enter_LogoutCallback(void *msgWork, void *data) {
    DpwTr_End();
    return 0;
}

static int Enter_End(WorldTradeWork *wk) {
    WorldTrade_TimeIconDel(wk);
    wk->subprocessSeq = ENTER_START;
    wk->subOutFlag = 1;
    return WT_SEQ_FADEOUT;
}

static int Enter_ServerErrorPrint(WorldTradeWork *wk, void *msgWork) {
    int msgNo;

    switch (wk->connectErrorNo) {
    case 1:
        msgNo = 0xa6;
        break;
    case 2:
        msgNo = 0xa7;
        break;
    case -1:
        msgNo = 0x9d;
        break;
    case -2:
        msgNo = 0xa9;
        break;
    case -3:
        msgNo = 0xa2;
        break;
    case -4:
        msgNo = 0xa4;
        break;
    case -5:
        msgNo = 0xa3;
        break;
    case -6:
        msgNo = 0x1b;
        break;
    case -7:
        msgNo = 0x1c;
        break;
    case -8:
        msgNo = 0x1d;
        break;
    case -9:
        msgNo = 0x1e;
        break;
    case -10:
        msgNo = 0x1f;
        break;
    case -11:
        msgNo = 0x20;
        break;
    case -12:
        msgNo = 0xa0;
        break;
    case -13:
        msgNo = 0xa5;
        break;
    case -14:
        msgNo = 0xaa;
        break;
    case -15:
        msgNo = 0xa1;
        break;
    default:
        msgNo = 0x9f;
        break;
    }
    WifiLoginMessage_PrintFrom(msgWork, wk->msgManager, msgNo);
    wk->localSeq = 0;
    return TRUE;
}

static int Enter_ServerErrorWait(WorldTradeWork *wk, void *msgWork) {
    switch (wk->localSeq) {
    case 0:
        wk->localSeq++;
        break;
    case 1:
        if (WifiLoginMessage_IsDone(msgWork)) {
            wk->localSeq++;
        }
        break;
    case 2:
        wk->localSeq++;
        break;
    case 3:
        DpwTr_End();
        DWC_ShutdownFriendsMatch();
        DWC_CleanupInet();
        wk->localSeq++;
        break;
    default:
        wk->localWait++;
        if (wk->localWait > 60) {
            return TRUE;
        }
        break;
    }
    return FALSE;
}

void Enter_MessagePrint(WorldTradeWork *wk, MsgData *msgManager, int msgNo, int wait, u16 dat) {
    Enter_MessagePrintCore(wk, msgManager, msgNo, wait, dat, TRUE);
}

void Enter_MessagePrintNoStream(WorldTradeWork *wk, MsgData *msgManager, int msgNo, int wait, u16 dat) {
    Enter_MessagePrintCore(wk, msgManager, msgNo, wait, dat, FALSE);
}

static void Enter_MessagePrintCore(WorldTradeWork *wk, MsgData *msgManager, int msgNo, int wait, u16 dat, int stream) {
    StrBuf *str = GFL_MsgDataLoadStrbufNew(msgManager, msgNo);

    GFL_WordSetFormatStrbuf(wk->wordSet, wk->talkString, str);
    GFL_StrBufFree(str);
    GFL_BitmapFill(BmpWin_GetBitmap(wk->msgWin), 15);
    BmpWin_FlushChar(wk->msgWin);
    BmpWin_DrawFrame(wk->msgWin, 0, 1, 14);
    if (stream) {
        WorldTrade_StreamPrint(wk->msgWin, 0, wk->talkString, 0, 0, &wk->print);
    } else {
        WorldTrade_Print(wk->msgWin, 0, wk->talkString, 0, 0, &wk->print);
    }
    wk->wait = 0;
}

static int Enter_PrintCommonFunc(BmpWin *win, StrBuf *str, int x, int flag, u16 color, int font,
                                 WorldTradePrint *print) {
    int length;

    switch (flag) {
    case 1:
        length = WorldTrade_GetStrWidth(print, font, str, 0);
        x = (BmpWin_GetSizeX(win) * 8 - length) / 2;
        break;
    case 2:
        length = WorldTrade_GetStrWidth(print, font, str, 0);
        x = BmpWin_GetSizeX(win) * 8 - length;
        break;
    }
    return x;
}

void WorldTrade_SysPrint(BmpWin *win, StrBuf *str, int x, int y, int flag, u16 color, WorldTradePrint *print) {
    x = Enter_PrintCommonFunc(win, str, x, flag, color, 0, print);
    WorldTrade_PrintColor(win, 0, str, x, y, 0, color, print);
}

void WorldTrade_TouchPrint(BmpWin *win, StrBuf *str, int x, int y, int flag, u16 color, WorldTradePrint *print) {
    x = Enter_PrintCommonFunc(win, str, x, flag, color, 0, print);
    WorldTrade_PrintColor(win, 0, str, x, y, 0, color, print);
}

static const u32 sExplainMsgTable[] = { 0xf7, 0xf8, 0xf9, 0xfa, 0xfb };

void WorldTrade_ExplainPrint(BmpWin *win, MsgData *msgManager, int no, WorldTradePrint *print) {
    StrBuf *str = GFL_MsgDataLoadStrbufNew(msgManager, sExplainMsgTable[no]);

    WorldTrade_SysPrint(win, str, 0, 0, 0, 0x440, print);
    GFL_StrBufFree(str);
}

void WorldTrade_WifiIconAdd(WorldTradeWork *wk) {
    GFL_NetSetIconScreen(FALSE, HEAPID_WORLDTRADE);
}
