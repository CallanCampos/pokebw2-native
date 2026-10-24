#include "types.h"
#include "app/pokemon_trade/pokemontrade_2d.h"
#include "app/pokemon_trade/pokemontrade_3d.h"
#include "app/pokemon_trade/pokemontrade_message.h"
#include "app/pokemon_trade/pokemontrade_proc.h"
#include "app/pokemon_trade/pokemontrade_save.h"
#include "app/pokemon_trade/pokemontrade_touchbar.h"
#include "app/pokemon_trade_local.h"
#include "constants/pokemon.h"
#include "constants/sound.h"
#include "constants/species.h"
#include "constants/version.h"
#include "field/unity_tower.h"
#include "gfl/bg_sys.h"
#include "gfl/bmpwin.h"
#include "gfl/clact.h"
#include "gfl/fade.h"
#include "gfl/gx_layers.h"
#include "gfl/heap.h"
#include "gfl/msg.h"
#include "gfl/net.h"
#include "gfl/net_handle.h"
#include "gfl/sound.h"
#include "gfl/std.h"
#include "pml/evolution.h"
#include "pml/item.h"
#include "pml/mail.h"
#include "pml/met_data.h"
#include "pml/poke_party.h"
#include "save/box.h"
#include "save/chatter.h"
#include "save/medal_box.h"
#include "save/player_info.h"
#include "save/pokedex.h"
#include "save/records.h"
#include "save/save_control.h"
#include "save/wifi_list.h"
#include "system/bmp_winframe.h"
#include "system/country_region.h"
#include "system/game_data.h"
#include "system/net_save.h"
#include "system/wipe.h"
#include "system/wordset.h"

// Writing the trade to the save: the copies of what it changes, kept to restore if the save fails, the trade itself,
// the evolution it may start and the saving between the two machines

static void func_ov194_021bead8(PokemonTradeWork *wk, int timer);
static void func_ov194_021beae4(PokemonTradeWork *wk);
static void func_ov194_021beb0c(PokemonTradeWork *wk);
static void func_ov194_021beb24(PokemonTradeWork *wk);
static void func_ov194_021bebf0(PokemonTradeWork *wk);
static void func_ov194_021bece8(PokemonTradeWork *wk);
static void func_ov194_021bed88(PokemonTradeWork *wk);
static void func_ov194_021bedb0(PokemonTradeWork *wk);
static void func_ov194_021bedd8(PokemonTradeWork *wk);
static void func_ov194_021bedfc(PokemonTradeWork *wk);
static void *func_ov194_021bef14(GameData *gameData, HeapID heapId);
static void func_ov194_021bef58(GameData *gameData, void *copy);
static void *func_ov194_021bef7c(GameData *gameData, HeapID heapId);
static void func_ov194_021befbc(GameData *gameData, void *copy);
static void *func_ov194_021befdc(GameData *gameData, HeapID heapId);
static void func_ov194_021bf01c(GameData *gameData, void *copy);
static void func_ov194_021bf03c(void *work);
static void func_ov194_021bf0c4(PokemonTradeWork *wk);
static BOOL func_ov194_021bf1ec(PokemonTradeWork *wk, PokeParty *party, int slot);
static void func_ov194_021bf278(PokemonTradeWork *wk);
static void func_ov194_021bf6ec(PokemonTradeWork *wk);
static void func_ov194_021bf75c(PokemonTradeWork *wk);
static void func_ov194_021bf784(PokemonTradeWork *wk);
static void func_ov194_021bf868(PokemonTradeWork *wk);
static void func_ov194_021bf9a8(PokemonTradeWork *wk);
static void func_ov194_021bf9c8(PokemonTradeWork *wk);
static void func_ov194_021bf9ec(PokemonTradeWork *wk);
static void func_ov194_021bfa48(PokemonTradeWork *wk);
static void func_ov194_021bfa7c(PokemonTradeWork *wk);
static void func_ov194_021bfac4(PokemonTradeWork *wk);

static void func_ov194_021bead8(PokemonTradeWork *wk, int timer) {
    wk->timer = timer;
}

