#include "types.h"
#include "gfl/backup_card.h"
#include "gfl/heap.h"
#include "gfl/net.h"
#include "gfl/net_command.h"
#include "gfl/net_handle.h"
#include "gfl/net_irc_wireless.h"
#include "gfl/net_lower_data.h"
#include "gfl/net_state.h"
#include "gfl/net_system.h"
#include "gfl/std.h"
#include "gfl/ui.h"
#include "gfl/wm_icon.h"

// The infrared packets' size limit
#define GFL_NET_IRC_SEND_MAX 0x80

static struct {
    // The heap net.c makes its heaps under, once GFL_NetBoot has made it
    HeapID parentHeapId;
    // The offset of BG 1 of the main engine
    int bg1Y;
    GFLNetSys *pNet;
    int bg1X;
} sNet;

static void GFL_NetFinishExit(void *work);
static void GFL_NetSetInitIconPos(int x, int y);
static void GFL_NetRefreshIcon(void);

void GFL_NetBoot(int a0, int a1, HeapID parentHeapId, HeapID heapId) {
    const GFLNetDevTable *pDevTable = NetDefine_LoadDevice(GFL_NET_TYPE_WIFI);

    if (pDevTable != NULL) {
        pDevTable->prepare(a0, a1);
    }
    NetDefine_UnloadDevice(GFL_NET_TYPE_WIFI);
    if (parentHeapId != heapId) {
        sNet.parentHeapId = heapId;
        GFL_HeapCreateChild(parentHeapId, heapId, 0x9e80);
    }
    WmIcon_SetPos(0, 0);
    WmIcon_Delete();
}

void GFL_NetInit(const GFLNetInitData *pNetInit, void (*callback)(void *work), void *work) {
    GFLNetSys *pNet;

    if (sNet.parentHeapId != 0) {
        GFL_HeapCreateChild(sNet.parentHeapId, pNetInit->heapId, 0x7800);
    } else {
        GFL_HeapCreateChild(pNetInit->parentHeapId, pNetInit->heapId, 0x7800);
    }
    pNet = GFL_HeapAllocate(pNetInit->heapId, sizeof(GFLNetSys), TRUE, "net.c", 106);
    sNet.pNet = pNet;
    pNet->rejectNegotiated = TRUE;
    pNet->pDevTable = NetDefine_LoadDevice(pNetInit->bNetType);
    GFL_ASSERT(pNet->pDevTable);
    if (pNetInit->bNetType == 4) {
        pNetInit = NetIw_Start(pNetInit, work);
    }
    MI_CpuCopy8(pNetInit, &pNet->aNetInit, sizeof(GFLNetInitData));
    switch (pNetInit->bNetType) {
    case GFL_NET_TYPE_WIFI:
    case GFL_NET_TYPE_WIFI_LOBBY:
    case GFL_NET_TYPE_WIFI_GTS:
        pNet->pDevTable->prepare(pNetInit->parentHeapId, 0);
        GFL_HeapCreateChild(pNetInit->parentHeapId, pNetInit->wifiHeapId | HEAPID_TAIL_BIT, pNetInit->wifiHeapSize);
        break;
    case 3:
    case 4:
        GFL_ASSERT(pNetInit->maxSendSize <= GFL_NET_IRC_SEND_MAX);
        if (sNet.parentHeapId != 0) {
            GFL_HeapCreateChild(sNet.parentHeapId, pNetInit->ircHeapId, 0x1b00);
        } else {
            GFL_HeapCreateChild(pNetInit->parentHeapId, pNetInit->ircHeapId, 0x1b00);
        }
        break;
    case 0:
    case 5:
        break;
    default:
        GFL_ASSERT(0);
        return;
    }
    pNet->devWork = work;
    NetState_Start(pNetInit->heapId, callback);
    NetHandle_InitAll(pNet);
    NetCommand_Init(pNetInit->gameCommandBase << 8, pNetInit->commandTable, pNetInit->commandNum, pNet->devWork,
                    pNetInit->heapId);
    if (pNetInit->bNetType != 3 && pNetInit->bNetType != 4) {
        BOOL isWifi = FALSE;

        if (pNetInit->bNetType == GFL_NET_TYPE_WIFI || pNetInit->bNetType == GFL_NET_TYPE_WIFI_LOBBY) {
            isWifi = TRUE;
        }
        WmIcon_Create(pNetInit->iconX, pNetInit->iconY, isWifi, pNetInit->heapId);
    }
}

