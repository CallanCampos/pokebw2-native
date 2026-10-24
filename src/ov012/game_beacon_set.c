#include "types.h"
#include "app/name_entry.h"
#include "field/game_beacon_set.h"
#include "gfl/str.h"
#include "save/high_link.h"
#include "save/save_control.h"
#include "system/game_comm.h"
#include "system/game_data.h"
#include "system/str_tool.h"

static void GameBeacon_TryWildBattleStart(u16 value);
static void GameBeacon_FillWildBattleStart(GameBeacon *info, u16 value);
static void GameBeacon_TryWildBattleWin(u16 value);
static void GameBeacon_FillWildBattleWin(GameBeacon *info, u16 value);
static void GameBeacon_TrySpecialBattleStart(u16 value);
static void GameBeacon_FillSpecialBattleStart(GameBeacon *info, u16 value);
static void GameBeacon_TrySpecialBattleWin(u16 value);
static void GameBeacon_FillSpecialBattleWin(GameBeacon *info, u16 value);
static void GameBeacon_FillTrainerBattleStart(GameBeacon *info, u16 itemId);
static void GameBeacon_FillTrainerBattleWin(GameBeacon *info, u16 itemId);
static void GameBeacon_FillLeaderBattleStart(GameBeacon *info, u16 itemId);
static void GameBeacon_FillLeaderBattleWin(GameBeacon *info, u16 itemId);
static void GameBeacon_FillSpecialTrainerBattleStart(GameBeacon *info, u16 itemId);
static void GameBeacon_FillSpecialTrainerBattleWin(GameBeacon *info, u16 itemId);
static void GameBeacon_FillPassPower(GameBeacon *info, u32 passPower);
static void GameBeacon_FillFoundItem(GameBeacon *info, u16 value);
static void GameBeacon_FillPokedexComplete(GameBeacon *info);
static void GameBeacon_FillThankedCount(GameBeacon *info, u32 value);
static void GameBeacon_FillUnionRoomReception(GameBeacon *info);
static void GameBeacon_FillBattleEscape(GameBeacon *info);
static void GameBeacon_FillLowHP(GameBeacon *info, const StrBuf *name);
static void GameBeacon_FillLowPP(GameBeacon *info, const StrBuf *name);
static void GameBeacon_FillFainted(GameBeacon *info, const StrBuf *name);
static void GameBeacon_FillStatusCondition(GameBeacon *info, const StrBuf *name);
static void GameBeacon_FillFieldMove(GameBeacon *info, u16 value);
static void GameBeacon_FillEggReceived(GameBeacon *info);
static void GameBeacon_FillEggHatched(GameBeacon *info, u16 species);
static void GameBeacon_FillShopping(GameBeacon *info);
static void GameBeacon_FillBattleSubwayChallenge(GameBeacon *info);
static void GameBeacon_FillBattleSubwayWinStreak(GameBeacon *info, u32 value);
static void GameBeacon_FillBattleSubwayTrophy(GameBeacon *info);
static void GameBeacon_FillBattleInstituteChallenge(GameBeacon *info);
static void GameBeacon_FillBattleInstituteRank(GameBeacon *info, u8 value);
static void GameBeacon_FillFerrisWheel(GameBeacon *info);
static void GameBeacon_FillPokeTransfer(GameBeacon *info);
static void GameBeacon_FillMusical(GameBeacon *info, const StrBuf *name);
static void GameBeacon_FillReceivedPassPower(GameBeacon *info, u32 passPower);
static void GameBeacon_FillThanks(GameBeacon *info, GameData *gameData, u16 value);
static void GameBeacon_FillPhrase(GameBeacon *info, const StrBuf *name, u8 value);
static void GameBeacon_FillPokestar(GameBeacon *info, u8 value, u16 type);
static void GameBeacon_FillTournament(GameBeacon *info, u16 value, u8 extra, u32 type);
static void GameBeacon_FillJoinAvenueShopping(GameBeacon *info);
static void GameBeacon_FillArea(GameBeacon *info, u8 extra, u16 value, u16 type);

static const u8 data_ov012_0216d718[5] = { 10, 30, 50, 100, 200 };

void GameBeacon_BroadcastWildBattleStart(u16 value) {
    if (GameBeacon_IsSpecialSpecies(value)) {
        GameBeacon_TrySpecialBattleStart(value);
    } else if (GameBeaconSys_AddMissionScore(4, value)) {
        GameBeaconSendSlot_SetMission(&GameBeaconSys->mine, 0x3e, value);
    } else {
        GameBeacon_TryWildBattleStart(value);
    }
}

