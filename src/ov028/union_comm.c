#include "field/union_comm.h"
#include "types.h"
#include "constants/language.h"
#include "constants/version.h"
#include "field/colosseum.h"
#include "field/comm_player.h"
#include "field/union_app.h"
#include "field/union_eff.h"
#include "field/union_main.h"
#include "field/unity_tower.h"
#include "gfl/heap.h"
#include "gfl/net.h"
#include "gfl/net_command.h"
#include "gfl/net_whpipe.h"
#include "gfl/random.h"
#include "gfl/std.h"
#include "nitro/os.h"
#include "save/wifi_list.h"
#include "system/game_data.h"
#include "system/game_system.h"

// The Union Room's communication: the system work that GameCommSys boots for GAME_COMM_NO_UNION, which restarts the
// network on request, receives the beacons of the machines around, keeps the group this machine is in and builds
// the beacon it sends. Names are not swan's

#define UNION_BEACON_TIMEOUT 450

static void UnionComm_Free(void *param, UnionSystem *unisys);
static void UnionComm_SetEnded(void *work);
static BOOL UnionComm_UpdateLink(UnionSystem *unisys);
static void UnionComm_UpdateGroup(UnionSystem *unisys);
static void UnionComm_UpdateEntries(UnionSystem *unisys);
static void UnionComm_UpdateColosseum(UnionSystem *unisys);
static void UnionComm_ReceiveBeacons(UnionSystem *unisys);
static BOOL UnionComm_AddBeacon(UnionSystem *unisys, UnionBeacon *beacon, const u8 *mac);
static BOOL UnionComm_IsMemberOfBeacons(UnionSystem *unisys, UnionBeacon *beacon, const u8 *mac);
static void *UnionComm_GetBeacon(void *work);
static void UnionComm_BuildBeacon(UnionSystem *unisys, UnionBeacon *beacon);
static int UnionComm_GetBeaconSize(void *work);
static BOOL UnionComm_IsBeaconJoinable(u8 gameCommandBase, u8 beaconCommandBase, void *work);
static void UnionComm_NetCallback24(NetHandle *handle, int a1, void *work);
static void UnionComm_NetCallback2C(void *work);
static void UnionComm_InitSelf(UnionSystem *unisys);
static void UnionComm_InitVisitors(UnionVisitorList *list);

static const NetCommand data_ov028_02172d04[22];

// clang-format off
static const GFLNetInitData data_ov028_02172c94 = {
    data_ov028_02172d04,
    22,
    NULL,
    NULL,
    NULL,
    NULL,
    UnionComm_GetBeacon,
    UnionComm_GetBeaconSize,
    UnionComm_IsBeaconJoinable,
    UnionComm_NetCallback24,
    NULL,
    UnionComm_NetCallback2C,
    NULL,
    {0},
    NULL,
    NULL,
    0,
    {1, 0, 0, 0, 0x80, 0x13, 0, 0},
    HEAPID_USER,
    HEAPID_TAIL(0xd),
    HEAPID_TAIL(0xf),
    HEAPID_TAIL(0xd),
    0xf0,
    0,
    5,
    0x5a,
    10,
    1,
    0,
    0,
    1,
    0x14,
    {0x2c, 1, 0, 0},
    0,
    0,
};

// clang-format on

static const NetCommand data_ov028_02172d04[22] = {
    { UnionNet_RecvGroupDone, NULL },
    { UnionNet_RecvUnk19, NULL },
    { UnionNet_RecvUnk1A, NULL },
    { UnionNet_RecvMac, NULL },
    { UnionNet_RecvUnk46_2, NULL },
    { UnionNet_RecvPlayerInfo, NULL },
    { UnionNet_RecvCard, UnionNet_GetRecvBuffer },
    { UnionNet_RecvColosseumMember, UnionNet_GetRecvBuffer },
    { UnionNet_RecvColosseumUnkDE, UnionNet_GetRecvBuffer },
    { UnionNet_RecvColosseumUnkCA, NULL },
    { UnionNet_RecvEntryRequest, NULL },
    { UnionNet_RecvEntryReply2, NULL },
    { UnionNet_RecvEntryReply1, NULL },
    { UnionNet_RecvEnqueue, NULL },
    { UnionNet_RecvUnkE, NULL },
    { UnionNet_RecvRequestStatus, NULL },
    { UnionNet_RecvStatus, NULL },
    { UnionNet_RecvAppProfile, NULL },
    { UnionNet_RecvProfile, NULL },
    { UnionNet_RecvConfirmProfile, NULL },
    { UnionNet_RecvLeave, NULL },
    { UnionNet_RecvJoinAvenue, UnionNet_GetRecvBuffer },
};