BOOL GFL_NetIsInitialized(void) {
    GFLNetSys *pNet = GFL_NetGetSys();

    if (pNet == NULL) {
        return FALSE;
    }
    return pNet->pDevTable->isRunning();
}

BOOL GFL_NetIsEnded(void) {
    if (GFL_NetGetSys() == NULL) {
        return TRUE;
    }
    return FALSE;
}

static void GFL_NetFinishExit(void *work) {
    GFLNetSys *pNet = GFL_NetGetSys();
    HeapID heapId = pNet->aNetInit.heapId;
    HeapID wifiHeapId = pNet->aNetInit.wifiHeapId;
    HeapID ircHeapId = pNet->aNetInit.ircHeapId;
    u8 type = pNet->aNetInit.bNetType;

    if (pNet->exitCallback != NULL) {
        pNet->exitCallback(pNet->devWork);
    }
    if (pNet->aNetInit.endCallback != NULL) {
        pNet->aNetInit.endCallback(pNet->devWork);
    }
    NetHandle_FreeAll(pNet);
    NetDefine_UnloadDevice(type);
    GFL_HeapFree(pNet);
    sNet.pNet = NULL;
    switch (type) {
    case GFL_NET_TYPE_WIFI:
    case GFL_NET_TYPE_WIFI_LOBBY:
    case GFL_NET_TYPE_WIFI_GTS:
        GFL_HeapDelete(wifiHeapId);
        break;
    case 3:
    case 4:
        GFL_HeapDelete(ircHeapId);
        break;
    case 0:
    case 5:
        break;
    default:
        GFL_ASSERT(0);
        return;
    }
    GFL_HeapDelete(heapId);
    GCTX_HIDUnblockSleep(4);
}

BOOL GFL_NetExit(void (*callback)(void *work)) {
    GFLNetSys *pNet = GFL_NetGetSys();

    NetIw_FreeIfIdle();
    if (pNet != NULL) {
        u8 type = pNet->aNetInit.bNetType;

        pNet->exitCallback = callback;
        if (type == GFL_NET_TYPE_WIFI) {
            NetState_EndWifi(GFL_NetFinishExit);
        } else {
            NetState_End(GFL_NetFinishExit);
        }
        WmIcon_Delete();
        return TRUE;
    }
    return FALSE;
}

void GFL_NetCancelEnd(void) {
    NetState_CancelEnd();
}

void *GFL_NetGetBeaconData(int index) {
    GFLNetSys *pNet = GFL_NetGetSys();

    if (pNet == NULL) {
        return NULL;
    }
    return pNet->pDevTable->getBeaconData(index);
}

u8 *GFL_NetGetBeaconMac(int index) {
    GFLNetSys *pNet = GFL_NetGetSys();

    return pNet->pDevTable->getBeaconMac(index);
}

BOOL GFL_NetMainWhileWaiting(void) {
    GFLNetSys *pNet = GFL_NetGetSys();

    NetLdata_Update();
    NetIw_Update();
    if (NetState_Update()) {
        return TRUE;
    }
    if (pNet == NULL) {
        return TRUE;
    }
    WmIcon_Update();
    NetHandle_Update();
    NetSystem_Update();
    return NetSystem_IsDataShareOk();
}

void GFL_NetUpdateDevice(void) {
    GFLNetSys *pNet = GFL_NetGetSys();

    if (!NetState_Update() && pNet != NULL && pNet->pDevTable != NULL) {
        u8 type = GFL_NetGetInitData()->bNetType;

        if (type != GFL_NET_TYPE_WIFI && type != GFL_NET_TYPE_WIFI_LOBBY) {
            pNet->pDevTable->update(0);
        }
    }
}

void GFL_NetStartAsParent(const u8 *mac) {
    NetState_ConnectMac(mac, TRUE);
}

void GFL_NetStartAsChild(const u8 *mac) {
    NetState_ConnectMac(mac, FALSE);
}

void GFL_NetStartScan(void) {
    NetState_StartScan();
}

void GFL_NetStartServer(void) {
    GFLNetSys *pNet = GFL_NetGetSys();

    switch (pNet->aNetInit.bNetType) {
    case GFL_NET_TYPE_WIFI:
    case GFL_NET_TYPE_WIFI_LOBBY:
        NetState_ConnectWifiAny();
        break;
    case 0:
    case 5:
        NetState_StartParent(pNet->aNetInit.heapId);
        break;
    }
}

