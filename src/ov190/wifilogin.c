// wifilogin.c, in overlay 190: the process that logs in to the Nintendo Wi-Fi Connection. It starts the network
// library for the connection, asks the player what to do when the settings need fixing, connects, saves the new user
// data and calls the caller's callback while it waits. The ROM has no file name for it, so it is named after its
// process (our name). It draws with wifilogin_disp.c and prints with wifilogin_message.c

#include "types.h"
#include "app/error_window_screen.h"
#include "app/wifi_login.h"
#include "app/wifilogin_disp.h"
#include "app/wifilogin_message.h"
#include "constants/sound.h"
#include "constants/text_banks.h"
#include "dwc/dwc.h"
#include "gfl/dwc_rap.h"
#include "gfl/heap.h"
#include "gfl/key.h"
#include "gfl/net.h"
#include "gfl/net_command.h"
#include "gfl/net_handle.h"
#include "gfl/net_state.h"
#include "gfl/net_system.h"
#include "gfl/proc.h"
#include "gfl/sound.h"
#include "gfl/std.h"
#include "gfl/touchpanel.h"
#include "nitro/os.h"
#include "save/box.h"
#include "save/player_info.h"
#include "save/tournament_block.h"
#include "save/wifi_list.h"
#include "system/dsi.h"
#include "system/game_data.h"
#include "system/wipe.h"
#include "text/system/wifi_error.h"

typedef struct WifiLoginWork WifiLoginWork;
typedef void (*WifiLoginStateFunc)(WifiLoginWork *work);

struct WifiLoginWork {
    WifiLoginStateFunc state;
    u8 unk4[8];
    HeapID heapId;
    u8 unkE[14];
    // Set by the network command when another machine sends it
    u32 unk1C;
    // Whether the DWC user data is not valid
    BOOL userDataInvalid;
    // Whether the user data was saved
    BOOL saved;
    u8 unk28[4];
    SaveControl *save;
    WifiLoginMessageMenu *menu;
    WifiLoginDisp *disp;
    WifiLoginMessage *msg;
    WifiList *wifiList;
    WifiLoginParam *param;
    GameData *gameData;
};

// The title of the top screen
#define WIFILOGIN_TITLE_MSG WifiError_Text_ConnectNintendoWfc

static BOOL WifiLogin_IsInputPressed(WifiLoginWork *work);
static void WifiLogin_SetState(WifiLoginWork *work, WifiLoginStateFunc state);
static void WifiLogin_OnNetCommand(int netId, int size, const void *data, void *work, NetHandle *handle);
static void *WifiLogin_GetInfo(void *work);
static int WifiLogin_GetInfoSize(void *work);
static int WifiLogin_IsSameGame(int gameCommandBase, int beaconCommandBase);
static sptr WifiLogin_GetUserData(void *work);
static sptr WifiLogin_GetFriendData(void *work);
static void WifiLogin_OnFriendCode(int code, int param, void *work);
static void WifiLogin_StateWaitMessageEnd(WifiLoginWork *work);
static void WifiLogin_StateWaitInputThenExit(WifiLoginWork *work);
static void WifiLogin_CheckLoggedIn(WifiLoginWork *work);
static void WifiLogin_StateWaitSave(WifiLoginWork *work);
static void WifiLogin_StateStartSave(WifiLoginWork *work);
static void WifiLogin_StateWaitMessageThenSave(WifiLoginWork *work);
static void WifiLogin_StateStartNet(WifiLoginWork *work);
static void WifiLogin_StateWaitConfirmStart(WifiLoginWork *work);
static void WifiLogin_StateWaitConfirmRetry(WifiLoginWork *work);
static void WifiLogin_StateOpenConfirmStart(WifiLoginWork *work);
static void WifiLogin_StateOpenConfirmError(WifiLoginWork *work);
static BOOL WifiLogin_CheckNetError(WifiLoginWork *work);
static void WifiLogin_StateOpenYesNo(WifiLoginWork *work);
static void WifiLogin_StateWaitResetConfirm(WifiLoginWork *work);
static void WifiLogin_StateOpenResetConfirm(WifiLoginWork *work);
static void WifiLogin_StateWaitProfileConfirm(WifiLoginWork *work);
static void WifiLogin_StateOpenProfileConfirm(WifiLoginWork *work);
static void WifiLogin_StateWaitFixConfirm(WifiLoginWork *work);
static void WifiLogin_StateOpenFixConfirm(WifiLoginWork *work);
static void WifiLogin_StateWaitInputThenFix(WifiLoginWork *work);
static void WifiLogin_StateCheckStart(WifiLoginWork *work);
static void WifiLogin_StateWaitFadeOut(WifiLoginWork *work);
static void WifiLogin_StateRunCallback(WifiLoginWork *work);
static void WifiLogin_StateStartFadeOut(WifiLoginWork *work);
static void WifiLogin_StateConnect(WifiLoginWork *work);
static void WifiLogin_StateStartConnect(WifiLoginWork *work);
static void WifiLogin_StateWaitConnect(WifiLoginWork *work);
static BOOL WifiLoginProc_Init(GameProc *proc, u32 *state, void *param, void *work);
static BOOL WifiLoginProc_Main(GameProc *proc, u32 *state, void *param, void *work);
static BOOL WifiLoginProc_Exit(GameProc *proc, u32 *state, void *param, void *work);

