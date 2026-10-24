#include "types.h"
#include "battle/btl_main.h"
#include "constants/language.h"
#include "constants/species.h"
#include "constants/version.h"
#include "field/festival.h"
#include "field/player_state.h"
#include "field/survey.h"
#include "field/townmap_util.h"
#include "field/unity_tower.h"
#include "field/zone.h"
#include "gfl/heap.h"
#include "gfl/net.h"
#include "gfl/net_whpipe.h"
#include "gfl/overlay.h"
#include "gfl/std.h"
#include "gfl/str.h"
#include "gfl/tcb.h"
#include "nitro/math.h"
#include "nitro/os.h"
#include "nitro/rtc.h"
#include "pml/item.h"
#include "save/high_link.h"
#include "save/join_avenue.h"
#include "save/medal_box.h"
#include "save/player_info.h"
#include "save/save_control.h"
#include "save/trainer_card.h"
#include "system/game_beacon.h"
#include "system/game_data.h"
#include "system/game_system.h"
#include "system/ir_check.h"
#include "system/pms.h"

// The game's beacons. The file's name is from the ROM's string

// Zones from this one on are new in Black 2 and White 2
#define ZONE_ID_BW1_MAX 0x1ab
// The last survey question
#define SURVEY_QUESTION_LAST 0x1d
// The survey question answered by play time, in tens of hours
#define SURVEY_QUESTION_PLAY_TIME 0x1d

// A beacon type's priority, against the current type's (GameBeaconSys_CanSendType), and how many frames it is sent
typedef struct {
    u32 priority : 8;
    u32 lifetime : 24;
} GameBeaconTypeInfo;

static BOOL GameBeaconSendSlot_UpdateTimer(GameBeaconSendSlot *slot);
static BOOL GameBeaconSys_CountSurveyAnswers(GameBeaconSystem *sys, const GameBeacon *beacon);
static void GameBeaconSys_AddLog(GameBeaconSystem *sys, const GameBeacon *beacon);
static BOOL GameBeacon_IsBW1Incompatible(const GameBeacon *beacon, BOOL b2w2Only);
static void GameBeacon_PrepareSend(GameBeacon *beacon, BOOL b2w2Only);
static void GameBeacon_FilterRecent(GameBeacon *beacon, BOOL received);
static void GameBeaconSys_CheckMission(GameBeaconSystem *sys, const GameBeacon *beacon);
static void GameBeaconSys_UpdateNotices(GameBeaconSystem *sys, const GameBeacon *beacon, BOOL isNew);
static GameBeacon *GameBeaconSys_GetLogEntry(GameBeaconSystem *sys, int index, GameBeaconTime *time);
static GameBeacon *GameBeaconSys_GetRecentEntry(GameBeaconSystem *sys, u32 n);
static void GameBeacon_Init(GameBeacon *beacon, GameData *gameData);
static BOOL GameBeaconSys_AddAvenueVisitor(GameBeaconSystem *sys, const GameBeacon *beacon, u32 a2);
static BOOL GameBeaconSys_UpdateAvenueVisitor(GameBeaconSystem *sys);
static void GameBeaconSendSlot_ResetHeader(GameBeaconSendSlot *slot, u8 targetVersions);
static void GameBeacon_StoreTextN(const StrBuf *str, u16 *dest, u32 length);
static void GameBeaconSys_SendCaptureWild(u16 species);
static void GameBeacon_SetCaptureWild(GameBeacon *beacon, u16 species);
static void GameBeaconSys_SendCaptureSpecial(u16 species);
static void GameBeacon_SetCaptureSpecial(GameBeacon *beacon, u16 species);
static BOOL GameBeaconSys_SendCaptureMission(u16 species, BOOL a1, BOOL a2);
static void GameBeacon_SetType10(GameBeacon *beacon, const StrBuf *nickname);
static void GameBeacon_SetEvolution(GameBeacon *beacon, u16 species, const StrBuf *nickname);
static void GameBeaconSys_SendPlayTime(u16 hours);
static void GameBeacon_SetPlayTime(GameBeacon *beacon, u16 hours);
static void GameBeacon_SetItem(GameBeacon *beacon, u16 item);
static void GameBeacon_UpdateZone(GameBeacon *beacon, u16 zoneId, GameData *gameData);
static void GameBeacon_SetCGearRecordFields(GameBeacon *beacon, const PMSData *record);
static void GameBeacon_SetMyCGearRecord(const PMSData *record);
static void GameBeaconSys_SetEnabled(u8 enabled);
static void GameBeaconSys_SendAvenueAd(GameBeaconSystem *sys);
static BOOL GameBeaconSys_UpdateAvenueAdTimer(GameBeaconSystem *sys);
static void GameBeaconSys_HideNewHBlank(TCB *tcb, void *data);

// The frames between Join Avenue advertisements, by advertisement
static const u16 sAvenueAdIntervals[7] = { 30, 30, 30, 30, 30, 30, 30 };

static const u16 sSpecialSpecies[25] = {
    SPECIES_ZOROARK,   SPECIES_COBALION,  SPECIES_TERRAKION, SPECIES_VIRIZION, SPECIES_TORNADUS,
    SPECIES_THUNDURUS, SPECIES_RESHIRAM,  SPECIES_ZEKROM,    SPECIES_LANDORUS, SPECIES_KYUREM,
    SPECIES_KELDEO,    SPECIES_MELOETTA,  SPECIES_GENESECT,  SPECIES_VICTINI,  SPECIES_REGICE,
    SPECIES_REGIROCK,  SPECIES_REGISTEEL, SPECIES_REGIGIGAS, SPECIES_LATIAS,   SPECIES_LATIOS,
    SPECIES_UXIE,      SPECIES_MESPRIT,   SPECIES_AZELF,     SPECIES_HEATRAN,  SPECIES_CRESSELIA,
};