void GFL_NetIwChangeoverConnect(void (*callback)(void *work), void (*a1)(void *work, BOOL a1), void (*a2)(void *work)) {
    GFLNetSys *pNet = GFL_NetGetSys();

    if (NetIw_IsRunning() == TRUE) {
        callback = NetIw_SetCallbacks(callback, a1, a2);
    }
    NetState_StartChangeover(pNet->aNetInit.heapId, callback, 1);
}

void GFL_NetIwSetGameCommandBase(int a0) {
    NetIw_SetGameCommandBase(a0);
}

void GFL_NetIwGetCollectedMac(u8 *mac, int index) {
    NetIw_GetCollectedMac(mac, index);
}

void GFL_NetIwSetPeerFilter(void *a0) {
    NetIw_SetPeerFilter(a0);
}

void GFL_NetIwFree(void) {
    NetIw_Free();
}

void GFL_NetChangeoverConnect(void (*callback)(void *work)) {
    GFLNetSys *pNet = GFL_NetGetSys();

    NetState_StartChangeover(pNet->aNetInit.heapId, callback, 1);
}

void GFL_NetStartSearch(int unused) {
    GFLNetSys *pNet = GFL_NetGetSys();

    NetState_StartSearch();
}

void GFL_NetHoldScan(int a0) {
    if (GFL_NetIsInitialized()) {
        NetSystem_HoldScan(a0);
    }
}

void GFL_NetSetChangeoverMode(int mode, int a1, const u8 *mac) {
    NetState_SetChangeoverMode(mode, a1, mac);
}

BOOL GFL_NetIwFinishCollecting(void) {
    return NetIw_FinishCollecting();
}

void GFL_NetIwOverrideNetId(int a0) {
    NetIw_OverrideNetId(a0);
}

u8 GFL_NetIwGetLinkedCount(void) {
    return NetIw_GetLinkedCount();
}

void GFL_NetSetTimeout(int a0) {
    GFLNetSys *pNet = GFL_NetGetSys();

    if (pNet->pDevTable != NULL) {
        pNet->pDevTable->setTimeout(a0);
    }
}

u8 GFL_NetGetHandleNetId(NetHandle *handle) {
    return NetHandle_GetNetId(handle);
}

int GFL_NetGetNegotiatedCount(void) {
    return NetHandle_CountNegotiated();
}

BOOL GFL_NetIsNegotiated(u8 netId) {
    if (GFL_NetGetSys() == NULL) {
        return FALSE;
    }
    return NetHandle_IsNegotiated(NetHandle_Get(netId));
}

void GFL_NetSetAcceptConnect(NetHandle *handle, int a1) {
    GFLNetSys *pNet = GFL_NetGetSys();

    if (pNet->pDevTable->setAcceptConnect != NULL) {
        pNet->pDevTable->setAcceptConnect(a1);
    }
}

BOOL GFL_NetIsNotInitialized(void) {
    if (sNet.pNet == NULL) {
        return TRUE;
    }
    if (!GFL_NetGetSys()->pDevTable->isRunning()) {
        return TRUE;
    }
    return FALSE;
}

static void GFL_NetSetInitIconPos(int x, int y) {
    GFLNetInitData *ini = GFL_NetGetInitData();

    if (ini != NULL) {
        ini->iconX = x;
        ini->iconY = y;
    }
}

BOOL GFL_NetIsInfrared(void) {
    GFLNetSys *pNet = GFL_NetGetSys();

    if (pNet != NULL && (pNet->aNetInit.bNetType == 3 || pNet->aNetInit.bNetType == 4)) {
        return TRUE;
    }
    return FALSE;
}

BOOL GFL_NetIsWifi(void) {
    GFLNetSys *pNet = GFL_NetGetSys();

    if (pNet != NULL &&
        (pNet->aNetInit.bNetType == GFL_NET_TYPE_WIFI || pNet->aNetInit.bNetType == GFL_NET_TYPE_WIFI_LOBBY)) {
        return TRUE;
    }
    return FALSE;
}

static void GFL_NetRefreshIcon(void) {
    GFLNetInitData *ini = GFL_NetGetInitData();
    GFLNetSys *pNet = GFL_NetGetSys();

    if (getLockIDStatus() == TRUE) {
        GFL_ASSERT(0);
        return;
    }
    if (pNet != NULL && ini->bNetType != 3) {
        BOOL isWifi = FALSE;

        if (pNet->aNetInit.bNetType == GFL_NET_TYPE_WIFI || pNet->aNetInit.bNetType == GFL_NET_TYPE_WIFI_LOBBY) {
            isWifi = TRUE;
        }
        WmIcon_Create(pNet->aNetInit.iconX, pNet->aNetInit.iconY, isWifi, pNet->aNetInit.heapId);
    } else {
        WmIcon_Delete();
    }
}