static const NetCommand sNetCommands[] = {
    { WifiLogin_OnNetCommand, NULL },
};

const GameProcFunctions WIFILOGIN_PROC_FUNCTIONS = {
    WifiLoginProc_Init,
    WifiLoginProc_Main,
    WifiLoginProc_Exit,
};

// clang-format off
static GFLNetInitData sNetInit = {
    sNetCommands,
    NELEMS(sNetCommands),
    NULL,
    NULL,
    NULL,
    NULL,
    WifiLogin_GetInfo,
    WifiLogin_GetInfoSize,
    WifiLogin_IsSameGame,
    NULL,
    (void (*)(NetHandle *, int, void *))ErrorWindowScreen_Show,
    NULL,
    NULL,
    {.wifiError = {{0}, WifiLogin_OnFriendCode}},
    WifiLogin_GetFriendData,
    WifiLogin_GetUserData,
    0x5e400,
    {1, 0, 0, 0, 0x80, 0x13, 0, 0},
    HEAPID_USER,
    0xd,
    0xf,
    0x10,
    0xf0,
    0,
    2,
    0x64,
    0x10,
    1,
    0,
    GFL_NET_TYPE_WIFI,
    1,
    0,
    {0xfe, 0xff, 0, 0},
    0,
    0,
};

// clang-format on

// The game service ID that the network library shares with the other machine, which the login gets from its parameter
static u32 sGameType;

// Whether the player presses A or B, or touches the screen when the login lets them
static BOOL WifiLogin_IsInputPressed(WifiLoginWork *work) {
    if (GCTX_HIDGetPressedKeys() & (PAD_BUTTON_A | PAD_BUTTON_B)) {
        return TRUE;
    }
    if (work->param->unk4 != 0 && GFL_HIDIsTouchPressed()) {
        return TRUE;
    }
    return FALSE;
}

static void WifiLogin_SetState(WifiLoginWork *work, WifiLoginStateFunc state) {
    work->state = state;
}

// The login's one network command
static void WifiLogin_OnNetCommand(int netId, int size, const void *data, void *work, NetHandle *handle) {
    WifiLoginWork *wk = work;

    if (handle == NetHandle_GetOwn()) {
        if (netId != NetSystem_GetNetId()) {
            wk->unk1C = 1;
        }
    }
}

static void *WifiLogin_GetInfo(void *work) {
    return &sGameType;
}

static int WifiLogin_GetInfoSize(void *work) {
    return sizeof(sGameType);
}

