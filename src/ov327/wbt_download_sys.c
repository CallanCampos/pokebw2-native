#include "types.h"
#include "app/t_download.h"
#include "app/wbt_download.h"
#include "app/wbt_download_net_util.h"
#include "app/wifi_login.h"
#include "constants/sound.h"
#include "field/wbt.h"
#include "gfl/heap.h"
#include "gfl/net.h"
#include "gfl/overlay.h"
#include "gfl/proc.h"
#include "gfl/sound.h"
#include "gfl/std.h"

// The process that downloads tournaments for the Pokémon World Tournament over Wi-Fi: it logs in, runs the download of
// wbt_download_net_util.c, logs out and shows the tournaments with overlay 326's T_DOWNLOAD. The name is the ROM's
// string, from GFL_HeapAllocate's call. Our names

// The steps of the download, which index sWbtDownloadSteps
enum {
    WBT_DOWNLOAD_STEP_LOGIN,
    WBT_DOWNLOAD_STEP_LOGOUT,
    WBT_DOWNLOAD_STEP_DOWNLOAD,
    WBT_DOWNLOAD_STEP_TOURNAMENTS,
};

// What the step runner does at each call
enum {
    WBT_DOWNLOAD_SEQ_NEXT,
    WBT_DOWNLOAD_SEQ_SETUP,
    WBT_DOWNLOAD_SEQ_RUN,
    WBT_DOWNLOAD_SEQ_FINISH,
    WBT_DOWNLOAD_SEQ_DONE,
};

typedef struct WbtDownloadWork WbtDownloadWork;

// A step: the process to run, and the functions that make its parameter and use its result
typedef struct {
    s32 overlayId;
    const GameProcFunctions *procFunctions;
    // Makes the process's parameter, from the step that ran before
    void *(*setup)(HeapID heapId, WbtDownloadWork *wk, u32 prevStep);
    // Takes the result, and frees the parameter
    void (*finish)(void *param, WbtDownloadWork *wk);
} WbtDownloadStep;

// Runs the steps one after the other, each as a process of its own
typedef struct {
    GameProcManager *p_manager;
    u32 state;
    void *p_proc_param;
    u32 unkC;
    u16 heapId;
    WbtDownloadWork *p_userdata;
    const WbtDownloadStep *steps;
    u32 prevStep;
    u32 nextStep;
    u32 currentStep;
} WbtDownloadSeq;

struct WbtDownloadWork {
    WbtDownloadSeq seq;
    WbtDownloadParam *param;
    void *buffer;
    // 0 if the download worked and the tournaments are to be shown
    u32 result;
};

static BOOL WbtDownload_Init(GameProc *proc, u32 *state, void *param, void *work);
static BOOL WbtDownload_Exit(GameProc *proc, u32 *state, void *param, void *work);
static BOOL WbtDownload_Main(GameProc *proc, u32 *state, void *param, void *work);
static void WbtDownloadSeq_Init(WbtDownloadSeq *p_wk, const WbtDownloadStep *steps, WbtDownloadWork *p_userdata,
                                HeapID heapId);
static BOOL WbtDownloadSeq_Update(WbtDownloadSeq *p_wk);
static void WbtDownloadSeq_Free(WbtDownloadSeq *p_wk);
static void WbtDownloadSeq_SetNext(WbtDownloadSeq *p_wk, u32 step);
static void *WbtDownload_SetupLogin(HeapID heapId, WbtDownloadWork *wk, u32 prevStep);
static void WbtDownload_FinishLogin(void *param, WbtDownloadWork *wk);
static void *WbtDownload_SetupLogout(HeapID heapId, WbtDownloadWork *wk, u32 prevStep);
static void WbtDownload_FinishLogout(void *param, WbtDownloadWork *wk);
static void *WbtDownload_SetupDownload(HeapID heapId, WbtDownloadWork *wk, u32 prevStep);
static void WbtDownload_FinishDownload(void *param, WbtDownloadWork *wk);
static void *WbtDownload_SetupTournaments(HeapID heapId, WbtDownloadWork *wk, u32 prevStep);
static void WbtDownload_FinishTournaments(void *param, WbtDownloadWork *wk);

const GameProcFunctions WBT_DOWNLOAD_PROC_FUNCTIONS = {
    WbtDownload_Init,
    WbtDownload_Main,
    WbtDownload_Exit,
};

