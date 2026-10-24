// The Union Room's steps on the field for this machine (its situ, UnionSelf): what it asks, answers and joins. Every
// step is a row of begin, run and end functions. The file's name is from its heap allocations; the actors of the
// players heard are union_actor.c's. Names are not swan's
#include "field/union_oneself.h"
#include "types.h"
#include "app/pms_select.h"
#include "app/pokelist.h"
#include "app/pokemon_trade.h"
#include "battle/regulation.h"
#include "constants/sound.h"
#include "constants/version.h"
#include "constants/zones.h"
#include "demo/shinka_demo.h"
#include "field/colosseum.h"
#include "field/comm_entry_menu.h"
#include "field/comm_player.h"
#include "field/field_actor.h"
#include "field/field_controller.h"
#include "field/field_player.h"
#include "field/trcard_sys.h"
#include "field/union_actor.h"
#include "field/union_comm.h"
#include "field/union_eff.h"
#include "field/union_main.h"
#include "field/union_msg.h"
#include "gfl/heap.h"
#include "gfl/key.h"
#include "gfl/net.h"
#include "gfl/net_system.h"
#include "gfl/sound.h"
#include "gfl/std.h"
#include "nitro/math.h"
#include "nitro/os.h"
#include "pml/poke_party.h"
#include "save/box.h"
#include "save/player_info.h"
#include "save/save_control.h"
#include "system/game_comm.h"
#include "system/game_data.h"
#include "system/game_system.h"
#include "system/wordset.h"

// The keys that end a message: A, B and the Control Pad
#define UNION_PAD_ANY (PAD_BUTTON_A | PAD_BUTTON_B | PAD_PLUS_KEY_MASK)

// What overlay 34 allocates for the commands it starts, for the draw and the battle
typedef struct {
    u32 unk00;
    GameData *gameData;
} UnionDrawParam;

typedef struct {
    u32 unk00;
    GameData *gameData;
    PokeParty *party;
    SaveControl *save;
    GameRecords *records;
    u32 unk14;
} UnionBattleParam;

// A step of the machine
typedef struct {
    UnionSituFunc begin;
    UnionSituFunc run;
    UnionSituFunc end;
} UnionSituStep;