static int WifiLogin_IsSameGame(int gameCommandBase, int beaconCommandBase) {
    if (gameCommandBase == beaconCommandBase) {
        return TRUE;
    }
    return FALSE;
}

static sptr WifiLogin_GetUserData(void *work) {
    WifiLoginWork *wk = work;

    return (sptr)WifiList_GetMyUserData(GameData_GetWifiList(wk->gameData));
}

static sptr WifiLogin_GetFriendData(void *work) {
    WifiLoginWork *wk = work;

    return (sptr)WifiList_GetFriendData(GameData_GetWifiList(wk->gameData), 0);
}

static void WifiLogin_OnFriendCode(int code, int param, void *work) {
    WifiLoginWork *wk = work;

    WifiList_MergeFriendRecord(GameData_GetWifiList(wk->gameData), code, param);
}

static void WifiLogin_StateWaitMessageEnd(WifiLoginWork *work) {
    if (GFL_WipeIsFinished() && WifiLoginMessage_IsDone(work->msg) && WifiLogin_IsInputPressed(work)) {
        WifiLogin_SetState(work, WifiLogin_StateRunCallback);
    }
}

static void WifiLogin_StateWaitInputThenExit(WifiLoginWork *work) {
    if (WifiLogin_IsInputPressed(work)) {
        WifiLoginMessage_ClearNotice(work->msg);
        if (work->param->unk4 == 0) {
            WifiLoginMessage_PrintTitle(work->msg, WIFILOGIN_TITLE_MSG);
        }
        if (work->param->buffer != NULL) {
            WifiLogin_SetState(work, WifiLogin_StateStartConnect);
        } else {
            WifiLogin_SetState(work, WifiLogin_StateRunCallback);
        }
    } else {
        WifiLogin_CheckNetError(work);
    }
}

// Goes on once the network is up: tells the player the settings were fixed, or goes to the connection
static void WifiLogin_CheckLoggedIn(WifiLoginWork *work) {
    if (NetState_IsWifiConnecting()) {
        if (work->userDataInvalid) {
            WifiLoginMessage_ClearTitle(work->msg);
            WifiLoginMessage_ClearWindow(work->msg);
            if (isRunningOnDSi()) {
                WifiLoginMessage_PrintNotice(work->msg, WifiError_Text_WiFiUserInformation_4);
            } else {
                WifiLoginMessage_PrintNotice(work->msg, WifiError_Text_WiFiUserInformation_2);
            }
            WifiLogin_SetState(work, WifiLogin_StateWaitInputThenExit);
        } else if (work->param->buffer != NULL) {
            WifiLogin_SetState(work, WifiLogin_StateStartConnect);
        } else {
            WifiLogin_SetState(work, WifiLogin_StateRunCallback);
        }
    } else {
        WifiLogin_CheckNetError(work);
    }
}

// Waits for the user data to be saved
static void WifiLogin_StateWaitSave(WifiLoginWork *work) {
    if (DWCRap_IsUserDataDirty()) {
        switch (GameData_UpdateSave(work->gameData)) {
        case 0:
        case 1:
            return;
        }
        DWCRap_ClearUserDataDirty();
    }
    WifiLogin_CheckLoggedIn(work);
}

static void WifiLogin_StateStartSave(WifiLoginWork *work) {
    if (WifiLoginMessage_IsDone(work->msg)) {
        PlayerInfo_InitProfileId(GetGameDataPlayerInfo(work->gameData), WifiList_GetMyGSID(work->wifiList));
        GameData_StartSave(work->gameData);
        work->saved = TRUE;
        WifiLogin_SetState(work, WifiLogin_StateWaitSave);
    }
}

static void WifiLogin_StateWaitMessageThenSave(WifiLoginWork *work) {
    if (WifiLoginMessage_IsDone(work->msg)) {
        if (DWCRap_IsUserDataDirty() && !work->saved && work->param->unk14 != 2) {
            WifiLoginMessage_PrintWaiting(work->msg, WifiError_Text_SavingDontTurnOff);
            WifiLogin_SetState(work, WifiLogin_StateStartSave);
        } else {
            WifiLogin_CheckLoggedIn(work);
        }
    }
}