UnionSystem *UnionComm_Create(UnionBootParam *param) {
    UnionSystem *unisys;

    GFL_HeapCreateChild(HEAPID_GAMEEVENT, HEAPID_UNION, 0x8a00);
    unisys = GFL_HeapAllocate(HEAPID_UNION, sizeof(UnionSystem), TRUE, "union_comm.c", 137);
    unisys->param = param;
    UnionComm_InitSelf(unisys);
    unisys->situ.changed = TRUE;
    UnionComm_InitVisitors(&unisys->visitorList);
    unisys->alloc.regulation = Regulation_Create(HEAPID_UNION);
    unisys->fieldActive = TRUE;
    return unisys;
}

static void UnionComm_Free(void *param, UnionSystem *unisys) {
    if (unisys->alloc.regulation != NULL) {
        GFL_HeapFree(unisys->alloc.regulation);
    }
    UnionComm_FreeResources(unisys);
    GFL_HeapFree(unisys);
    GFL_HeapDelete(HEAPID_UNION);
    GFL_HeapFree(param);
}

void *UnionComm_Init(u32 *seq, void *param) {
    UnionSystem *unisys = UnionComm_Create(param);

    unisys->commState = 1;
    GFL_NetInit(&data_ov028_02172c94, UnionComm_OnNetReady, unisys);
    return unisys;
}

BOOL UnionComm_InitWait(u32 *seq, void *param, void *work) {
    UnionSystem *unisys = work;

    if (unisys->commState >= 2) {
        unisys->commState = 3;
        GFL_NetStartSearch(0);
        return TRUE;
    }
    return FALSE;
}

void UnionComm_OnNetReady(void *work) {
    UnionSystem *unisys = work;

    unisys->commState = 2;
    GFL_NetSetRejectNegotiated(FALSE);
}

BOOL UnionComm_Exit(u32 *seq, void *param, void *work) {
    UnionSystem *unisys = work;

    unisys->unk3634 = TRUE;
    if (unisys->commState < 3) {
        return FALSE;
    }
    unisys->commState = 4;
    if (!GFL_NetExit(UnionComm_SetEnded)) {
        UnionComm_SetEnded(unisys);
    }
    return TRUE;
}

BOOL UnionComm_ExitWait(u32 *seq, void *param, void *work) {
    UnionSystem *unisys = work;

    if (unisys->commState == 5) {
        UnionComm_Free(param, unisys);
        if (GFL_NetErrCheck()) {
            GFL_NetErrClear();
        }
        return TRUE;
    }
    return FALSE;
}

static void UnionComm_SetEnded(void *work) {
    UnionSystem *unisys = work;

    unisys->commState = 5;
}

void UnionComm_RequestLink0(UnionSystem *unisys) {
    if (unisys->linkState == 0) {
        unisys->linkState = 1;
        unisys->linkMode = 0;
    }
}

void UnionComm_RequestLink1(UnionSystem *unisys) {
    if (unisys->linkState == 0) {
        unisys->linkState = 1;
        unisys->linkMode = 1;
    }
}

void UnionComm_RequestLink2(UnionSystem *unisys) {
    if (unisys->linkState == 0) {
        unisys->linkState = 1;
        unisys->linkMode = 2;
    }
}

BOOL UnionComm_IsLinkRequested(UnionSystem *unisys) {
    if (unisys->linkState != 0) {
        return TRUE;
    }
    return FALSE;
}