static void UnionOneself_CheckParentEntry(UnionSystem *unisys, UnionBeaconEntry *entry);
static BOOL UnionOneself_SetNextStep(UnionSystem *unisys, u32 step);
static BOOL UnionOneself_SetNextStepIfRunning(UnionSystem *unisys, u32 step);
static u32 UnionOneself_GetRegulationFile(u32 activity);
static BOOL UnionOneself_IsDoubleActivity(u32 activity);
static BOOL UnionOneself_IsBattleActivity(u32 activity);
static PokeParty *UnionOneself_CreateBoxParty(UnionSystem *unisys);
static void UnionOneself_FreeBoxParty(PokeParty *party);
static void UnionOneself_SetRegulation(UnionSystem *unisys, u32 activity);
static u32 UnionOneself_CheckPartyRegulation(UnionSystem *unisys, u32 *result);
static u32 UnionOneself_CheckBoxRegulation(UnionSystem *unisys, u32 *result);
static BOOL UnionOneself_CheckRegulation(UnionSystem *unisys, u32 activity, u32 *partyResult, u32 *boxResult);
static BOOL UnionOneself_CheckNetError(UnionSystem *unisys);
static BOOL UnionOneself_CheckNetErrorEnd(UnionSystem *unisys);
static BOOL UnionOneself_StepIdleBegin(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepIdleRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepNetErrorRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepNone(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepWaitCounter(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepGreeting(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepAskBegin(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepAskRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepAskEnd(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepAskedBegin(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepAskedRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepAskedEnd(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepParentBegin(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepParentRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepParentEnd(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepOfferRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepOfferBegin(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepOfferParty(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepOfferPartyEnd(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepRuleSelectRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepAnswerRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepAnswerMember(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static u32 UnionOneself_GetCardVersion(UnionSystem *unisys, BOOL remote);
static BOOL UnionOneself_StepCardRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepTradeRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepRecruitRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepLeaveRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepJoinRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepColosseumChildRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepColosseumParentRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepColosseumRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static void UnionOneself_AddColosseumMember(void *work, const PlayerInfo *info, const u8 *mac);
static void UnionOneself_RemoveColosseumMember(void *work, const PlayerInfo *info, const u8 *mac);
static BOOL UnionOneself_StepColosseumSendRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepIdleColosseumBegin(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepIdleColosseumRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepColosseumSeatRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepColosseumLeaveRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepPartySelectRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepPartyWaitRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepPartySendRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepBattleListRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepQuitRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepColosseumEndRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);
static BOOL UnionOneself_StepColosseumCardRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);

// clang-format off
static const UnionSituStep sUnionSituSteps[] = {
    { UnionOneself_StepIdleBegin, UnionOneself_StepIdleRun, NULL }, // UNION_STEP_IDLE
    { NULL, UnionOneself_StepNetErrorRun, NULL }, // UNION_STEP_RESET
    { NULL, UnionOneself_StepNone, NULL }, // UNION_STEP_NONE
    { NULL, UnionOneself_StepWaitCounter, NULL }, // UNION_STEP_COUNTER
    { NULL, UnionOneself_StepGreeting, NULL }, // UNION_STEP_GREETING
    { UnionOneself_StepAskBegin, UnionOneself_StepAskRun, UnionOneself_StepAskEnd }, // UNION_STEP_ASK
    { UnionOneself_StepAskedBegin, UnionOneself_StepAskedRun, UnionOneself_StepAskedEnd }, // UNION_STEP_ASKED
    { UnionOneself_StepParentBegin, UnionOneself_StepParentRun, UnionOneself_StepParentEnd }, // UNION_STEP_PARENT
    { NULL, UnionOneself_StepOfferRun, NULL }, // UNION_STEP_OFFER
    { UnionOneself_StepOfferBegin, UnionOneself_StepOfferParty, UnionOneself_StepOfferPartyEnd }, // UNION_STEP_OFFER_PARTY
    { NULL, UnionOneself_StepRuleSelectRun, NULL }, // UNION_STEP_RULE_SELECT
    { NULL, UnionOneself_StepAnswerRun, NULL }, // UNION_STEP_ANSWER
    { NULL, UnionOneself_StepAnswerMember, NULL }, // UNION_STEP_ANSWER_MEMBER
    { NULL, UnionOneself_StepCardRun, NULL }, // UNION_STEP_CARD
    { NULL, UnionOneself_StepJoinRun, NULL }, // UNION_STEP_JOIN_DRAW
    { NULL, UnionOneself_StepColosseumChildRun, NULL }, // UNION_STEP_JOIN_BATTLE_0
    { NULL, UnionOneself_StepColosseumChildRun, NULL }, // UNION_STEP_JOIN_BATTLE_1
    { NULL, UnionOneself_StepColosseumChildRun, NULL }, // UNION_STEP_JOIN_BATTLE_2
    { NULL, UnionOneself_StepColosseumChildRun, NULL }, // UNION_STEP_JOIN_BATTLE_3
    { NULL, UnionOneself_StepColosseumChildRun, NULL }, // UNION_STEP_JOIN_BATTLE_4
    { NULL, UnionOneself_StepColosseumChildRun, NULL }, // UNION_STEP_JOIN_BATTLE_5
    { NULL, UnionOneself_StepColosseumChildRun, NULL }, // UNION_STEP_JOIN_BATTLE_6
    { NULL, UnionOneself_StepColosseumChildRun, NULL }, // UNION_STEP_JOIN_BATTLE_7
    { NULL, UnionOneself_StepColosseumChildRun, NULL }, // UNION_STEP_JOIN_BATTLE_8
    { NULL, UnionOneself_StepColosseumChildRun, NULL }, // UNION_STEP_JOIN_BATTLE_9
    { NULL, UnionOneself_StepColosseumChildRun, NULL }, // UNION_STEP_JOIN_BATTLE_10
    { NULL, UnionOneself_StepColosseumChildRun, NULL }, // UNION_STEP_JOIN_BATTLE_11
    { NULL, UnionOneself_StepColosseumChildRun, NULL }, // UNION_STEP_JOIN_BATTLE_12
    { NULL, UnionOneself_StepColosseumChildRun, NULL }, // UNION_STEP_JOIN_BATTLE_13
    { NULL, UnionOneself_StepColosseumChildRun, NULL }, // UNION_STEP_JOIN_BATTLE_14
    { NULL, UnionOneself_StepColosseumChildRun, NULL }, // UNION_STEP_JOIN_BATTLE_15
    { NULL, UnionOneself_StepColosseumParentRun, NULL }, // UNION_STEP_JOIN_MULTI_0
    { NULL, UnionOneself_StepColosseumParentRun, NULL }, // UNION_STEP_JOIN_MULTI_1
    { NULL, UnionOneself_StepColosseumParentRun, NULL }, // UNION_STEP_JOIN_MULTI_2
    { NULL, UnionOneself_StepColosseumParentRun, NULL }, // UNION_STEP_JOIN_MULTI_3
    { NULL, UnionOneself_StepTradeRun, NULL }, // UNION_STEP_TRADE
    { NULL, UnionOneself_StepJoinRun, NULL }, // UNION_STEP_JOIN_EGG
    { NULL, UnionOneself_StepLeaveRun, NULL }, // UNION_STEP_LEAVE_2
    { NULL, UnionOneself_StepRecruitRun, NULL }, // UNION_STEP_RECRUIT
    { NULL, UnionOneself_StepLeaveRun, NULL }, // UNION_STEP_LEAVE
    { NULL, UnionOneself_StepColosseumRun, NULL }, // UNION_STEP_COLOSSEUM
    { NULL, UnionOneself_StepColosseumSendRun, NULL }, // UNION_STEP_COLOSSEUM_SEND
    { UnionOneself_StepIdleColosseumBegin, UnionOneself_StepIdleColosseumRun, NULL }, // UNION_STEP_COLOSSEUM_IDLE
    { NULL, UnionOneself_StepColosseumSeatRun, NULL }, // UNION_STEP_COLOSSEUM_SIT
    { NULL, UnionOneself_StepColosseumLeaveRun, NULL }, // UNION_STEP_COLOSSEUM_STAND
    { NULL, UnionOneself_StepPartySelectRun, NULL }, // UNION_STEP_PARTY_SELECT
    { NULL, UnionOneself_StepPartyWaitRun, NULL }, // UNION_STEP_PARTY_WAIT
    { NULL, UnionOneself_StepPartySendRun, NULL }, // UNION_STEP_PARTY_SEND
    { NULL, UnionOneself_StepBattleListRun, NULL }, // UNION_STEP_BATTLE_LIST
    { NULL, UnionOneself_StepQuitRun, NULL }, // UNION_STEP_QUIT
    { NULL, UnionOneself_StepColosseumEndRun, NULL }, // UNION_STEP_COLOSSEUM_END
    { NULL, UnionOneself_StepColosseumCardRun, NULL }, // UNION_STEP_COLOSSEUM_CARD
    { NULL, NULL, NULL }, // UNION_STEP_END
};

// clang-format on

void UnionOneself_CheckEntries(UnionSystem *unisys) {
    int i;
    UnionBeaconEntry *entry = unisys->found;

    for (i = 0; i < UNION_BEACON_ENTRY_MAX; i++, entry++) {
        if (entry->beacon.valid == 1 && entry->status != 0) {
            UnionOneself_CheckParentEntry(unisys, entry);
            entry->status = 0;
        }
    }
}

static void UnionOneself_CheckParentEntry(UnionSystem *unisys, UnionBeaconEntry *entry) {
    UnionSelf *situ = &unisys->situ;
    u8 ownMac[6];

    UnionChat_AppendLine(unisys, entry, NULL, 0);
    if (situ->step == 0 && situ->nextStep == 0 && entry->beacon.unk0A == 5 && entry->timeout != 0) {
        OS_GetMacAddress(ownMac);
        if (GFL_STD_MemCmp(entry, ownMac, 6) == 0 &&
            UnionOneself_SetNextStepIfRunning(unisys, UNION_STEP_ASKED) == TRUE) {
            UnionComm_SetSelf(unisys, 2, (uptr)entry);
            UnionComm_SetSelf(unisys, 0, (uptr)entry);
        }
    }
}

GameEvent *UnionOneself_CreateCommandEvent(GameSystem *gsys, Field *field) {
    UnionSystem *unisys = GameCommSys_GetWork(GSYS_GetGameCommSystem(gsys));
    GameEvent *event = NULL;
    UnionSubproc *subproc;

    subproc = &unisys->subproc;
    if (subproc->id != UNION_SUBPROC_ID_NULL && subproc->active == FALSE) {
        event = UnionCommand_CreateEvent(gsys, field, unisys);
    }
    return event;
}

GameEvent *UnionOneself_ReturnFalse(GameSystem *gsys, Field *field) {
    return NULL;
}

u8 UnionOneself_RunStep(UnionSystem *unisys, Field *field) {
    UnionSelf *situ = &unisys->situ;
    UnionSituFunc func;
    BOOL done = TRUE;

    switch (situ->phase) {
    case 0:
        func = sUnionSituSteps[situ->step].begin;
        break;
    case 1:
        func = sUnionSituSteps[situ->step].run;
        break;
    case 2:
        func = sUnionSituSteps[situ->step].end;
        break;
    }
    if (func != NULL) {
        done = func(unisys, situ, field, &situ->seq);
    }
    if (done == TRUE) {
        situ->seq = 0;
        situ->phase++;
        if (situ->phase > 2) {
            situ->phase = 0;
            situ->prevStep = situ->step;
            situ->step = situ->nextStep;
            situ->nextStep = 0;
        }
    }
    return situ->step;
}

static BOOL UnionOneself_SetNextStep(UnionSystem *unisys, u32 step) {
    unisys->situ.nextStep = step;
    return TRUE;
}

static BOOL UnionOneself_SetNextStepIfRunning(UnionSystem *unisys, u32 step) {
    if (unisys->situ.phase == 1) {
        return UnionOneself_SetNextStep(unisys, step);
    }
    return FALSE;
}

// The regulation of an activity, by its file
static u32 UnionOneself_GetRegulationFile(u32 activity) {
    switch (activity) {
    case 4:
    case 5:
        return 5;
    case 6:
    case 7:
        return 0x18;
    case 8:
    case 9:
        return 6;
    case 10:
    case 11:
        return 0x19;
    case 12:
    case 13:
        return 7;
    case 14:
    case 15:
        return 0x1a;
    case 16:
    case 17:
        return 8;
    case 18:
    case 19:
        return 0x1b;
    case 20:
    case 21:
        return 9;
    case 22:
    case 23:
        return 0x1c;
    default:
        return 5;
    }
}

static BOOL UnionOneself_IsDoubleActivity(u32 activity) {
    switch (activity) {
    case 4:
    case 6:
    case 8:
    case 10:
    case 12:
    case 14:
    case 16:
    case 18:
    case 20:
    case 22:
        return TRUE;
    default:
        return FALSE;
    }
}

static BOOL UnionOneself_IsBattleActivity(u32 activity) {
    if (activity < 4 || activity > 0x17) {
        return FALSE;
    }
    return TRUE;
}

static PokeParty *UnionOneself_CreateBoxParty(UnionSystem *unisys) {
    BattleBoxSave *battleBox = getBattleBox(GameData_GetSaveControl(unisys->param->gameData));

    if (BattleBoxSave_HasPkm(battleBox) == FALSE) {
        return NULL;
    }
    return convertBoxedPokeSetToParty(battleBox, HEAPID_UNION);
}

static void UnionOneself_FreeBoxParty(PokeParty *party) {
    GFL_ASSERT(party != NULL);
    GFL_HeapFree(party);
}

static void UnionOneself_SetRegulation(UnionSystem *unisys, u32 activity) {
    PokeRegulation_Load(UnionOneself_GetRegulationFile(activity), unisys->alloc.regulation);
    if (UnionOneself_IsDoubleActivity(activity) == TRUE) {
        Regulation_SetParam(unisys->alloc.regulation, REGULATION_PARAM_LAUNCHER, 1);
    } else {
        Regulation_SetParam(unisys->alloc.regulation, REGULATION_PARAM_LAUNCHER, 0);
    }
}

static u32 UnionOneself_CheckPartyRegulation(UnionSystem *unisys, u32 *result) {
    *result = 0;
    return PokeRegulation_CheckParty(unisys->alloc.regulation, GameData_GetParty(unisys->param->gameData), result);
}

static u32 UnionOneself_CheckBoxRegulation(UnionSystem *unisys, u32 *result) {
    PokeParty *party = UnionOneself_CreateBoxParty(unisys);
    u32 ret;

    *result = 0;
    if (party != NULL) {
        ret = PokeRegulation_CheckParty(unisys->alloc.regulation, party, result);
        UnionOneself_FreeBoxParty(party);
    } else {
        *result = -1;
        ret = 5;
    }
    return ret;
}

static BOOL UnionOneself_CheckRegulation(UnionSystem *unisys, u32 activity, u32 *partyResult, u32 *boxResult) {
    u32 partyRet;
    u32 boxRet;

    *partyResult = 0;
    *boxResult = 0;
    if (!UnionOneself_IsBattleActivity(activity)) {
        return TRUE;
    }
    UnionOneself_SetRegulation(unisys, activity);
    partyRet = UnionOneself_CheckPartyRegulation(unisys, partyResult);
    boxRet = UnionOneself_CheckBoxRegulation(unisys, boxResult);
    if (partyRet == 0 || boxRet == 0) {
        return TRUE;
    }
    return FALSE;
}

void UnionOneself_SetUpdateDisabled(UnionSystem *unisys, Field *field, u8 disable) {
    unisys->updateDisabled = disable;
    FieldmapCtrlGrid_SetUpdateDisable(field, disable);
}

static BOOL UnionOneself_CheckNetError(UnionSystem *unisys) {
    if (GFL_NetErrCheck()) {
        UnionOneself_SetNextStep(unisys, UNION_STEP_RESET);
        return TRUE;
    }
    return FALSE;
}

static BOOL UnionOneself_CheckNetErrorEnd(UnionSystem *unisys) {
    if (UnionOneself_CheckNetError(unisys) == TRUE) {
        UnionOneself_SetNextStep(unisys, UNION_STEP_COLOSSEUM_END);
        return TRUE;
    }
    return FALSE;
}

static BOOL UnionOneself_StepIdleBegin(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    UnionGroup_Init(unisys, &situ->mycomm);
    UnionComm_SetSelf(unisys, 4, 0);
    UnionMsg_FreeAll(unisys);
    UnionOneself_SetUpdateDisabled(unisys, field, FALSE);
    unisys->unk282E = 0;
    return TRUE;
}

static BOOL UnionOneself_StepIdleRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    FieldPlayer *player = Field_GetPlayer(field);
    MMSys *actorSystem = Field_GetActorSystem(field);
    s16 x, y, z;

    if (UnionOneself_CheckNetError(unisys) == TRUE) {
        return TRUE;
    }
    if (UnionComm_IsLinkRequested(unisys) == TRUE) {
        return FALSE;
    }
    if (situ->nextStep != 0) {
        return TRUE;
    }
    if (GFL_NetGetNegotiatedCount() > 1) {
        UnionComm_RequestLink0(unisys);
        return FALSE;
    }
    if (GCTX_HIDGetPressedKeys() & 1) {
        FieldActor *actor;
        u16 id;
        u16 entryIndex;

        FieldPlayer_GetGPosInFront(player, &x, &y, &z);
        actor = GetFirstActorOnGPos(actorSystem, x, z, FALSE);
        if (actor == NULL) {
            return FALSE;
        }
        id = GetActorUID(actor);
        if (UnionActor_IsId(id) == FALSE || UnionOneself_CanTalkTo(unisys, id) == FALSE) {
            return FALSE;
        }
        situ->mycomm.unk44 = id;
        entryIndex = UnionOneself_GetEntryIndex(id);
        UnionComm_SetSelf(unisys, 0, (uptr)&unisys->found[entryIndex]);
        if (UnionOneself_IsMemberId(id) == UNION_ACTOR_PARENT) {
            if (unisys->found[entryIndex].beacon.activity == 0) {
                UnionComm_SetSelf(unisys, 1, (uptr)&unisys->found[entryIndex]);
                UnionOneself_SetNextStep(unisys, UNION_STEP_ASK);
            } else {
                UnionOneself_SetNextStep(unisys, UNION_STEP_ANSWER);
            }
        } else {
            UnionOneself_SetNextStep(unisys, UNION_STEP_ANSWER_MEMBER);
        }
        UnionOneself_SetUpdateDisabled(unisys, field, TRUE);
        return TRUE;
    }
    if (unisys->unk282E == 1) {
        UnionOneself_SetNextStep(unisys, UNION_STEP_GREETING);
        return TRUE;
    }
    if (UnionEff_IsPlayerOnSpot(field) == TRUE) {
        if (unisys->unk362C == 1) {
            UnionOneself_SetUpdateDisabled(unisys, field, TRUE);
            UnionOneself_SetNextStep(unisys, UNION_STEP_COUNTER);
        }
    } else {
        unisys->unk362C = 1;
    }
    return FALSE;
}

static BOOL UnionOneself_StepNetErrorRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    switch (*seq) {
    case 0:
        UnionOneself_SetUpdateDisabled(unisys, field, TRUE);
        (*seq)++;
        break;
    case 1:
        if (UnionMsg_IsPrintDone(unisys) == TRUE) {
            UnionMsg_FreeAll(unisys);
            (*seq)++;
        }
        break;
    case 2:
        if (GFL_NetErrCheck()) {
            GSYS_SetNetErrorPending(unisys->param->gsys, 1);
            return FALSE;
        }
        UnionComm_RequestLink1(unisys);
        (*seq)++;
        break;
    case 3:
        if (!UnionComm_IsLinkRequested(unisys)) {
            (*seq)++;
        }
        break;
    case 4:
        UnionComm_FreeResources(unisys);
        UnionGroup_Init(unisys, &situ->mycomm);
        UnionComm_RequestLink2(unisys);
        (*seq)++;
        break;
    case 5:
        if (!UnionComm_IsLinkRequested(unisys)) {
            (*seq)++;
        }
        break;
    default:
        UnionOneself_SetNextStep(unisys, UNION_STEP_IDLE);
        return TRUE;
    }
    return FALSE;
}

static BOOL UnionOneself_StepNone(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    return TRUE;
}

static BOOL UnionOneself_StepWaitCounter(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    if (*seq == 0) {
        unisys->unk3630 = 1;
        (*seq)++;
    }
    return FALSE;
}

static BOOL UnionOneself_StepGreeting(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    PMSSelectParam *param;
    PMSData *result;

    switch (*seq) {
    case 0:
        if (unisys->unk282E == 0) {
            return TRUE;
        }
        unisys->unk282E = 0;
        GFL_SndSEPlay(SEQ_SE_SELECT4);
        param = GFL_HeapAllocate(HEAPID_GAMEEVENT, sizeof(PMSSelectParam), TRUE, "union_oneself.c", 1050);
        param->save = GameData_GetSaveControl(unisys->param->gameData);
        unisys->cmdArg = param;
        UnionCommand_Set(unisys, 29, param);
        (*seq)++;
        break;
    case 1:
        if (!UnionCommand_IsActive(unisys)) {
            (*seq)++;
        }
        break;
    case 2:
        param = unisys->cmdArg;
        result = param->result;
        if (param->cancel == 0) {
            UnionComm_SetLook(unisys, (const u16 *)result);
        }
        GFL_HeapFree(unisys->cmdArg);
        unisys->cmdArg = NULL;
        return TRUE;
    }
    return FALSE;
}

static BOOL UnionOneself_StepAskBegin(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    GFL_SndSEPlay(SEQ_SE_MESSAGE);
    UnionMsg_CreateAndPrint(unisys, field, 0xe2);
    return TRUE;
}

static BOOL UnionOneself_StepAskRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    int i;

    if (UnionOneself_CheckNetError(unisys) == TRUE) {
        return TRUE;
    }
    switch (*seq) {
    case 0:
        GFL_NetSetChangeoverMode(1, 1, NULL);
        situ->wait = 0;
        (*seq)++;
        break;
    case 1:
        if (UnionMsg_IsPrintDone(unisys)) {
            if (GFL_NetGetNegotiatedCount() > 1) {
                for (i = 2; i < 5; i++) {
                    if (GFL_NetIsNegotiated((u8)i) == TRUE) {
                        break;
                    }
                }
                if (i < 5 || NetSystem_CountConnected() > 2) {
                    UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetNoReplyMessage(unisys));
                    UnionOneself_SetNextStep(unisys, UNION_STEP_IDLE);
                    UnionComm_RequestLink0(unisys);
                    *seq = 4;
                } else {
                    GFL_NetSetDisconnectError(TRUE);
                    GFL_NetSetAcceptConnect(NetHandle_GetOwn(), 0);
                    *seq = 2;
                }
            } else {
                situ->wait++;
                if (situ->wait > 600 || (GCTX_HIDGetPressedKeys() & PAD_BUTTON_B)) {
                    UnionComm_RequestLink0(unisys);
                    UnionOneself_SetNextStep(unisys, UNION_STEP_IDLE);
                    UnionComm_SetSelf(unisys, 1, 0);
                    if (situ->wait > 600) {
                        UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetNoReplyMessage(unisys));
                    } else {
                        UnionMsg_CreateAndPrint(unisys, field, 0xa);
                    }
                    *seq = 5;
                }
            }
        }
        break;
    case 2:
        if (situ->mycomm.unk10[6] == 1) {
            if (GFL_STD_MemCmp(situ->mycomm.unk10, situ->mycomm.unk4->mac, 6) != 0) {
                if (UnionMain_SendUnk46_2(2) == TRUE) {
                    GFL_NetSetDisconnectError(FALSE);
                    *seq = 6;
                }
            } else if (UnionMain_SendUnk46_2(1) == TRUE) {
                GFL_SndSEPlay(SEQ_SE_SYS_101);
                UnionOneself_SetNextStep(unisys, UNION_STEP_PARENT);
                UnionComm_SetSelf(unisys, 3, (uptr)situ->mycomm.unk4);
                UnionComm_SetSelf(unisys, 1, 0);
                *seq = 3;
            }
        }
        break;
    case 3:
        if (UnionMsg_IsPrintDone(unisys) == TRUE) {
            if (situ->mycomm.parent->beacon.version == VERSION_WHITE2 ||
                situ->mycomm.parent->beacon.version == VERSION_BLACK2) {
                *seq = 8;
            } else {
                return TRUE;
            }
        }
        break;
    case 4:
        if (UnionMsg_IsPrintDone(unisys) == TRUE && (GCTX_HIDGetPressedKeys() & UNION_PAD_ANY)) {
            return TRUE;
        }
        break;
    case 5:
        if (!UnionComm_IsLinkRequested(unisys)) {
            *seq = 4;
        }
        break;
    case 6:
        if (NetSystem_CountConnected() <= 1) {
            UnionComm_RequestLink0(unisys);
            (*seq)++;
        }
        break;
    case 7:
        if (!UnionComm_IsLinkRequested(unisys)) {
            situ->mycomm.unk10[6] = 0;
            *seq = 0;
        }
        break;
    case 8:
        if (UnionMain_SendJoinAvenue(unisys)) {
            *seq = 9;
        }
        break;
    case 9:
        if (situ->mycomm.unk4C != 0) {
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static BOOL UnionOneself_StepAskEnd(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    UnionComm_SetSelf(unisys, 1, 0);
    return TRUE;
}

static BOOL UnionOneself_StepAskedBegin(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    GFL_SndSEPlay(SEQ_SE_SYS_101);
    UnionMsg_CreateAndPrint(unisys, field, 7);
    UnionOneself_SetUpdateDisabled(unisys, field, TRUE);
    return TRUE;
}

static BOOL UnionOneself_StepAskedRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    if (UnionOneself_CheckNetError(unisys) == TRUE) {
        return TRUE;
    }
    switch (*seq) {
    case 0:
        GFL_ASSERT(situ->mycomm.answer_pc != NULL);
        GFL_NetSetChangeoverMode(2, 1, situ->mycomm.answer_pc->mac);
        situ->wait = 0;
        *seq = 1;
        break;
    case 1:
        if (GFL_NetGetNegotiatedCount() > 1) {
            GFL_NetSetDisconnectError(TRUE);
            *seq = 3;
        } else {
            situ->wait++;
            if (situ->wait > 600 || (GCTX_HIDGetPressedKeys() & PAD_BUTTON_B)) {
                UnionComm_RequestLink0(unisys);
                UnionOneself_SetNextStep(unisys, UNION_STEP_IDLE);
                UnionComm_SetSelf(unisys, 2, 0);
                if (situ->wait > 600) {
                    UnionMsg_CreateAndPrint(unisys, field, 8);
                } else {
                    UnionMsg_CreateAndPrint(unisys, field, 0xa);
                }
                *seq = 2;
            }
        }
        break;
    case 2:
        if (!UnionComm_IsLinkRequested(unisys) && UnionMsg_IsPrintDone(unisys) == TRUE &&
            (GCTX_HIDGetPressedKeys() & UNION_PAD_ANY)) {
            return TRUE;
        }
        break;
    case 3:
        if (UnionMain_SendMac() == TRUE) {
            if (situ->mycomm.answer_pc->beacon.version == VERSION_WHITE2 ||
                situ->mycomm.answer_pc->beacon.version == VERSION_BLACK2) {
                *seq = 6;
            } else {
                *seq = 4;
            }
        }
        break;
    case 4:
        switch (situ->mycomm.unk46_2) {
        case 1:
            UnionComm_SetSelf(unisys, 3, (uptr)situ->mycomm.answer_pc);
            UnionComm_SetSelf(unisys, 2, 0);
            UnionOneself_SetNextStep(unisys, UNION_STEP_OFFER_PARTY);
            return TRUE;
        case 2:
            UnionComm_SetSelf(unisys, 2, 0);
            UnionMsg_CreateAndPrint(unisys, field, 8);
            UnionComm_RequestLink0(unisys);
            *seq = 5;
            break;
        }
        break;
    case 5:
        if (!UnionComm_IsLinkRequested(unisys) && UnionMsg_IsPrintDone(unisys) == TRUE &&
            (GCTX_HIDGetPressedKeys() & UNION_PAD_ANY)) {
            UnionOneself_SetNextStep(unisys, UNION_STEP_IDLE);
            return TRUE;
        }
        break;
    case 6:
        if (UnionMain_SendJoinAvenue(unisys)) {
            *seq = 7;
        }
        break;
    case 7:
        if (situ->mycomm.unk4C != 0) {
            *seq = 4;
        }
        break;
    }
    return FALSE;
}

static BOOL UnionOneself_StepAskedEnd(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    UnionComm_SetSelf(unisys, 2, 0);
    return TRUE;
}

static BOOL UnionOneself_StepParentBegin(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    if (UnionOneself_CheckNetError(unisys) == TRUE) {
        return TRUE;
    }
    if (situ->prevStep == UNION_STEP_RULE_SELECT) {
        situ->result = 1;
        return TRUE;
    }
    situ->result = 0;
    switch (*seq) {
    case 0:
        UnionGroup_ResetTargets(&situ->mycomm);
        UnionComm_SetSelf(unisys, 4, 1);
        UnionGroup_AddBeaconMember(&situ->mycomm, situ->mycomm.parent);
        NetHandle_SendTimingPair(NetHandle_GetOwn(), 2, 0x14);
        (*seq)++;
        break;
    case 1:
        if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 2, 0x14) == TRUE) {
            (*seq)++;
        }
        break;
    case 2:
        if (UnionMain_SendPlayerInfo(unisys) == TRUE) {
            (*seq)++;
        }
        break;
    case 3:
        if (MATH_CountPopulation(situ->mycomm.unk47) > 1) {
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static BOOL UnionOneself_StepParentRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    PokeParty *party;
    int choice;

    if (UnionOneself_CheckNetError(unisys) == TRUE) {
        return TRUE;
    }
    switch (*seq) {
    case 0:
        if (situ->result == 1) {
            situ->result = 0;
        } else if (situ->mycomm.unk48 == 0) {
            UnionMsg_CreateAndPrint(unisys, field,
                                    UnionMsg_GetContactGreetingMessage(situ->mycomm.target->beacon.gender));
            situ->mycomm.unk48++;
        } else {
            UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetWhatToDoMessage(situ->mycomm.target->beacon.gender));
        }
        (*seq)++;
        break;
    case 1:
        if (UnionMsg_IsPrintDone(unisys) == TRUE) {
            (*seq)++;
        }
        break;
    case 2:
        UnionMsg_CreateMainMenu(unisys, field);
        (*seq)++;
        break;
    case 3:
        party = GameData_GetParty(unisys->param->gameData);
        choice = UnionMsg_UpdateMainMenu(unisys, field);
        if (choice == -1) {
            break;
        }
        UnionMsg_FreeMainMenu(unisys);
        if (choice == -2 || choice == 0) {
            UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetJustAskMessage(situ->mycomm.target->beacon.gender));
            situ->mycomm.unk19 = 0;
            UnionOneself_SetNextStep(unisys, UNION_STEP_OFFER);
            return TRUE;
        }
        if (choice == UNION_ACTIVITY_TRADE) {
            GSYS_GetGameData(unisys->param->gsys);
            if (howManyPartyPokesAreNotEggs(party) < 2) {
                UnionMsg_CreateAndPrint(unisys, field,
                                        UnionMsg_GetOnePokemonCannotTradeMessage(situ->mycomm.target->beacon.gender));
                *seq = 4;
                break;
            }
        } else if (choice == UNION_ACTIVITY_EGG_TRADE) {
            if (countSanityEggsInParty(party) > 0) {
                UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetEggCannotTradeMessage(unisys));
                *seq = 4;
                break;
            } else if (countAllEggsInParty(party) == 0) {
                UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetNeedEggMessage(situ->mycomm.target->beacon.gender));
                *seq = 4;
                break;
            }
        }
        if (choice == UNION_ACTIVITY_NONE) {
            UnionOneself_SetNextStep(unisys, UNION_STEP_RULE_SELECT);
            return TRUE;
        }
        situ->mycomm.unk19 = choice;
        UnionOneself_SetNextStep(unisys, UNION_STEP_OFFER);
        return TRUE;
    case 4:
        if (UnionMsg_IsPrintDone(unisys) == TRUE) {
            *seq = 0;
        }
        break;
    }
    return FALSE;
}

static BOOL UnionOneself_StepParentEnd(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    UnionMsg_FreeMainMenu(unisys);
    return TRUE;
}

static BOOL UnionOneself_StepOfferRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    if (UnionOneself_CheckNetError(unisys) == TRUE) {
        return TRUE;
    }
    switch (*seq) {
    case 0:
        if (UnionMain_SendUnk19(situ->mycomm.unk19) == TRUE) {
            if (situ->mycomm.unk19 == 0) {
                UnionOneself_SetNextStep(unisys, UNION_STEP_LEAVE);
                return TRUE;
            }
            UnionMsg_CreateAndPrint(
                unisys, field, UnionMsg_GetShowCardMessage(situ->mycomm.unk19, situ->mycomm.target->beacon.gender));
            (*seq)++;
        }
        break;
    case 1:
        if (UnionMsg_IsPrintDone(unisys)) {
            if (situ->mycomm.unk1A == 1) {
                UnionOneself_SetNextStep(unisys, UNION_STEP_FOR_ACTIVITY(situ->mycomm.unk19));
                (*seq)++;
            } else if (situ->mycomm.unk1A == 0) {
                UnionOneself_SetNextStep(unisys, UNION_STEP_LEAVE);
                UnionMsg_CreateAndPrint(
                    unisys, field,
                    UnionMsg_GetCardMissingMessage(situ->mycomm.unk19, situ->mycomm.target->beacon.gender));
                return TRUE;
            }
        }
        break;
    case 2:
        if (UnionMsg_IsPrintDone(unisys) == TRUE) {
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static BOOL UnionOneself_StepOfferBegin(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    if (UnionOneself_CheckNetError(unisys) == TRUE) {
        return TRUE;
    }
    switch (*seq) {
    case 0:
        UnionGroup_ResetTargets(&situ->mycomm);
        UnionComm_SetSelf(unisys, 4, 1);
        UnionGroup_AddBeaconMember(&situ->mycomm, situ->mycomm.parent);
        NetHandle_SendTimingPair(NetHandle_GetOwn(), 2, 0x14);
        (*seq)++;
        break;
    case 1:
        if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 2, 0x14) == TRUE) {
            (*seq)++;
        }
        break;
    case 2:
        if (UnionMain_SendPlayerInfo(unisys) == TRUE) {
            (*seq)++;
        }
        break;
    case 3:
        if (MATH_CountPopulation(situ->mycomm.unk47) > 1) {
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static BOOL UnionOneself_StepOfferParty(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    BOOL yes;
    PokeParty *party;

    if (UnionOneself_CheckNetError(unisys) == TRUE) {
        return TRUE;
    }
    if (!UnionMsg_IsPrintDone(unisys)) {
        return FALSE;
    }
    switch (*seq) {
    case 0:
        UnionMsg_CreateAndPrint(unisys, field, 9);
        (*seq)++;
        break;
    case 1:
        if (situ->mycomm.unk19 == 0xff) {
            break;
        }
        if (situ->mycomm.unk19 == 0) {
            UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetBusyMessage(situ->mycomm.target->beacon.gender));
            UnionOneself_SetNextStep(unisys, UNION_STEP_LEAVE);
            return TRUE;
        }
        if (situ->mycomm.unk19 < UNION_ACTIVITY_NONE) {
            UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetOfferMessage(situ->mycomm.unk19));
            if (UnionOneself_IsBattleActivity(situ->mycomm.unk19) == TRUE) {
                UnionOneself_SetRegulation(unisys, situ->mycomm.unk19);
                UnionMsg_CreateRulesWindow(unisys, field, 0, UnionOneself_IsDoubleActivity(situ->mycomm.unk19), 0);
                *seq = 2;
            } else {
                *seq = 3;
            }
        } else if (UnionMain_SendUnk1A(0) == TRUE) {
            UnionMsg_CreateAndPrint(unisys, field, 0x13d);
            UnionOneself_SetNextStep(unisys, UNION_STEP_LEAVE);
            return TRUE;
        }
        break;
    case 2:
        if (UnionMsg_IsRulesPrinted(unisys, field) == TRUE) {
            *seq = 3;
        }
        break;
    case 3:
        UnionMsg_FreeRulesWindow(unisys);
        UnionMsg_CreateYesNo(unisys, field);
        (*seq)++;
        break;
    case 4:
        situ->result = 0;
        if (UnionMsg_UpdateYesNo(unisys, &yes) == TRUE) {
            party = GameData_GetParty(unisys->param->gameData);
            UnionMsg_FreeYesNo(unisys);
            if (yes == 1) {
                if (!UnionOneself_CheckRegulation(unisys, situ->mycomm.unk19, &situ->partyCheck, &situ->boxCheck)) {
                    UnionMsg_CreateAndPrint(unisys, field, 0x13e);
                    yes = 0;
                } else if (situ->mycomm.unk19 == UNION_ACTIVITY_TRADE && yes == 1) {
                    GSYS_GetGameData(unisys->param->gsys);
                    if (howManyPartyPokesAreNotEggs(party) < 2) {
                        yes = 0;
                        situ->result = 1;
                    }
                } else if (situ->mycomm.unk19 == UNION_ACTIVITY_EGG_TRADE && yes == 1) {
                    if (countSanityEggsInParty(party) > 0) {
                        yes = 0;
                        situ->result = 3;
                    } else if (countAllEggsInParty(party) == 0) {
                        yes = 0;
                        situ->result = 2;
                    }
                }
            } else {
                situ->partyCheck = 0;
                situ->boxCheck = 0;
            }
            situ->mycomm.unk1A = yes;
            (*seq)++;
        }
        break;
    case 5:
        if (UnionMain_SendUnk1A(situ->mycomm.unk1A) == TRUE) {
            if (situ->partyCheck != 0 && situ->boxCheck != 0) {
                *seq = 6;
            } else {
                *seq = 10;
            }
        }
        break;
    case 6:
        UnionMsg_CreateRulesWindow(unisys, field, situ->partyCheck, UnionOneself_IsDoubleActivity(situ->mycomm.unk19),
                                   1);
        (*seq)++;
        break;
    case 7:
        if (UnionMsg_IsRulesPrinted(unisys, field) == TRUE &&
            (GCTX_HIDGetPressedKeys() & (PAD_BUTTON_A | PAD_BUTTON_B))) {
            UnionMsg_FreeRulesWindow(unisys);
            if (situ->boxCheck == -1) {
                *seq = 10;
            } else {
                (*seq)++;
            }
        }
        break;
    case 8:
        UnionMsg_CreateRulesWindow(unisys, field, situ->boxCheck, UnionOneself_IsDoubleActivity(situ->mycomm.unk19), 2);
        (*seq)++;
        break;
    case 9:
        if (UnionMsg_IsRulesPrinted(unisys, field) == TRUE &&
            (GCTX_HIDGetPressedKeys() & (PAD_BUTTON_A | PAD_BUTTON_B))) {
            UnionMsg_FreeRulesWindow(unisys);
            *seq = 10;
        }
        break;
    case 10:
        if (situ->mycomm.unk1A == 0) {
            if (situ->result == 1) {
                UnionMsg_CreateAndPrint(unisys, field, 0x28);
            } else if (situ->result == 2) {
                UnionMsg_CreateAndPrint(unisys, field, 0x2f);
            } else if (situ->result == 3) {
                UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetEggCannotTradeResultMessage(unisys));
            } else {
                UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetDeclinedMessage(situ->mycomm.unk19));
            }
            UnionOneself_SetNextStep(unisys, UNION_STEP_LEAVE);
            return TRUE;
        }
        UnionOneself_SetNextStep(unisys, UNION_STEP_FOR_ACTIVITY(situ->mycomm.unk19));
        *seq = 11;
        break;
    case 11:
        return TRUE;
    }
    return FALSE;
}

static BOOL UnionOneself_StepOfferPartyEnd(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    return TRUE;
}

static BOOL UnionOneself_StepRuleSelectRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    BOOL enter;
    BOOL fixed;
    int choice;

    if (UnionOneself_CheckNetError(unisys) == TRUE) {
        return TRUE;
    }
    if (!UnionMsg_IsPrintDone(unisys)) {
        return FALSE;
    }
    switch (*seq) {
    case 0:
        situ->result = 0;
        (*seq)++;
        break;
    case 1:
        if (situ->result == 5) {
            UnionMsg_CreateInfoWindow(unisys, field);
        } else if (situ->result == 0) {
            UnionMsg_FreeInfoWindow(unisys);
        }
        UnionMsg_CreateRuleMenu(unisys, field, situ->result, situ->ruleSel);
        (*seq)++;
        break;
    case 2:
        choice = UnionMsg_UpdateRuleMenu(unisys, &enter, situ->ruleSel, &fixed, field);
        if (enter == 1) {
            situ->result = choice;
            UnionMsg_FreeRuleMenu(unisys);
            (*seq)--;
        } else if (choice == -2 || choice == 0) {
            UnionMsg_FreeRuleMenu(unisys);
            if (situ->result > 0) {
                situ->result = 0;
                (*seq)--;
            } else {
                UnionMsg_FreeInfoWindow(unisys);
                UnionOneself_SetNextStep(unisys, UNION_STEP_PARENT);
                return TRUE;
            }
        } else if (fixed == 1) {
            UnionMsg_FreeInfoWindow(unisys);
            UnionMsg_FreeRuleMenu(unisys);
            UnionOneself_SetRegulation(unisys, choice);
            situ->mycomm.unk19 = choice;
            *seq = 3;
        } else if (choice != -1) {
            UnionMsg_FreeInfoWindow(unisys);
            UnionMsg_FreeRuleMenu(unisys);
            if (!UnionOneself_CheckRegulation(unisys, choice, &situ->partyCheck, &situ->boxCheck)) {
                UnionMsg_CreateAndPrint(unisys, field, 0x13e);
                *seq = 5;
            } else {
                situ->mycomm.unk19 = choice;
                UnionOneself_SetNextStep(unisys, UNION_STEP_OFFER);
                return TRUE;
            }
        }
        break;
    case 3:
        UnionMsg_CreateRulesWindow(unisys, field, 0, UnionOneself_IsDoubleActivity(situ->mycomm.unk19), 0);
        (*seq)++;
        break;
    case 4:
        if (UnionMsg_IsRulesPrinted(unisys, field) == TRUE &&
            (GCTX_HIDGetPressedKeys() & (PAD_BUTTON_A | PAD_BUTTON_B))) {
            UnionMsg_FreeRulesWindow(unisys);
            *seq = 1;
        }
        break;
    case 5:
        UnionMsg_CreateRulesWindow(unisys, field, situ->partyCheck, UnionOneself_IsDoubleActivity(situ->mycomm.unk19),
                                   1);
        (*seq)++;
        break;
    case 6:
        if (UnionMsg_IsRulesPrinted(unisys, field) == TRUE &&
            (GCTX_HIDGetPressedKeys() & (PAD_BUTTON_A | PAD_BUTTON_B))) {
            UnionMsg_FreeRulesWindow(unisys);
            if (situ->boxCheck == -1) {
                *seq = 9;
            } else {
                (*seq)++;
            }
        }
        break;
    case 7:
        UnionMsg_CreateRulesWindow(unisys, field, situ->boxCheck, UnionOneself_IsDoubleActivity(situ->mycomm.unk19), 2);
        (*seq)++;
        break;
    case 8:
        if (UnionMsg_IsRulesPrinted(unisys, field) == TRUE &&
            (GCTX_HIDGetPressedKeys() & (PAD_BUTTON_A | PAD_BUTTON_B))) {
            UnionMsg_FreeRulesWindow(unisys);
            *seq = 9;
        }
        break;
    case 9:
        UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetWhatToDoMessage(situ->mycomm.target->beacon.gender));
        *seq = 0;
        break;
    }
    return FALSE;
}

static BOOL UnionOneself_StepAnswerRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    u16 index = UnionOneself_GetEntryIndex(situ->mycomm.unk44);
    u32 activity = unisys->found[index].beacon.activity;

    switch (*seq) {
    case 0:
        situ->mycomm.unk19 = activity;
        switch (activity) {
        case 1:
            UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetBusyTrainerMessage(situ->mycomm.target->beacon.gender));
            UnionOneself_SetNextStep(unisys, UNION_STEP_IDLE);
            *seq = 2;
            break;
        case 2:
            UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetCardChatMessage(situ->mycomm.target->beacon.gender));
            UnionOneself_SetNextStep(unisys, UNION_STEP_IDLE);
            *seq = 2;
            break;
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
            UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetBattleChatMessage(situ->mycomm.target->beacon.gender));
            UnionOneself_SetNextStep(unisys, UNION_STEP_IDLE);
            *seq = 2;
            break;
        case 20:
        case 21:
        case 22:
        case 23:
            if (unisys->found[index].beacon.memberCount < UnionMsg_GetTableByte(activity)) {
                UnionMsg_CreateAndPrint(unisys, field,
                                        UnionMsg_GetJoinMultiBattleMessage(situ->mycomm.target->beacon.gender));
                UnionOneself_SetRegulation(unisys, activity);
                UnionMsg_CreateRulesWindow(unisys, field, 0, UnionOneself_IsDoubleActivity(activity), 0);
                *seq = 3;
            } else {
                UnionMsg_CreateAndPrint(unisys, field,
                                        UnionMsg_GetBattleChatMessage(situ->mycomm.target->beacon.gender));
                UnionOneself_SetNextStep(unisys, UNION_STEP_IDLE);
                *seq = 2;
            }
            break;
        case UNION_ACTIVITY_EGG_TRADE:
            if (unisys->found[index].beacon.memberCount < UnionMsg_GetTableByte(activity) &&
                !unisys->found[index].beacon.entryClosed) {
                UnionMsg_CreateAndPrint(unisys, field,
                                        UnionMsg_GetJoinSpinTradeMessage(situ->mycomm.target->beacon.gender));
                *seq = 10;
            } else {
                UnionMsg_CreateAndPrint(unisys, field,
                                        UnionMsg_GetSpinTradeChatMessage(situ->mycomm.target->beacon.gender));
                UnionOneself_SetNextStep(unisys, UNION_STEP_IDLE);
                *seq = 2;
            }
            break;
        case 3:
            if (unisys->found[index].beacon.memberCount < UnionMsg_GetTableByte(activity)) {
                UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetJoinDrawMessage(situ->mycomm.target->beacon.gender));
                *seq = 10;
            } else {
                UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetDrawChatMessage(situ->mycomm.target->beacon.gender));
                UnionOneself_SetNextStep(unisys, UNION_STEP_IDLE);
                *seq = 2;
            }
            break;
        case UNION_ACTIVITY_TRADE:
            UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetTradeChatMessage(situ->mycomm.target->beacon.gender));
            UnionOneself_SetNextStep(unisys, UNION_STEP_IDLE);
            *seq = 2;
            break;
        default:
            UnionMsg_CreateAndPrint(unisys, field, 0x151);
            UnionOneself_SetNextStep(unisys, UNION_STEP_IDLE);
            *seq = 2;
            break;
        }
        break;
    case 2:
        if (UnionMsg_IsPrintDone(unisys) == TRUE && (GCTX_HIDGetPressedKeys() & UNION_PAD_ANY)) {
            return TRUE;
        }
        break;
    case 1:
        if (UnionMsg_IsPrintDone(unisys) == TRUE) {
            return TRUE;
        }
        break;
    case 3:
        if (UnionMsg_IsRulesPrinted(unisys, field) == TRUE) {
            *seq = 4;
        }
        break;
    case 4:
        if (UnionMsg_IsPrintDone(unisys) == TRUE) {
            UnionMsg_FreeRulesWindow(unisys);
            UnionMsg_CreateYesNo(unisys, field);
            (*seq)++;
        }
        break;
    case 5: {
        BOOL yes;

        if (UnionMsg_UpdateYesNo(unisys, &yes) == TRUE) {
            UnionMsg_FreeYesNo(unisys);
            if (yes == 0) {
                UnionMsg_CreateAndPrint(unisys, field,
                                        UnionMsg_GetBattleDeclineMessage(situ->mycomm.target->beacon.gender));
                UnionOneself_SetNextStep(unisys, UNION_STEP_IDLE);
                *seq = 2;
            } else if (!UnionOneself_CheckRegulation(unisys, situ->mycomm.unk19, &situ->partyCheck, &situ->boxCheck)) {
                UnionMsg_CreateAndPrint(unisys, field, 0x13e);
                UnionOneself_SetNextStep(unisys, UNION_STEP_IDLE);
                *seq = 6;
            } else {
                situ->mycomm.unk1A = yes;
                UnionOneself_SetNextStep(unisys, UNION_STEP_RECRUIT);
                UnionMsg_CreateAndPrint(unisys, field,
                                        UnionMsg_GetBattleAcceptMessage(situ->mycomm.target->beacon.gender));
                *seq = 1;
            }
        }
        break;
    }
    case 6:
        if (UnionMsg_IsPrintDone(unisys) == TRUE) {
            return TRUE;
        }
        UnionMsg_CreateRulesWindow(unisys, field, situ->partyCheck, UnionOneself_IsDoubleActivity(situ->mycomm.unk19),
                                   1);
        (*seq)++;
        break;
    case 7:
        if (UnionMsg_IsRulesPrinted(unisys, field) == TRUE &&
            (GCTX_HIDGetPressedKeys() & (PAD_BUTTON_A | PAD_BUTTON_B))) {
            UnionMsg_FreeRulesWindow(unisys);
            if (situ->boxCheck == -1) {
                *seq = 1;
            } else {
                (*seq)++;
            }
        }
        break;
    case 8:
        UnionMsg_CreateRulesWindow(unisys, field, situ->boxCheck, UnionOneself_IsDoubleActivity(situ->mycomm.unk19), 2);
        (*seq)++;
        break;
    case 9:
        if (UnionMsg_IsRulesPrinted(unisys, field) == TRUE &&
            (GCTX_HIDGetPressedKeys() & (PAD_BUTTON_A | PAD_BUTTON_B))) {
            UnionMsg_FreeRulesWindow(unisys);
            *seq = 1;
        }
        break;
    case 10:
        if (UnionMsg_IsPrintDone(unisys) == TRUE) {
            UnionMsg_CreateYesNo(unisys, field);
            (*seq)++;
        }
        break;
    case 11: {
        BOOL yes;

        if (UnionMsg_UpdateYesNo(unisys, &yes) == TRUE) {
            UnionMsg_FreeYesNo(unisys);
            if (yes == 0) {
                switch (situ->mycomm.unk19) {
                case 3:
                    UnionMsg_CreateAndPrint(unisys, field,
                                            UnionMsg_GetDrawDeclineMessage(situ->mycomm.target->beacon.gender));
                    break;
                case UNION_ACTIVITY_EGG_TRADE:
                    UnionMsg_CreateAndPrint(unisys, field,
                                            UnionMsg_GetEggTradeDeclineMessage(situ->mycomm.target->beacon.gender));
                    break;
                }
                UnionOneself_SetNextStep(unisys, UNION_STEP_IDLE);
                *seq = 2;
            } else {
                situ->mycomm.unk1A = yes;
                UnionOneself_SetNextStep(unisys, UNION_STEP_RECRUIT);
                *seq = 1;
                switch (situ->mycomm.unk19) {
                case 3:
                    UnionMsg_CreateAndPrint(unisys, field,
                                            UnionMsg_GetDrawAcceptMessage(situ->mycomm.target->beacon.gender));
                    break;
                case UNION_ACTIVITY_EGG_TRADE: {
                    PokeParty *party = GameData_GetParty(unisys->param->gameData);

                    if (countSanityEggsInParty(party) > 0) {
                        UnionMsg_CreateAndPrint(
                            unisys, field, UnionMsg_GetTargetEggCannotTradeMessage(situ->mycomm.target->beacon.gender));
                        UnionOneself_SetNextStep(unisys, UNION_STEP_IDLE);
                        *seq = 2;
                    } else if (countAllEggsInParty(party) == 0) {
                        UnionMsg_CreateAndPrint(unisys, field,
                                                UnionMsg_GetNoEggMessage(situ->mycomm.target->beacon.gender));
                        UnionOneself_SetNextStep(unisys, UNION_STEP_IDLE);
                        *seq = 2;
                    } else {
                        UnionMsg_CreateAndPrint(unisys, field,
                                                UnionMsg_GetEggTradeAcceptMessage(situ->mycomm.target->beacon.gender));
                    }
                    break;
                }
                }
            }
        }
        break;
    }
    }
    return FALSE;
}