void GameBeacon_BroadcastWildBattleWin(u16 value) {
    if (GameBeacon_IsSpecialSpecies(value)) {
        GameBeacon_TrySpecialBattleWin(value);
    } else {
        GameBeacon_TryWildBattleWin(value);
    }
}

static void GameBeacon_TryWildBattleStart(u16 value) {
    if (GameBeaconSys_CanSendType(2)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_FillWildBattleStart(&GameBeaconSys->mine.beacon, value);
    }
}

static void GameBeacon_FillWildBattleStart(GameBeacon *info, u16 value) {
    info->type = 2;
    info->arg.value = value;
    GameBeacon_SetRecentKind0(info, value);
}

static void GameBeacon_TryWildBattleWin(u16 value) {
    if (GameBeaconSys_CanSendType(3)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_FillWildBattleWin(&GameBeaconSys->mine.beacon, value);
    }
}

static void GameBeacon_FillWildBattleWin(GameBeacon *info, u16 value) {
    info->type = 3;
    info->arg.value = value;
    GameBeacon_ClearRecent(info);
}

static void GameBeacon_TrySpecialBattleStart(u16 value) {
    if (GameBeaconSys_CanSendType(4)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_FillSpecialBattleStart(&GameBeaconSys->mine.beacon, value);
    }
}

static void GameBeacon_FillSpecialBattleStart(GameBeacon *info, u16 value) {
    info->type = 4;
    info->arg.value = value;
    GameBeacon_SetRecentKind1(info, value);
}

static void GameBeacon_TrySpecialBattleWin(u16 value) {
    if (GameBeaconSys_CanSendType(5)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_FillSpecialBattleWin(&GameBeaconSys->mine.beacon, value);
    }
}