// Starts the network library for the Wi-Fi connection
static void WifiLogin_StateStartNet(WifiLoginWork *work) {
    if (OS_IsRunOnTwl()) {
        sNetInit.wifiHeapSize = 0x79000;
    } else {
        sNetInit.wifiHeapSize = 0x5e400;
    }
    if (work->param->unkC == 0x2c) {
        sNetInit.wifiHeapSize = 0x50000;
    } else if (work->param->unkC == 0x24) {
        sNetInit.wifiHeapSize = 0x6e000;
    }
    if (work->param->unkC != 0xa) {
        sNetInit.getFriendData = NULL;
    }
    sNetInit.gameCommandBase = work->param->unkC;
    sGameType = work->param->unkC;
    GFL_NetInit(&sNetInit, NULL, work);
    NetState_StartWifiLogin();
    if (work->param->unk8 == 0) {
        GFL_NetSetIconScreen(FALSE, work->heapId);
    } else {
        GFL_NetSetIconScreen(TRUE, work->heapId);
    }
    WifiLoginMessage_PrintWaiting(work->msg, WifiError_Text_ConnectingNintendoWiFi);
    WifiLoginDisp_SetUnk9C4(work->disp);
    WifiLogin_SetState(work, WifiLogin_StateWaitMessageThenSave);
}

static void WifiLogin_StateWaitConfirmStart(WifiLoginWork *work) {
    if (WifiLoginMessage_IsMenuAnswered(work->menu)) {
        if (WifiLoginMessage_GetMenuAnswer(work->menu) == 0) {
            work->param->result = 0;
            WifiLogin_SetState(work, WifiLogin_StateStartNet);
        } else {
            work->param->result = 1;
            WifiLogin_SetState(work, WifiLogin_StateRunCallback);
        }
        WifiLoginMessage_ClearNotice(work->msg);
        if (work->param->unk4 == 0) {
            WifiLoginMessage_PrintTitle(work->msg, WIFILOGIN_TITLE_MSG);
        }
        WifiLoginMessage_FreeMenu(work->menu);
        work->menu = NULL;
    }
}

static void WifiLogin_StateWaitConfirmRetry(WifiLoginWork *work) {
    if (WifiLoginMessage_IsMenuAnswered(work->menu)) {
        if (WifiLoginMessage_GetMenuAnswer(work->menu) == 0) {
            work->param->result = 0;
            WifiLogin_SetState(work, WifiLogin_StateStartNet);
        } else {
            work->param->result = 1;
            WifiLogin_SetState(work, WifiLogin_StateRunCallback);
        }
        WifiLoginMessage_ClearNotice(work->msg);
        WifiLoginMessage_ClearWindow(work->msg);
        WifiLoginMessage_FreeMenu(work->menu);
        work->menu = NULL;
        if (work->param->unk4 == 0) {
            WifiLoginMessage_PrintTitle(work->msg, WIFILOGIN_TITLE_MSG);
        }
    }
}

static void WifiLogin_StateOpenConfirmStart(WifiLoginWork *work) {
    if (GFL_WipeIsFinished() && WifiLoginMessage_IsDone(work->msg)) {
        work->menu = WifiLoginMessage_CreateMenu(work->msg, 1, work->param->unk4 == 0, 0);
        WifiLogin_SetState(work, WifiLogin_StateWaitConfirmStart);
    }
}

static void WifiLogin_StateOpenConfirmError(WifiLoginWork *work) {
    if (GFL_WipeIsFinished()) {
        WifiLoginMessage_ClearTitle(work->msg);
        work->menu = WifiLoginMessage_CreateMenu(work->msg, 1, work->param->unk4 == 0, 0);
        WifiLogin_SetState(work, WifiLogin_StateWaitConfirmRetry);
    }
}