static void func_ov194_021beae4(PokemonTradeWork *wk) {
    if (wk->bgmCount != 0) {
        GFL_SndBGMPop();
        wk->bgmCount--;
        GFL_SndBGMSetPaused(FALSE);
        GFL_SndBGMFadeIn(60);
    }
}

static void func_ov194_021beb0c(PokemonTradeWork *wk) {
    GFL_SndBGMPop();
    wk->bgmCount--;
}

static void func_ov194_021beb24(PokemonTradeWork *wk) {
    GFL_SndBGMPop();
    wk->bgmCount--;
    GFL_SndBGMSetPaused(FALSE);
    GFL_SndBGMFadeIn(24);
}

// After the trade's animation
void func_ov194_021beb48(PokemonTradeWork *wk) {
    ClActUnit_Delete(wk->clactUnit);
    ClActSys_Delete();
    func_ov194_021c2c64(wk);
    wk->clactUnit = ClActUnit_Create(340, 0, wk->heapId);
    if (wk->type == 5) {
        func_ov194_021beae4(wk);
        wk->param->next = 3;
        PokemonTrade_SetState(wk, PokemonTrade_FadeOutToEnd);
        return;
    }
    if (wk->type == 6) {
        func_ov194_021beae4(wk);
        wk->param->next = 3;
        PokemonTrade_SetState(wk, NULL);
        return;
    }
    func_ov194_021bfedc(wk);
    GFL_SndBGMSetPaused(TRUE);
    GFL_SndBGMPush();
    wk->bgmCount++;
    GFL_SndBGMFadeOut(6);
    wk->timer = 0;
    PokemonTrade_SetState(wk, func_ov194_021bebf0);
}

static void func_ov194_021bebf0(PokemonTradeWork *wk) {
    PartyPkm *pkm;
    if (wk->timer >= 6) {
        GFL_SndBGMPlay(SEQ_ME_POKEGET, 0xffff);
        GFL_SndBGMFadeIn(6);
        if (wk->type == 7) {
            GFL_MsgDataLoadStrbuf(wk->msgData, 154, wk->strbufTemplate);
        } else if (wk->type == 8) {
            GFL_MsgDataLoadStrbuf(wk->msgData, 157, wk->strbufTemplate);
        } else if (wk->unk11EE != 0) {
            GFL_MsgDataLoadStrbuf(wk->msgData, 132, wk->strbufTemplate);
        } else {
            GFL_MsgDataLoadStrbuf(wk->msgData, 48, wk->strbufTemplate);
        }
        if (PokemonTrade_IsNegoType(wk)) {
            pkm = PokemonTrade_GetPkm(wk, 0);
        } else {
            pkm = PokemonTrade_GetPkm(wk, 1);
        }
        loadPokemonNicknameToStrbuf(wk->wordSet, 1, pkm);
        copyVarForText(wk->wordSet, 0, wk->partnerInfo);
        GFL_WordSetFormatStrbuf(wk->wordSet, wk->strbuf, wk->strbufTemplate);
        wk->cursorImage = LoadCursorImageEndOfHeap(6, 15, 0, wk->heapId);
        func_ov194_021bfdf8(wk, TRUE, 0);
        func_ov194_021bead8(wk, 0);
        GFL_BGSysSetBGEnabled(6, TRUE);
        wk->timer = 0;
        PokemonTrade_SetState(wk, func_ov194_021bece8);
    }
}

static void func_ov194_021bece8(PokemonTradeWork *wk) {
    PartyPkm *pkm;
    if (wk->timer >= 200) {
        if (PokemonTrade_IsNegoType(wk)) {
            pkm = PokemonTrade_GetPkm(wk, 0);
        } else {
            pkm = PokemonTrade_GetPkm(wk, 1);
        }
        loadPokemonNicknameToStrbuf(wk->wordSet, 1, pkm);
        copyVarForText(wk->wordSet, 0, wk->partnerInfo);
        if (wk->type != 7 && wk->type != 8) {
            if (wk->unk11EE != 0) {
                GFL_MsgDataLoadStrbuf(wk->msgData, 133, wk->strbufTemplate);
                GFL_WordSetFormatStrbuf(wk->wordSet, wk->strbuf, wk->strbufTemplate);
                func_ov194_021bfdf8(wk, TRUE, 0);
            } else {
                GFL_MsgDataLoadStrbuf(wk->msgData, 49, wk->strbufTemplate);
                GFL_WordSetFormatStrbuf(wk->wordSet, wk->strbuf, wk->strbufTemplate);
                func_ov194_021bfdf8(wk, TRUE, 0);
            }
        }
        PokemonTrade_SetState(wk, func_ov194_021bed88);
    }
}