static void GameBeacon_FillSpecialBattleWin(GameBeacon *info, u16 value) {
    info->type = 5;
    info->arg.value = value;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastTrainerBattleStart(u16 itemId) {
    if (itemId < 0x32e) {
        if (GameBeaconSys_AddMissionScore(8, itemId)) {
            GameBeaconSendSlot_SetMission(&GameBeaconSys->mine, 0x42, itemId);
        } else if (GameBeaconSys_CanSendType(6)) {
            GameBeaconSendSlot_ResetB2W2Only(&GameBeaconSys->mine);
            GameBeacon_FillTrainerBattleStart(&GameBeaconSys->mine.beacon, itemId);
        }
    }
}

static void GameBeacon_FillTrainerBattleStart(GameBeacon *info, u16 itemId) {
    info->type = 6;
    info->payload.value = itemId;
    GameBeacon_SetRecentKind2(info, itemId);
}

void GameBeacon_BroadcastTrainerBattleWin(u16 itemId) {
    if (itemId < 0x32e) {
        if (GameBeaconSys_AddMissionScore(9, itemId)) {
            GameBeaconSendSlot_SetMission(&GameBeaconSys->mine, 0x43, itemId);
        } else if (GameBeaconSys_CanSendType(7)) {
            GameBeaconSendSlot_ResetB2W2Only(&GameBeaconSys->mine);
            GameBeacon_FillTrainerBattleWin(&GameBeaconSys->mine.beacon, itemId);
        }
    }
}

static void GameBeacon_FillTrainerBattleWin(GameBeacon *info, u16 itemId) {
    info->type = 7;
    info->payload.value = itemId;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastLeaderBattleStart(u16 itemId) {
    if (itemId < 0x32e) {
        if (GameBeaconSys_AddMissionScore(8, itemId)) {
            GameBeaconSendSlot_SetMission(&GameBeaconSys->mine, 0x42, itemId);
        } else if (GameBeaconSys_CanSendType(8)) {
            GameBeaconSendSlot_ResetB2W2Only(&GameBeaconSys->mine);
            GameBeacon_FillLeaderBattleStart(&GameBeaconSys->mine.beacon, itemId);
        }
    }
}

static void GameBeacon_FillLeaderBattleStart(GameBeacon *info, u16 itemId) {
    info->type = 8;
    info->payload.value = itemId;
    GameBeacon_SetRecentKind3(info, itemId);
}

void GameBeacon_BroadcastLeaderBattleWin(u16 itemId) {
    if (itemId < 0x32e) {
        if (GameBeaconSys_AddMissionScore(9, itemId)) {
            GameBeaconSendSlot_SetMission(&GameBeaconSys->mine, 0x43, itemId);
        } else if (GameBeaconSys_CanSendType(9)) {
            GameBeaconSendSlot_ResetB2W2Only(&GameBeaconSys->mine);
            GameBeacon_FillLeaderBattleWin(&GameBeaconSys->mine.beacon, itemId);
        }
    }
}

static void GameBeacon_FillLeaderBattleWin(GameBeacon *info, u16 itemId) {
    info->type = 9;
    info->payload.value = itemId;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastSpecialTrainerBattleStart(u16 itemId) {
    if (itemId < 0x32e) {
        if (GameBeaconSys_AddMissionScore(8, itemId)) {
            GameBeaconSendSlot_SetMission(&GameBeaconSys->mine, 0x42, itemId);
        } else if (GameBeaconSys_CanSendType(10)) {
            GameBeaconSendSlot_ResetB2W2Only(&GameBeaconSys->mine);
            GameBeacon_FillSpecialTrainerBattleStart(&GameBeaconSys->mine.beacon, itemId);
        }
    }
}

static void GameBeacon_FillSpecialTrainerBattleStart(GameBeacon *info, u16 itemId) {
    info->type = 10;
    info->payload.value = itemId;
    GameBeacon_SetRecentKind4(info, itemId);
}

void GameBeacon_BroadcastSpecialTrainerBattleWin(u16 itemId) {
    if (itemId < 0x32e) {
        if (GameBeaconSys_AddMissionScore(9, itemId)) {
            GameBeaconSendSlot_SetMission(&GameBeaconSys->mine, 0x43, itemId);
        } else if (GameBeaconSys_CanSendType(11)) {
            GameBeaconSendSlot_ResetB2W2Only(&GameBeaconSys->mine);
            GameBeacon_FillSpecialTrainerBattleWin(&GameBeaconSys->mine.beacon, itemId);
        }
    }
}

static void GameBeacon_FillSpecialTrainerBattleWin(GameBeacon *info, u16 itemId) {
    info->type = 11;
    info->payload.value = itemId;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastPassPower(u32 passPower) {
    if (GameBeaconSys_AddMissionScore(15, passPower)) {
        GameBeaconSendSlot_SetMission(&GameBeaconSys->mine, 0x49, passPower);
    } else if (GameBeaconSys_CanSendType(0x12)) {
        if (PassPower_IsBW1Compatible(passPower)) {
            GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        } else {
            GameBeaconSendSlot_ResetB2W2Only(&GameBeaconSys->mine);
        }
        GameBeacon_FillPassPower(&GameBeaconSys->mine.beacon, passPower);
    }
}

static void GameBeacon_FillPassPower(GameBeacon *info, u32 passPower) {
    info->type = 0x12;
    info->passPower = passPower;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastFoundItem(u16 value) {
    if (GameBeaconSys_AddMissionScore(12, value)) {
        GameBeaconSendSlot_SetMission(&GameBeaconSys->mine, 0x46, value);
    } else if (GameBeaconSys_CanSendType(0x13)) {
        if (PML_ItemIsB2W2Only(value)) {
            GameBeaconSendSlot_ResetB2W2Only(&GameBeaconSys->mine);
        } else {
            GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        }
        GameBeacon_FillFoundItem(&GameBeaconSys->mine.beacon, value);
    }
}

static void GameBeacon_FillFoundItem(GameBeacon *info, u16 value) {
    info->type = 0x13;
    info->payload.value = value;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastPokedexComplete(void) {
    if (GameBeaconSys_CanSendType(0x15)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_FillPokedexComplete(&GameBeaconSys->mine.beacon);
    }
}

static void GameBeacon_FillPokedexComplete(GameBeacon *info) {
    info->type = 0x15;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastThankedCount(u32 value) {
    u32 i;

    if (GameBeaconSys_CanSendType(0x16)) {
        for (i = 0; i < NELEMS(data_ov012_0216d718); i++) {
            if (value == data_ov012_0216d718[i]) {
                GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
                GameBeacon_FillThankedCount(&GameBeaconSys->mine.beacon, value);
                return;
            }
        }
    }
}

static void GameBeacon_FillThankedCount(GameBeacon *info, u32 value) {
    info->type = 0x16;
    info->payload.value32 = value;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastUnionRoomReception(void) {
    if (GameBeaconSys_CanSendType(0x17)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_FillUnionRoomReception(&GameBeaconSys->mine.beacon);
    }
}

static void GameBeacon_FillUnionRoomReception(GameBeacon *info) {
    info->type = 0x17;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastBattleEscape(u16 value) {
    if (GameBeaconSys_AddMissionScore(7, value)) {
        GameBeaconSendSlot_SetMission(&GameBeaconSys->mine, 0x41, value);
    } else if (GameBeaconSys_CanSendType(0x1e)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_FillBattleEscape(&GameBeaconSys->mine.beacon);
    }
}

static void GameBeacon_FillBattleEscape(GameBeacon *info) {
    info->type = 0x1e;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastLowHP(u16 value, const StrBuf *name) {
    if (GameBeaconSys_AddMissionScore(0x2b, value)) {
        GameBeaconSendSlot_SetMission(&GameBeaconSys->mine, 0x65, value);
    } else if (GameBeaconSys_CanSendType(0x1f)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_FillLowHP(&GameBeaconSys->mine.beacon, name);
    }
}

static void GameBeacon_FillLowHP(GameBeacon *info, const StrBuf *name) {
    info->type = 0x1f;
    GameBeacon_StoreText(name, info->payload.text);
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastLowPP(u16 value, const StrBuf *name) {
    if (GameBeaconSys_AddMissionScore(0x2c, value)) {
        GameBeaconSendSlot_SetMission(&GameBeaconSys->mine, 0x66, value);
    } else if (GameBeaconSys_CanSendType(0x20)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_FillLowPP(&GameBeaconSys->mine.beacon, name);
    }
}

static void GameBeacon_FillLowPP(GameBeacon *info, const StrBuf *name) {
    info->type = 0x20;
    GameBeacon_StoreText(name, info->payload.text);
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastFainted(u16 value, const StrBuf *name) {
    if (GameBeaconSys_AddMissionScore(0x2a, value)) {
        GameBeaconSendSlot_SetMission(&GameBeaconSys->mine, 0x64, value);
    } else if (GameBeaconSys_CanSendType(0x21)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_FillFainted(&GameBeaconSys->mine.beacon, name);
    }
}

static void GameBeacon_FillFainted(GameBeacon *info, const StrBuf *name) {
    info->type = 0x21;
    GameBeacon_StoreText(name, info->payload.text);
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastStatusCondition(const StrBuf *name) {
    if (GameBeaconSys_CanSendType(0x22)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_FillStatusCondition(&GameBeaconSys->mine.beacon, name);
    }
}

static void GameBeacon_FillStatusCondition(GameBeacon *info, const StrBuf *name) {
    info->type = 0x22;
    GameBeacon_StoreText(name, info->payload.text);
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastFieldMove(u16 value) {
    if (GameBeaconSys_AddMissionScore(13, value)) {
        GameBeaconSendSlot_SetMission(&GameBeaconSys->mine, 0x47, value);
    } else if (GameBeaconSys_CanSendType(0x24)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_FillFieldMove(&GameBeaconSys->mine.beacon, value);
    }
}

static void GameBeacon_FillFieldMove(GameBeacon *info, u16 value) {
    info->type = 0x24;
    info->payload.value = value;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastEggReceived(void) {
    if (GameBeaconSys_AddMissionScore(0x12, 0)) {
        GameBeaconSendSlot_SetMission(&GameBeaconSys->mine, 0x4c, 0);
    } else if (GameBeaconSys_CanSendType(0x25)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_FillEggReceived(&GameBeaconSys->mine.beacon);
    }
}

static void GameBeacon_FillEggReceived(GameBeacon *info) {
    info->type = 0x25;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastEggHatched(u16 species) {
    if (GameBeaconSys_AddMissionScore(0x1b, species)) {
        GameBeaconSendSlot_SetMission(&GameBeaconSys->mine, 0x55, species);
    } else if (GameBeaconSys_CanSendType(0x26)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_FillEggHatched(&GameBeaconSys->mine.beacon, species);
    }
}

static void GameBeacon_FillEggHatched(GameBeacon *info, u16 species) {
    info->type = 0x26;
    info->arg.value = species;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastShopping(void) {
    if (GameBeaconSys_AddMissionScore(14, 0)) {
        GameBeaconSendSlot_SetMission(&GameBeaconSys->mine, 0x48, 0);
    } else if (GameBeaconSys_CanSendType(0x27)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_FillShopping(&GameBeaconSys->mine.beacon);
    }
}

static void GameBeacon_FillShopping(GameBeacon *info) {
    info->type = 0x27;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastBattleSubwayChallenge(void) {
    if (GameBeaconSys_CanSendType(0x28)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_FillBattleSubwayChallenge(&GameBeaconSys->mine.beacon);
    }
}

static void GameBeacon_FillBattleSubwayChallenge(GameBeacon *info) {
    info->type = 0x28;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastBattleSubwayWinStreak(u32 value) {
    if (GameBeaconSys_CanSendType(0x29)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_FillBattleSubwayWinStreak(&GameBeaconSys->mine.beacon, value);
    }
}

static void GameBeacon_FillBattleSubwayWinStreak(GameBeacon *info, u32 value) {
    info->type = 0x29;
    info->payload.value32 = value;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastBattleSubwayTrophy(void) {
    if (GameBeaconSys_CanSendType(0x2a)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_FillBattleSubwayTrophy(&GameBeaconSys->mine.beacon);
    }
}

static void GameBeacon_FillBattleSubwayTrophy(GameBeacon *info) {
    info->type = 0x2a;
    info->payload.value32 = 0;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastBattleInstituteChallenge(void) {
    if (GameBeaconSys_CanSendType(0x2b)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_FillBattleInstituteChallenge(&GameBeaconSys->mine.beacon);
    }
}

static void GameBeacon_FillBattleInstituteChallenge(GameBeacon *info) {
    info->type = 0x2b;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastBattleInstituteRank(u8 value) {
    if (GameBeaconSys_CanSendType(0x2c)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_FillBattleInstituteRank(&GameBeaconSys->mine.beacon, value);
    }
}

static void GameBeacon_FillBattleInstituteRank(GameBeacon *info, u8 value) {
    info->type = 0x2c;
    info->payload.value8 = value;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastFerrisWheel(void) {
    if (GameBeaconSys_AddMissionScore(16, 0)) {
        GameBeaconSendSlot_SetMission(&GameBeaconSys->mine, 0x4a, 0);
    } else if (GameBeaconSys_CanSendType(0x2d)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_FillFerrisWheel(&GameBeaconSys->mine.beacon);
    }
}

static void GameBeacon_FillFerrisWheel(GameBeacon *info) {
    info->type = 0x2d;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastPokeTransfer(void) {
    if (GameBeaconSys_CanSendType(0x2e)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_FillPokeTransfer(&GameBeaconSys->mine.beacon);
    }
}

static void GameBeacon_FillPokeTransfer(GameBeacon *info) {
    info->type = 0x2e;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastMusical(const StrBuf *name) {
    if (GameBeaconSys_CanSendType(0x2f)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_FillMusical(&GameBeaconSys->mine.beacon, name);
    }
}

static void GameBeacon_FillMusical(GameBeacon *info, const StrBuf *name) {
    info->type = 0x2f;
    GameBeacon_StoreText(name, info->payload.text);
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastReceivedPassPower(u32 passPower) {
    if (GameBeaconSys_CanSendType(0x30)) {
        if (PassPower_IsBW1Compatible(passPower)) {
            GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        } else {
            GameBeaconSendSlot_ResetB2W2Only(&GameBeaconSys->mine);
        }
        GameBeacon_FillReceivedPassPower(&GameBeaconSys->mine.beacon, passPower);
    }
}

static void GameBeacon_FillReceivedPassPower(GameBeacon *info, u32 passPower) {
    info->type = 0x30;
    info->payload.value = passPower;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastThanks(GameData *gameData, u16 value) {
    if (GameBeaconSys_CanSendType(0x18)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_FillThanks(&GameBeaconSys->mine.beacon, gameData, value);
    }
}

static void GameBeacon_FillThanks(GameBeacon *info, GameData *gameData, u16 value) {
    wcharsncpy(TrainerGameInfo_GetThanks(getTrainerGameInfoAddress(GameData_GetSaveControl(gameData))),
               info->payload.named.name, 9);
    info->type = 0x18;
    info->payload.named.value = value;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastPhrase(StrBuf *name, u8 value) {
    if (GameBeaconSys_AddMissionScore(0x2d, 0)) {
        GameBeaconSendSlot_SetMissionEx(&GameBeaconSys->mine, 0x67, (uptr)name, value);
    } else if (GameBeaconSys_CanSendType(0x31)) {
        GameBeaconSendSlot_Reset(&GameBeaconSys->mine);
        GameBeacon_FillPhrase(&GameBeaconSys->mine.beacon, name, value);
    }
}

static void GameBeacon_FillPhrase(GameBeacon *info, const StrBuf *name, u8 value) {
    GFL_StrBufStoreString(name, info->payload.namedByte.name, 9);
    info->payload.namedByte.value = value;
    info->type = 0x31;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastPokestarShooting(u8 value) {
    if (GameBeaconSys_CanSendType(0x33)) {
        GameBeaconSendSlot_ResetB2W2Only(&GameBeaconSys->mine);
        GameBeacon_FillPokestar(&GameBeaconSys->mine.beacon, value, 0x33);
    }
}

void GameBeacon_BroadcastPokestarWatching(u8 value) {
    if (GameBeaconSys_CanSendType(0x34)) {
        GameBeaconSendSlot_ResetB2W2Only(&GameBeaconSys->mine);
        GameBeacon_FillPokestar(&GameBeaconSys->mine.beacon, value, 0x34);
    }
}

static void GameBeacon_FillPokestar(GameBeacon *info, u8 value, u16 type) {
    info->type = type;
    info->payload.value8 = value;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastTournamentChallenge(u16 value) {
    if (GameBeaconSys_CanSendType(0x35)) {
        GameBeaconSendSlot_ResetB2W2Only(&GameBeaconSys->mine);
        GameBeacon_FillTournament(&GameBeaconSys->mine.beacon, value, 0, 0x35);
    }
}

void GameBeacon_BroadcastTournamentWin(u16 value, u8 kind) {
    u32 type = 0x38;

    if (kind != 3) {
        type = 0x36;
    }
    if (GameBeaconSys_CanSendType(type)) {
        GameBeaconSendSlot_ResetB2W2Only(&GameBeaconSys->mine);
        GameBeacon_FillTournament(&GameBeaconSys->mine.beacon, value, kind, type);
    }
}

void GameBeacon_BroadcastTournamentLoss(u16 value, u8 extra) {
    if (GameBeaconSys_CanSendType(0x37)) {
        GameBeaconSendSlot_ResetB2W2Only(&GameBeaconSys->mine);
        GameBeacon_FillTournament(&GameBeaconSys->mine.beacon, value, extra, 0x37);
    }
}

static void GameBeacon_FillTournament(GameBeacon *info, u16 value, u8 extra, u32 type) {
    info->type = type;
    info->payload.pair.a = value;
    info->payload.pair.b = extra;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastJoinAvenueShopping(void) {
    if (GameBeaconSys_CanSendType(0x39)) {
        GameBeaconSendSlot_ResetB2W2Only(&GameBeaconSys->mine);
        GameBeacon_FillJoinAvenueShopping(&GameBeaconSys->mine.beacon);
    }
}

static void GameBeacon_FillJoinAvenueShopping(GameBeacon *info) {
    info->type = 0x39;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastAreaChallenge(u8 extra, u16 value) {
    if (GameBeaconSys_CanSendType(0x3a)) {
        GameBeaconSendSlot_ResetB2W2Only(&GameBeaconSys->mine);
        GameBeacon_FillArea(&GameBeaconSys->mine.beacon, extra, value, 0x3a);
    }
}

void GameBeacon_BroadcastAreaCleared(u8 extra, u16 value) {
    if (GameBeaconSys_CanSendType(0x3b)) {
        GameBeaconSendSlot_ResetB2W2Only(&GameBeaconSys->mine);
        GameBeacon_FillArea(&GameBeaconSys->mine.beacon, extra, value, 0x3b);
    }
}

static void GameBeacon_FillArea(GameBeacon *info, u8 extra, u16 value, u16 type) {
    info->type = type;
    info->payload.withExtra.value = value;
    info->payload.withExtra.extra = extra;
    GameBeacon_ClearRecent(info);
}

void GameBeacon_BroadcastBattleLostMission(void) {
    if (GameBeaconSys_AddMissionScore(10, 0)) {
        GameBeaconSendSlot_SetMission(&GameBeaconSys->mine, 0x44, 0);
    }
}

void GameBeacon_BroadcastMission(u8 type, u16 value) {
    if (type >= 4 && GameBeaconSys_AddMissionScore(type, value)) {
        GameBeaconSendSlot_SetMission(&GameBeaconSys->mine, type + 0x3a, value);
    }
}