// Stops at a network error and tells the player, returning whether there was one
static BOOL WifiLogin_CheckNetError(WifiLoginWork *work) {
    if (GFL_NetErrCheck()) {
        switch (NetState_GetErrorInfo()->errorType) {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
            WifiLoginMessage_ClearNotice(work->msg);
            WifiLoginMessage_ClearTitle(work->msg);
            WifiLoginMessage_ClearWindow(work->msg);
            GFL_NetErrShowScreen();
            WifiLoginDisp_ResetBlend(work->disp);
            if (GFL_NetIsInitialized()) {
                GFL_NetErrShutdown();
            }
            NetState_ClearWifiError();
            GFL_NetErrClear();
            NetState_ClearError();
            WifiLoginMessage_PrintNotice(work->msg, WifiError_Text_DisconnectedFromNintendoWi);
            WifiLogin_SetState(work, WifiLogin_StateOpenConfirmError);
            return TRUE;
        case 7:
            GFL_NetErrAbort();
            break;
        }
    }
    return FALSE;
}

static void WifiLogin_StateOpenYesNo(WifiLoginWork *work) {
    if (GFL_WipeIsFinished() && WifiLoginMessage_IsDone(work->msg)) {
        work->menu = WifiLoginMessage_CreateMenu(work->msg, 0, work->param->unk4 == 0, 0);
        WifiLogin_SetState(work, WifiLogin_StateWaitConfirmRetry);
    }
}

static void WifiLogin_StateWaitResetConfirm(WifiLoginWork *work) {
    if (WifiLoginMessage_IsMenuAnswered(work->menu)) {
        if (WifiLoginMessage_GetMenuAnswer(work->menu) == 0) {
            void *entry =
                TournamentBlock_GetEntry(getTournamentBlockAddress(GameData_GetSaveControl(work->gameData)), 0);

            BattleBoxSave_ClearFlags(getBattleBox(GameData_GetSaveControl(work->gameData)), 1);
            TournamentEntry_SetField(entry, TOURNAMENT_FIELD_UNK15B, 5);
            work->param->result = 0;
            initWifi(work->wifiList);
            WifiLogin_SetState(work, WifiLogin_StateStartNet);
        } else {
            work->param->result = 1;
            WifiLogin_SetState(work, WifiLogin_StateCheckStart);
        }
        WifiLoginMessage_ClearNotice(work->msg);
        if (work->param->unk4 == 0) {
            WifiLoginMessage_PrintTitle(work->msg, WIFILOGIN_TITLE_MSG);
        }
        WifiLoginMessage_FreeMenu(work->menu);
        work->menu = NULL;
    }
}

static void WifiLogin_StateOpenResetConfirm(WifiLoginWork *work) {
    work->menu = WifiLoginMessage_CreateMenu(work->msg, 1, work->param->unk4 == 0, 1);
    WifiLogin_SetState(work, WifiLogin_StateWaitResetConfirm);
}

static void WifiLogin_StateWaitProfileConfirm(WifiLoginWork *work) {
    if (WifiLoginMessage_IsMenuAnswered(work->menu)) {
        u8 answer = WifiLoginMessage_GetMenuAnswer(work->menu);

        WifiLoginMessage_ClearNotice(work->msg);
        if (work->param->unk4 == 0) {
            WifiLoginMessage_PrintTitle(work->msg, WIFILOGIN_TITLE_MSG);
        }
        WifiLoginMessage_FreeMenu(work->menu);
        work->menu = NULL;
        if (answer == 0) {
            void *entry =
                TournamentBlock_GetEntry(getTournamentBlockAddress(GameData_GetSaveControl(work->gameData)), 0);
            u32 field2 = TournamentEntry_GetField(entry, TOURNAMENT_FIELD_CUP_ID);
            u32 field10 = TournamentEntry_GetField(entry, TOURNAMENT_FIELD_UNK15B);

            if (field2 != 0 && field10 >= 1 && field10 <= 4) {
                WifiLoginMessage_ClearTitle(work->msg);
                WifiLoginMessage_PrintNotice(work->msg, WifiError_Text_SureWantUpdateWi);
                WifiLogin_SetState(work, WifiLogin_StateOpenResetConfirm);
                return;
            }
            work->param->result = 0;
            initWifi(work->wifiList);
            WifiLogin_SetState(work, WifiLogin_StateStartNet);
        } else {
            work->param->result = 1;
            WifiLogin_SetState(work, WifiLogin_StateCheckStart);
        }
    }
}