static void func_ov194_021bed88(PokemonTradeWork *wk) {
    if (wk->timer >= 270) {
        func_ov194_021beb24(wk);
        PokemonTrade_SetState(wk, func_ov194_021bedb0);
    }
}

static void func_ov194_021bedb0(PokemonTradeWork *wk) {
    if (wk->timer >= 420) {
        func_ov194_021bead8(wk, 0);
        PokemonTrade_SetState(wk, func_ov194_021bf6ec);
    }
}

static void func_ov194_021bedd8(PokemonTradeWork *wk) {
    if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 0x17, 8)) {
        PokemonTrade_SetState(wk, func_ov194_021bf784);
    }
}

// Sends this player's profile, with the Pokémon traded, for the other machine's records
static void func_ov194_021bedfc(PokemonTradeWork *wk) {
    UnityTowerVisitor profile;
    UnityTowerSurveySave *survey = getUnityTower_SurveySaveBlkAddrress(GameData_GetSaveControl(wk->gameData));
    PartyPkm *sent;
    PartyPkm *received;
    if (PokemonTrade_IsNegoType(wk)) {
        sent = PokemonTrade_GetPkm(wk, 0);
        received = PokemonTrade_GetPkm(wk, 1);
    } else {
        sent = PokemonTrade_GetPkm(wk, 1);
        received = PokemonTrade_GetPkm(wk, 0);
    }
    PlayerInfo_Copy(GetGameDataPlayerInfo(wk->gameData), &profile.info);
    if (PokeParty_GetParam(sent, PKM_PARAM_IS_EGG, NULL)) {
        profile.sentSpecies = SPECIES_EGG;
    } else {
        profile.sentSpecies = PokeParty_GetParam(sent, PKM_PARAM_SPECIES, NULL);
    }
    if (PokeParty_GetParam(received, PKM_PARAM_IS_EGG, NULL)) {
        profile.receivedSpecies = SPECIES_EGG;
    } else {
        profile.receivedSpecies = PokeParty_GetParam(received, PKM_PARAM_SPECIES, NULL);
    }
    profile.hobby = getPlayerSurveys(survey);
    profile.countryCount = UnityTower_GetCountryCount(survey);
    profile.unk26_0 = func_02009d28(survey);
    if (PokemonTrade_IsNetwork(wk)) {
        if (GFL_NetSendCommand(NetHandle_GetOwn(), TRADE_NET_CMD_UNK12, sizeof(profile), &profile)) {
            NetHandle_SendTimingPair(NetHandle_GetOwn(), 0x17, 8);
            PokemonTrade_SetState(wk, func_ov194_021bedd8);
        }
    } else {
        PokemonTrade_SetState(wk, func_ov194_021bfa7c);
    }
}

// GFLib's GFL_STD_MemCopy, an inline over the copy: the box's copy reads its destination before the call that gives
// the source
static inline void GFL_STD_MemCopy(const void *src, void *dest, u32 size) {
    MI_CpuCopy8(src, dest, size);
}

// The copies of the mail and of the records are inlined, and not `static`: each has a file name string of its own
// in `.data`, ahead of the one the file's other allocations share, which MWCC gives only to an external inline
inline void *BackupMail(GameData *gameData, HeapID heapId) {
    void *copy = GFL_HeapAllocate(heapId, MailBox_GetSize(), FALSE, "pokemontrade_save.c", 370);
    u32 size = MailBox_GetSize();
    GFL_STD_MemCopy(MailBox_GetFromGameData(gameData), copy, size);
    return copy;
}

inline void *BackupRecords(GameData *gameData, HeapID heapId) {
    void *copy = GFL_HeapAllocate(heapId, GameRecords_GetSize(), FALSE, "pokemontrade_save.c", 401);
    u32 size = GameRecords_GetSize();
    GFL_STD_MemCopy(GameData_GetRecords(gameData), copy, size);
    return copy;
}

