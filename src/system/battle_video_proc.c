#include "types.h"
#include "app/battle_recorder.h"
#include "app/battle_video.h"
#include "app/wifi_login.h"
#include "dwc/connectinet.h"
#include "gfl/gx_layers.h"
#include "gfl/heap.h"
#include "gfl/net.h"
#include "gfl/proc.h"
#include "gfl/std.h"
#include "nitro/gx.h"

// The proc of the Global Link's Battle Videos and musical photos: logs in, runs the Battle Recorder of overlay 272
// online and logs out, with a proc manager of its own. The file's name is a guess, from what it does: the ROM has no
// string for it

#define HEAPID_BATTLE_VIDEO 0x5b

enum {
    BATTLE_VIDEO_START,
    BATTLE_VIDEO_LOGIN,
    BATTLE_VIDEO_WAIT_LOGIN,
    BATTLE_VIDEO_RECORDER,
    BATTLE_VIDEO_WAIT_RECORDER,
    BATTLE_VIDEO_LOGOUT,
    BATTLE_VIDEO_WAIT_LOGOUT,
    BATTLE_VIDEO_END,
};

typedef struct {
    BattleVideoParam *param;
    // Set while logged in
    BOOL online;
    GameProcManager *manager;
    u8 loginBuffer[0x174];
    // The parameter of the proc that runs
    union {
        WifiLoginParam login;
        BattleRecorderParam recorder;
        WifiLogoutParam logout;
    } sub;
} BattleVideoWork;

static BOOL BattleVideo_Init(GameProc *proc, u32 *state, void *param, void *work);
static BOOL BattleVideo_Main(GameProc *proc, u32 *state, void *param, void *work);
static BOOL BattleVideo_Exit(GameProc *proc, u32 *state, void *param, void *work);

const GameProcFunctions BATTLE_VIDEO_PROC_FUNCTIONS = {
    BattleVideo_Init,
    BattleVideo_Main,
    BattleVideo_Exit,
};

static BOOL BattleVideo_Init(GameProc *proc, u32 *state, void *param, void *work) {
    BattleVideoWork *wk;

    GFL_BGSysSetEnabledBGsA(0);
    GFL_BGSysSetEnabledBGsB(0);
    GX_SetVisiblePlane(0);
    GXS_SetVisiblePlane(0);
    GX_SetVisibleWnd(GX_WNDMASK_NONE);
    GXS_SetVisibleWnd(GX_WNDMASK_NONE);
    G2_BlendNone();
    G2S_BlendNone();
    GFL_HeapCreateChild(HEAPID_USER, HEAPID_BATTLE_VIDEO, 0x800);
    wk = GFL_ProcInitSubsystem(proc, sizeof(BattleVideoWork), HEAPID_BATTLE_VIDEO);
    MI_CpuFill8(wk, 0, sizeof(BattleVideoWork));
    wk->param = param;
    wk->manager = CreateGameProcManager(HEAPID_BATTLE_VIDEO);
    return TRUE;
}

static BOOL BattleVideo_Main(GameProc *proc, u32 *state, void *param, void *work) {
    BattleVideoWork *wk = work;
    int status = GFL_ProcMgrUpdate(wk->manager);

    if (status == 2 || status == 0) {
        GFL_NetErrShow(0);
    }
    switch (*state) {
    case BATTLE_VIDEO_START:
        (*state)++;
        break;
    case BATTLE_VIDEO_LOGIN: {
        // Whether the Battle Recorder lost the connection, and so comes back here
        BOOL retry = wk->sub.recorder.result == 2;

        MI_CpuFill8(&wk->sub.login, 0, sizeof(WifiLoginParam));
        wk->sub.login.gameData = wk->param->gameData;
        wk->sub.login.unk4 = 0;
        wk->sub.login.unk8 = 1;
        wk->sub.login.buffer = wk->loginBuffer;
        wk->sub.login.unkC = 0x21;
        if (retry) {
            wk->sub.login.unk14 = 1;
        } else {
            wk->sub.login.unk14 = 0;
        }
        QueueGameProc(wk->manager, OVERLAY_WIFILOGIN, &WIFILOGIN_PROC_FUNCTIONS, &wk->sub.login);
        (*state)++;
        break;
    }
    case BATTLE_VIDEO_WAIT_LOGIN:
        if (status == 0) {
            if (wk->sub.login.result == 0) {
                wk->online = TRUE;
                (*state)++;
            } else {
                *state = BATTLE_VIDEO_END;
            }
        }
        break;
    case BATTLE_VIDEO_RECORDER:
        MI_CpuFill8(&wk->sub.recorder, 0, sizeof(BattleRecorderParam));
        wk->sub.recorder.mode = wk->param->mode;
        wk->sub.recorder.gameData = wk->param->gameData;
        wk->sub.recorder.unk8 = (uptr)wk->loginBuffer;
        QueueGameProc(wk->manager, OVERLAY_ID(272), &BR_MAIN_PROC_FUNCTIONS, &wk->sub.recorder);
        (*state)++;
        break;
    case BATTLE_VIDEO_WAIT_RECORDER:
        if (status == 0) {
            if (wk->sub.recorder.result == 2) {
                *state = BATTLE_VIDEO_LOGIN;
            } else {
                (*state)++;
            }
        }
        break;
    case BATTLE_VIDEO_LOGOUT:
        MI_CpuFill8(&wk->sub.logout, 0, sizeof(WifiLogoutParam));
        wk->sub.logout.gameData = wk->param->gameData;
        wk->sub.logout.unk4 = 0;
        wk->sub.logout.unk8 = 1;
        QueueGameProc(wk->manager, OVERLAY_WIFILOGIN, &WIFILOGOUT_PROC_FUNCTIONS, &wk->sub.logout);
        wk->online = FALSE;
        (*state)++;
        break;
    case BATTLE_VIDEO_WAIT_LOGOUT:
        if (status == 0) {
            (*state)++;
        }
        break;
    case BATTLE_VIDEO_END:
        return TRUE;
    }
    if (wk->online == TRUE) {
        DWC_UpdateConnection();
    }
    return FALSE;
}

static BOOL BattleVideo_Exit(GameProc *proc, u32 *state, void *param, void *work) {
    BattleVideoWork *wk = work;

    FreeGameProcManager(wk->manager);
    GFL_ProcReleaseSubsystem(proc);
    GFL_HeapDelete(HEAPID_BATTLE_VIDEO);
    return TRUE;
}