void GFL_NetSetIconScreen(BOOL top, HeapID heapId) {
    GFL_NetSetInitIconPos(240, 0);
    WmIcon_SetScreen(top, heapId);
    GFL_NetRefreshIcon();
}

BOOL GFL_NetIsParent(void) {
    BOOL ret = FALSE;

    if (NetSystem_GetNetId() == 0) {
        ret = TRUE;
    }
    return ret;
}

BOOL GFL_NetIsHandleNegotiated(NetHandle *handle) {
    if (handle->state == NET_HANDLE_STATE_NEGOTIATED) {
        return TRUE;
    }
    return FALSE;
}

BOOL GFL_NetSendCommand(NetHandle *handle, int command, u16 size, const void *data) {
    GFLNetInitData *ini = GFL_NetGetInitData();

    return NetSystem_QueueCommand(command, (u8 *)data, size, 0, NetHandle_GetNetId(handle), 0xff, FALSE);
}

BOOL GFL_NetSendCommandToMachine(NetHandle *handle, u8 sendID, u16 command, u32 size, const void *data, u32 a5, BOOL a6,
                                 BOOL noCopy) {
    GFLNetInitData *ini = GFL_NetGetInitData();
    u8 dest = 0xff;

    if (!GFL_NetIsHandleNegotiated(handle) && command >= GFL_NET_CMD_BASE_COUNT) {
        return FALSE;
    }
    if (a6 && NetSystem_HasQueuedCommand(command, NetHandle_GetNetId(handle))) {
        return FALSE;
    }
    if (sendID != 0xff) {
        GFL_ASSERT(sendID < 7);
        dest = 1 << sendID;
    }
    return NetSystem_QueueCommand(command, (u8 *)data, size, a5, NetHandle_GetNetId(handle), dest, noCopy);
}

BOOL GFL_NetSendCommandToBits(NetHandle *handle, int dest, u16 command, int size, const void *data, u32 a5, BOOL a6,
                              BOOL noCopy) {
    GFLNetInitData *ini = GFL_NetGetInitData();

    if (!GFL_NetIsHandleNegotiated(handle) && command >= GFL_NET_CMD_BASE_COUNT) {
        return FALSE;
    }
    if (a6 && NetSystem_HasQueuedCommand(command, NetHandle_GetNetId(handle))) {
        return FALSE;
    }
    return NetSystem_QueueCommand(command, (u8 *)data, size, a5, NetHandle_GetNetId(handle), dest, noCopy);
}

BOOL GFL_NetIsSendQueueEmpty(NetHandle *handle) {
    return NetSystem_IsSendQueueEmpty();
}

void GFL_NetSendTiming(NetHandle *handle, u16 timing) {
    NetHandle_SendTiming(handle, timing);
}

BOOL GFL_NetIsTimingRecv(NetHandle *handle, u16 timing) {
    return NetHandle_IsTimingRecv(handle, timing);
}

void GFL_NetSetGameCommandBase(u8 gameCommandBase, u8 unused) {
    GFLNetSys *pNet = GFL_NetGetSys();

    GFL_NetGetInitData()->gameCommandBase = gameCommandBase;
    NetHandle_SetNegotiationCommandBase(NetHandle_Get(GFL_NET_NETID_SERVER), gameCommandBase);
}

u8 GFL_NetGetGameCommandBase(void) {
    GFLNetInitData *ini = GFL_NetGetInitData();

    if (ini != NULL) {
        return ini->gameCommandBase;
    }
    return 0;
}

sptr GFL_NetGetUserData(void) {
    GFLNetSys *pNet = GFL_NetGetSys();

    if (pNet->aNetInit.getUserData != NULL) {
        return pNet->aNetInit.getUserData(pNet->devWork);
    }
    return 0;
}

sptr GFL_NetGetFriendData(void) {
    GFLNetSys *pNet = GFL_NetGetSys();

    if (pNet->aNetInit.getFriendData != NULL) {
        return pNet->aNetInit.getFriendData(pNet->devWork);
    }
    return 0;
}

u8 func_02042d80(void) {
    GFLNetSys *pNet = GFL_NetGetSys();

    return pNet->aNetInit.unk62;
}

void GFL_NetSetMPSendEnabled(BOOL a0) {
    NetSystem_SetMPSendEnabled(a0);
}