static void *func_ov194_021bef14(GameData *gameData, HeapID heapId) {
    SaveControl *save = GameData_GetSaveControl(gameData);
    void *copy = GFL_HeapAllocate(heapId, UnityTower_GetSurveySaveSize(), FALSE, "pokemontrade_save.c", 431);
    u32 size = UnityTower_GetSurveySaveSize();
    MI_CpuCopy8(getUnityTower_SurveySaveBlkAddrress(save), copy, size);
    return copy;
}

static void func_ov194_021bef58(GameData *gameData, void *copy) {
    MI_CpuCopy8(copy, getUnityTower_SurveySaveBlkAddrress(GameData_GetSaveControl(gameData)),
                UnityTower_GetSurveySaveSize());
    GFL_HeapFree(copy);
}

static void *func_ov194_021bef7c(GameData *gameData, HeapID heapId) {
    void *copy = GFL_HeapAllocate(heapId, getSizeofPokedexData(), FALSE, "pokemontrade_save.c", 460);
    u32 size = getSizeofPokedexData();
    MI_CpuCopy8(GameData_GetPokedex(gameData), copy, size);
    return copy;
}

static void func_ov194_021befbc(GameData *gameData, void *copy) {
    MI_CpuCopy8(copy, GameData_GetPokedex(gameData), getSizeofPokedexData());
    GFL_HeapFree(copy);
}

static void *func_ov194_021befdc(GameData *gameData, HeapID heapId) {
    void *copy = GFL_HeapAllocate(heapId, PalPadFriendList_GetSize(), FALSE, "pokemontrade_save.c", 489);
    u32 size = PalPadFriendList_GetSize();
    MI_CpuCopy8(GameData_GetPalPadFriendList(gameData), copy, size);
    return copy;
}

static void func_ov194_021bf01c(GameData *gameData, void *copy) {
    MI_CpuCopy8(copy, GameData_GetPalPadFriendList(gameData), PalPadFriendList_GetSize());
    GFL_HeapFree(copy);
}

// Frees the copies once the save has gone through
static void func_ov194_021bf03c(void *work) {
    PokemonTradeWork *wk = work;
    if (wk->backup.mail != NULL) {
        GFL_HeapFree(wk->backup.mail);
    }
    if (wk->backup.records != NULL) {
        GFL_HeapFree(wk->backup.records);
    }
    if (wk->backup.survey != NULL) {
        GFL_HeapFree(wk->backup.survey);
    }
    if (wk->backup.pokedex != NULL) {
        GFL_HeapFree(wk->backup.pokedex);
    }
    if (wk->backup.playersMet != NULL) {
        GFL_HeapFree(wk->backup.playersMet);
    }
    if (wk->backup.party != NULL) {
        GFL_HeapFree(wk->backup.party);
    }
    if (wk->backup.box != NULL) {
        GFL_HeapFree(wk->backup.box);
    }
    MI_CpuFill8(&wk->backup, 0, sizeof(wk->backup));
    wk->unk11F7 = 0;
}

// Restores what the trade changed from the copies
static void func_ov194_021bf0c4(PokemonTradeWork *wk) {
    void *copy;
    if ((copy = wk->backup.mail) != NULL) {
        GameData *gameData = wk->gameData;
        void *block;
        // The save is fetched and left unused, as if the block once came from it
        GameData_GetSaveControl(gameData);
        block = MailBox_GetFromGameData(gameData);
        MI_CpuCopy8(copy, block, MailBox_GetSize());
        GFL_HeapFree(copy);
    }
    if ((copy = wk->backup.records) != NULL) {
        GameRecords *records = GameData_GetRecords(wk->gameData);
        MI_CpuCopy8(copy, records, GameRecords_GetSize());
        GFL_HeapFree(copy);
    }
    if (wk->backup.survey != NULL) {
        func_ov194_021bef58(wk->gameData, wk->backup.survey);
    }
    if (wk->backup.pokedex != NULL) {
        func_ov194_021befbc(wk->gameData, wk->backup.pokedex);
    }
    if (wk->backup.playersMet != NULL) {
        func_ov194_021bf01c(wk->gameData, wk->backup.playersMet);
    }
    if (wk->backup.party != NULL) {
        PokeParty_Copy(wk->backup.party, wk->party);
        PlayTime_SetLastSaved(GameData_GetPlayTime(wk->gameData), &wk->backup.unk1C);
        GFL_HeapFree(wk->backup.party);
        wk->backup.party = NULL;
    }
    if (wk->backup.box != NULL) {
        u32 size = getSizeofBox();
        MI_CpuCopy8(wk->backup.box, BoxSaveAccessor_GetBox(wk->boxes, wk->param->box), size);
        GFL_HeapFree(wk->backup.box);
    }
    if (wk->backup.chatot) {
        setChatotPresent(getChatterDataAddress(wk->gameData));
    }
    MI_CpuFill8(&wk->backup, 0, sizeof(wk->backup));
}

