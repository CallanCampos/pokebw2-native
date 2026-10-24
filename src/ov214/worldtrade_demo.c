#include "types.h"
#include "app/pokemon_trade.h"
#include "app/ui/ui_scene.h"
#include "constants/pokemon.h"
#include "demo/shinka_demo.h"
#include "gfl/heap.h"
#include "gfl/overlay.h"
#include "gfl/proc.h"
#include "gfl/std.h"
#include "nitro/gx.h"
#include "pml/evolution.h"
#include "pml/poke_party.h"
#include "save/box.h"
#include "save/player_info.h"
#include "save/worldtrade_data.h"
#include "system/game_data.h"
#include "system/game_system.h"
#include "worldtrade_local.h"

// The Global Trade Station's trade demo, and the evolution demo of a Pokémon that evolves by the trade. The names are
// ours, guessed

static PartyPkm *Demo_GetTradePokemon(WorldTradeWork *wk, int mode);
static void Demo_StoreTradedPokemon(WorldTradeWork *wk);

int WorldTrade_Demo_Init(WorldTradeWork *wk, int seq) {
    PokemonTradeGtsParam *param;
    const GameProcFunctions *procs;

    WorldTrade_ExitGraphics(wk);
    wk->subProcParam =
        GFL_HeapAllocate(HEAPID_WORLDTRADE, sizeof(PokemonTradeGtsParam), FALSE, "worldtrade_demo.c", 94);
    MI_CpuFill8(wk->subProcParam, 0, sizeof(PokemonTradeGtsParam));
    param = wk->subProcParam;
    wk->sentPokemon = WorldTrade_AllocPartyPkm(HEAPID_WORLDTRADE);
    param->gameData = GSYS_GetGameData(wk->param->gsys);
    param->myStatus = wk->param->mystatus;

    switch (wk->subProcessMode) {
    case WT_MODE_UPLOAD:
        param->recvPkm = param->sendPkm = WorldTradeData_GetPkmPtr(wk->param->worldtrade_data);
        param->partnerStatus = wk->partnerStatus = WorldTrade_MakePartnerStatus(&wk->uploadPokemonData);
        procs = &data_ov194_021c6400;
        break;
    case WT_MODE_DOWNLOAD:
        if (wk->uploadPokemonData.isTrade) {
            param->recvPkm = (PartyPkm *)wk->uploadPokemonData.postData;
        } else {
            param->recvPkm = WorldTradeData_GetPkmPtr(wk->param->worldtrade_data);
        }
        param->sendPkm = param->recvPkm;
        param->partnerStatus = wk->partnerStatus = WorldTrade_MakePartnerStatus(&wk->uploadPokemonData);
        procs = &data_ov194_021c63b8;
        break;
    case WT_MODE_DOWNLOAD_EX:
        param->recvPkm = wk->uploadPokemonData.isTrade ? (PartyPkm *)wk->uploadPokemonData.postData
                                                       : WorldTradeData_GetPkmPtr(wk->param->worldtrade_data);
        WorldTradeData_GetPkm(wk->param->worldtrade_data, wk->sentPokemon);
        param->sendPkm = wk->sentPokemon;
        param->partnerStatus = wk->partnerStatus = WorldTrade_MakePartnerStatus(&wk->uploadPokemonData);
        procs = &data_ov194_021c63d0;
        break;
    case WT_MODE_EXCHANGE:
        copyPartyPkm(wk->demoPokemon, wk->sentPokemon);
        param->sendPkm = wk->demoPokemon;
        param->recvPkm = (PartyPkm *)wk->downloadPokemonData[wk->touchTrainerPos].postData;
        param->partnerStatus = wk->partnerStatus =
            WorldTrade_MakePartnerStatus(&wk->downloadPokemonData[wk->touchTrainerPos]);
        procs = &data_ov194_021c63d0;
        break;
    default:
        GFL_ASSERT(0);
        break;
    }

    QueueGameProc(wk->procManager, OVERLAY_POKEMONTRADE, procs, wk->subProcParam);
    wk->subprocFlag = 1;
    wk->subLcdBgKeep = 0;
    return WT_SEQ_FADEIN;
}