static void WifiLogin_StateOpenProfileConfirm(WifiLoginWork *work) {
    work->menu = WifiLoginMessage_CreateMenu(work->msg, 1, work->param->unk4 == 0, 1);
    WifiLogin_SetState(work, WifiLogin_StateWaitProfileConfirm);
}

static void WifiLogin_StateWaitFixConfirm(WifiLoginWork *work) {
    if (WifiLoginMessage_IsMenuAnswered(work->menu)) {
        if (WifiLoginMessage_GetMenuAnswer(work->menu) == 0) {
            work->param->result = 0;
            WifiLoginMessage_ClearTitle(work->msg);
            WifiLoginMessage_PrintNotice(work->msg, WifiError_Text_ConnectNintendoWiFi_2);
            WifiLogin_SetState(work, WifiLogin_StateOpenProfileConfirm);
        } else {
            work->param->result = 1;
            WifiLoginMessage_ClearNotice(work->msg);
            if (work->param->unk4 == 0) {
                WifiLoginMessage_PrintTitle(work->msg, WIFILOGIN_TITLE_MSG);
            }
            WifiLogin_SetState(work, WifiLogin_StateRunCallback);
        }
        WifiLoginMessage_FreeMenu(work->menu);
        work->menu = NULL;
    }
}

static void WifiLogin_StateOpenFixConfirm(WifiLoginWork *work) {
    work->menu = WifiLoginMessage_CreateMenu(work->msg, 1, work->param->unk4 == 0, 1);
    WifiLogin_SetState(work, WifiLogin_StateWaitFixConfirm);
}

static void WifiLogin_StateWaitInputThenFix(WifiLoginWork *work) {
    if (GFL_WipeIsFinished() && WifiLogin_IsInputPressed(work)) {
        WifiLoginMessage_ClearTitle(work->msg);
        if (isRunningOnDSi()) {
            WifiLoginMessage_PrintNotice(work->msg, WifiError_Text_UpdateWiFiUser_2);
        } else {
            WifiLoginMessage_PrintNotice(work->msg, WifiError_Text_UpdateWiFiUser);
        }
        WifiLogin_SetState(work, WifiLogin_StateOpenFixConfirm);
    }
}