static const WbtDownloadStep sWbtDownloadSteps[] = {
    { OVERLAY_WIFILOGIN, &WIFILOGIN_PROC_FUNCTIONS, WbtDownload_SetupLogin, WbtDownload_FinishLogin },
    { OVERLAY_WIFILOGIN, &WIFILOGOUT_PROC_FUNCTIONS, WbtDownload_SetupLogout, WbtDownload_FinishLogout },
    { OVERLAY_NONE, &WBT_DOWNLOAD_NET_PROC_FUNCTIONS, WbtDownload_SetupDownload, WbtDownload_FinishDownload },
    { OVERLAY_T_DOWNLOAD, &T_DOWNLOAD_PROC_FUNCTIONS, WbtDownload_SetupTournaments, WbtDownload_FinishTournaments },
};

static BOOL WbtDownload_Init(GameProc *proc, u32 *state, void *param, void *work) {
    WbtDownloadWork *wk;

    GFL_HeapCreateChild(HEAPID_USER, HEAPID_WBT_DOWNLOAD, 0x20000);
    wk = GFL_ProcInitSubsystem(proc, sizeof(WbtDownloadWork), HEAPID_WBT_DOWNLOAD);
    MI_CpuFill8(wk, 0, sizeof(WbtDownloadWork));
    wk->buffer = GFL_HeapAllocate(HEAPID_WBT_DOWNLOAD, 0xe000, TRUE, "wbt_download_sys.c", 225);
    wk->param = param;
    WbtDownloadSeq_Init(&wk->seq, sWbtDownloadSteps, wk, HEAPID_WBT_DOWNLOAD);
    GFL_SndBGMPush();
    WbtDownloadSeq_SetNext(&wk->seq, WBT_DOWNLOAD_STEP_LOGIN);
    return TRUE;
}

static BOOL WbtDownload_Exit(GameProc *proc, u32 *state, void *param, void *work) {
    WbtDownloadWork *wk = work;

    WbtDownloadSeq_Free(&wk->seq);
    GFL_HeapFree(wk->buffer);
    GFL_ProcReleaseSubsystem(proc);
    GFL_HeapDelete(HEAPID_WBT_DOWNLOAD);
    return TRUE;
}

static BOOL WbtDownload_Main(GameProc *proc, u32 *state, void *param, void *work) {
    WbtDownloadWork *wk = work;

    switch (*state) {
    case 0:
        (*state)++;
        break;
    case 1:
        if (WbtDownloadSeq_Update(&wk->seq)) {
            (*state)++;
        }
        break;
    case 2:
        return TRUE;
    }
    return FALSE;
}

static void WbtDownloadSeq_Init(WbtDownloadSeq *p_wk, const WbtDownloadStep *steps, WbtDownloadWork *p_userdata,
                                HeapID heapId) {
    MI_CpuFill8(p_wk, 0, sizeof(WbtDownloadSeq));
    p_wk->p_manager = CreateGameProcManager(heapId);
    p_wk->p_userdata = p_userdata;
    p_wk->steps = steps;
    p_wk->heapId = heapId;
}

static BOOL WbtDownloadSeq_Update(WbtDownloadSeq *p_wk) {
    switch (p_wk->state) {
    case WBT_DOWNLOAD_SEQ_NEXT:
        p_wk->prevStep = p_wk->currentStep;
        p_wk->currentStep = p_wk->nextStep;
        p_wk->state = WBT_DOWNLOAD_SEQ_SETUP;
        break;
    case WBT_DOWNLOAD_SEQ_SETUP:
        if (p_wk->steps[p_wk->currentStep].setup != NULL) {
            p_wk->p_proc_param = p_wk->steps[p_wk->currentStep].setup(p_wk->heapId, p_wk->p_userdata, p_wk->prevStep);
        } else {
            p_wk->p_proc_param = NULL;
        }
        QueueGameProc(p_wk->p_manager, p_wk->steps[p_wk->currentStep].overlayId,
                      p_wk->steps[p_wk->currentStep].procFunctions, p_wk->p_proc_param);
        p_wk->state = WBT_DOWNLOAD_SEQ_RUN;
        break;
    case WBT_DOWNLOAD_SEQ_RUN: {
        BOOL running = GFL_ProcMgrUpdate(p_wk->p_manager);

        if (running == 0 || running == 2) {
            GFL_NetErrShow(0);
        }
        if (running == 0) {
            p_wk->state = WBT_DOWNLOAD_SEQ_FINISH;
        }
        break;
    }
    case WBT_DOWNLOAD_SEQ_FINISH: {
        const WbtDownloadStep *step = &p_wk->steps[p_wk->currentStep];

        if (step->finish != NULL) {
            step->finish(p_wk->p_proc_param, p_wk->p_userdata);
            p_wk->p_proc_param = NULL;
        }
        if (p_wk->currentStep != p_wk->nextStep) {
            p_wk->state = WBT_DOWNLOAD_SEQ_NEXT;
        } else {
            p_wk->state = WBT_DOWNLOAD_SEQ_DONE;
        }
        break;
    }
    case WBT_DOWNLOAD_SEQ_DONE:
        return TRUE;
    }
    return FALSE;
}