void *GFL_NetGetWork(void) {
    GFLNetSys *pNet = GFL_NetGetSys();

    if (pNet != NULL) {
        return pNet->devWork;
    }
    return NULL;
}

void GFL_NetSetWork(void *work) {
    GFLNetSys *pNet = GFL_NetGetSys();

    if (pNet != NULL) {
        pNet->devWork = work;
    }
}

int GFL_NetGetMaxConnectNum(void) {
    GFLNetSys *pNet = GFL_NetGetSys();

    GFL_ASSERT(pNet->aNetInit.maxConnectNum <= GFL_NET_MACHINE_MAX);
    return pNet->aNetInit.maxConnectNum;
}

int GFL_NetGetMaxSendSize(void) {
    GFLNetSys *pNet = GFL_NetGetSys();

    return pNet->aNetInit.maxSendSize;
}

u16 GFL_NetGetParentPacketSize(void) {
    GFLNetSys *pNet = GFL_NetGetSys();

    GFL_ASSERT(pNet);
    return pNet->aNetInit.parentPacketSize;
}

void GFL_NetNotifyChildConnect(void) {
    GFLNetSys *pNet = GFL_NetGetSys();

    if (pNet->aNetInit.childConnectCallback != NULL) {
        pNet->aNetInit.childConnectCallback(pNet->devWork);
    }
}

void func_02042e30(int a0) {
    GFLNetSys *pNet = GFL_NetGetSys();

    if (pNet->aNetInit.unk28 != NULL) {
        pNet->aNetInit.unk28(NetHandle_GetOwn(), a0, pNet->devWork);
    }
}

void GFL_NetNotifyError(int a0) {
    GFLNetSys *pNet = GFL_NetGetSys();

    if (pNet->aNetInit.errorCallback != NULL) {
        pNet->aNetInit.errorCallback(NetHandle_GetOwn(), a0, pNet->devWork);
    }
}

GFLNetSys *GFL_NetGetSys(void) {
    return sNet.pNet;
}

GFLNetInitData *GFL_NetGetInitData(void) {
    if (sNet.pNet != NULL) {
        return &sNet.pNet->aNetInit;
    }
    return NULL;
}

void func_02042e94(u8 a0) {
    func_02042410(a0);
}

void GFL_NetSetDisconnectError(BOOL a0) {
    NetState_SetDisconnectError(a0);
}

// Allocates 32-byte aligned memory, keeping the allocation just before it for GFL_NetFreeAligned32
void *allocConfigDSSoftwareFeature(HeapID heapId, u32 size, const char *file, u16 line) {
    void *mem = GFL_HeapAllocate(heapId, ((size + sizeof(void *) + 32) & ~31) + 32, TRUE, file, line);
    void **aligned = (void **)(((uptr)mem + 32) & ~31);

    aligned[-1] = mem;
    return aligned;
}

void GFL_NetFreeAligned32(void *ptr) {
    if (ptr != NULL) {
        GFL_HeapFree(((void **)ptr)[-1]);
    }
}

void GFL_NetSetBG1Offset(int x, int y) {
    sNet.bg1X = x;
    sNet.bg1Y = y;
}

void GFL_NetGetBG1Offset(int *x, int *y) {
    *x = sNet.bg1X;
    *y = sNet.bg1Y;
}

void GFL_NetSetInitData(const GFLNetInitData *pNetInit) {
    MI_CpuCopy8(pNetInit, &GFL_NetGetSys()->aNetInit, sizeof(GFLNetInitData));
}

void GFL_NetClearCallbacks(void) {
    GFLNetSys *pNet = GFL_NetGetSys();

    if (pNet != NULL) {
        pNet->aNetInit.endCallback = NULL;
        pNet->aNetInit.getBeaconSize = NULL;
    }
}

BOOL GFL_NetHasError(void) {
    return NetSystem_HasErrorChecked();
}

void GFL_NetMoveIcon(int x, int y) {
    WmIcon_SetPos(x, y);
    GFL_NetRefreshIcon();
}

void GFL_NetResetIconPos(void) {
    WmIcon_SetPos(0, 0);
    GFL_NetRefreshIcon();
}

void GFL_NetSetRejectNegotiated(BOOL a0) {
    if (sNet.pNet != NULL) {
        if (a0 == TRUE) {
            sNet.pNet->rejectNegotiated = TRUE;
        } else {
            sNet.pNet->rejectNegotiated = FALSE;
        }
    }
}