static BOOL UnionComm_UpdateLink(UnionSystem *unisys) {
    switch (unisys->linkState) {
    case 0:
        if (unisys->linkMode == 1) {
            return TRUE;
        }
        return FALSE;
    case 1:
        switch (unisys->linkMode) {
        case 0:
        case 1:
            unisys->linkState = 2;
            break;
        case 2:
            unisys->linkState = 4;
            break;
        default:
            unisys->linkState = 0;
            break;
        }
        break;
    case 2:
        unisys->commState = 4;
        if (!GFL_NetExit(UnionComm_SetEnded)) {
            UnionComm_SetEnded(unisys);
        }
        unisys->linkState++;
        break;
    case 3:
        if (unisys->commState == 5) {
            if (unisys->linkMode == 1) {
                unisys->linkState = 0;
            } else {
                unisys->linkState++;
            }
        }
        break;
    case 4:
        unisys->commState = 1;
        GFL_NetInit(&data_ov028_02172c94, UnionComm_OnNetReady, unisys);
        unisys->linkState++;
        break;
    case 5:
        if (unisys->commState >= 2) {
            unisys->commState = 3;
            GFL_NetStartSearch(0);
            unisys->linkState = 0;
        }
        break;
    }
    return TRUE;
}

void UnionComm_Main(u32 *seq, void *param, void *work) {
    UnionSystem *unisys = work;

    if (UnionComm_UpdateLink(unisys) == TRUE) {
        return;
    }
    if (GFL_NetIsInitialized()) {
        if (unisys->beaconReady == 1) {
            NetWhpipe_UpdateGameBeacon();
            unisys->beaconReady = 0;
        }
        UnionComm_ReceiveBeacons(unisys);
        UnionComm_UpdateColosseum(unisys);
        UnionComm_UpdateEntries(unisys);
        UnionComm_UpdateGroup(unisys);
        if (GFL_NetErrCheck() && GFL_NetIsParent() == TRUE) {
            UnionColosseum *colosseum = unisys->colosseum_sys;

            if (colosseum != NULL && colosseum->unkC8_0 == 1 && unisys->situ.step == 0x2a && !colosseum->unkC8_1) {
                UnionComm_RequestLink1(unisys);
                unisys->colosseum_sys->unkC8_1 = TRUE;
            }
        }
    }
}

void UnionComm_ActivateGroup(UnionSystem *unisys) {
    unisys->situ.mycomm.active = TRUE;
}

static void UnionComm_UpdateGroup(UnionSystem *unisys) {
    UnionSelf *self = &unisys->situ;

    if (!self->mycomm.active) {
        return;
    }
    if (GFL_NetErrCheck()) {
        return;
    }
    switch (self->mycomm.step) {
    case 0:
        if (UnionMain_SendGroupDone() == TRUE) {
            self->mycomm.step++;
        }
        break;
    case 1:
        if (MATH_CountPopulation(self->mycomm.doneMask) >= GFL_NetGetNegotiatedCount()) {
            GFL_NetSetDisconnectError(FALSE);
            self->mycomm.step++;
        }
        break;
    case 2:
        if (GFL_NetIsParent() == TRUE) {
            if (GFL_NetGetNegotiatedCount() > 1) {
                break;
            }
            UnionComm_RequestLink1(unisys);
            self->mycomm.step++;
        } else {
            UnionComm_RequestLink1(unisys);
            self->mycomm.step++;
        }
        break;
    }
}

static void UnionComm_UpdateEntries(UnionSystem *unisys) {
    if (GFL_NetGetNegotiatedCount() > 1) {
        if (unisys->entryRepliedTo != 0) {
            if (UnionMain_SendEntryReply2(unisys->entryRepliedTo) == 1) {
                unisys->entryRepliedTo = 0;
            }
        }
        if (unisys->entryAcceptedTo != 0) {
            if (UnionMain_SendEntryReply1(unisys->entryAcceptedTo) == 1) {
                unisys->entryAcceptedTo = 0;
            }
        }
    } else {
        unisys->entryRepliedTo = 0;
        unisys->entryAcceptedTo = 0;
    }
}