int WorldTrade_Demo_Main(WorldTradeWork *wk, int seq) {
    int ret = WT_SEQ_MAIN;
    ShinkaDemoParam *evolution;

    switch (wk->subprocessSeq) {
    case 0:
        if (wk->procResult) {
            break;
        }
        if (wk->subProcessMode == WT_MODE_EXCHANGE) {
            PartyPkm *pkm;
            u32 species;
            u32 method;

            pkm = Demo_GetTradePokemon(wk, wk->subProcessMode);
            PokeParty_GetParam(pkm, PKM_PARAM_ITEM, NULL);
            species =
                CheckEvolveSpecies(NULL, pkm, 1, (uptr)wk->sentPokemon,
                                   GameData_GetSeason(GSYS_GetGameData(wk->param->gsys)), &method, HEAPID_WORLDTRADE);
            if (species != 0) {
                if (wk->subProcParam != NULL) {
                    GFL_HeapFree(wk->subProcParam);
                    wk->subProcParam = NULL;
                }
                wk->subProcParam =
                    GFL_HeapAllocate(HEAPID_WORLDTRADE, sizeof(ShinkaDemoParam), FALSE, "worldtrade_demo.c", 235);
                MI_CpuFill8(wk->subProcParam, 0, sizeof(ShinkaDemoParam));
                evolution = wk->subProcParam;
                evolution->gameData = GSYS_GetGameData(wk->param->gsys);
                evolution->party = PokeParty_Create(HEAPID_WORLDTRADE);
                PokeParty_AddPkm(evolution->party, pkm);
                evolution->species = species;
                evolution->partyIndex = 0;
                evolution->method = method;
                evolution->unkC = 1;
                evolution->canCancel = FALSE;
                GFL_OvlUnload(OVERLAY_APP_UI);
                QueueGameProc(wk->procManager, OVERLAY_SHINKA_DEMO, &SHINKA_DEMO_PROC_FUNCTIONS, wk->subProcParam);
                wk->subprocessSeq = 1;
            } else {
                WorldTrade_SubProcessChange(wk, WORLDTRADE_TITLE, 0);
                ret = WT_SEQ_FADEOUT;
            }
        } else if (wk->subProcessMode == WT_MODE_DOWNLOAD || wk->subProcessMode == WT_MODE_DOWNLOAD_EX) {
            PartyPkm *pkm;
            PartyPkm *sent;
            u32 species;
            u32 method;

            pkm = Demo_GetTradePokemon(wk, wk->subProcessMode);
            sent = WorldTrade_AllocPartyPkm(HEAPID_WORLDTRADE);
            WorldTradeData_GetPkm(wk->param->worldtrade_data, sent);
            if (PokeParty_GetParam(pkm, PKM_PARAM_SPECIES, NULL) != PokeParty_GetParam(sent, PKM_PARAM_SPECIES, NULL) ||
                PokeParty_GetParam(pkm, PKM_PARAM_PID, NULL) != PokeParty_GetParam(sent, PKM_PARAM_PID, NULL)) {
                PokeParty_GetParam(pkm, PKM_PARAM_ITEM, NULL);
                species = 0;
                if (wk->checkEvolution) {
                    wk->checkEvolution = FALSE;
                    species = CheckEvolveSpecies(NULL, pkm, 1, (uptr)sent,
                                                 GameData_GetSeason(GSYS_GetGameData(wk->param->gsys)), &method,
                                                 HEAPID_WORLDTRADE);
                }
                if (species != 0) {
                    if (wk->subProcParam != NULL) {
                        GFL_HeapFree(wk->subProcParam);
                        wk->subProcParam = NULL;
                    }
                    wk->subProcParam =
                        GFL_HeapAllocate(HEAPID_WORLDTRADE, sizeof(ShinkaDemoParam), FALSE, "worldtrade_demo.c", 304);
                    MI_CpuFill8(wk->subProcParam, 0, sizeof(ShinkaDemoParam));
                    evolution = wk->subProcParam;
                    evolution->gameData = GSYS_GetGameData(wk->param->gsys);
                    evolution->party = PokeParty_Create(HEAPID_WORLDTRADE);
                    PokeParty_AddPkm(evolution->party, pkm);
                    evolution->species = species;
                    evolution->partyIndex = 0;
                    evolution->method = method;
                    evolution->unkC = 1;
                    evolution->canCancel = FALSE;
                    GFL_OvlUnload(OVERLAY_APP_UI);
                    QueueGameProc(wk->procManager, OVERLAY_SHINKA_DEMO, &SHINKA_DEMO_PROC_FUNCTIONS, wk->subProcParam);
                    wk->subprocessSeq = 1;
                } else {
                    WorldTrade_SubProcessChange(wk, WORLDTRADE_TITLE, 0);
                    ret = WT_SEQ_FADEOUT;
                }
            } else {
                WorldTrade_SubProcessChange(wk, WORLDTRADE_TITLE, 0);
                ret = WT_SEQ_FADEOUT;
            }
            GFL_HeapFree(sent);
        } else {
            WorldTrade_SubProcessChange(wk, WORLDTRADE_TITLE, 0);
            ret = WT_SEQ_FADEOUT;
        }
        break;
    case 1:
        if (wk->procResult) {
            break;
        }
        if (wk->subProcParam != NULL) {
            PartyPkm *pkm;

            evolution = wk->subProcParam;
            pkm = Demo_GetTradePokemon(wk, wk->subProcessMode);
            copyPartyPkm(PokeParty_GetPkm(evolution->party, 0), pkm);
            PokeParty_SetParam(pkm, PKM_PARAM_HAPPINESS, 70);
            GFL_HeapFree(evolution->party);
            GFL_HeapFree(wk->subProcParam);
            wk->subProcParam = NULL;
        }
        Demo_StoreTradedPokemon(wk);
        GX_SetVisibleWnd(GX_WNDMASK_NONE);
        GFL_OvlLoad(OVERLAY_APP_UI);
        WorldTrade_SubProcessChange(wk, WORLDTRADE_UPLOAD, WT_MODE_POKEMON_EVO_SAVE);
        ret = WT_SEQ_FADEOUT;
        break;
    }
    return ret;
}