static const GameBeaconTypeInfo sGameBeaconTypeInfo[GAME_BEACON_TYPE_MAX] = {
    { 0, 600 },    // 0x00
    { 0, 600 },    // 0x01
    { 0, 600 },    // 0x02
    { 0, 600 },    // 0x03
    { 0, 600 },    // 0x04
    { 0, 600 },    // 0x05
    { 0, 600 },    // 0x06
    { 0, 600 },    // 0x07
    { 0, 900 },    // 0x08
    { 0, 900 },    // 0x09
    { 0, 900 },    // 0x0a
    { 0, 900 },    // 0x0b
    { 0, 900 },    // 0x0c
    { 0, 900 },    // 0x0d
    { 0, 600 },    // 0x0e
    { 1, 900 },    // 0x0f
    { 0, 600 },    // 0x10
    { 0, 600 },    // 0x11
    { 5, 1200 },   // 0x12
    { 0, 600 },    // 0x13
    { 2, 900 },    // 0x14
    { 2, 900 },    // 0x15
    { 2, 900 },    // 0x16
    { 0, 600 },    // 0x17
    { 3, 600 },    // 0x18
    { 5, 1200 },   // 0x19
    { 5, 1200 },   // 0x1a
    { 5, 1200 },   // 0x1b
    { 1, 600 },    // 0x1c
    { 1, 600 },    // 0x1d
    { 4, 600 },    // 0x1e
    { 2, 600 },    // 0x1f
    { 1, 600 },    // 0x20
    { 3, 600 },    // 0x21
    { 1, 600 },    // 0x22
    { 0, 600 },    // 0x23
    { 0, 600 },    // 0x24
    { 0, 600 },    // 0x25
    { 0, 600 },    // 0x26
    { 0, 600 },    // 0x27
    { 1, 600 },    // 0x28
    { 2, 600 },    // 0x29
    { 3, 900 },    // 0x2a
    { 1, 600 },    // 0x2b
    { 2, 600 },    // 0x2c
    { 1, 600 },    // 0x2d
    { 1, 600 },    // 0x2e
    { 1, 600 },    // 0x2f
    { 2, 600 },    // 0x30
    { 3, 1200 },   // 0x31
    { 5, 1200 },   // 0x32
    { 0, 600 },    // 0x33
    { 0, 600 },    // 0x34
    { 0, 600 },    // 0x35
    { 0, 600 },    // 0x36
    { 0, 600 },    // 0x37
    { 0, 600 },    // 0x38
    { 0, 600 },    // 0x39
    { 0, 600 },    // 0x3a
    { 0, 600 },    // 0x3b
    { 1, 600 },    // 0x3c
    { 255, 1200 }, // 0x3d
    { 1, 1200 },   // 0x3e
    { 1, 1200 },   // 0x3f
    { 1, 1200 },   // 0x40
    { 1, 1200 },   // 0x41
    { 1, 1200 },   // 0x42
    { 1, 1200 },   // 0x43
    { 5, 1200 },   // 0x44
    { 1, 1200 },   // 0x45
    { 1, 1200 },   // 0x46
    { 1, 1200 },   // 0x47
    { 2, 1200 },   // 0x48
    { 5, 1200 },   // 0x49
    { 5, 1200 },   // 0x4a
    { 2, 1200 },   // 0x4b
    { 5, 1200 },   // 0x4c
    { 5, 1200 },   // 0x4d
    { 5, 1200 },   // 0x4e
    { 5, 1200 },   // 0x4f
    { 5, 1200 },   // 0x50
    { 5, 1200 },   // 0x51
    { 5, 1200 },   // 0x52
    { 5, 1200 },   // 0x53
    { 5, 1200 },   // 0x54
    { 5, 1200 },   // 0x55
    { 5, 1200 },   // 0x56
    { 5, 1200 },   // 0x57
    { 5, 1200 },   // 0x58
    { 5, 1200 },   // 0x59
    { 5, 1200 },   // 0x5a
    { 5, 1200 },   // 0x5b
    { 5, 1200 },   // 0x5c
    { 5, 1200 },   // 0x5d
    { 5, 1200 },   // 0x5e
    { 5, 1200 },   // 0x5f
    { 5, 1200 },   // 0x60
    { 5, 1200 },   // 0x61
    { 5, 1200 },   // 0x62
    { 5, 1200 },   // 0x63
    { 5, 1200 },   // 0x64
    { 5, 1200 },   // 0x65
    { 5, 1200 },   // 0x66
    { 5, 1200 },   // 0x67
};

GameBeaconSystem *GameBeaconSys;

void GameBeaconSys_Create(HeapID heapId) {
    GFL_ASSERT(GameBeaconSys == NULL);
    GameBeaconSys = GFL_HeapAllocate(heapId, sizeof(GameBeaconSystem), TRUE, "game_beacon.c", 169);
    GameBeaconSys->logNewest = -1;
    GameBeaconSys->b2w2Only = FALSE;
    GameBeaconSys->nameBuf = GFL_StrBufCreate(16, heapId);
    GameBeaconSys->avenueWork = JoinAvenueVisitor_Create(heapId);
    GameBeaconSys_SetEnabled(TRUE);
}

void GameBeaconSys_SetGameSystem(GameSystem *gsys) {
    GFL_ASSERT(GameBeaconSys != NULL);
    GameBeaconSys->gsys = gsys;
}

void GameBeaconSys_Update(void) {
    GameBeaconSystem *sys = GameBeaconSys;
    u16 hours;
    int i;

    if (sys == NULL || GFL_NetIsInitialized() == FALSE) {
        return;
    }
    if (sys->gameData != NULL) {
        hours = PlayTime_GetHours(GameData_GetPlayTime(sys->gameData));
        if (sys->playHours != hours) {
            switch (hours) {
            case 10:
            case 30:
            case 50:
            case 100:
                GameBeaconSys_SendPlayTime(hours);
                break;
            }
        }
        sys->playHours = hours;
    }
    if (GameBeaconSendSlot_UpdateTimer(&sys->mine)) {
        sys->avenueAdRequest = TRUE;
    }
    if (GFL_NetGetGameCommandBase() == 3 && !NetWhpipe_IsScanning() && GameBeaconSys_UpdateAvenueAdTimer(sys) &&
        sys->mine.beacon.type <= 1) {
        sys->avenueAdRequest = TRUE;
    }
    if (sys->gameData != NULL && !GameData_CheckEventsPaused(sys->gameData) && sys->enabled &&
        GFL_NetGetGameCommandBase() == 3) {
        if (!NetWhpipe_IsScanning()) {
            if (sys->mine.beacon.type <= 1 && sys->avenueAdRequest == TRUE) {
                GameBeaconSys_SendAvenueAd(sys);
                sys->avenueAdRequest = FALSE;
            }
        } else if (sys->avenueVisitorPending == TRUE && GameBeaconSys_UpdateAvenueVisitor(sys)) {
            sys->avenueVisitorPending = FALSE;
        }
    }
    for (i = 0; i < GAME_BEACON_LOG_MAX; i++) {
        if (sys->log[i].beacon.targetVersions != 0 && sys->log[i].age < 0xffff) {
            sys->log[i].age++;
        }
    }
}