static void UnionComm_UpdateColosseum(UnionSystem *unisys) {
    if (unisys->colosseum_sys == NULL || !unisys->colosseum_sys->unkC8_0) {
        return;
    }
    if (UnionMain_IsFieldReady(unisys) == TRUE) {
        if (!UnionCommand_IsActive(unisys)) {
            CommPlayer_Update(unisys->param->gsys, unisys->colosseum_sys->commPlayer);
            if (unisys->colosseum_sys->unk17F == 0) {
                if (CommPlayer_GetMyStatus(unisys->colosseum_sys->commPlayer,
                                           (CommPlayerStatus *)unisys->colosseum_sys->unk04) == TRUE) {
                    unisys->colosseum_sys->unk17F = 1;
                }
            }
            if (unisys->colosseum_sys->unk17F == 1) {
                if (Colosseum_SendChoice((ColosseumChoice *)unisys->colosseum_sys->unk04) == TRUE) {
                    unisys->colosseum_sys->unk17F = 0;
                }
            }
        }
        UnionEff_UpdateColosseumCommPlayers(unisys->colosseum_sys);
    }
    Colosseum_SendVoteReplies(unisys->colosseum_sys);
    Colosseum_UpdateReadySync(unisys->colosseum_sys);
}

static void UnionComm_ReceiveBeacons(UnionSystem *unisys) {
    int i;

    for (i = 0; i < 10; i++) {
        UnionBeacon *beacon = GFL_NetGetBeaconData(i);

        if (beacon != NULL) {
            u8 *mac = GFL_NetGetBeaconMac(i);

            if (mac != NULL) {
                UnionComm_AddBeacon(unisys, beacon, mac);
            }
            NetWhpipe_ConsumeBeacon(i);
        }
    }
}

static BOOL UnionComm_AddBeacon(UnionSystem *unisys, UnionBeacon *beacon, const u8 *mac) {
    int i;
    UnionBeaconEntry *found = unisys->found;

    if (beacon->valid != 1) {
        return FALSE;
    }
    for (i = 0; i < UNION_BEACON_ENTRY_MAX; i++) {
        UnionBeaconEntry *entry = &found[i];

        if (entry->beacon.valid == 1 && GFL_STD_MemCmp(mac, entry->mac, 6) == 0) {
            MI_CpuCopy8(beacon, entry, sizeof(UnionBeacon));
            if (found[i].status != 1) {
                found[i].status = 2;
            }
            found[i].timeout = UNION_BEACON_TIMEOUT;
            return TRUE;
        }
    }
    if (!UnionComm_IsMemberOfBeacons(unisys, beacon, mac)) {
        return FALSE;
    }
    for (i = 0; i < UNION_BEACON_ENTRY_MAX; i++) {
        UnionBeaconEntry *entry = &found[i];

        if (entry->beacon.valid != 1) {
            MI_CpuCopy8(beacon, entry, sizeof(UnionBeacon));
            MI_CpuCopy8(mac, entry->mac, 6);
            found[i].index = i;
            found[i].status = 1;
            found[i].timeout = UNION_BEACON_TIMEOUT;
            return TRUE;
        }
    }
    return FALSE;
}

// Whether the machine is a member of a group that one of the beacons found is the parent of, which keeps it from
// being added (and gives that beacon the shortest timeout)
static BOOL UnionComm_IsMemberOfBeacons(UnionSystem *unisys, UnionBeacon *beacon, const u8 *mac) {
    UnionBeaconEntry *entry = unisys->found;
    int i;
    int j;

    for (i = 0; i < UNION_BEACON_ENTRY_MAX; i++, entry++) {
        if (entry->beacon.valid == 1) {
            for (j = 0; j < UNION_MEMBER_MAX; j++) {
                if (entry->beacon.members[j].used == 1 && GFL_STD_MemCmp(mac, entry->beacon.members[j].mac, 6) == 0) {
                    if (entry->timeout != 0) {
                        entry->timeout = 1;
                    }
                    return FALSE;
                }
            }
        }
    }
    return TRUE;
}

static void *UnionComm_GetBeacon(void *work) {
    UnionSystem *unisys = work;

    UnionComm_BuildBeacon(unisys, &unisys->beacon);
    return &unisys->beacon;
}