static BOOL UnionOneself_StepAnswerMember(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    u8 gender;
    u8 activity;

    switch (*seq) {
    case 0:
        gender = UnionOneself_GetActorGender(unisys, situ->mycomm.unk44);
        activity = unisys->found[UnionOneself_GetEntryIndex(situ->mycomm.unk44)].beacon.activity;
        switch (activity) {
        case 1:
            UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetBusyTrainerMessage(gender));
            break;
        case 2:
            UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetCardChatMessage(gender));
            break;
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
            UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetBattleChatMessage(gender));
            break;
        case UNION_ACTIVITY_EGG_TRADE:
            UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetSpinTradeChatMessage(gender));
            break;
        case 3:
            UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetDrawChatMessage(gender));
            break;
        case UNION_ACTIVITY_TRADE:
            UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetTradeChatMessage(gender));
            break;
        default:
            UnionMsg_CreateAndPrint(unisys, field, 0x151);
            break;
        }
        UnionOneself_SetNextStep(unisys, UNION_STEP_IDLE);
        (*seq)++;
        break;
    case 1:
        if (UnionMsg_IsPrintDone(unisys) == TRUE && (GCTX_HIDGetPressedKeys() & UNION_PAD_ANY)) {
            return TRUE;
        }
        break;
    }
    return FALSE;
}