static void WbtDownloadSeq_Free(WbtDownloadSeq *p_wk) {
    GFL_ASSERT(p_wk->p_proc_param == NULL);
    FreeGameProcManager(p_wk->p_manager);
    MI_CpuFill8(p_wk, 0, sizeof(WbtDownloadSeq));
}

static void WbtDownloadSeq_SetNext(WbtDownloadSeq *p_wk, u32 step) {
    p_wk->nextStep = step;
}

static void *WbtDownload_SetupLogin(HeapID heapId, WbtDownloadWork *wk, u32 prevStep) {
    WifiLoginParam *login = GFL_HeapAllocate(heapId, sizeof(WifiLoginParam), TRUE, "wbt_download_sys.c", 494);

    login->gameData = wk->param->gameData;
    login->unk4 = 0;
    login->unk8 = 1;
    login->buffer = NULL;
    login->unk18 = 1;
    login->unkC = 0x36;
    if (prevStep == WBT_DOWNLOAD_STEP_DOWNLOAD) {
        login->unk14 = 1;
    } else {
        GFL_SndBGMPlay(SEQ_BGM_WIFI_ACCESS, SND_CHANNEL_MASK_ALL);
        GFL_SndBGMFadeIn(6);
        login->unk14 = 0;
    }
    return login;
}

static void WbtDownload_FinishLogin(void *param, WbtDownloadWork *wk) {
    WifiLoginParam *login = param;

    switch (login->result) {
    case 0:
        WbtDownloadSeq_SetNext(&wk->seq, WBT_DOWNLOAD_STEP_DOWNLOAD);
        break;
    case 1:
        GFL_SndBGMPop();
        break;
    }
    GFL_HeapFree(login);
}

static void *WbtDownload_SetupLogout(HeapID heapId, WbtDownloadWork *wk, u32 prevStep) {
    WifiLogoutParam *logout = GFL_HeapAllocate(heapId, sizeof(WifiLogoutParam), TRUE, "wbt_download_sys.c", 559);

    logout->gameData = wk->param->gameData;
    logout->unk4 = 0;
    logout->unk8 = 1;
    logout->unkC = 1;
    logout->unk10 = 0;
    return logout;
}

static void WbtDownload_FinishLogout(void *param, WbtDownloadWork *wk) {
    WifiLogoutParam *logout = param;

    switch (wk->result) {
    case 0:
        WbtDownloadSeq_SetNext(&wk->seq, WBT_DOWNLOAD_STEP_TOURNAMENTS);
        break;
    case 1:
        break;
    }
    GFL_SndBGMFadeOut(6);
    GFL_SndBGMPop();
    GFL_HeapFree(logout);
}

static void *WbtDownload_SetupDownload(HeapID heapId, WbtDownloadWork *wk, u32 prevStep) {
    WbtDownloadNetParam *net = GFL_HeapAllocate(heapId, sizeof(WbtDownloadNetParam), TRUE, "wbt_download_sys.c", 614);

    net->gameData = wk->param->gameData;
    net->buffer = wk->buffer;
    return net;
}

static void WbtDownload_FinishDownload(void *param, WbtDownloadWork *wk) {
    WbtDownloadNetParam *net = param;

    switch (net->result) {
    case 0:
    default:
        wk->result = 0;
        WbtDownloadSeq_SetNext(&wk->seq, WBT_DOWNLOAD_STEP_LOGOUT);
        break;
    case 2:
        wk->result = 1;
        WbtDownloadSeq_SetNext(&wk->seq, WBT_DOWNLOAD_STEP_LOGOUT);
        break;
    case 1:
        WbtDownloadSeq_SetNext(&wk->seq, WBT_DOWNLOAD_STEP_LOGIN);
        break;
    }
    GFL_HeapFree(net);
}

static void *WbtDownload_SetupTournaments(HeapID heapId, WbtDownloadWork *wk, u32 prevStep) {
    WbtOv326Param2 *tournaments = GFL_HeapAllocate(heapId, sizeof(WbtOv326Param2), TRUE, "wbt_download_sys.c", 673);

    tournaments->gsys = wk->param->gsys;
    tournaments->unk4 = (uptr)wk->buffer;
    tournaments->unk8 = 1;
    return tournaments;
}

static void WbtDownload_FinishTournaments(void *param, WbtDownloadWork *wk) {
    GFL_HeapFree(param);
}