// Checks that the console can connect, and what state its settings are in
static void WifiLogin_StateCheckStart(WifiLoginWork *work) {
    if (!isWirelessEnabled()) {
        WifiLoginMessage_ClearTitle(work->msg);
        WifiLoginMessage_PrintNotice(work->msg, WifiError_Text_WirelessCommunicationsTurnedOff);
        work->param->result = 1;
        WifiLogin_SetState(work, WifiLogin_StateWaitMessageEnd);
        return;
    }
    if (!hasLicenseBeenAccepted()) {
        WifiLoginMessage_ClearTitle(work->msg);
        WifiLoginMessage_PrintNotice(work->msg, WifiError_Text_UseFeatureMustAccept);
        work->param->result = 1;
        WifiLogin_SetState(work, WifiLogin_StateWaitMessageEnd);
        return;
    }
    if (work->param->unk14 == 2) {
        WifiLoginMessage_Print(work->msg, WifiError_Text_ConnectNintendoWiFi);
        WifiLogin_SetState(work, WifiLogin_StateOpenYesNo);
        return;
    }
    if (!DWC_CheckHasProfile(WifiList_GetMyUserData(work->wifiList))) {
        work->userDataInvalid = TRUE;
        WifiLoginMessage_ClearTitle(work->msg);
        if (!isRunningOnDSi()) {
            WifiLoginMessage_PrintNotice(work->msg, WifiError_Text_NintendoWiFiConnection_4);
        } else {
            WifiLoginMessage_PrintNotice(work->msg, WifiError_Text_NintendoWiFiConnection_5);
        }
        WifiLogin_SetState(work, WifiLogin_StateOpenConfirmStart);
        return;
    }
    if (!DWC_CheckValidConsole(WifiList_GetMyUserData(work->wifiList))) {
        work->userDataInvalid = TRUE;
        WifiLoginMessage_ClearTitle(work->msg);
        if (!isRunningOnDSi()) {
            WifiLoginMessage_PrintNotice(work->msg, WifiError_Text_UnableConnectNintendoWi_4);
        } else {
            WifiLoginMessage_PrintNotice(work->msg, WifiError_Text_UnableConnectNintendoWi_5);
        }
        WifiLogin_SetState(work, WifiLogin_StateWaitInputThenFix);
        return;
    }
    if (work->param->unk14 == 1) {
        WifiLoginMessage_ClearTitle(work->msg);
        WifiLoginMessage_PrintNotice(work->msg, WifiError_Text_DisconnectedFromNintendoWi);
        WifiLogin_SetState(work, WifiLogin_StateOpenConfirmError);
    } else {
        WifiLoginMessage_Print(work->msg, WifiError_Text_ConnectNintendoWiFi);
        WifiLogin_SetState(work, WifiLogin_StateOpenYesNo);
    }
}

static void WifiLogin_StateWaitFadeOut(WifiLoginWork *work) {
    if (GFL_WipeIsFinished()) {
        WifiLogin_SetState(work, NULL);
    }
}

// Calls the caller's callback until it is done, then fades out
static void WifiLogin_StateRunCallback(WifiLoginWork *work) {
    if (work->param->unk20 != NULL && work->param->result == 0) {
        int status = work->param->unk20(work->msg, work->param->unk24);

        if (status == 2) {
            return;
        }
        switch (status) {
        case 0:
            work->param->result = 0;
            WifiLogin_SetState(work, WifiLogin_StateStartFadeOut);
            break;
        case 1:
            work->param->result = 1;
            WifiLoginMessage_ClearNotice(work->msg);
            WifiLoginMessage_ClearTitle(work->msg);
            WifiLoginMessage_ClearWindow(work->msg);
            if (GFL_NetIsInitialized()) {
                GFL_NetErrShutdown();
            }
            NetState_ClearWifiError();
            GFL_NetErrClear();
            NetState_ClearError();
            WifiLoginMessage_PrintNotice(work->msg, WifiError_Text_DisconnectedFromNintendoWi);
            WifiLogin_SetState(work, WifiLogin_StateOpenConfirmError);
            break;
        }
        return;
    }
    WifiLogin_SetState(work, WifiLogin_StateStartFadeOut);
}

static void WifiLogin_StateStartFadeOut(WifiLoginWork *work) {
    GFL_WipeSet(WIPE_MODE_BOTH, WIPE_TYPE_FADE_OUT, WIPE_TYPE_FADE_OUT, WIPE_COLOR_BLACK, 6, 1, work->heapId);
    if (work->param->unk4 == 0 && work->param->unk18 == 0) {
        GFL_SndBGMFadeOut(8);
    }
    WifiLogin_SetState(work, WifiLogin_StateWaitFadeOut);
}

// Connects to the Nintendo Wi-Fi Connection
static void WifiLogin_StateConnect(WifiLoginWork *work) {
    if (!WifiLogin_CheckNetError(work)) {
        if (DWC_SVLGetTokenAsync("", work->param->buffer)) {
            WifiLogin_SetState(work, WifiLogin_StateWaitConnect);
        } else {
            WifiLogin_CheckNetError(work);
        }
    }
}