// Forgets the Chatot's cry if the only Chatot of the party is traded away
static BOOL func_ov194_021bf1ec(PokemonTradeWork *wk, PokeParty *party, int slot) {
    int chatots = 0;
    int count, i;
    void *chatter = getChatterDataAddress(wk->gameData);
    if (!doesChatotExist(chatter)) {
        return FALSE;
    }
    count = PokeParty_GetPkmCount(party);
    for (i = 0; i < count; i++) {
        if (PokeParty_GetParam(PokeParty_GetPkm(party, i), PKM_PARAM_SPECIES, NULL) == SPECIES_CHATOT) {
            chatots++;
        }
    }
    if (chatots != 1) {
        return FALSE;
    }
    if (PokeParty_GetParam(PokeParty_GetPkm(party, slot), PKM_PARAM_SPECIES, NULL) == SPECIES_CHATOT) {
        setChatotNotPresent(chatter);
        return TRUE;
    }
    return FALSE;
}

// Trades the Pokémon in the save, keeping copies of what changes
static void func_ov194_021bf278(PokemonTradeWork *wk) {
    PartyPkm *pkm;
    GameRecords *records;
    if (!PokemonTrade_IsNegoType(wk)) {
        wk->backup.mail = BackupMail(wk->gameData, wk->heapId);
        wk->backup.records = BackupRecords(wk->gameData, wk->heapId);
        wk->backup.survey = func_ov194_021bef14(wk->gameData, wk->heapId);
        wk->backup.pokedex = func_ov194_021bef7c(wk->gameData, wk->heapId);
        wk->backup.playersMet = func_ov194_021befdc(wk->gameData, wk->heapId);
        PlayTime_GetLastSaved(GameData_GetPlayTime(wk->gameData), &wk->backup.unk1C);
        if (wk->param->box == BoxSaveAccessor_GetAvailableBoxCount(wk->boxes)) {
            wk->backup.party = PokeParty_Create(wk->heapId);
            PokeParty_Copy(wk->party, wk->backup.party);
            wk->backup.chatot = func_ov194_021bf1ec(wk, wk->party, wk->param->slot);
        } else {
            u32 size;
            wk->backup.box = GFL_HeapAllocate(wk->heapId, getSizeofPokeBox(), FALSE, "pokemontrade_save.c", 640);
            size = getSizeofPokeBox();
            GFL_STD_MemCopy(BoxSaveAccessor_GetBox(wk->boxes, wk->param->box), wk->backup.box, size);
        }
    } else {
        PokemonTradeParam *param = wk->param;
        if (param->box == BoxSaveAccessor_GetAvailableBoxCount(wk->boxes)) {
            func_ov194_021bf1ec(wk, wk->party, param->slot);
        }
        wk->unk11F7 = 1;
    }
    {
        PokemonTradeParam *param = wk->param;
        if (param->box != BoxSaveAccessor_GetAvailableBoxCount(wk->boxes)) {
            pkm = PokeParty_GetPkm(param->party, 0);
            if (PML_ItemIsMail(PokeParty_GetParam(pkm, PKM_PARAM_ITEM, NULL))) {
                void *block = MailBox_GetFromGameData(wk->gameData);
                int mailSlot = MailBox_GetFreeSlot(block, 0);
                if (mailSlot != -1) {
                    MailData *mail = CreateMailData(wk->heapId);
                    PokeParty_GetParam(pkm, PKM_PARAM_MAIL, mail);
                    MailBox_SetMail(block, 0, mailSlot, mail);
                    ResetMailData(mail);
                    PokeParty_SetParam(pkm, PKM_PARAM_MAIL, (uptr)mail);
                    PokeParty_SetParam(pkm, PKM_PARAM_ITEM, 0);
                    GFL_HeapFree(mail);
                }
            }
        }
    }
    {
        u8 country = UnityTowerVisitor_GetCountry(wk->partnerInfo);
        u8 province = UnityTowerVisitor_GetProvince(wk->partnerInfo);
        u32 validCountry = Country_GetValidCountry(country, province, TrainerInfo_GetRegion(wk->partnerInfo));
        u32 validProvince = Country_GetValidRegion(country, province, TrainerInfo_GetRegion(wk->partnerInfo));
        if (country != validCountry || province != validProvince) {
            PlayerInfo_SetCountryAndProvince(wk->partnerInfo, 0, 0);
        }
    }
    PalPadFriendList_AddPlayer(GameData_GetPalPadFriendList(wk->gameData), wk->partnerInfo);
    {
        // A trade with a game of the other colour
        BOOL medal = FALSE;
        u8 version = PlayerInfo_GetVersion(wk->partnerInfo);
#ifdef BLACK2
        if (version == VERSION_WHITE || version == VERSION_WHITE2) {
            medal = TRUE;
        }
#else
        if (version == VERSION_BLACK || version == VERSION_BLACK2) {
            medal = TRUE;
        }
#endif
        if (medal) {
            MedalBox_GiveMedal(SaveControl_GetMedalBox(GameData_GetSaveControl(wk->gameData)), 165);
        }
    }
    {
        int i;
        UnityTowerSurveySave *survey = getUnityTower_SurveySaveBlkAddrress(GameData_GetSaveControl(wk->gameData));
        PlayerInfo *infos[2];
        infos[0] = wk->myInfo;
        infos[1] = wk->partnerInfo;
        for (i = 0; i < 2; i++) {
            PlayerInfo *info = infos[i];
            if (info != NULL) {
                u8 country = UnityTowerVisitor_GetCountry(info);
                u8 province = UnityTowerVisitor_GetProvince(info);
                // The region is read and left unused
                TrainerInfo_GetRegion(info);
                if (!UnityTower_GetRegionState(survey, country, province)) {
                    UnityTower_SetRegionState(survey, country, province, TRUE);
                }
            }
        }
    }
    pkm = PokeParty_GetPkm(wk->param->party, 0);
    records = GameData_GetRecords(wk->gameData);
    PokeParty_Recover(pkm);
    if (!PokeParty_GetParam(pkm, PKM_PARAM_IS_EGG, NULL)) {
        PokeParty_SetParam(pkm, PKM_PARAM_HAPPINESS, 70);
    }
    if (PokeParty_GetParam(pkm, PKM_PARAM_IS_EGG, NULL)) {
        PokeParty_SetupMetData(pkm, 6, wk->myInfo, 30003, wk->heapId);
    }
    switch (wk->type) {
    case 2:
    case 3:
        RecordAddOne(records, 16);
        break;
    case 1:
        RecordAddOne(records, 12);
        break;
    case 0:
        RecordAddOne(records, 36);
        break;
    }
    addPkmToDex(GameData_GetPokedex(wk->gameData), pkm);
    if (wk->type == 3) {
        PalPadFriendList_IncrementTradeCount(GameData_GetPalPadFriendList(wk->gameData));
    }
    if (PokeParty_GetParam(pkm, PKM_PARAM_SPECIES, NULL) == SPECIES_SHAYMIN &&
        PokeParty_GetParam(pkm, PKM_PARAM_FORM, NULL) == 1) {
        PokeParty_ChangeForme(pkm, 0);
        addPkmToDex(GameData_GetPokedex(wk->gameData), pkm);
    }
    if (wk->param->box == BoxSaveAccessor_GetAvailableBoxCount(wk->boxes)) {
        copyPkmIntoPartyBlk(wk->party, wk->param->slot, pkm);
    } else {
        BoxSaveAccessor_SetPkm(wk->boxes, wk->param->box, wk->param->slot, PokeParty_ToBoxPkm(pkm));
    }
    switch (wk->type) {
    case 2:
        WifiList_AddFriendRecord(GameData_GetWifiList(wk->gameData), wk->param->friendIndex - 1, 0, 0, 1);
        break;
    case 0:
    case 1: {
        u32 index;
        if (WifiList_FindFriendByPlayerInfo(GameData_GetWifiList(wk->gameData), wk->partnerInfo, &index)) {
            WifiList_AddFriendRecord(GameData_GetWifiList(wk->gameData), index, 0, 0, 1);
        }
        break;
    }
    }
}