int WorldTrade_Demo_End(WorldTradeWork *wk, int seq) {
    if (wk->partnerStatus != NULL) {
        GFL_HeapFree(wk->partnerStatus);
        wk->partnerStatus = NULL;
    }
    GFL_HeapFree(wk->sentPokemon);
    if (wk->subProcParam != NULL) {
        GFL_HeapFree(wk->subProcParam);
        wk->subProcParam = NULL;
    }
    wk->checkEvolution = FALSE;
    WorldTrade_InitGraphics(wk);
    WorldTrade_SubProcessUpdate(wk);
    G2_BlendNone();
    return WT_SEQ_INIT;
}

PlayerInfo *WorldTrade_MakePartnerStatus(Dpw_Tr_Data *dtd) {
    PlayerInfo *info = PlayerInfo_Create(HEAPID_WORLDTRADE);

    PlayerInfo_Init(info);
    copyTrainerName(info, dtd->name);
    PlayerInfo_SetVersion(info, dtd->versionCode);
    PlayerInfo_SetRegion(info, dtd->langCode);
    setTrainerGender(info, dtd->gender);
    PlayerInfo_SetUnionView(info, dtd->trainerType);
    PlayerInfo_SetCountryAndProvince(info, dtd->countryCode, dtd->localCode);
    info->id = dtd->trainerID;
    info->profileId = dtd->id;
    return info;
}

static PartyPkm *Demo_GetTradePokemon(WorldTradeWork *wk, int mode) {
    if (mode == WT_MODE_EXCHANGE) {
        return (PartyPkm *)wk->downloadPokemonData[wk->touchTrainerPos].postData;
    }
    if (mode == WT_MODE_DOWNLOAD_EX) {
        return (PartyPkm *)wk->uploadPokemonData.postData;
    }
    if (mode == WT_MODE_DOWNLOAD) {
        return (PartyPkm *)wk->uploadPokemonData.postData;
    }
    // "The mode passed is wrong"
    GFL_ASSERT_MSG(FALSE,
                   "\x93\x6e\x82\xb7\x83\x82\x81\x5b\x83\x68\x82\xf0\x8a\xd4\x88\xe1\x82\xc1\x82\xc4\x82\xa2\x82\xe9");
    return NULL;
}

static void Demo_StoreTradedPokemon(WorldTradeWork *wk) {
    PartyPkm *pkm = Demo_GetTradePokemon(wk, wk->subProcessMode);

    if (wk->evoPokeInfo.boxNo == 0xff) {
        WorldTrade_CopyPartyPkm(pkm, PokeParty_GetPkm(wk->param->myparty, wk->evoPokeInfo.pos));
    } else {
        int box = 0;
        int slot = 0;

        BoxSaveAccessor_ClearPkm(wk->param->mybox, wk->evoPokeInfo.boxNo, wk->evoPokeInfo.pos);
        BoxSaveAccessor_GetNextFreeBoxSlot(wk->param->mybox, &box, &slot);
        BoxSaveAccessor_InsertPkmCore(wk->param->mybox, box, PokeParty_ToBoxPkm(pkm));
    }
}