void GameBeaconSys_SetGameData(GameData *gameData) {
    GameBeacon *beacon = &GameBeaconSys->mine.beacon;

    GameBeaconSys->gameData = gameData;
    GameBeacon_Init(beacon, gameData);
    GameBeaconSys->playHours = PlayTime_GetHours(GameData_GetPlayTime(gameData));
}

void GameBeaconSys_GetSendBeacon(GameBeacon *dest) {
    GameBeaconSystem *sys = GameBeaconSys;

    MI_CpuCopy8(&sys->mine.beacon, dest, sizeof(GameBeacon));
    GameBeacon_PrepareSend(dest, sys->b2w2Only);
}

void GameBeaconSys_ToggleB2W2Only(void) {
    GameBeaconSystem *sys = GameBeaconSys;

    if (sys != NULL) {
        if (sys->b2w2Only == FALSE) {
            sys->b2w2Only = TRUE;
        } else {
            sys->b2w2Only = FALSE;
        }
    }
}

void GameBeaconSys_SendIfUpdated(void) {
    if (GameBeaconSys->mine.updated == TRUE && GFL_NetIsInitialized() == TRUE) {
        NetWhpipe_UpdateGameBeacon();
        GameBeaconSys->mine.updated = FALSE;
    }
}

void GameBeaconSys_SetNotice(u32 bit) {
    GameBeaconSystem *sys = GameBeaconSys;

    if (sys != NULL) {
        sys->noticeFlags |= 1 << bit;
    }
}

void GameBeaconSys_ClearNotice(u32 bit) {
    GameBeaconSystem *sys = GameBeaconSys;

    if (sys != NULL) {
        sys->noticeFlags &= 0xff ^ (1 << bit);
    }
}

BOOL GameBeaconSys_CheckNotice(u32 bit) {
    GameBeaconSystem *sys = GameBeaconSys;

    if (sys == NULL) {
        return FALSE;
    }
    return (sys->noticeFlags >> bit) & 1;
}