static void UnionComm_BuildBeacon(UnionSystem *unisys, UnionBeacon *beacon) {
    UnionSelf *self = &unisys->situ;

    MI_CpuFill8(beacon, 0, sizeof(UnionBeacon));
    if (self->mycomm.parent != NULL) {
        if (GFL_NetIsParent() == TRUE) {
            OS_GetMacAddress(beacon->mac);
        } else {
            MI_CpuCopy8(self->mycomm.parent->mac, beacon->mac, 6);
        }
    } else if (self->mycomm.unk4 != NULL) {
        MI_CpuCopy8(self->mycomm.unk4->mac, beacon->mac, 6);
    }
    beacon->memberCount = GFL_NetGetNegotiatedCount();
    beacon->version = GAME_VERSION;
    beacon->language = GAME_LANGUAGE;
    beacon->unk0A = self->step;
    beacon->unk07_5 = self->unk02;
    beacon->activity = self->activity;
    copyName(unisys->param->playerInfo->name, beacon->name, 8);
    beacon->look = PlayerInfo_GetUnionView(unisys->param->playerInfo);
    beacon->gender = getTrainerGender(unisys->param->playerInfo);
    beacon->trainerId = getIDAsUInt(unisys->param->playerInfo);
    beacon->state = self->state;
    beacon->look2[0] = self->look[0];
    beacon->look2[1] = self->look[1];
    beacon->look2[2] = self->look[2];
    beacon->look2[3] = self->look[3];
    beacon->counter = self->counter;
    beacon->country = UnityTowerVisitor_GetCountry(unisys->param->playerInfo);
    beacon->province = UnityTowerVisitor_GetProvince(unisys->param->playerInfo);
    beacon->memberList = self->mycomm.memberList;
    WifiList_CreateExchangeToken(GameData_GetWifiList(GSYS_GetGameData(unisys->param->gsys)), &beacon->unk50);
    if (unisys->alloc.uniapp != NULL) {
        beacon->entryClosed = UnionApp_IsEntryClosed(unisys->alloc.uniapp);
    } else {
        beacon->entryClosed = FALSE;
    }
    beacon->valid = TRUE;
    unisys->beaconReady = TRUE;
}

static int UnionComm_GetBeaconSize(void *work) {
    return sizeof(UnionBeacon);
}

static BOOL UnionComm_IsBeaconJoinable(u8 gameCommandBase, u8 beaconCommandBase, void *work) {
    UnionSystem *unisys = work;

    if (unisys->situ.mycomm.unk46_0 == 1) {
        if (gameCommandBase == 0x14 || beaconCommandBase == 0x14) {
            return FALSE;
        }
        if (gameCommandBase == beaconCommandBase) {
            return TRUE;
        }
        return FALSE;
    }
    if (beaconCommandBase >= 0x14 && beaconCommandBase <= 0x1b) {
        return TRUE;
    }
    return FALSE;
}

static void UnionComm_NetCallback24(NetHandle *handle, int a1, void *work) {
}

static void UnionComm_NetCallback2C(void *work) {
}

void UnionComm_ClearBeacons(UnionSystem *unisys) {
    MI_CpuFill8(unisys->found, 0, sizeof(unisys->found));
}

void UnionComm_FreeResources(UnionSystem *unisys) {
    unisys->alloc.cardParam = NULL;
    if (unisys->alloc.my_card != NULL) {
        GFL_HeapFree(unisys->alloc.my_card);
        unisys->alloc.my_card = NULL;
    }
    if (unisys->alloc.target_card != NULL) {
        GFL_HeapFree(unisys->alloc.target_card);
        unisys->alloc.target_card = NULL;
    }
    if (unisys->alloc.boxParty != NULL) {
        GFL_HeapFree(unisys->alloc.boxParty);
        unisys->alloc.boxParty = NULL;
    }
    if (unisys->alloc.partySelect != NULL) {
        GFL_HeapFree(unisys->alloc.partySelect);
        unisys->alloc.partySelect = NULL;
    }
    if (unisys->alloc.uniapp != NULL) {
        UnionApp_Free(unisys->alloc.uniapp);
        unisys->alloc.uniapp = NULL;
    }
    if (unisys->colosseum_sys != NULL) {
        Colosseum_Free(unisys->colosseum_sys, unisys->param->gsys);
        unisys->colosseum_sys = NULL;
    }
}

void UnionComm_SetSelf(UnionSystem *unisys, u32 which, uptr value) {
    UnionSelf *self = &unisys->situ;

    switch (which) {
    case 0:
        self->mycomm.target = (UnionBeaconEntry *)value;
        break;
    case 1:
        self->mycomm.unk4 = (UnionBeaconEntry *)value;
        break;
    case 2:
        self->mycomm.answer_pc = (UnionBeaconEntry *)value;
        break;
    case 3:
        self->mycomm.parent = (UnionBeaconEntry *)value;
        break;
    case 4:
        self->activity = value;
        break;
    }
}