static void func_ov194_021bf6ec(PokemonTradeWork *wk) {
    func_ov194_021c24dc(wk, 0);
    func_ov194_021c24dc(wk, 1);
    func_ov194_021c24dc(wk, 2);
    func_ov194_021c24dc(wk, 3);
    if (wk->type == 4 || wk->type == 7 || wk->type == 8) {
        wk->param->next = 3;
        func_ov194_021beae4(wk);
        PokemonTrade_SetState(wk, PokemonTrade_FadeOutToEnd);
    } else if (wk->type == 1 || wk->type == 2 || wk->type == 3) {
        PokemonTrade_SetState(wk, func_ov194_021bedfc);
    } else {
        PokemonTrade_SetState(wk, func_ov194_021bf784);
    }
}

static void func_ov194_021bf75c(PokemonTradeWork *wk) {
    if (func_ov194_021c00b0(wk) && wk->timer > 180) {
        PokemonTrade_SetState(wk, func_ov194_021bf868);
    }
}

// Puts the received Pokémon in the parameter's party, for the event
static void func_ov194_021bf784(PokemonTradeWork *wk) {
    PartyPkm *pkm;
    int slot;
    if (PokemonTrade_IsNegoType(wk)) {
        int index = wk->unkFA0 % 3;
        pkm = PokemonTrade_GetPkm(wk, 0);
        wk->param->box = wk->negoBox[0][index];
        slot = wk->negoSlot[0][index];
    } else {
        pkm = PokemonTrade_GetPkm(wk, 1);
        wk->param->box = wk->selectBox;
        slot = wk->selectSlot;
    }
    wk->param->slot = slot;
    PokeParty_InitCore(wk->param->party, 6);
    PokeParty_AddPkm(wk->param->party, pkm);
    addPkmToDex(GameData_GetPokedex(wk->gameData), pkm);
    if (wk->param->box != BoxSaveAccessor_GetAvailableBoxCount(wk->boxes) &&
        PML_ItemIsMail(PokeParty_GetParam(PokeParty_GetPkm(wk->param->party, 0), PKM_PARAM_ITEM, NULL))) {
        GFL_MsgDataLoadStrbuf(wk->msgData, 107, wk->strbuf);
        func_ov194_021bfe28(wk);
        PokemonTrade_SetState(wk, func_ov194_021bf75c);
        wk->timer = 0;
        return;
    }
    PokemonTrade_SetState(wk, func_ov194_021bf868);
}