static BOOL GameBeaconSendSlot_UpdateTimer(GameBeaconSendSlot *slot) {
    if (slot->beacon.type != 1) {
        slot->timer++;
        if (slot->timer > sGameBeaconTypeInfo[slot->beacon.type].lifetime) {
            slot->timer = 0;
            slot->beacon.type = 0;
            slot->beacon.targetVersions = 0xff;
            slot->beacon.passPower = GAME_BEACON_PASS_POWER_NONE;
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL GameBeaconSys_CountSurveyAnswers(GameBeaconSystem *sys, const GameBeacon *beacon) {
    SaveControl *save = GameData_GetSaveControl(sys->gameData);
    void *survey = SurveySave_Get(save);
    TrainerGameInfoSave *info = getTrainerGameInfoAddress(save);
    int i;
    BOOL fromBW1 = FALSE;
    u16 question;
    u32 answer;
    u32 answerCount;
    int j;

    if (beacon->version == VERSION_WHITE || beacon->version == VERSION_BLACK) {
        fromBW1 = TRUE;
    } else if (beacon->targetVersions &
               ((1 << (VERSION_WHITE - VERSION_WHITE)) | (1 << (VERSION_BLACK - VERSION_WHITE)))) {
        return fromBW1;
    }
    for (i = 0; i < 3; i++) {
        question = SurveySave_GetQuestion(survey, i);
        if (question == 0xff) {
            continue;
        }
        if (fromBW1 && SurveyQuestion_IsNewInB2W2(question)) {
            continue;
        }
        if (question == SURVEY_QUESTION_PLAY_TIME) {
            answerCount = GetSurveyAnswerMsgIDCount(question);
            answer = beacon->playHours / 10 + 1;
            if (answer > answerCount) {
                answer = answerCount;
            }
        } else {
            answer = SurveySave_ReadAnswer((void *)beacon->surveyAnswers, question);
        }
        if (answer == 0) {
            continue;
        }
        if (!SurveySave_IsTodayAnswerFull(survey, question, answer)) {
            SurveySave_AddTodayCount(survey, question, 1);
            SurveySave_AddTodayAnswerCount(survey, question, answer, 1);
            sys->surveyUpdated = TRUE;
        }
        for (j = 0; j < 3; j++) {
            u16 myQuestion = TrainerGameInfo_GetSurveyAnswer(info, j);

            if (myQuestion != 0xff && myQuestion == question) {
                TrainerGameInfo_AddSurveyCount(info, j, 1);
            }
        }
    }
    return TRUE;
}

static void GameBeaconSys_AddLog(GameBeaconSystem *sys, const GameBeacon *beacon) {
    RTCTime time;
    GameBeaconLogEntry *entry;

    sys->logNewest++;
    if (sys->logNewest >= GAME_BEACON_LOG_MAX) {
        sys->logNewest = 0;
    }
    entry = &sys->log[sys->logNewest];
    if (sys->logCount > 0 && sys->logNewest == sys->logOldest) {
        sys->logOldest++;
        if (sys->logOldest >= GAME_BEACON_LOG_MAX) {
            sys->logOldest = 0;
        }
    }
    RTC_GetCachedTime(&time);
    MI_CpuFill8(entry, 0, sizeof(GameBeaconLogEntry));
    entry->beacon = *beacon;
    entry->age = 0;
    entry->time.hour = time.hour;
    entry->time.minute = time.minute;
    entry->time.surveyCounted = GameBeaconSys_CountSurveyAnswers(sys, beacon);
    GameBeacon_FilterRecent(&entry->beacon, TRUE);
    sys->newMask |= 1 << sys->logNewest;
    if (sys->logCount < GAME_BEACON_LOG_MAX) {
        sys->logCount++;
    }
}

static BOOL GameBeacon_IsBW1Incompatible(const GameBeacon *beacon, BOOL b2w2Only) {
    if (b2w2Only == TRUE || beacon->targetVersions == 0xfc) {
        return TRUE;
    }
    if (beacon->zoneId >= ZONE_ID_BW1_MAX || beacon->parentZoneId >= ZONE_ID_BW1_MAX) {
        return TRUE;
    }
    if (beacon->passPower != GAME_BEACON_PASS_POWER_NONE && !PassPower_IsBW1Compatible(beacon->passPower)) {
        return TRUE;
    }
    return FALSE;
}

static void GameBeacon_PrepareSend(GameBeacon *beacon, BOOL b2w2Only) {
    int i;

    if (GameBeacon_IsBW1Incompatible(beacon, b2w2Only)) {
        beacon->targetVersions = 0xfc;
        return;
    }
    // Black and White don't have these questions
    for (i = 0; i <= SURVEY_QUESTION_LAST; i++) {
        if (SurveyQuestion_IsNewInB2W2(i)) {
            SurveySave_WriteAnswer(beacon->surveyAnswers, i, 0);
        }
    }
    GameBeacon_FilterRecent(beacon, FALSE);
}

static void GameBeacon_FilterRecent(GameBeacon *beacon, BOOL received) {
    if (received && (beacon->version == VERSION_WHITE2 || beacon->version == VERSION_BLACK2)) {
        return;
    }
    if (beacon->recentKind >= 2 && beacon->recentKind <= 4) {
        GameBeacon_ClearRecent(beacon);
        beacon->recentValue = 0;
    }
}

static void GameBeaconSys_CheckMission(GameBeaconSystem *sys, const GameBeacon *beacon) {
    LinkFestival *festival;
    u32 status;
    int result;
    u32 value;
    FestMissionEntry entry;

    if (sys->gsys == NULL) {
        return;
    }
    festival = GSYS_GetLinkFestival(sys->gsys);
    status = getStatusOfFesMission(festival);
    if (status != 0) {
        FestMissionPassPowers_AddBeacon(LinkFestival_GetPassPowers(festival), beacon);
    }
    if (!GameBeacon_IsMissionType(beacon)) {
        return;
    }
    if (FestMission_IsBeaconInvalid(festival, beacon)) {
        return;
    }
    FestMissionOffers_AddBeacon(festival, beacon);
    if (status == 0 || beacon->type == 0x3d) {
        return;
    }
    result = LinkFestival_GetState(festival);
    if (result < 1 || result > 3) {
        return;
    }
    FestMissionEntry_Clear(&entry);
    entry.record.member.trainerId = beacon->trainerId;
    entry.record.member.zoneId = beacon->parentZoneId;
    entry.record.member.score = beacon->arg.mission.missionId;
    FestMissionConfig_FromBeacon(&entry.config, beacon);
    result = LinkFestival_UpdateMember(festival, &entry, &value);
    if (result == 0) {
        return;
    }
    if (result < 4) {
        if (result == 1) {
            FestMissionLog_AddBeacon(festival, beacon, value, 0, 0);
        }
        FestMissionLog_AddBeacon(festival, beacon, value, 1, 0);
    }
    if (result == 3 || result == 5) {
        FestMissionLog_AddBeacon(festival, beacon, value, 3, 0);
    } else {
        FestMissionLog_AddBeacon(festival, beacon, value, 2, 0);
    }
    GFL_StrBufLoadFixedString(sys->nameBuf, beacon->name, 8);
    LinkFestival_UpdateBest(festival, beacon->trainerId, value, sys->nameBuf);
}

static void GameBeaconSys_UpdateNotices(GameBeaconSystem *sys, const GameBeacon *beacon, BOOL isNew) {
    if (sys->gsys == NULL) {
        return;
    }
    if (Survey_IsCurrentDone(sys->gameData)) {
        GameBeaconSys_SetNotice(0);
    }
    if (isNew) {
        GameBeaconSys_SetNotice(1);
    } else if (beacon->type == 0x12 || beacon->type == 0x32) {
        GameBeaconSys_SetNotice(1);
    }
}

BOOL GameBeaconSys_Receive(const GameBeacon *beacon) {
    GameBeaconSystem *sys = GameBeaconSys;
    BOOL isUpdate = FALSE;
    BOOL isSame = FALSE;
    BOOL isAvenueUpdate = FALSE;
    int index;
    int i;
    RTCTime time;

    if (!(beacon->targetVersions & (1 << (GAME_VERSION - VERSION_WHITE)))) {
        return isUpdate;
    }
    if (beacon->type == 0 || beacon->type >= GAME_BEACON_TYPE_MAX) {
        return FALSE;
    }
    if (beacon->type == 0x18 && sys->mine.beacon.trainerId != beacon->payload.message.trainerId) {
        return isUpdate;
    }
    if (GameBeacon_IsInvalid(beacon)) {
        return FALSE;
    }

    index = sys->logOldest;
    for (i = 0; i < sys->logCount; i++) {
        if (beacon->trainerId == sys->log[index].beacon.trainerId) {
            if (beacon->serial == sys->log[index].beacon.serial) {
                isSame = TRUE;
            } else if (beacon->type == 1) {
                isAvenueUpdate = TRUE;
            } else {
                isUpdate = TRUE;
            }
            break;
        }
        index++;
        if (index >= GAME_BEACON_LOG_MAX) {
            index = 0;
        }
    }
    if (isSame) {
        return FALSE;
    }

    if (beacon->type == 1) {
        if (sys->avenueVisitorPending == FALSE && GameBeaconSys_AddAvenueVisitor(sys, beacon, 8)) {
            sys->avenueVisitorPending = TRUE;
        }
    } else if (GameBeacon_IsMissionType(beacon) && beacon->type != 0x3d && sys->avenueVisitorPending == FALSE &&
               GameBeaconSys_AddAvenueVisitor(sys, beacon, 9)) {
        sys->avenueVisitorPending = TRUE;
    }

    if (isAvenueUpdate) {
        if (!sys->log[index].time.surveyCounted) {
            sys->log[index].time.surveyCounted = GameBeaconSys_CountSurveyAnswers(sys, beacon);
        }
        return FALSE;
    }

    if (beacon->type == 0x19) {
        if (beacon->payload.gift.key != 0x2932013a) {
            return FALSE;
        }
    } else if (beacon->type == 0x1a) {
        if (beacon->payload.gift.key != 0x48841dc1) {
            return FALSE;
        }
    } else if (beacon->type == 0x1b) {
        if (beacon->payload.gift.key != 0x701ddc92) {
            return FALSE;
        }
    } else if (beacon->type == 0x32) {
        if (beacon->payload.gift.key != 0xfee48201) {
            return FALSE;
        }
    }

    GameBeaconSys_CheckMission(sys, beacon);
    GameBeaconSys_UpdateNotices(sys, beacon, isUpdate ? FALSE : TRUE);
    if (isUpdate == TRUE) {
        RTC_GetCachedTime(&time);
        sys->log[index].beacon = *beacon;
        sys->log[index].age = 0;
        sys->log[index].time.hour = time.hour;
        sys->log[index].time.minute = time.minute;
        GameBeacon_FilterRecent(&sys->log[index].beacon, TRUE);
        if (!sys->log[index].time.surveyCounted) {
            sys->log[index].time.surveyCounted = GameBeaconSys_CountSurveyAnswers(sys, beacon);
        }
        sys->newMask |= 1 << index;
    } else {
        GameBeaconSys_AddLog(sys, beacon);
        sys->recvCount++;
    }
    return TRUE;
}

static GameBeacon *GameBeaconSys_GetLogEntry(GameBeaconSystem *sys, int index, GameBeaconTime *time) {
    if (sys->logCount == 0 || sys->logCount - index <= 0) {
        time->hour = 0;
        time->minute = 0;
        return NULL;
    }
    *time = sys->log[index].time;
    return &sys->log[index].beacon;
}

GameBeacon *GameBeaconSys_GetLog(int index, GameBeaconTime *time) {
    return GameBeaconSys_GetLogEntry(GameBeaconSys, index, time);
}

static GameBeacon *GameBeaconSys_GetRecentEntry(GameBeaconSystem *sys, u32 n) {
    int index;

    if (sys->recvCount == 0 || sys->recvCount - n == 0) {
        return NULL;
    }
    index = sys->logNewest - (sys->recvCount - 1 - n);
    if (MATH_ABS(index) >= GAME_BEACON_LOG_MAX) {
        return NULL;
    }
    if (index < 0) {
        index += GAME_BEACON_LOG_MAX;
    }
    return &sys->log[index].beacon;
}

GameBeacon *GameBeaconSys_GetRecent(u32 n) {
    return GameBeaconSys_GetRecentEntry(GameBeaconSys, n);
}

u32 GameBeaconSys_GetReceiveCount(void) {
    return GameBeaconSys->recvCount;
}

int GameBeaconSys_GetNextNew(int *index) {
    for (; *index < GAME_BEACON_LOG_MAX; (*index)++) {
        if (GameBeaconSys->newMask & (1 << *index)) {
            (*index)++;
            return *index - 1;
        }
    }
    return GAME_BEACON_LOG_MAX;
}

void GameBeaconSys_ClearNew(int index) {
    GameBeaconSys->newMask &= 0xffffffff ^ (1 << index);
}

BOOL GameBeaconSys_MarkNew(const GameBeacon *beacon) {
    GameBeaconSystem *sys = GameBeaconSys;
    int i;

    for (i = 0; i < GAME_BEACON_LOG_MAX; i++) {
        if (beacon->trainerId == sys->log[i].beacon.trainerId) {
            sys->newMask |= 1 << i;
            return TRUE;
        }
    }
    return FALSE;
}

u16 GameBeaconSys_GetAge(u16 trainerId) {
    GameBeaconSystem *sys = GameBeaconSys;
    int i;

    for (i = 0; i < GAME_BEACON_LOG_MAX; i++) {
        if (sys->log[i].beacon.targetVersions != 0 && trainerId == sys->log[i].beacon.trainerId) {
            return sys->log[i].age;
        }
    }
    return 0xffff;
}

static void GameBeacon_Init(GameBeacon *beacon, GameData *gameData) {
    PlayerInfo *player = GetGameDataPlayerInfo(gameData);
    SaveControl *save = GameData_GetSaveControl(gameData);
    TrainerGameInfoSave *info = getTrainerGameInfoAddress(save);
    void *cgear = CGearPhrases_GetBlock(save);
    OSOwnerInfo owner;
    PMSData cgearRecord;
    MedalBox *medalBox;
    PlayTime *playTime;
    u16 terminator;
    const u16 *str;
    int i;

    OS_GetOwnerInfo(&owner);
    beacon->targetVersions = 0xff;
    beacon->zoneId = PlayerState_GetZoneID(GameData_GetPlayerState(gameData));
    if (GetZoneIsUnionRoom(beacon->zoneId) == TRUE || IsZone150Or151(beacon->zoneId) == TRUE) {
        beacon->zoneId = GameData_GetNextZone(gameData)->zoneId;
    }
    beacon->parentZoneId = TownMap_GetMapZone(gameData, beacon->zoneId);
    if (IsZoneBlackCityOrWhiteForestLobby(beacon->zoneId)) {
        beacon->zoneId = beacon->parentZoneId;
    }
    beacon->passPower = GAME_BEACON_PASS_POWER_NONE;
    beacon->trainerId = getTrainerID(player);
    beacon->favoriteColor = owner.favoriteColor;
    beacon->gender = getTrainerGender(player);
    if (beacon->gender == GENDER_MALE) {
        beacon->trainerView = PlayerInfo_GetUnionView(player);
    } else {
        beacon->trainerView = PlayerInfo_GetUnionView(player) - 8;
    }
    beacon->version = GAME_VERSION;
    beacon->language = GAME_LANGUAGE;
    beacon->country = UnityTowerVisitor_GetCountry(player);
    beacon->region = UnityTowerVisitor_GetProvince(player);
    beacon->surveyRank = TrainerGameInfo_GetSurveyRank(info);
    beacon->thankedCount = TrainerCard_GetThankedCount(info);
    beacon->passerbyCount = TrainerCard_GetPasserbyCount(info);
    medalBox = SaveControl_GetMedalBox(save);
    beacon->favoriteMedal = MedalBox_GetLastMedal(medalBox);
    beacon->medalCount = MedalBox_GetObtainedCount(medalBox, 0);
    playTime = GameData_GetPlayTime(gameData);
    beacon->playHours = PlayTime_GetHours(playTime);
    beacon->playMinutes = PlayTime_GetMinutes(playTime);
    MI_CpuCopy8(SurveySave_GetOwnAnswers(SurveySave_Get(save)), beacon->surveyAnswers, sizeof(beacon->surveyAnswers));
    CGearPhrases_Get(cgear, 0, &cgearRecord);
    GameBeacon_SetMyCGearRecord(&cgearRecord);
    terminator = GFL_StrBufGetTerminator();
    str = GetPlayerName(player);
    for (i = 0; i < 7; i++) {
        beacon->name[i] = str[i];
        if (terminator == str[i]) {
            break;
        }
    }
    str = TrainerGameInfo_GetGreeting(info);
    for (i = 0; i < 8; i++) {
        beacon->greeting[i] = str[i];
        if (terminator == str[i]) {
            break;
        }
    }
    GameBeacon_ClearRecent(beacon);
    beacon->type = 0;
}

static BOOL GameBeaconSys_AddAvenueVisitor(GameBeaconSystem *sys, const GameBeacon *beacon, u32 a2) {
    JoinAvenueSave *joinAvenue;

    if (!GameData_CheckEventsPaused(sys->gameData)) {
        joinAvenue = SaveControl_GetJoinAvenue(GameData_GetSaveControl(sys->gameData));
        JoinAvenueVisitor_InitFromBeacon(GameBeaconSys->avenueWork, beacon, a2);
        JoinAvenue_ResetAddStep(joinAvenue);
        return TRUE;
    }
    return FALSE;
}

static BOOL GameBeaconSys_UpdateAvenueVisitor(GameBeaconSystem *sys) {
    JoinAvenueSave *joinAvenue = SaveControl_GetJoinAvenue(GameData_GetSaveControl(sys->gameData));
    u32 result;

    return JoinAvenue_AddStepwise(joinAvenue, sys->gameData, GameBeaconSys->avenueWork, 0, &result);
}

static void GameBeaconSendSlot_ResetHeader(GameBeaconSendSlot *slot, u8 targetVersions) {
    SaveControl *save = GameData_GetSaveControl(GameBeaconSys->gameData);
    PlayTime *playTime = GameData_GetPlayTime(GameBeaconSys->gameData);
    TrainerGameInfoSave *info = getTrainerGameInfoAddress(save);

    slot->beacon.playHours = PlayTime_GetHours(playTime);
    slot->beacon.playMinutes = PlayTime_GetMinutes(playTime);
    slot->beacon.thankedCount = TrainerCard_GetThankedCount(info);
    slot->beacon.passerbyCount = TrainerCard_GetPasserbyCount(info);
    slot->beacon.targetVersions = targetVersions;
    slot->beacon.missionWord = 0;
    slot->beacon.passPower = GAME_BEACON_PASS_POWER_NONE;
    slot->beacon.serial++;
    slot->timer = 0;
    slot->updated = TRUE;
}

void GameBeaconSendSlot_Reset(GameBeaconSendSlot *slot) {
    GameBeaconSendSlot_ResetHeader(slot, 0xff);
}

void GameBeaconSendSlot_ResetB2W2Only(GameBeaconSendSlot *slot) {
    GameBeaconSendSlot_ResetHeader(slot, 0xfc);
}

void GameBeaconSendSlot_SetMission(GameBeaconSendSlot *slot, u16 type, u32 value) {
    GameBeaconSendSlot_SetMissionEx(slot, type, value, 0);
}

void GameBeaconSendSlot_SetMissionEx(GameBeaconSendSlot *slot, u16 type, uptr value, u32 extra) {
    LinkFestival *festival;
    FestMissionConfig *config;
    GameBeaconSendSlot newSlot;
    GameBeacon *beacon = &newSlot.beacon;

    if (GameBeaconSys->gsys == NULL) {
        return;
    }
    festival = GSYS_GetLinkFestival(GameBeaconSys->gsys);
    config = GetFestMissionCfg(festival);
    newSlot = *slot;
    GameBeaconSendSlot_ResetB2W2Only(&newSlot);
    LinkFestival_FillMissionInfo(festival, &beacon->payload, value);
    if (type == 0x67) {
        GameBeacon_StoreTextN((const StrBuf *)value, beacon->payload.mission.text, 8);
        beacon->arg.mission.unk = extra;
    } else {
        beacon->arg.mission.unk = 0;
    }
    beacon->type = type;
    beacon->arg.mission.missionId = config->missionId;
    beacon->missionWord = config->seed;
    GameBeacon_ClearRecent(beacon);
    if (type != 0x3c) {
        FestMissionLog_AddBeacon(festival, &newSlot.beacon, LinkFestival_GetScore(festival), 2, 1);
    }
    if (GameBeaconSys_CanSendType(type) == TRUE) {
        *slot = newSlot;
    }
}

BOOL GameBeacon_IsMissionType(const GameBeacon *beacon) {
    if (beacon->type < 0x3c || beacon->type > 0x67) {
        return FALSE;
    }
    return TRUE;
}

BOOL GameBeacon_HasMissionInfo(const GameBeacon *beacon) {
    if (beacon->type < 0x3c || beacon->type > 0x66) {
        return FALSE;
    }
    return TRUE;
}

u32 GameBeaconSys_AddMissionScore(u32 a0, u16 a1) {
    if (GameBeaconSys == NULL || GameBeaconSys->gsys == NULL) {
        return 0;
    }
    if (getStatusOfFesMission(GSYS_GetLinkFestival(GameBeaconSys->gsys)) != 0) {
        return LinkFestival_AddMissionScore(GameBeaconSys->gsys, a0, a1) + 1;
    }
    return 0;
}

BOOL GameBeaconSys_CanSendType(u16 type) {
    if (sGameBeaconTypeInfo[type].priority >= sGameBeaconTypeInfo[GameBeaconSys->mine.beacon.type].priority) {
        return TRUE;
    }
    return FALSE;
}

BOOL GameBeacon_IsSpecialSpecies(u16 species) {
    u32 i;

    for (i = 0; i < NELEMS(sSpecialSpecies); i++) {
        if (species == sSpecialSpecies[i]) {
            return TRUE;
        }
    }
    return FALSE;
}

static void GameBeacon_StoreTextN(const StrBuf *str, u16 *dest, u32 length) {
    u16 buf[GAME_BEACON_TEXT_LEN + 1];
    u16 terminator = GFL_StrBufGetTerminator();
    u32 i;

    MIi_CpuClear16(terminator, dest, length * 2);
    GFL_StrBufStoreString(str, buf, length + 1);
    for (i = 0; i < length; i++) {
        dest[i] = buf[i];
        if (terminator == buf[i]) {
            break;
        }
    }
}

void GameBeacon_StoreText(const StrBuf *str, u16 *dest) {
    GameBeacon_StoreTextN(str, dest, GAME_BEACON_TEXT_LEN);
}

u8 GameBeaconSys_PopSurveyUpdated(void) {
    GameBeaconSystem *sys = GameBeaconSys;
    u8 updated;

    if (sys == NULL) {
        return FALSE;
    }
    updated = sys->surveyUpdated;
    sys->surveyUpdated = FALSE;
    return updated;
}

void GameBeaconSys_SetSurveyAnswers(const void *answers) {
    GameBeaconSendSlot *slot = &GameBeaconSys->mine;

    MI_CpuCopy8(answers, slot->beacon.surveyAnswers, sizeof(slot->beacon.surveyAnswers));
    slot->updated = TRUE;
}

void GameBeaconSys_SetCountryRegion(u8 country, u8 region) {
    GameBeaconSendSlot *slot = &GameBeaconSys->mine;

    slot->beacon.country = country;
    slot->beacon.region = region;
    slot->updated = TRUE;
}

void GameBeaconSys_SetSurveyRank(u8 rank) {
    GameBeaconSendSlot *slot = &GameBeaconSys->mine;

    slot->beacon.surveyRank = rank;
    slot->updated = TRUE;
}

void GameBeaconSys_SetCGearRecord(const PMSData *record) {
    GameBeaconSendSlot *slot = &GameBeaconSys->mine;

    GameBeacon_SetMyCGearRecord(record);
    slot->updated = TRUE;
}

void GameBeaconSys_SetTrainerView(u32 trainerView) {
    GameBeaconSendSlot *slot = &GameBeaconSys->mine;

    slot->beacon.trainerView = trainerView;
    slot->updated = TRUE;
}

void GameBeaconSys_UpdateGreeting(void) {
    GameBeaconSendSlot *slot = &GameBeaconSys->mine;
    TrainerGameInfoSave *info = getTrainerGameInfoAddress(GameData_GetSaveControl(GameBeaconSys->gameData));
    u16 terminator = GFL_StrBufGetTerminator();
    const u16 *greeting = TrainerGameInfo_GetGreeting(info);
    int i;

    for (i = 0; i < 8; i++) {
        slot->beacon.greeting[i] = greeting[i];
        if (terminator == greeting[i]) {
            break;
        }
    }
    slot->updated = TRUE;
}

void GameBeaconSys_SetMedalCount(u8 count) {
    GameBeaconSendSlot *slot = &GameBeaconSys->mine;

    slot->beacon.medalCount = count;
    slot->updated = TRUE;
}

void GameBeaconSys_SetFavoriteMedal(u32 value) {
    GameBeaconSendSlot *slot = &GameBeaconSys->mine;

    slot->beacon.favoriteMedal = value;
    slot->updated = TRUE;
}

void GameBeaconSys_SendCapture(u16 species, BOOL a1, BOOL a2) {
    if (GameBeacon_IsSpecialSpecies(species)) {
        GameBeaconSys_SendCaptureSpecial(species);
        return;
    }
    if (!GameBeaconSys_SendCaptureMission(species, a1, a2)) {
        GameBeaconSys_SendCaptureWild(species);
    }
}

static void GameBeaconSys_SendCaptureWild(u16 species) {
    if (GameBeaconSys_CanSendType(0xe)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_SetCaptureWild(&GameBeaconSys->mine.beacon, species);
    }
}

static void GameBeacon_SetCaptureWild(GameBeacon *beacon, u16 species) {
    beacon->type = 0xe;
    beacon->arg.value = species;
    GameBeacon_ClearRecent(beacon);
}

static void GameBeaconSys_SendCaptureSpecial(u16 species) {
    if (GameBeaconSys_CanSendType(0xf)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_SetCaptureSpecial(&GameBeaconSys->mine.beacon, species);
    }
}

static void GameBeacon_SetCaptureSpecial(GameBeacon *beacon, u16 species) {
    beacon->type = 0xf;
    beacon->arg.value = species;
    GameBeacon_ClearRecent(beacon);
}

static BOOL GameBeaconSys_SendCaptureMission(u16 species, BOOL a1, BOOL a2) {
    u16 type;
    u32 mission;

    if (a1) {
        type = 0x4e;
        mission = 0x14;
    } else if (a2) {
        type = 0x50;
        mission = 0x16;
    } else {
        type = 0x40;
        mission = 6;
    }
    if (GameBeaconSys_AddMissionScore(mission, species)) {
        GameBeaconSendSlot_SetMission(&GameBeaconSys->mine, type, species);
        return TRUE;
    }
    return FALSE;
}

void GameBeaconSys_SendDefeatMission(u16 species, BOOL a1, BOOL a2) {
    u16 type;
    u32 mission;

    if (a1) {
        type = 0x4d;
        mission = 0x13;
    } else if (a2) {
        type = 0x4f;
        mission = 0x15;
    } else {
        type = 0x3f;
        mission = 5;
    }
    if (GameBeaconSys_AddMissionScore(mission, species)) {
        GameBeaconSendSlot_SetMission(&GameBeaconSys->mine, type, species);
    }
}

void GameBeaconSys_SendNickname(const StrBuf *nickname) {
    if (GameBeaconSys_CanSendType(0x10)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_SetType10(&GameBeaconSys->mine.beacon, nickname);
    }
}

static void GameBeacon_SetType10(GameBeacon *beacon, const StrBuf *nickname) {
    beacon->type = 0x10;
    GameBeacon_StoreText(nickname, beacon->payload.text);
}

void GameBeaconSys_SendEvolution(u16 species, const StrBuf *nickname) {
    if (GameBeaconSys_CanSendType(0x11)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_SetEvolution(&GameBeaconSys->mine.beacon, species, nickname);
    }
}

static void GameBeacon_SetEvolution(GameBeacon *beacon, u16 species, const StrBuf *nickname) {
    beacon->type = 0x11;
    beacon->arg.value = species;
    GameBeacon_StoreText(nickname, beacon->payload.text);
    GameBeacon_ClearRecent(beacon);
}

static void GameBeaconSys_SendPlayTime(u16 hours) {
    if (GameBeaconSys_CanSendType(0x14)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_SetPlayTime(&GameBeaconSys->mine.beacon, hours);
    }
}

static void GameBeacon_SetPlayTime(GameBeacon *beacon, u16 hours) {
    beacon->type = 0x14;
    beacon->payload.value = hours;
    GameBeacon_ClearRecent(beacon);
}

void GameBeaconSys_SendItemUse(u16 item) {
    if (GameBeaconSys_AddMissionScore(0xb, item)) {
        GameBeaconSendSlot_SetMission(&GameBeaconSys->mine, 0x45, item);
        return;
    }
    if (GameBeaconSys_CanSendType(0x23)) {
        if (PML_ItemIsB2W2Only(item)) {
            GameBeaconSendSlot_ResetB2W2Only(&GameBeaconSys->mine);
        } else {
            GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        }
        GameBeacon_SetItem(&GameBeaconSys->mine.beacon, item);
    }
}

static void GameBeacon_SetItem(GameBeacon *beacon, u16 item) {
    beacon->type = 0x23;
    beacon->payload.value = item;
    GameBeacon_ClearRecent(beacon);
}

void GameBeacon_SetZone(u16 zoneId, GameData *gameData) {
    if (!IsZoneFlashbackMemoryPostFX(zoneId)) {
        GameBeacon_UpdateZone(&GameBeaconSys->mine.beacon, zoneId, gameData);
        GameBeaconSys->mine.updated = TRUE;
    }
}

static void GameBeacon_UpdateZone(GameBeacon *beacon, u16 zoneId, GameData *gameData) {
    LinkFestival *festival;

    if (IsZoneBlackCityOrWhiteForestLobby(zoneId)) {
        beacon->zoneId = TownMap_GetMapZone(gameData, zoneId);
    } else {
        beacon->zoneId = zoneId;
    }
    beacon->parentZoneId = TownMap_GetMapZone(gameData, zoneId);
    beacon->recentKind = GAME_BEACON_RECENT_NONE;
    GameBeacon_ClearRecent(beacon);
    festival = GSYS_GetLinkFestival(GameBeaconSys->gsys);
    if (getStatusOfFesMission(festival) && LinkFestival_IsNewZoneFromOutside(festival, beacon->parentZoneId)) {
        FestMissionLog_AddBeacon(festival, beacon, LinkFestival_GetScore(festival), 1, 1);
    }
}

void GameBeacon_SetRecentKind0(GameBeacon *beacon, u16 species) {
    beacon->recentKind = 0;
    beacon->recentValue = species;
}

void GameBeacon_SetRecentKind1(GameBeacon *beacon, u16 species) {
    beacon->recentKind = 1;
    beacon->recentValue = species;
}

void GameBeacon_SetRecentKind2(GameBeacon *beacon, u16 item) {
    beacon->recentKind = 2;
    beacon->recentValue = item;
}

void GameBeacon_SetRecentKind3(GameBeacon *beacon, u16 item) {
    beacon->recentKind = 3;
    beacon->recentValue = item;
}

void GameBeacon_SetRecentKind4(GameBeacon *beacon, u16 item) {
    beacon->recentKind = 4;
    beacon->recentValue = item;
}

void GameBeacon_ClearRecent(GameBeacon *beacon) {
    beacon->recentKind = GAME_BEACON_RECENT_NONE;
}

// Sets the game's own beacon, whichever beacon it is given; its only caller gives that one
static void GameBeacon_SetCGearRecordFields(GameBeacon *beacon, const PMSData *record) {
    GameBeacon *mine = &GameBeaconSys->mine.beacon;

    mine->cgear0 = record->type;
    mine->cgear1 = record->id;
    mine->cgear2 = record->words[0];
    mine->cgear3 = record->words[1];
}

static void GameBeacon_SetMyCGearRecord(const PMSData *record) {
    GameBeacon_SetCGearRecordFields(&GameBeaconSys->mine.beacon, record);
}

void GameBeaconSys_SendMission(void) {
    u16 type = 0x3c;

    if (GameBeaconSys_CanSendType(type)) {
        GameBeaconSendSlot_SetMission(&GameBeaconSys->mine, type, 0);
    }
}

static void GameBeaconSys_SetEnabled(u8 enabled) {
    GameBeaconSystem *sys = GameBeaconSys;

    if (sys != NULL) {
        sys->enabled = enabled;
    }
}

void GameBeaconSys_SetAvenuePeople(JoinAvenuePersonList *list) {
    GameBeaconSystem *sys = GameBeaconSys;

    if (sys != NULL) {
        sys->avenuePeople = list;
    }
}

static void GameBeaconSys_SendAvenueAd(GameBeaconSystem *sys) {
    void *work = JoinAvenueVisitor_Create(HEAPID_TAIL(HEAPID_USER));

    JoinAvenueVisitor_InitPlayer(work, sys->avenueAdIndex, sys->gameData);
    JoinAvenueVisitor_StoreInBeacon(work, &sys->mine.beacon);
    GameBeaconSendSlot_Reset(&sys->mine);
    JoinAvenueVisitor_Free(work);
    if (sys->mine.beacon.type == 0) {
        sys->mine.beacon.type = 1;
    }
}

static BOOL GameBeaconSys_UpdateAvenueAdTimer(GameBeaconSystem *sys) {
    if (sys->avenueAdTimer++ > sAvenueAdIntervals[sys->avenueAdIndex]) {
        sys->avenueAdTimer = 0;
        sys->avenueAdIndex++;
        sys->avenueAdIndex %= 7;
        return TRUE;
    }
    return FALSE;
}

void GameBeaconSys_CheckGenuineCard(void) {
    BOOL ok;

    GFL_OvlLoad(OVERLAY_ID(338));
    ok = IrCheck_IsGenuineCard();
    GFL_OvlUnload(OVERLAY_ID(338));
    if (!ok) {
        GFL_HBlankTCBAdd(GameBeaconSys_HideNewHBlank, NULL, 3);
    }
}

static void GameBeaconSys_HideNewHBlank(TCB *tcb, void *data) {
    if (GameBeaconSys != NULL) {
        GameBeaconSys->newMask = 0;
        GameBeaconSys->mine.updated = FALSE;
        GameBeaconSys->newMask = 0;
    }
}