// The version of the game of the players in the communication, for the card to show: 21 when a player has the first
// pair's
static u32 UnionOneself_GetCardVersion(UnionSystem *unisys, BOOL remote) {
    GameData *gameData = GSYS_GetGameData(unisys->param->gsys);
    int i;

    for (i = 0; i < GFL_NetGetNegotiatedCount(); i++) {
        PlayerInfo *info;

        if (remote) {
            info = &unisys->colosseum_sys->members[i].info;
        } else {
            info = GameData_GetPlayerInfoByNetID(gameData, i);
        }
        u32 version = PlayerInfo_GetVersion(info);

        if (version == VERSION_WHITE || version == VERSION_BLACK) {
            return 0x15;
        }
    }
    return 0x16;
}

static BOOL UnionOneself_StepCardRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    if (UnionOneself_CheckNetError(unisys) == TRUE) {
        if (*seq >= 5 && *seq <= 7 && unisys->alloc.cardParam != NULL) {
            GFL_HeapFree(unisys->alloc.cardParam);
            unisys->alloc.cardParam = NULL;
        }
        return TRUE;
    }
    switch (*seq) {
    case 0:
        GFL_ASSERT(unisys->alloc.my_card == NULL && unisys->alloc.target_card == NULL);
        unisys->alloc.my_card = GFL_HeapAllocate(HEAPID_UNION, sizeof(TrainerCardData), TRUE, "union_oneself.c", 2594);
        unisys->alloc.target_card =
            GFL_HeapAllocate(HEAPID_UNION, sizeof(TrainerCardData), TRUE, "union_oneself.c", 2596);
        TrainerCardData_Fill(unisys->alloc.my_card, unisys->param->gameData, TRUE, FALSE, HEAPID_UNION);
        TrainerCardData_FillExtras(unisys->alloc.my_card, unisys->param->gameData, HEAPID_UNION);
        MI_CpuCopy8(unisys->alloc.my_card, unisys->alloc.target_card, 0x678);
        NetHandle_SendTimingPair(NetHandle_GetOwn(), 3, 0x14);
        UnionMsg_CreateAndPrint(unisys, field,
                                UnionMsg_GetOriginMessage(unisys, situ->mycomm.target->beacon.country,
                                                          situ->mycomm.target->beacon.province));
        (*seq)++;
        break;
    case 1:
        if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 3, 0x14) == TRUE) {
            if (UnionOneself_GetCardVersion(unisys, FALSE) == 0x16) {
                *seq = 2;
            } else {
                *seq = 3;
            }
        }
        break;
    case 2:
        if (UnionMain_SendCard(unisys) == TRUE) {
            *seq = 4;
        }
        break;
    case 3:
        if (UnionMain_SendCardHead(unisys) == TRUE) {
            *seq = 4;
        }
        break;
    case 4:
        if (situ->mycomm.recvFlag == 1 && UnionMsg_IsPrintDone(unisys) == TRUE) {
            unisys->alloc.target_card->unk04_5 = 1;
            unisys->alloc.cardParam =
                TrainerCardParam_CreateView(unisys->param->gameData, unisys->alloc.target_card, HEAPID_UNION);
            (*seq)++;
        }
        break;
    case 5:
        NetHandle_SendTimingPair(NetHandle_GetOwn(), 4, 0x14);
        (*seq)++;
        break;
    case 6:
        if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 4, 0x14) == TRUE) {
            (*seq)++;
        }
        break;
    case 7:
        UnionCommand_Set(unisys, 1, unisys->alloc.cardParam);
        (*seq)++;
        break;
    case 8:
        if (!UnionCommand_IsActive(unisys)) {
            (*seq)++;
        }
        break;
    case 9:
        UnionMsg_CreateAndPrint(unisys, field, 0x10);
        NetHandle_SendTimingPair(NetHandle_GetOwn(), 5, 0x14);
        (*seq)++;
        break;
    case 10:
        if (UnionMsg_IsPrintDone(unisys) && NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 5, 0x14) == TRUE) {
            GFL_HeapFree(unisys->alloc.my_card);
            GFL_HeapFree(unisys->alloc.target_card);
            unisys->alloc.cardParam = NULL;
            unisys->alloc.my_card = NULL;
            unisys->alloc.target_card = NULL;
            if (GFL_NetIsParent() == TRUE) {
                UnionOneself_SetNextStep(unisys, UNION_STEP_PARENT);
            } else {
                UnionOneself_SetNextStep(unisys, UNION_STEP_OFFER_PARTY);
            }
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static BOOL UnionOneself_StepTradeRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    if (*seq != 4 && UnionOneself_CheckNetError(unisys) == TRUE) {
        return TRUE;
    }
    switch (*seq) {
    case 0:
        UnionMsg_CreateAndPrint(unisys, field, 0x26);
        (*seq)++;
        break;
    case 1:
        if (UnionMsg_IsPrintDone(unisys) == TRUE) {
            NetHandle_SendTimingPair(NetHandle_GetOwn(), 6, 0x14);
            (*seq)++;
        }
        break;
    case 2:
        if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 6, 0x14) == TRUE) {
            GameData *gameData = GSYS_GetGameData(unisys->param->gsys);
            RivalDataSave *rival = getHollow_RivalData(GameData_GetSaveControl(gameData));

            Misc_AddMetTrainer(rival, getIDAsUInt(GameData_GetPlayerInfoByNetID(
                                          gameData, GFL_NetGetHandleNetId(NetHandle_GetOwn()) ^ 1)));
            (*seq)++;
        }
        break;
    case 3: {
        PokemonTradeParam *param =
            GFL_HeapAllocate(HEAPID_UNION, sizeof(PokemonTradeParam), TRUE, "union_oneself.c", 2766);
        ShinkaDemoParam *evolution;

        param->gameData = GSYS_GetGameData(unisys->param->gsys);
        param->party = PokeParty_Create(HEAPID_UNION);
        evolution = GFL_HeapAllocate(HEAPID_GAMEEVENT, sizeof(ShinkaDemoParam), FALSE, "union_oneself.c", 2773);
        evolution->gameData = param->gameData;
        evolution->party = param->party;
        evolution->species = 0;
        evolution->partyIndex = 0;
        evolution->method = 0;
        evolution->unkC = 1;
        evolution->canCancel = FALSE;
        param->evolution = evolution;
        param->next = 0;
        unisys->cmdArg = param;
        UnionCommand_Set(unisys, 2, param);
        (*seq)++;
        break;
    }
    case 4:
        if (!UnionCommand_IsActive(unisys)) {
            PokemonTradeParam *param = unisys->cmdArg;

            GFL_HeapFree(param->evolution);
            GFL_HeapFree(param->party);
            GFL_HeapFree(unisys->cmdArg);
            unisys->cmdArg = NULL;
            (*seq)++;
        }
        break;
    case 5:
        UnionMsg_CreateAndPrint(unisys, field, 0x10);
        NetHandle_SendTimingPair(NetHandle_GetOwn(), 7, 0x14);
        (*seq)++;
        break;
    case 6:
        if (UnionMsg_IsPrintDone(unisys) && NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 7, 0x14) == TRUE) {
            if (GFL_NetIsParent() == TRUE) {
                UnionOneself_SetNextStep(unisys, UNION_STEP_PARENT);
            } else {
                UnionOneself_SetNextStep(unisys, UNION_STEP_OFFER_PARTY);
            }
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static BOOL UnionOneself_StepRecruitRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    u16 entryIndex = UnionOneself_GetEntryIndex(situ->mycomm.unk44);
    int needed;

    if (UnionOneself_CheckNetError(unisys) == TRUE) {
        return TRUE;
    }
    switch (*seq) {
    case 0:
        situ->mycomm.unk46_0 = 1;
        UnionComm_SetCommandBase(situ->mycomm.unk19);
        GFL_NetSetChangeoverMode(2, 1, unisys->found[entryIndex].mac);
        situ->wait = 0;
        (*seq)++;
        break;
    case 1:
        if (UnionMsg_IsPrintDone(unisys)) {
            if (GFL_NetGetNegotiatedCount() > 1) {
                GFL_NetSetDisconnectError(TRUE);
                needed = UnionMsg_GetTableByte(situ->mycomm.unk19);
                if (needed <= GFL_NetGetHandleNetId(NetHandle_GetOwn())) {
                    *seq = 4;
                } else {
                    UnionComm_SetSelf(unisys, 3, (uptr)&unisys->found[entryIndex]);
                    situ->mycomm.unk46_1 = 1;
                    switch (situ->mycomm.unk19) {
                    case 20:
                        UnionOneself_SetNextStep(unisys, UNION_STEP_JOIN_MULTI_0);
                        break;
                    case 21:
                        UnionOneself_SetNextStep(unisys, UNION_STEP_JOIN_MULTI_1);
                        break;
                    case 22:
                        UnionOneself_SetNextStep(unisys, UNION_STEP_JOIN_MULTI_2);
                        break;
                    case 23:
                        UnionOneself_SetNextStep(unisys, UNION_STEP_JOIN_MULTI_3);
                        break;
                    case 3:
                        UnionOneself_SetNextStep(unisys, UNION_STEP_JOIN_DRAW);
                        break;
                    case UNION_ACTIVITY_EGG_TRADE:
                        UnionOneself_SetNextStep(unisys, UNION_STEP_JOIN_EGG);
                        break;
                    default:
                        GFL_ASSERT(0);
                        break;
                    }
                    *seq = 3;
                }
            } else {
                situ->wait++;
                if (situ->wait > 600 || (GCTX_HIDGetPressedKeys() & PAD_BUTTON_B)) {
                    situ->mycomm.unk46_0 = 0;
                    UnionOneself_SetNextStep(unisys, UNION_STEP_IDLE);
                    *seq = 4;
                }
            }
        }
        break;
    case 2:
        switch (situ->mycomm.unk19) {
        case 20:
        case 21:
        case 22:
        case 23:
            UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetBattleBusyMessage(situ->mycomm.target->beacon.gender));
            break;
        case 3:
            UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetDrawBusyMessage(situ->mycomm.target->beacon.gender));
            break;
        case UNION_ACTIVITY_EGG_TRADE:
            UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetEggTradeBusyMessage(situ->mycomm.target->beacon.gender));
            break;
        }
        *seq = 3;
        break;
    case 3:
        if (UnionMsg_IsPrintDone(unisys) == TRUE) {
            return TRUE;
        }
        break;
    case 4:
        UnionComm_RequestLink0(unisys);
        (*seq)++;
        break;
    case 5:
        if (!UnionComm_IsLinkRequested(unisys)) {
            *seq = 2;
        }
        break;
    }
    return FALSE;
}