// Starts the received Pokémon's evolution if it evolves by trade
static void func_ov194_021bf868(PokemonTradeWork *wk) {
    if (wk->gameData == NULL) {
        PokemonTrade_SetState(wk, func_ov194_021bfa7c);
    } else {
        if (wk->param != NULL) {
            PartyPkm *pkm;
            PartyPkm *other;
            u32 species;
            u32 method;
            if (PokemonTrade_IsNegoType(wk)) {
                pkm = PokemonTrade_GetPkm(wk, 0);
                other = PokemonTrade_GetPkm(wk, 1);
            } else {
                pkm = PokemonTrade_GetPkm(wk, 1);
                other = PokemonTrade_GetPkm(wk, 0);
            }
            species =
                CheckEvolveSpecies(NULL, pkm, 1, (uptr)other, GameData_GetSeason(wk->gameData), &method, wk->heapId);
            if (species != 0) {
                wk->param->next = TRADE_NEXT_EVOLVE;
                wk->param->evolveSpecies = species;
                wk->param->evolveMethod = method;
                PokemonTrade_SetState(wk, PokemonTrade_FadeOutToEnd);
                func_ov194_021beb0c(wk);
                return;
            }
        }
        GFL_MsgDataLoadStrbuf(wk->msgData, 50, wk->strbuf);
        func_ov194_021bfe28(wk);
        func_ov194_021bfe34(wk);
        PokemonTrade_SetState(wk, func_ov194_021bf9a8);
    }
    func_ov194_021beae4(wk);
}