static void WifiLogin_StateStartConnect(WifiLoginWork *work) {
    WifiLogin_SetState(work, WifiLogin_StateConnect);
}

static void WifiLogin_StateWaitConnect(WifiLoginWork *work) {
    if (!WifiLogin_CheckNetError(work)) {
        int connection = DWC_SVLProcess();

        if (connection == 3) {
            WifiLogin_SetState(work, WifiLogin_StateRunCallback);
        } else if (connection == 4 || connection == 0 || connection == 5) {
            WifiLogin_CheckNetError(work);
        }
    }
}

static BOOL WifiLoginProc_Init(GameProc *proc, u32 *state, void *param, void *work) {
    WifiLoginParam *loginParam = param;
    WifiLoginWork *wk;

    GFL_ASSERT_MSG(loginParam->unkC != 0, "\x83\x51\x81\x5b\x83\x80\x83\x54\x81\x5b\x83\x72\x83\x58ID\x82\xf0\x93\xfc"
                                          "\x82\xea\x82\xc4\x82\xad\x82\xbe\x82\xb3\x82\xa2\n");
    GFL_HeapCreateChild(HEAPID_USER, HEAPID_WIFILOGIN, 0x18000);
    wk = GFL_ProcInitSubsystem(proc, sizeof(WifiLoginWork), HEAPID_WIFILOGIN);
    MI_CpuFill8(wk, 0, sizeof(WifiLoginWork));
    wk->heapId = HEAPID_WIFILOGIN;
    wk->gameData = loginParam->gameData;
    wk->disp = WifiLoginDisp_Create(wk->heapId, loginParam->unk4, loginParam->unk8);
    wk->msg = WifiLoginMessage_Create(wk->heapId, TEXT_BANK_WIFI_ERROR, loginParam->unk8);
    wk->save = GameData_GetSaveControl(loginParam->gameData);
    wk->wifiList = GameData_GetWifiList(loginParam->gameData);
    GFL_WipeSet(WIPE_MODE_BOTH, WIPE_TYPE_FADE_IN, WIPE_TYPE_FADE_IN, WIPE_COLOR_BLACK, 6, 1, wk->heapId);
    wk->param = loginParam;
    if (GFL_NetIsInitialized()) {
        NetState_ClearWifiError();
        NetState_ClearError();
        GFL_NetErrShutdown();
        GFL_NetErrClear();
    }
    WifiLogin_SetState(wk, WifiLogin_StateCheckStart);
    if (wk->param->unk4 == 0) {
        WifiLoginMessage_PrintTitle(wk->msg, WIFILOGIN_TITLE_MSG);
        if (wk->param->unk18 == 0) {
            GFL_SndBGMPlay(SEQ_BGM_WIFI_ACCESS, SND_CHANNEL_MASK_ALL);
        }
    } else if (wk->param->unk18 == 0) {
        GFL_SndBGMPlay(SEQ_BGM_GAME_SYNC, SND_CHANNEL_MASK_ALL);
    }
    return TRUE;
}

static BOOL WifiLoginProc_Main(GameProc *proc, u32 *state, void *param, void *work) {
    WifiLoginWork *wk = work;
    BOOL done = TRUE;

    WifiLoginDisp_Main(wk->disp);
    WifiLoginMessage_Update(wk->msg);
    if (wk->state != NULL) {
        wk->state(wk);
        done = FALSE;
    }
    if (wk->menu != NULL) {
        WifiLoginMessage_UpdateMenu(wk->menu);
    }
    return done;
}

static BOOL WifiLoginProc_Exit(GameProc *proc, u32 *state, void *param, void *work) {
    WifiLoginWork *wk = work;

    GFL_BGSysReleaseBG(7);
    WifiLoginMessage_Free(wk->msg);
    WifiLoginDisp_Free(wk->disp);
    GFL_ProcReleaseSubsystem(proc);
    GFL_HeapDelete(HEAPID_WIFILOGIN);
    return TRUE;
}