static BOOL UnionOneself_StepLeaveRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    if (*seq < 3 && UnionOneself_CheckNetError(unisys) == TRUE) {
        return TRUE;
    }
    switch (*seq) {
    case 0:
        UnionComm_ActivateGroup(unisys);
        (*seq)++;
        break;
    case 1:
        if (!UnionComm_IsLinkRequested(unisys) && GFL_NetIsEnded() == TRUE) {
            if (situ->mycomm.parent != NULL) {
                UnionGroup_RemoveBeaconMember(&situ->mycomm, situ->mycomm.parent);
                UnionComm_SetSelf(unisys, 3, 0);
            }
            UnionComm_RequestLink2(unisys);
            (*seq)++;
        }
        break;
    case 2:
        if (!UnionComm_IsLinkRequested(unisys)) {
            (*seq)++;
        }
        break;
    case 3:
        if (UnionMsg_IsPrintDone(unisys) == TRUE && (GCTX_HIDGetPressedKeys() & UNION_PAD_ANY)) {
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static BOOL UnionOneself_StepJoinRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    if (*seq != 4 && *seq != 5 && *seq != 8 && UnionOneself_CheckNetError(unisys) == TRUE) {
        return TRUE;
    }
    switch (*seq) {
    case 0:
        if (situ->mycomm.unk46_1 == 1) {
            *seq = 6;
            break;
        }
        (*seq)++;
        break;
    case 1:
        NetHandle_SendTimingPair(NetHandle_GetOwn(), 8, 0x14);
        (*seq)++;
        break;
    case 2:
        if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 8, 0x14) == TRUE) {
            (*seq)++;
        }
        break;
    case 3:
        if (UnionMsg_IsPrintDone(unisys) == TRUE) {
            if (situ->step == UNION_STEP_JOIN_DRAW) {
                UnionDrawParam *param =
                    GFL_HeapAllocate(HEAPID_UNION, sizeof(UnionDrawParam), TRUE, "union_oneself.c", 3092);

                param->gameData = unisys->param->gameData;
                unisys->cmdArg = param;
                UnionCommand_Set(unisys, 3, param);
            } else {
                UnionBattleParam *param =
                    GFL_HeapAllocate(HEAPID_UNION, sizeof(UnionBattleParam), TRUE, "union_oneself.c", 3100);

                param->gameData = unisys->param->gameData;
                param->party = GameData_GetParty(unisys->param->gameData);
                param->save = GameData_GetSaveControl(unisys->param->gameData);
                param->records = GameData_GetRecords(unisys->param->gameData);
                unisys->cmdArg = param;
                UnionCommand_Set(unisys, 4, param);
            }
            (*seq)++;
        }
        break;
    case 4:
        if (!UnionCommand_IsActive(unisys)) {
            GFL_HeapFree(unisys->cmdArg);
            unisys->cmdArg = NULL;
            if (GFL_NetErrCheck()) {
                UnionOneself_SetNextStep(unisys, UNION_STEP_RESET);
                return TRUE;
            }
            UnionComm_RequestLink2(unisys);
            (*seq)++;
        }
        break;
    case 5:
        if (!UnionComm_IsLinkRequested(unisys)) {
            return TRUE;
        }
        break;
    case 6:
        if (UnionMain_SendEntryRequest(unisys) == TRUE) {
            (*seq)++;
        }
        break;
    case 7:
        switch (unisys->entryReply) {
        case 1:
            if (situ->step == UNION_STEP_JOIN_DRAW) {
                UnionMsg_CreateAndPrint(unisys, field,
                                        UnionMsg_GetDrawStartMessage(situ->mycomm.target->beacon.gender));
            } else {
                UnionMsg_CreateAndPrint(unisys, field,
                                        UnionMsg_GetSpinTradeStartMessage(situ->mycomm.target->beacon.gender));
            }
            *seq = 3;
            break;
        case 2:
            if (situ->step == UNION_STEP_JOIN_DRAW) {
                UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetDrawBusyMessage(situ->mycomm.target->beacon.gender));
            } else {
                UnionMsg_CreateAndPrint(unisys, field,
                                        UnionMsg_GetEggTradeBusyMessage(situ->mycomm.target->beacon.gender));
            }
            UnionComm_RequestLink0(unisys);
            *seq = 8;
            break;
        }
        break;
    case 8:
        if (UnionMsg_IsPrintDone(unisys) == TRUE && !UnionComm_IsLinkRequested(unisys)) {
            UnionComm_SetSelf(unisys, 3, 0);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static BOOL UnionOneself_StepColosseumChildRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    if (*seq < 4 && UnionOneself_CheckNetError(unisys) == TRUE) {
        return TRUE;
    }
    switch (*seq) {
    case 0:
        GFL_ASSERT(unisys->colosseum_sys == NULL);
        unisys->colosseum_sys = Colosseum_Create(unisys->param->gameData, unisys->param->gsys,
                                                 unisys->param->playerInfo, situ->mycomm.unk46_1);
        UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetBattleStartMessage(situ->mycomm.target->beacon.gender));
        (*seq)++;
        break;
    case 1:
        if (UnionMsg_IsPrintDone(unisys) == TRUE) {
            (*seq)++;
        }
        break;
    case 2:
        NetHandle_SendTimingPair(NetHandle_GetOwn(), 0xb, 0x14);
        (*seq)++;
        break;
    case 3:
        if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 0xb, 0x14) == TRUE) {
            (*seq)++;
        }
        break;
    case 4:
        UnionCommand_Set(unisys, situ->mycomm.unk19 + 1, NULL);
        (*seq)++;
        break;
    case 5:
        if (!UnionCommand_IsActive(unisys)) {
            UnionOneself_SetUpdateDisabled(unisys, field, TRUE);
            UnionOneself_SetNextStep(unisys, UNION_STEP_COLOSSEUM);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static BOOL UnionOneself_StepColosseumParentRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    if (*seq < 4 && UnionOneself_CheckNetError(unisys) == TRUE) {
        return TRUE;
    }
    switch (*seq) {
    case 0:
        if (situ->mycomm.unk46_1 == 0) {
            UnionMsg_CreateAndPrint(unisys, field, UnionMsg_GetBattleStartMessage(situ->mycomm.target->beacon.gender));
        } else {
            UnionMsg_CreateAndPrint(unisys, field,
                                    UnionMsg_GetMultiBattleStartMessage(situ->mycomm.target->beacon.gender));
        }
        (*seq)++;
        break;
    case 1:
        if (UnionMsg_IsPrintDone(unisys) == TRUE) {
            (*seq)++;
        }
        break;
    case 2:
        if (situ->mycomm.unk46_1 == 0) {
            NetHandle_SendTimingPair(NetHandle_GetOwn(), 0xb, 0x14);
        }
        (*seq)++;
        break;
    case 3:
        if (situ->mycomm.unk46_1 == 0) {
            if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 0xb, 0x14) == TRUE) {
                (*seq)++;
            }
        } else {
            (*seq)++;
        }
        break;
    case 4:
        UnionCommand_Set(unisys, situ->mycomm.unk19 + 1, NULL);
        (*seq)++;
        break;
    case 5:
        if (!UnionCommand_IsActive(unisys)) {
            GFL_ASSERT(unisys->colosseum_sys == NULL);
            unisys->colosseum_sys = Colosseum_Create(unisys->param->gameData, unisys->param->gsys,
                                                     unisys->param->playerInfo, situ->mycomm.unk46_1);
            UnionOneself_SetUpdateDisabled(unisys, field, TRUE);
            UnionOneself_SetNextStep(unisys, UNION_STEP_COLOSSEUM);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static BOOL UnionOneself_StepColosseumRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    UnionColosseum *clsys = unisys->colosseum_sys;
    PlayerInfo *info;

    GFL_NetGetHandleNetId(NetHandle_GetOwn());
    GFL_ASSERT(clsys != NULL);
    if (UnionOneself_CheckNetErrorEnd(unisys) == TRUE) {
        if (clsys->entryMenu != NULL) {
            CommEntryMenu_Free(clsys->entryMenu);
        }
        UnionMsg_FreeAll(unisys);
        return TRUE;
    }
    switch (*seq) {
    case 0:
        if (UnionMsg_GetTableByte(situ->mycomm.unk19) == 2) {
            clsys->unkDE = 1;
            if (GFL_NetIsParent() == TRUE) {
                if (UnionMain_SendColosseumUnkCA() == TRUE) {
                    *seq = 0x64;
                }
            } else {
                *seq = 0x64;
            }
            break;
        }
        GFL_NetSetDisconnectError(FALSE);
        if (GFL_NetIsParent() == TRUE) {
            info = GetGameDataPlayerInfo(unisys->param->gameData);
            clsys->entryMenu = CommEntryMenu_Create(info, field, UnionMsg_GetTableByte(situ->mycomm.unk19),
                                                    UnionMsg_GetTableByte(situ->mycomm.unk19), HEAPID_UNION,
                                                    COMM_ENTRY_MENU_LEADER, COMM_ENTRY_MENU_MULTI_BATTLE, NULL);
            CommEntryMenu_SetCallbacks(clsys->entryMenu, unisys, UnionOneself_AddColosseumMember,
                                       UnionOneself_RemoveColosseumMember);
        } else if (situ->mycomm.unk46_1 == 0) {
            info = GetGameDataPlayerInfo(unisys->param->gameData);
            clsys->entryMenu = CommEntryMenu_Create(info, field, UnionMsg_GetTableByte(situ->mycomm.unk19),
                                                    UnionMsg_GetTableByte(situ->mycomm.unk19), HEAPID_UNION,
                                                    COMM_ENTRY_MENU_JOIN, COMM_ENTRY_MENU_MULTI_BATTLE, NULL);
        } else {
            info = GetGameDataPlayerInfo(unisys->param->gameData);
            clsys->entryMenu = CommEntryMenu_Create(info, field, UnionMsg_GetTableByte(situ->mycomm.unk19),
                                                    UnionMsg_GetTableByte(situ->mycomm.unk19), HEAPID_UNION,
                                                    COMM_ENTRY_MENU_ASK_TO_JOIN, COMM_ENTRY_MENU_MULTI_BATTLE, NULL);
        }
        if (situ->mycomm.unk46_1 == 0) {
            UnionComm_SetCommandBase(situ->activity);
            NetHandle_SendTimingPair(NetHandle_GetOwn(), 0xc, 0x14);
        }
        (*seq)++;
        break;
    case 1:
        if (situ->mycomm.unk46_1 == 0) {
            if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 0xc, 0x14) == TRUE) {
                GFL_NetSetAcceptConnect(NetHandle_GetOwn(), 1);
                (*seq)++;
            }
        } else {
            (*seq)++;
        }
        break;
    case 2:
        (*seq)++;
        break;
    case 4:
        break;
    case 3: {
        u32 result = CommEntryMenu_Update(clsys->entryMenu);

        if (result != COMM_ENTRY_MENU_RUNNING) {
            clsys->unk17C[2] = CommEntryMenu_Free(clsys->entryMenu);
            clsys->entryMenu = NULL;
            switch (result) {
            case COMM_ENTRY_MENU_DONE:
                *seq = 0x64;
                GFL_NetSetDisconnectError(TRUE);
                break;
            case COMM_ENTRY_MENU_CANCELLED:
                *seq = 0xc8;
                break;
            case COMM_ENTRY_MENU_REFUSED:
                *seq = 0xc8;
                break;
            case COMM_ENTRY_MENU_ERROR:
                *seq = 0xc8;
                break;
            default:
                GFL_ASSERT(0);
                break;
            }
        }
        break;
    }
    case 0x64:
        clsys->unkDE = 1;
        UnionOneself_SetNextStep(unisys, UNION_STEP_COLOSSEUM_SEND);
        return TRUE;
    case 0xc8:
        UnionMsg_CreateAndPrint(unisys, field, 0x14c);
        clsys->unkDE = 2;
        (*seq)++;
        break;
    case 0xc9:
        if (UnionMsg_IsPrintDone(unisys) == TRUE) {
            UnionOneself_SetNextStep(unisys, UNION_STEP_COLOSSEUM_END);
            UnionOneself_SetUpdateDisabled(unisys, field, TRUE);
            return TRUE;
        }
        break;
    case 0x12c:
        clsys->unkDE = 2;
        UnionOneself_SetNextStep(unisys, UNION_STEP_COLOSSEUM_END);
        UnionOneself_SetUpdateDisabled(unisys, field, TRUE);
        return TRUE;
    }
    return FALSE;
}