static void UnionComm_InitSelf(UnionSystem *unisys) {
    UnionSelf *self = &unisys->situ;

    MI_CpuFill8(self, 0, sizeof(UnionSelf));
    self->counter = GFL_RandomLC(0xffff);
    self->state = 1;
    self->step = 2;
    UnionGroup_Init(unisys, &self->mycomm);
}

void UnionGroup_ResetTargets(UnionGroup *group) {
    group->unk19 = 0xff;
    group->unk1B = 0xff;
    group->unk1A = 0xff;
}

void UnionGroup_Init(UnionSystem *unisys, UnionGroup *group) {
    UnionMember *self;

    MI_CpuFill8(group, 0, sizeof(UnionGroup));
    UnionGroup_ResetTargets(group);
    self = &group->members[0];
    OS_GetMacAddress(self->mac);
    self->look = PlayerInfo_GetUnionView(unisys->param->playerInfo);
    self->gender = getTrainerGender(unisys->param->playerInfo);
    self->used = TRUE;
    group->unk44 = 0xffff;
}

int UnionGroup_AddMember(UnionGroup *group, const u8 *mac, u8 look, u8 gender) {
    int i;
    int j;

    for (i = 0; i < UNION_MEMBER_MAX; i++) {
        UnionMember *member = &group->members[i];

        if (member->used == 1 && GFL_STD_MemCmp(mac, member->mac, 6) == 0) {
            return i;
        }
    }
    for (j = 0; j < UNION_MEMBER_MAX; j++) {
        UnionMember *member = &group->members[j];

        if (!member->used) {
            MI_CpuCopy8(mac, member->mac, 6);
            member->look = look;
            member->gender = gender;
            member->used = TRUE;
            return j;
        }
    }
    return 0;
}

void UnionGroup_AddBeaconMember(UnionGroup *group, const UnionBeaconEntry *entry) {
    UnionGroup_AddMember(group, entry->mac, entry->beacon.look, entry->beacon.gender);
}

void UnionGroup_RemoveMember(UnionGroup *group, const u8 *mac) {
    int i;

    for (i = 0; i < UNION_MEMBER_MAX; i++) {
        UnionMember *member = &group->members[i];

        if (member->used == 1 && GFL_STD_MemCmp(mac, member->mac, 6) == 0) {
            MI_CpuFill8(member, 0, sizeof(UnionMember));
            return;
        }
    }
}

void UnionGroup_RemoveBeaconMember(UnionGroup *group, const UnionBeaconEntry *entry) {
    UnionGroup_RemoveMember(group, entry->mac);
}

static void UnionComm_InitVisitors(UnionVisitorList *list) {
    int i;

    MI_CpuFill8(list, 0, sizeof(UnionVisitorList));
    for (i = 0; i < UNION_VISITOR_MAX; i++) {
        MI_CpuFill8(list->visitors[i].mac, 0xff, 6);
    }
    list->count = -1;
    list->viewEnd = -1;
    list->drawnEnd = -1;
}

void UnionComm_SetLook(UnionSystem *unisys, const u16 *look) {
    UnionSelf *self = &unisys->situ;

    self->look[0] = look[0];
    self->look[1] = look[1];
    self->look[2] = look[2];
    self->look[3] = look[3];
    self->state = 0;
    self->counter++;
    self->changed = TRUE;
}

void UnionComm_SetState(UnionSystem *unisys, u8 state) {
    UnionSelf *self = &unisys->situ;

    self->state = state;
    self->counter++;
    self->changed = TRUE;
}

void UnionComm_SetCommandBase(u32 activity) {
    switch (activity) {
    case 0x14:
        GFL_NetSetGameCommandBase(0x17, 5);
        break;
    case 0x15:
        GFL_NetSetGameCommandBase(0x18, 5);
        break;
    case 0x16:
        GFL_NetSetGameCommandBase(0x19, 5);
        break;
    case 0x17:
        GFL_NetSetGameCommandBase(0x1a, 5);
        break;
    case 3:
        GFL_NetSetGameCommandBase(0x16, 5);
        break;
    case 0x19:
        GFL_NetSetGameCommandBase(0x1b, 5);
        break;
    }
}