void func_ov194_021bf938(PokemonTradeWork *wk) {
    func_ov194_021bfedc(wk);
    GFL_MsgDataLoadStrbuf(wk->msgData, 50, wk->strbuf);
    wk->cursorImage = LoadCursorImageEndOfHeap(6, 15, 0, wk->heapId);
    func_ov194_021bfe28(wk);
    func_ov194_021bfe34(wk);
    func_ov194_021bead8(wk, 0);
    GFL_BGSysSetBGEnabled(6, TRUE);
    GFL_NetSetIconScreen(TRUE, wk->heapId);
    GFL_FadeSet(3, 16, 0, 2);
    PokemonTrade_SetState(wk, func_ov194_021bf9a8);
}

static void func_ov194_021bf9a8(PokemonTradeWork *wk) {
    NetHandle_SendTimingPair(NetHandle_GetOwn(), 0x18, 8);
    PokemonTrade_SetState(wk, func_ov194_021bf9c8);
}

static void func_ov194_021bf9c8(PokemonTradeWork *wk) {
    if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 0x18, 8)) {
        PokemonTrade_SetState(wk, func_ov194_021bf9ec);
    }
}

static void func_ov194_021bf9ec(PokemonTradeWork *wk) {
    if (func_ov194_021c00b0(wk)) {
        if (PokemonTrade_IsNetwork(wk)) {
            func_ov194_021bf278(wk);
            wk->netSave = NetSave_CreateWithCallback(wk->heapId, wk->gameData, func_ov194_021bf03c, wk);
            PokemonTrade_SetState(wk, func_ov194_021bfa48);
        } else {
            PokemonTrade_SetState(wk, func_ov194_021bfa7c);
        }
    }
}

static void func_ov194_021bfa48(PokemonTradeWork *wk) {
    if (NetSave_Update(wk->netSave)) {
        func_ov194_021bf0c4(wk);
        NetSave_Free(wk->netSave);
        wk->netSave = NULL;
        PokemonTrade_SetState(wk, func_ov194_021bfa7c);
    }
}

static void func_ov194_021bfa7c(PokemonTradeWork *wk) {
    GFL_WipeSet(0, 0, 0, 0, 6, 1, wk->heapId);
    wk->timer = 0;
    wk->unk11F7 = 0;
    PokemonTrade_SetState(wk, func_ov194_021bfac4);
}

static void func_ov194_021bfac4(PokemonTradeWork *wk) {
    if (wk->timer >= 6 && GFL_WipeIsFinished()) {
        if (wk->cursorImage != 0) {
            // The library takes the position and size as u16s, which the prototype doesn't say
            GFL_BGSysFreeCharMemory(6, (u16)wk->cursorImage, (u16)(wk->cursorImage >> 16));
        }
        wk->cursorImage = 0;
        GFL_BGSysSetEnabledBGsA(0);
        GFL_BGSysSetEnabledBGsB(0);
        ClActUnit_Delete(wk->clactUnit);
        wk->clactUnit = ClActUnit_Create(340, 0, wk->heapId);
        func_ov194_021bfe70(wk);
        func_ov194_021c45a8(wk);
        func_ov194_021c41d0(wk);
        func_ov194_021c45ec(0);
        func_ov194_021c4484(wk);
        func_ov194_021c46a4(wk);
        func_ov194_021c3480(wk);
        func_ov194_021c3224(wk);
        func_ov194_021c2c84(wk);
        func_ov194_021c2de8(wk);
        func_ov194_021c2d78(wk);
        func_ov194_021c200c(wk, 0);
        GFL_BGSysSetEnabledBGsA(0x1f);
        GFL_BGSysSetEnabledBGsB(0x1e);
        PokemonTrade_CreateTouchBar(wk);
        GFL_NetSetIconScreen(TRUE, wk->heapId);
        wk->checkCount = 0;
        PokemonTrade_SetState(wk, func_ov194_021b9a38);
    }
}