static void UnionOneself_AddColosseumMember(void *work, const PlayerInfo *info, const u8 *mac) {
    UnionSystem *unisys = work;

    UnionGroup_AddMember(&unisys->situ.mycomm, mac, PlayerInfo_GetUnionView(info), getTrainerGender(info));
}

static void UnionOneself_RemoveColosseumMember(void *work, const PlayerInfo *info, const u8 *mac) {
    UnionSystem *unisys = work;

    UnionGroup_RemoveMember(&unisys->situ.mycomm, mac);
}

static BOOL UnionOneself_StepColosseumSendRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    UnionColosseum *clsys = unisys->colosseum_sys;
    u8 netId = GFL_NetGetHandleNetId(NetHandle_GetOwn());

    GFL_ASSERT(clsys != NULL);
    if (UnionOneself_CheckNetErrorEnd(unisys) == TRUE) {
        UnionMsg_FreeAll(unisys);
        return TRUE;
    }
    switch (*seq) {
    case 0:
        NetHandle_SendTimingPair(NetHandle_GetOwn(), 0xd, 0x14);
        (*seq)++;
        break;
    case 1:
        if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 0xd, 0x14) == TRUE) {
            (*seq)++;
        }
        break;
    case 2:
        Colosseum_RegisterCommands(unisys);
        NetHandle_SendTimingPair(NetHandle_GetOwn(), 0xe, 0x14);
        (*seq)++;
        break;
    case 3:
        if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 0xe, 0x14) == TRUE) {
            (*seq)++;
        }
        break;
    case 4:
        if (Colosseum_SendMember(&clsys->members[netId], FALSE) == TRUE) {
            (*seq)++;
        }
        break;
    case 5:
        if (UnionEff_AllColosseumMembersHere(clsys) == TRUE) {
            int count = GFL_NetGetNegotiatedCount();
            int i;

            for (i = 0; i < count; i++) {
                if (clsys->members[i].unk26 == 1) {
                    UnionGroup_AddMember(&situ->mycomm, clsys->members[i].mac,
                                         PlayerInfo_GetUnionView(&clsys->members[i].info),
                                         getTrainerGender(&clsys->members[i].info));
                }
            }
            (*seq)++;
        }
        break;
    case 6:
        NetHandle_SendTimingPair(NetHandle_GetOwn(), 0xf, 0x14);
        (*seq)++;
        break;
    case 7:
        if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 0xf, 0x14) == TRUE) {
            if (UnionOneself_GetCardVersion(unisys, TRUE) == 0x16) {
                *seq = 8;
            } else {
                *seq = 0x12;
            }
        }
        break;
    case 8:
        if (Colosseum_SendCard(clsys->card[netId]) == TRUE) {
            *seq = 9;
        }
        break;
    case 0x12:
        if (Colosseum_SendCardHead(clsys->cardCheck[netId]) == TRUE) {
            *seq = 9;
        }
        break;
    case 9:
        if (UnionEff_AllColosseumCardsIn(clsys) == TRUE) {
            (*seq)++;
        }
        break;
    case 10: {
        RivalDataSave *rival = getHollow_RivalData(GameData_GetSaveControl(GSYS_GetGameData(unisys->param->gsys)));
        int count = GFL_NetGetNegotiatedCount();
        int i;

        for (i = 0; i < count; i++) {
            if (clsys->members[i].unk26 == 1 && i != GFL_NetGetHandleNetId(NetHandle_GetOwn())) {
                Misc_AddMetTrainer(rival, getIDAsUInt(&clsys->members[i].info));
            }
        }
        Colosseum_SetUnkC8_0(clsys, TRUE);
        UnionOneself_SetNextStep(unisys, UNION_STEP_COLOSSEUM_IDLE);
        return TRUE;
    }
    }
    return FALSE;
}

static BOOL UnionOneself_StepIdleColosseumBegin(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    UnionMsg_FreeAll(unisys);
    UnionOneself_SetUpdateDisabled(unisys, field, FALSE);
    return TRUE;
}

static BOOL UnionOneself_StepIdleColosseumRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    UnionColosseum *clsys = unisys->colosseum_sys;
    FieldPlayer *player = Field_GetPlayer(field);
    s16 z, y, x;
    u32 dir = 0;
    u32 out;
    u32 pos;

    if (UnionOneself_CheckNetErrorEnd(unisys) == TRUE) {
        UnionMsg_FreeAll(unisys);
        return TRUE;
    }
    if (*seq == 0) {
        if (UnionEff_GetEntrySpot(field, UnionMsg_GetTableByte(situ->mycomm.unk19), &pos) == TRUE) {
            Colosseum_SetUnkDC(clsys, pos);
            UnionOneself_SetNextStep(unisys, UNION_STEP_COLOSSEUM_SIT);
            UnionOneself_SetUpdateDisabled(unisys, field, TRUE);
            return TRUE;
        }
        if (UnionEff_AnyColosseumUnk172(clsys) == TRUE) {
            UnionOneself_SetNextStep(unisys, UNION_STEP_COLOSSEUM_END);
            UnionOneself_SetUpdateDisabled(unisys, field, TRUE);
            return TRUE;
        }
        if (UnionEff_IsOnSeatSpot(field) == TRUE) {
            UnionOneself_SetNextStep(unisys, UNION_STEP_QUIT);
            UnionOneself_SetUpdateDisabled(unisys, field, TRUE);
            return TRUE;
        }
        if (GCTX_HIDGetPressedKeys() & PAD_BUTTON_A) {
            u32 dir = 0;
            s16 x, y, z;

            FieldPlayer_GetGPosInFront(player, &x, &y, &z);
            if (CommPlayer_FindActor(clsys->commPlayer, x, y, z, &out, &dir) == TRUE) {
                clsys->unk17C[0] = out;
                UnionOneself_SetNextStep(unisys, UNION_STEP_COLOSSEUM_CARD);
                UnionOneself_SetUpdateDisabled(unisys, field, TRUE);
                return TRUE;
            }
        }
    }
    return FALSE;
}

static BOOL UnionOneself_StepColosseumSeatRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    UnionColosseum *clsys = unisys->colosseum_sys;
    FieldActor *actor;
    u32 pos;

    if (UnionOneself_CheckNetErrorEnd(unisys) == TRUE) {
        UnionMsg_FreeAll(unisys);
        return TRUE;
    }
    switch (*seq) {
    case 0:
        (*seq)++;
        break;
    case 1:
        if (Colosseum_SendUnk4(clsys) == TRUE) {
            (*seq)++;
        }
        break;
    case 2:
        if (UnionMsg_IsPrintDone(unisys)) {
            u8 result = Colosseum_GetUnkDD(clsys);

            if (result == 1) {
                (*seq)++;
            } else if (result == 0) {
                UnionOneself_SetNextStep(unisys, UNION_STEP_COLOSSEUM_STAND);
                return TRUE;
            }
        }
        break;
    case 3:
        actor = FieldPlayer_GetActor(Field_GetPlayer(field));
        if (IsAllActorAcmdFinished(actor) == TRUE) {
            UnionEff_GetEntrySpot(field, UnionMsg_GetTableByte(situ->mycomm.unk19), &pos);
            FldAct_SetAcmd(actor, (pos & 1) ? 2 : 3);
            (*seq)++;
        }
        break;
    case 4:
        actor = FieldPlayer_GetActor(Field_GetPlayer(field));
        if (FldAct_CheckAcmdEnd(actor) == TRUE) {
            FldAct_EndAcmd(actor);
            UnionOneself_SetNextStep(unisys, UNION_STEP_PARTY_SELECT);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static BOOL UnionOneself_StepColosseumLeaveRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    UnionColosseum *clsys = unisys->colosseum_sys;
    FieldActor *actor = FieldPlayer_GetActor(Field_GetPlayer(field));
    u32 pos;

    if (UnionOneself_CheckNetErrorEnd(unisys) == TRUE) {
        UnionMsg_FreeAll(unisys);
        return TRUE;
    }
    switch (*seq) {
    case 0:
        Colosseum_SetUnkDC(clsys, 0xff);
        (*seq)++;
        break;
    case 1:
        if (Colosseum_SendUnk4(clsys) == TRUE) {
            (*seq)++;
        }
        break;
    case 2:
        if (Colosseum_GetUnkDD(clsys) != 0xff) {
            (*seq)++;
        }
        break;
    case 3:
        UnionMsg_FreeAll(unisys);
        (*seq)++;
        break;
    case 4:
        if (IsAllActorAcmdFinished(actor) == TRUE) {
            UnionEff_GetEntrySpot(field, UnionMsg_GetTableByte(situ->mycomm.unk19), &pos);
            FldAct_SetAcmd(actor, (pos & 1) ? 0xb : 0xa);
            (*seq)++;
        }
        break;
    case 5:
        if (FldAct_CheckAcmdEnd(actor) == TRUE) {
            FldAct_EndAcmd(actor);
            UnionOneself_SetNextStep(unisys, UNION_STEP_COLOSSEUM_IDLE);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static BOOL UnionOneself_StepPartySelectRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    UnionColosseum *clsys = unisys->colosseum_sys;
    u32 partyResult;
    u32 boxResult;
    u32 selected;
    u32 result;
    u32 kind;
    u8 partyOk;
    u8 boxOk;

    if (!UnionMsg_IsPrintDone(unisys)) {
        return FALSE;
    }
    switch (*seq) {
    case 0:
        situ->result = 0;
        (*seq)++;
        // fallthrough
    case 1:
        if (BattleBoxSave_HasPkm(getBattleBox(GameData_GetSaveControl(unisys->param->gameData))) == TRUE) {
            (*seq)++;
        } else {
            clsys->unk17C[1] = 0;
            situ->result++;
            *seq = 4;
        }
        break;
    case 2:
        UnionMsg_FreeAll(unisys);
        UnionOneself_CheckPartyRegulation(unisys, &partyResult);
        UnionOneself_CheckBoxRegulation(unisys, &boxResult);
        partyOk = TRUE;
        if (partyResult == 0) {
            partyOk = FALSE;
        }
        boxOk = TRUE;
        if (boxResult == 0) {
            boxOk = FALSE;
        }
        unisys->alloc.boxParty = UnionOneself_CreateBoxParty(unisys);
        unisys->alloc.partySelect = func_ov036_021c3180(field, GameData_GetParty(unisys->param->gameData),
                                                        unisys->alloc.boxParty, partyOk, boxOk, 0x15);
        (*seq)++;
        break;
    case 3:
        if (func_ov036_021c3278(unisys->alloc.partySelect) == TRUE) {
            result = func_ov036_021c3218(unisys->alloc.partySelect, &selected);
            unisys->alloc.partySelect = NULL;
            if (result == 2) {
                UnionOneself_FreeBoxParty(unisys->alloc.boxParty);
                unisys->alloc.boxParty = NULL;
                UnionOneself_SetNextStep(unisys, UNION_STEP_COLOSSEUM_STAND);
                return TRUE;
            }
            switch (result) {
            case 0:
                clsys->unk17C[1] = 0;
                break;
            case 1:
                clsys->unk17C[1] = 1;
                break;
            }
            UnionOneself_FreeBoxParty(unisys->alloc.boxParty);
            unisys->alloc.boxParty = NULL;
            (*seq)++;
        }
        break;
    case 4: {
        u32 count = 0;

        if (clsys->unk17C[1] == 0) {
            UnionOneself_CheckPartyRegulation(unisys, &count);
            kind = 1;
        } else {
            UnionOneself_CheckBoxRegulation(unisys, &count);
            kind = 2;
        }
        if (count != 0) {
            UnionMsg_CreateMsgSystem(unisys, field);
            UnionMsg_CreateRulesWindow(unisys, field, count, UnionOneself_IsDoubleActivity(situ->activity), kind);
            *seq = 5;
        } else {
            UnionOneself_SetNextStep(unisys, UNION_STEP_PARTY_WAIT);
            return TRUE;
        }
        break;
    }
    case 5:
        if (UnionMsg_IsRulesPrinted(unisys, field) == TRUE) {
            UnionMsg_CreateAndPrint(unisys, field, 0x115);
            (*seq)++;
        }
        break;
    case 6:
        if (GCTX_HIDGetPressedKeys() & (PAD_BUTTON_A | PAD_BUTTON_B)) {
            UnionMsg_FreeRulesWindow(unisys);
            if (situ->result == 0) {
                *seq = 0;
            } else {
                UnionOneself_SetNextStep(unisys, UNION_STEP_COLOSSEUM_STAND);
                return TRUE;
            }
        }
        break;
    }
    return FALSE;
}

static BOOL UnionOneself_StepPartyWaitRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    UnionColosseum *clsys = unisys->colosseum_sys;

    if (UnionOneself_CheckNetErrorEnd(unisys) == TRUE) {
        UnionMsg_FreeAll(unisys);
        return TRUE;
    }
    if (clsys->unkCB == 1) {
        UnionOneself_SetNextStep(unisys, UNION_STEP_PARTY_SEND);
        clsys->unkCB = 0;
        clsys->unkDF = 0;
        return TRUE;
    }
    switch (*seq) {
    case 0:
        UnionMsg_CreateAndPrint(unisys, field, 0x116);
        (*seq)++;
        break;
    case 1:
        if (Colosseum_SendUnk8() == TRUE) {
            clsys->unkDF = 1;
            (*seq)++;
        }
        break;
    case 2:
        if (UnionMsg_IsPrintDone(unisys) == TRUE && (GCTX_HIDGetPressedKeys() & PAD_BUTTON_B)) {
            *seq = 0x64;
        }
        break;
    case 0x64:
        if (Colosseum_SendUnk9() == TRUE) {
            (*seq)++;
        }
        break;
    case 0x65:
        if (clsys->unkDF == 0) {
            UnionOneself_SetNextStep(unisys, UNION_STEP_COLOSSEUM_STAND);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static BOOL UnionOneself_StepPartySendRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    UnionColosseum *clsys = unisys->colosseum_sys;
    u8 netId = GFL_NetGetHandleNetId(NetHandle_GetOwn());

    if (UnionOneself_CheckNetErrorEnd(unisys) == TRUE) {
        UnionMsg_FreeAll(unisys);
        return TRUE;
    }
    switch (*seq) {
    case 0:
        switch (clsys->unk17C[1]) {
        case 0:
        default:
            PokeParty_Copy(GameData_GetParty(unisys->param->gameData), clsys->party[netId]);
            break;
        case 1: {
            PokeParty *boxParty = UnionOneself_CreateBoxParty(unisys);

            PokeParty_Copy(boxParty, clsys->party[netId]);
            UnionOneself_FreeBoxParty(boxParty);
            break;
        }
        }
        PokeRegulation_ApplyLevelToParty(unisys->alloc.regulation, clsys->party[netId]);
        (*seq)++;
        break;
    case 1:
        Colosseum_ClearParties(clsys, TRUE);
        UnionEff_ClearColosseumUnk170(clsys);
        Colosseum_ResetUnk171(clsys);
        NetHandle_SendTimingPair(NetHandle_GetOwn(), 0x14, 0x14);
        (*seq)++;
        break;
    case 2:
        if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 0x14, 0x14) == TRUE) {
            (*seq)++;
        }
        break;
    case 3:
        if (Colosseum_SendParty(clsys->party[netId]) == TRUE) {
            (*seq)++;
        }
        break;
    case 4:
        if (Colosseum_AreAllPartiesReceived(clsys) == TRUE) {
            (*seq)++;
        }
        break;
    case 5:
        NetHandle_SendTimingPair(NetHandle_GetOwn(), 0x10, 0x14);
        (*seq)++;
        break;
    case 6:
        if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 0x10, 0x14) == TRUE) {
            (*seq)++;
        }
        break;
    case 7:
        if (GFL_NetIsParent() == TRUE) {
            if (Colosseum_SendResult(clsys->unkCC) == TRUE) {
                (*seq)++;
            }
        } else {
            (*seq)++;
        }
        break;
    case 8:
        if (UnionEff_IsColosseumUnk170Set(clsys) == TRUE) {
            UnionOneself_SetNextStep(unisys, UNION_STEP_BATTLE_LIST);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static BOOL UnionOneself_StepBattleListRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    UnionColosseum *clsys = unisys->colosseum_sys;
    UnionColosseumParam *param;
    int netId = GFL_NetGetHandleNetId(NetHandle_GetOwn());

    if (*seq < 3 && UnionOneself_CheckNetErrorEnd(unisys) == TRUE) {
        UnionMsg_FreeAll(unisys);
        return TRUE;
    }
    switch (*seq) {
    case 0:
        if (UnionMsg_IsPrintDone(unisys) == TRUE) {
            u32 msg;

            switch (situ->mycomm.unk19) {
            default:
                msg = 0x117;
                break;
            case 6:
            case 7:
            case 22:
            case 23:
                msg = 0x118;
                break;
            case 20:
            case 21:
                msg = 0x11b;
                break;
            case 10:
            case 11:
            case 18:
            case 19:
                msg = 0x119;
                break;
            case 14:
            case 15:
                msg = 0x11a;
                break;
            }
            UnionMsg_CreateAndPrint(unisys, field, msg);
            (*seq)++;
        }
        break;
    case 1:
        if (UnionMsg_IsPrintDone(unisys) == TRUE) {
            NetHandle_SendTimingPair(NetHandle_GetOwn(), 0x11, 0x14);
            (*seq)++;
        }
        break;
    case 2:
        if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 0x11, 0x14) == TRUE) {
            UnionMsg_FreeAll(unisys);
            (*seq)++;
        }
        break;
    case 3: {
        int i;
        int count;

        param = GFL_HeapAllocate(HEAPID_UNION, sizeof(UnionColosseumParam), TRUE, "union_oneself.c", 4426);

        PokeListParam_Setup(&param->list, unisys->param->gameData, 0x1a, clsys->party[netId]);
        param->list.regulation = unisys->alloc.regulation;
        param->battle = GFL_HeapAllocate(HEAPID_UNION, sizeof(ColosseumBattleParam), TRUE, "union_oneself.c", 4433);
        count = 0;
        for (i = 0; i < GFL_NetGetNegotiatedCount(); i++) {
            if (i != netId) {
                int slot;

                if ((clsys->unk16C[netId] & 1) == (clsys->unk16C[i] & 1)) {
                    slot = 0;
                } else {
                    slot = ++count;
                }
                param->list.partners[slot].party = clsys->party[i];
                param->list.partners[slot].name = GetPlayerName(&clsys->members[i].info);
                param->list.partners[slot].gender = getTrainerGender(&clsys->members[i].info);
            }
        }
        switch (situ->mycomm.unk19) {
        case 20:
        case 21:
        case 22:
        case 23:
            param->list.partnerCount = 2;
            break;
        default:
            param->list.partnerCount = 1;
            break;
        }
        param->list.showPartners = TRUE;
        param->list.timerEnabled = FALSE;
        param->list.timeLeft = 0;
        unisys->cmdArg = param;
        UnionCommand_Set(unisys, 0x1a, param);
        (*seq)++;
        break;
    }
    case 4:
        if (UnionCommand_IsActive(unisys) != TRUE) {
            param = unisys->cmdArg;
            GFL_HeapFree(param->battle);
            GFL_HeapFree(unisys->cmdArg);
            unisys->cmdArg = NULL;
            UnionOneself_SetNextStep(unisys, UNION_STEP_COLOSSEUM_STAND);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static BOOL UnionOneself_StepQuitRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    FieldActor *actor = FieldPlayer_GetActor(Field_GetPlayer(field));
    BOOL yes;

    if (UnionOneself_CheckNetErrorEnd(unisys) == TRUE) {
        UnionMsg_FreeAll(unisys);
        return TRUE;
    }
    switch (*seq) {
    case 0:
        UnionMsg_CreateAndPrint(unisys, field, 0x14e);
        (*seq)++;
        break;
    case 1:
        if (UnionMsg_IsPrintDone(unisys)) {
            UnionMsg_CreateYesNo(unisys, field);
            (*seq)++;
        }
        break;
    case 2:
        if (UnionMsg_UpdateYesNo(unisys, &yes) == TRUE) {
            UnionMsg_FreeYesNo(unisys);
            if (yes == 0) {
                *seq = 3;
            } else {
                UnionOneself_SetNextStep(unisys, UNION_STEP_COLOSSEUM_END);
                return TRUE;
            }
        }
        break;
    case 3:
        if (IsAllActorAcmdFinished(actor) == TRUE) {
            FldAct_SetAcmd(actor, 8);
            (*seq)++;
        }
        break;
    case 4:
        if (FldAct_CheckAcmdEnd(actor) == TRUE) {
            FldAct_EndAcmd(actor);
            UnionOneself_SetNextStep(unisys, UNION_STEP_COLOSSEUM_IDLE);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static BOOL UnionOneself_StepColosseumEndRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    UnionColosseum *clsys = unisys->colosseum_sys;
    int i;

    if (*seq < 7 && GFL_NetErrCheck() && UnionMsg_IsPrintDone(unisys) == TRUE) {
        UnionOneself_SetUpdateDisabled(unisys, field, TRUE);
        *seq = 7;
    }
    switch (*seq) {
    case 0:
        UnionOneself_SetUpdateDisabled(unisys, field, TRUE);
        if (clsys->unkDE == 2) {
            if (UnionMsg_IsPrintDone(unisys) == TRUE) {
                *seq = 6;
            }
        } else {
            UnionMsg_CreateAndPrint(unisys, field, 0x14c);
            (*seq)++;
        }
        break;
    case 1:
        if (Colosseum_SendUnkD() == TRUE) {
            (*seq)++;
        }
        break;
    case 2:
        if (UnionEff_AllColosseumReady(clsys) == TRUE) {
            (*seq)++;
        }
        break;
    case 3:
        if (UnionMsg_IsPrintDone(unisys) == TRUE) {
            (*seq)++;
        }
        break;
    case 4:
        NetHandle_SendTimingPair(NetHandle_GetOwn(), 0x13, 0x14);
        (*seq)++;
        break;
    case 5:
        if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 0x13, 0x14) == TRUE) {
            GFL_NetSetDisconnectError(FALSE);
            (*seq)++;
        }
        break;
    case 6:
        if (GFL_NetIsParent() == TRUE) {
            u8 mask = clsys->unk17C[2];

            if (NetSystem_CountConnected() <= 1) {
                (*seq)++;
            } else {
                for (i = 0; i < 5; i++) {
                    if (i != NetSystem_GetNetId() && (mask & (1 << i)) && GFL_NetIsNegotiated((u8)i) == TRUE) {
                        break;
                    }
                }
                if (i == 5) {
                    (*seq)++;
                }
            }
        } else {
            (*seq)++;
        }
        break;
    case 7:
        if (clsys->unkC8_1 == 1 && UnionComm_IsLinkRequested(unisys) == TRUE) {
            return FALSE;
        }
        if (GFL_NetErrCheck()) {
            UnionMsg_FreeAll(unisys);
            GSYS_SetNetErrorPending(unisys->param->gsys, 1);
            return FALSE;
        }
        UnionComm_RequestLink1(unisys);
        (*seq)++;
        break;
    case 8:
        if (!UnionComm_IsLinkRequested(unisys)) {
            (*seq)++;
        }
        break;
    case 9:
        clsys->unkC9 = 1;
        UnionComm_ClearBeacons(unisys);
        UnionOneself_ClearActors(unisys);
        UnionGroup_Init(unisys, &situ->mycomm);
        UnionCommand_Set(unisys, 0x19, NULL);
        (*seq)++;
        break;
    case 10:
        if (!UnionCommand_IsActive(unisys)) {
            if (unisys->colosseum_sys != NULL) {
                Colosseum_Free(unisys->colosseum_sys, unisys->param->gsys);
                unisys->colosseum_sys = NULL;
            }
            UnionComm_RequestLink2(unisys);
            (*seq)++;
        }
        break;
    case 11:
        if (!UnionComm_IsLinkRequested(unisys)) {
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static BOOL UnionOneself_StepColosseumCardRun(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq) {
    UnionColosseum *clsys = unisys->colosseum_sys;

    switch (*seq) {
    case 0: {
        u32 msg;
        u32 version;

        UnionOneself_SetUpdateDisabled(unisys, field, TRUE);
        version = PlayerInfo_GetVersion(&clsys->members[clsys->unk17C[0]].info);
        if (version - VERSION_WHITE <= 3) {
            int stars = clsys->cardCheck[clsys->unk17C[0]]->stars;

            if (stars < 5) {
                msg = 0x156 + stars;
            } else if (stars == 5) {
                switch (version) {
                case VERSION_BLACK:
                case VERSION_BLACK2:
                    msg = 0x15c;
                    break;
                case VERSION_WHITE:
                case VERSION_WHITE2:
                    msg = 0x15b;
                    break;
                default:
                    msg = 0x15d;
                    break;
                }
            } else {
                msg = 0x15d;
            }
        } else {
            msg = 0x15d;
        }
        UnionMsg_CreateMsgSystem(unisys, field);
        copyVarForText(unisys->wordset, 0, &clsys->members[clsys->unk17C[0]].info);
        UnionMsg_PrintMessage(unisys, msg);
        (*seq)++;
        break;
    }
    case 1:
        if (UnionMsg_IsPrintDone(unisys) == TRUE && (GCTX_HIDGetPressedKeys() & (PAD_BUTTON_A | PAD_BUTTON_B))) {
            UnionMsg_FreeAll(unisys);
            clsys->cardCheck[clsys->unk17C[0]]->unk04_5 = 1;
            unisys->cmdArg =
                TrainerCardParam_CreateView(unisys->param->gameData, clsys->cardCheck[clsys->unk17C[0]], HEAPID_UNION);
            UnionCommand_Set(unisys, 0x1c, unisys->cmdArg);
            (*seq)++;
        }
        break;
    case 2:
        if (!UnionCommand_IsActive(unisys)) {
            unisys->cmdArg = NULL;
            (*seq)++;
        }
        break;
    case 3:
        UnionOneself_SetNextStep(unisys, UNION_STEP_COLOSSEUM_IDLE);
        return TRUE;
    }
    return FALSE;
}
