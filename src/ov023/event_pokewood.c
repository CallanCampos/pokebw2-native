// Pokéstar Studios' (Pokewood's) field events: filming a movie against its cast, watching a recorded or downloaded
// one, and the small events that put up the screens of overlays 321 to 325. The name is the ROM's own, from
// GFL_HeapAllocate's file argument.
// Function and data names are our own: swan names none of this overlay.
#include "types.h"
#include "app/pkwd_count.h"
#include "battle/battle_proc.h"
#include "battle/battle_rec_tool.h"
#include "battle/btl_setup.h"
#include "constants/arc.h"
#include "constants/flags.h"
#include "constants/pokemon.h"
#include "constants/sound.h"
#include "constants/text_banks.h"
#include "field/event_pokewood.h"
#include "field/event_sound.h"
#include "field/field.h"
#include "field/field_event.h"
#include "field/field_sound.h"
#include "field/game_beacon_set.h"
#include "field/pokewood_system.h"
#include "gfl/arc.h"
#include "gfl/heap.h"
#include "gfl/msg.h"
#include "gfl/overlay.h"
#include "gfl/random.h"
#include "gfl/sound.h"
#include "gfl/std.h"
#include "gfl/str.h"
#include "pml/met_data.h"
#include "pml/poke_party.h"
#include "save/config.h"
#include "save/event_work.h"
#include "save/player_info.h"
#include "save/pokewood.h"
#include "save/save_control.h"
#include "system/game_data.h"
#include "system/game_event.h"
#include "system/game_system.h"
#include "text/system/event_pass_power_startmenu.h"

// The kinds of movie event, in PokewoodMovieParam's kind
enum {
    POKEWOOD_KIND_FILM = 1,
    POKEWOOD_KIND_WATCH,
};

// The slot of the movie that is still in the Pokewood block, the one just filmed
#define POKEWOOD_BLOCK_SLOT 8

// The species of the Pokéstar-only cast, which have their names in the movie text bank from this message on, offset
// by POKEWOOD_CAST_MSG_OFFSET
#define POKEWOOD_CAST_SPECIES_FIRST 652
#define POKEWOOD_CAST_SPECIES_LAST 683
#define POKEWOOD_CAST_MSG_OFFSET 488

// The cast of a movie: the players' and the opponents' lists in one arc file
typedef struct {
    s16 species;
    // The species the Pokémon is shown as. The Pokéstar-only cast has its own
    s16 shownSpecies;
    u16 level;
    u16 form;
    s16 sex;
    s16 nature;
    s16 ability;
    s16 heldItem;
    u16 moves[4];
    struct {
        s16 iv;
        s16 ev;
    } stats[6];
} PokewoodCastMember;

typedef struct {
    // How many Pokémon each side has: the player's side, then the opponent's. The members follow in that order
    s16 count[2];
    PokewoodCastMember members[1];
} PokewoodCastFile;

// A movie's data in arc 269, of which only these fields matter here
typedef struct {
    u8 unk0[0x18];
    // The trainer class of each side's trainer, for each sex of the player: the side that films, then the one that
    // watches
    s16 trainerClass[4];
    u8 unk20[2];
    s16 recordedTrainerClass;
    u8 unk24[2];
    s16 bgm;
    s16 bgm2;
    s16 unk2a;
    s16 unk2c;
} PokewoodMovieFile;

// What GameEvent_CreateOverlayDelegate hands the events that run a movie, which also is their data
typedef struct {
    GameSystem *gsys;
    u8 kind;
    // The movie to film
    u8 movie;
    // The slot of the movie to watch
    u32 slot;
    u16 heapId;
    PokewoodTitleParam title;
    PkwdBriefingParam briefing;
    PkwdBriefingParam result;
    StrBuf *introText;
    // How the downloaded movies of the slots are shown
    u8 slotInfo[8];
} PokewoodMovieParam;

// The data of the event that runs a movie
typedef struct {
    BtlSetup *setup;
    PokewoodMovieParam *param;
} PokewoodMovieEvent;

typedef struct {
    GameSystem *gsys;
    Field *field;
    PkwdBookParam param;
} PokewoodPickerEvent;

// The same for overlay 323
typedef struct {
    GameSystem *gsys;
    Field *field;
    PkwdSelectScreenParam param;
} PokewoodMenuEvent;

// The same for the record monitor, overlay 325
typedef struct {
    GameSystem *gsys;
    Field *field;
    PkwdRecordMonitorParam param;
} PokewoodScreenEvent;

// The data of the event that picks the cast
typedef struct {
    GameSystem *gsys;
    Field *field;
    PokewoodSystem *sys;
    PStatusParam status;
    PokeListParam pokeList;
} PokewoodCastEvent;

// How the battle's grade, which is 0 to 4, sets the number of ticks the movie runs
static const struct {
    u32 base;
    u32 range;
} sGradeTicks[] = {
    { 5, 5 }, { 7, 6 }, { 10, 9 }, { 16, 6 }, { 22, 1 },
};

// What each movie the downloaded movie's field 0 says is shown as, plus one. The original has room for four more
static const u8 sSlotInfo[POKEWOOD_MOVIE_COUNT + 4] = {
    0, 11, 11, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4,  4,  4,  5,  5,
    5, 6,  6,  6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 9, 9, 10, 10, 10, 10, 12,
};

// How each movie unlocks: by a kind that EventPokewood_UnlockMovies checks, and the kind's parameter
static const struct {
    u8 kind;
    u8 param;
} sMovieUnlocks[POKEWOOD_MOVIE_COUNT] = {
    { 0, 0 },  { 7, 0 }, { 1, 1 },  { 4, 0 },  { 1, 3 },  { 1, 4 },  { 1, 5 },  { 6, 0 },  { 1, 7 },  { 1, 8 },
    { 1, 9 },  { 2, 0 }, { 1, 11 }, { 1, 12 }, { 1, 13 }, { 6, 0 },  { 1, 15 }, { 1, 16 }, { 2, 0 },  { 1, 18 },
    { 1, 19 }, { 6, 0 }, { 1, 21 }, { 1, 22 }, { 3, 2 },  { 1, 24 }, { 1, 25 }, { 1, 26 }, { 6, 27 }, { 1, 28 },
    { 1, 29 }, { 3, 3 }, { 1, 31 }, { 1, 32 }, { 1, 33 }, { 2, 0 },  { 1, 35 }, { 1, 36 }, { 1, 37 }, { 5, 0 },
};

static u32 EventPokewood_GetTitleMsgBase(u32 kind, u32 gender);
static void EventPokewood_SetupMovieTrainer(u32 movie, u32 kind, BtlSetupTrainer *trainer, PlayerInfo *playerInfo,
                                            HeapID heapId);
static void EventPokewood_SetupRecordTrainer(u32 movie, BtlSetupTrainer *trainer, PlayerInfo *playerInfo,
                                             HeapID heapId);
static BtlSetup *EventPokewood_CreateFilmSetup(GameSystem *gsys, Field *field, HeapID heapId, u8 movie);
static void EventPokewood_FinishWatchSetup(GameData *gameData, BtlSetup *setup, u32 movie, HeapID heapId);
static BtlSetup *EventPokewood_LoadDownloadedSetup(GameSystem *gsys, Field *field, HeapID heapId, u32 slot);
static BtlSetup *EventPokewood_LoadBlockSetup(GameSystem *gsys, Field *field, HeapID heapId);
static BtlSetup *EventPokewood_LoadWatchSetup(GameSystem *gsys, Field *field, HeapID heapId, u8 slot);
static void EventPokewood_LoadIntroText(PokewoodMovieParam *param, BtlSetup *setup, u32 movie, u32 kind, HeapID heapId);
static BtlSetup *EventPokewood_CreateSetup(PokewoodMovieParam *param, GameSystem *gsys, GameData *gameData,
                                           Field *field, HeapID heapId);
static void EventPokewood_FreeSetup(BtlSetup *setup);
static void EventPokewood_StoreRecordedSetup(GameData *gameData, BtlSetup *setup, HeapID heapId);
static void EventPokewood_InitTitleParam(PokewoodTitleParam *title, BtlSetup *setup);
static void EventPokewood_FreeTitleParam(PokewoodTitleParam *title);
static void EventPokewood_InitBriefingParam(PkwdBriefingParam *briefing, BtlSetup *setup, PlayerInfo *playerInfo,
                                            GameData *gameData);
static void EventPokewood_InitResultParam(PkwdBriefingParam *result, BtlSetup *setup, PlayerInfo *playerInfo,
                                          GameData *gameData);
static void EventPokewood_OnBriefingDone(PkwdBriefingParam *briefing);
static void EventPokewood_ResumeFieldMusic(GameSystem *gsys, GameData *gameData, GameEvent *event, Field *field,
                                           FieldSound *fieldSound);
static GameEventReturnCode EventPokewood_RunWatch(GameEvent *event, u32 *state, void *data);
static void EventPokewood_LoadSlotInfo(GameData *gameData, HeapID heapId, PokewoodMovieParam *param);
static GameEventReturnCode EventPokewood_RunFilm(GameEvent *event, u32 *state, void *data);
static GameEvent *EventPokewood_CreateRunEvent(GameSystem *gsys, PokewoodMovieParam *param);
static GameEventReturnCode EventPokewood_RunMovie(GameEvent *event, u32 *state, void *data);
static GameEventReturnCode EventPokewood_RunPicker(GameEvent *event, u32 *state, void *data);
static GameEventReturnCode EventPokewood_RunMenu(GameEvent *event, u32 *state, void *data);
static GameEventReturnCode EventPokewood_RunCastSelect(GameEvent *event, u32 *state, void *data);
static void EventPokewood_OnMovieUnlocked(PokewoodSave *save, u32 movie);
static GameEventReturnCode EventPokewood_RunScreen(GameEvent *event, u32 *state, void *data);
static void EventPokewood_SetupRecordedOpponent(BtlSetup *setup, HeapID heapId);
static void EventPokewood_ShowCastNames(BtlSetup *setup, HeapID heapId);

// Builds the cast of one side of a movie as a party
void EventPokewood_CreateCastParty(PlayerInfo *playerInfo, PokeParty *dest, HeapID heapId, u32 movie, u32 side) {
    u32 gender;
    s32 i;
    s32 j;
    u32 trainerId;
    PartyPkm *pkm;
    PokeParty *party;
    PokewoodCastFile *file;
    s32 first;
    s16 count;

    trainerId = getIDAsUInt(playerInfo);
    gender = getTrainerGender(playerInfo);
    party = PokeParty_Create(heapId);
    pkm = GFL_HeapAllocate(heapId, PokeParty_GetPkmRawSize(), TRUE, "event_pokewood.c", 156);
    file = GFL_ArcSysReadHeapNew(ARCID_POKEWOOD_CAST, movie, heapId);
    count = file->count[side];
    PokeParty_InitCore(party, count);
    first = 0;
    if (side == 1) {
        first = file->count[0];
    }
    for (i = 0; i < count; i++) {
        PokewoodCastMember *member = &file->members[first + i];

        PokeParty_ClearPkm(pkm);
        PokeParty_CreatePkm(
            pkm, member->species, member->level, trainerId, PKM_IVS_RANDOM,
            makeSpecialPID(trainerId, member->species, member->form, member->sex, member->ability, FALSE));
        PokeParty_SetupMetData(pkm, 0, playerInfo, 4, heapId);
        for (j = 0; j < 4; j++) {
            PokeParty_SetMove(pkm, member->moves[j], j);
        }
        PokeParty_SetParam(pkm, PKM_PARAM_NATURE, member->nature);
        PokeParty_SetParam(pkm, PKM_PARAM_ITEM, member->heldItem);
        PokeParty_SetParam(pkm, PKM_PARAM_IV_HP, member->stats[0].iv);
        PokeParty_SetParam(pkm, PKM_PARAM_EV_HP, member->stats[0].ev);
        PokeParty_SetParam(pkm, PKM_PARAM_IV_ATTACK, member->stats[1].iv);
        PokeParty_SetParam(pkm, PKM_PARAM_EV_HP + 1, member->stats[1].ev);
        PokeParty_SetParam(pkm, PKM_PARAM_IV_DEFENSE, member->stats[2].iv);
        PokeParty_SetParam(pkm, PKM_PARAM_EV_HP + 2, member->stats[2].ev);
        PokeParty_SetParam(pkm, PKM_PARAM_IV_SPEED, member->stats[3].iv);
        PokeParty_SetParam(pkm, PKM_PARAM_EV_HP + 3, member->stats[3].ev);
        PokeParty_SetParam(pkm, PKM_PARAM_IV_SP_ATTACK, member->stats[4].iv);
        PokeParty_SetParam(pkm, PKM_PARAM_EV_HP + 4, member->stats[4].ev);
        PokeParty_SetParam(pkm, PKM_PARAM_IV_SP_DEFENSE, member->stats[5].iv);
        PokeParty_SetParam(pkm, PKM_PARAM_EV_HP + 5, member->stats[5].ev);
        PokeParty_RecalcStats(pkm);
        PokeParty_AddPkm(party, pkm);
    }
    GFL_HeapFree(file);
    PokeParty_Copy(party, dest);
    GFL_HeapFree(pkm);
    GFL_HeapFree(party);
}

void EventPokewood_CreateCastPartyFromSave(PlayerInfo *playerInfo, PokeParty *dest, HeapID heapId, u32 movie,
                                           u32 side) {
    EventPokewood_CreateCastParty(playerInfo, dest, heapId, movie, side);
}

// The first message of the movie's lines in the bank of its trainers' names, by the kind of the trainer and the sex
static u32 EventPokewood_GetTitleMsgBase(u32 kind, u32 gender) {
    if (kind == POKEWOOD_KIND_FILM && gender == 0) {
        return EventPassPowerStartmenu_Text_Brycen;
    }
    if (kind == POKEWOOD_KIND_FILM && gender == 1) {
        return EventPassPowerStartmenu_Text_Brycen_8;
    }
    if (kind == POKEWOOD_KIND_WATCH && gender == 0) {
        return EventPassPowerStartmenu_Text_BrycenMan;
    }
    if (kind == POKEWOOD_KIND_WATCH && gender == 1) {
        return EventPassPowerStartmenu_Text_BrycenMan_5;
    }
    return EventPassPowerStartmenu_Text_Brycen;
}

// Gives a trainer of the battle the class and name that the movie has for its kind
static void EventPokewood_SetupMovieTrainer(u32 movie, u32 kind, BtlSetupTrainer *trainer, PlayerInfo *playerInfo,
                                            HeapID heapId) {
    PokewoodMovieFile *file = GFL_ArcSysReadHeapNew(ARCID_POKEWOOD_MOVIE, movie, HEAPID_TAIL(heapId));
    unsigned int gender = getTrainerGender(playerInfo);
    MsgData *msgData;
    u32 msgBase;

    trainer->trainerId = 0;
    trainer->aiFlags = 0;
    if (kind == POKEWOOD_KIND_FILM) {
        trainer->trainerClass = file->trainerClass[gender * 2];
    } else if (kind == POKEWOOD_KIND_WATCH) {
        trainer->trainerClass = file->trainerClass[gender * 2 + 1];
    }
    msgData = GFL_MsgSysLoadData(FALSE, ARCID_SYSTEM_MESSAGE, TEXT_BANK_EVENT_PASS_POWER_STARTMENU, heapId);
    msgBase = EventPokewood_GetTitleMsgBase(kind, gender);
    GFL_MsgDataLoadStrbuf(msgData, msgBase + movie, trainer->name);
    GFL_MsgDataFree(msgData);
    PMSData_Clear(&trainer->unk18);
    PMSData_Clear(&trainer->unk20);
    GFL_HeapFree(file);
}

static void EventPokewood_SetupRecordTrainer(u32 movie, BtlSetupTrainer *trainer, PlayerInfo *playerInfo,
                                             HeapID heapId) {
    PokewoodMovieFile *file = GFL_ArcSysReadHeapNew(ARCID_POKEWOOD_MOVIE, movie, HEAPID_TAIL(heapId));
    u32 gender = getTrainerGender(playerInfo);

    trainer->trainerId = 1;
    trainer->aiFlags = 0;
    if (gender == 0) {
        trainer->trainerClass = file->recordedTrainerClass;
    } else if (gender == 1) {
        trainer->trainerClass = file->recordedTrainerClass + 1;
    }
    GFL_HeapFree(file);
}

// Sets up the battle of a movie that the player films
static BtlSetup *EventPokewood_CreateFilmSetup(GameSystem *gsys, Field *field, HeapID heapId, u8 movie) {
    GameData *gameData = GSYS_GetGameData(gsys);
    BtlSetup *setup = BtlSetup_Create(heapId);
    PokewoodSave *save = PokewoodSave_Get(gameData);
    void *buffer = GFL_HeapAllocate(heapId, PokeParty_GetPkmRawSize(), TRUE, "event_pokewood.c", 329);
    PlayerInfo *playerInfo = GetGameDataPlayerInfo(gameData);
    PokewoodSystem **sys = GameData_GetPokewoodSystemPtr(gameData);
    BtlFieldStatus status;
    u32 grade;

    ResetBtlFieldStatus(&status);
    status.bgId = 9;
    status.terrain = 14;
    BtlSetup_SetTrainer1v1Single(setup, gameData, &status, 0, heapId);
    BtlSetup_AllocRecBuffer(setup, heapId);
    EventPokewood_SetupMovieTrainer(movie, POKEWOOD_KIND_FILM, setup->trainers[1], playerInfo, heapId);
    EventPokewood_CreateCastParty(playerInfo, setup->party[1], heapId, movie, 1);
    setup->unk34[0] = GetGameDataPlayerInfo(gameData);
    if (PokewoodSystem_GetUsePlayerCast(*sys) == 1) {
        PokewoodSystem_CopyCast(*sys, gameData, setup->party[0]);
        setup->unkE0 = 0;
    } else {
        EventPokewood_CreateCastParty(playerInfo, setup->party[0], heapId, movie, 0);
        setup->unkE0 = 1;
    }
    GFL_HeapFree(buffer);
    setup->unkDD_3 = 1;
    setup->unkDF = movie;
    grade = PokewoodSave_GetLevelOfPoints(PokewoodSave_GetLevelPoints(save));
    setup->unk128 = sGradeTicks[grade].base + GFL_RandomLC(sGradeTicks[grade].range);
    setup->unk129 = grade;
    return setup;
}

// Finishes the setup of a movie that is watched: the cast and its trainers
static void EventPokewood_FinishWatchSetup(GameData *gameData, BtlSetup *setup, u32 movie, HeapID heapId) {
    PlayerInfo *playerInfo = GetGameDataPlayerInfo(gameData);

    EventPokewood_CreateCastParty(playerInfo, setup->party[1], heapId, movie, 1);
    EventPokewood_ShowCastNames(setup, heapId);
    EventPokewood_SetupRecordedOpponent(setup, heapId);
}

static BtlSetup *EventPokewood_LoadDownloadedSetup(GameSystem *gsys, Field *field, HeapID heapId, u32 slot) {
    GameData *gameData = GSYS_GetGameData(gsys);
    BtlSetup *setup;
    u32 movie;

    if (Pokewood_LoadDownloadedMovie(GameData_GetSaveControl(gameData), heapId, slot) == 1) {
        setup = BtlSetup_Create(heapId);
        BtlSetup_Reset(setup);
        BtlSetup_InitForRecord(setup, gameData, heapId);
        BattleRec_LoadToolOverlay();
        movie = Pokewood_GetLoadedMovieField(0, 0);
        BattleRecTool_LoadSetup(setup, 1, heapId);
        BattleRec_UnloadToolOverlay();
        EventPokewood_FinishWatchSetup(gameData, setup, movie, heapId);
        return setup;
    }
    return NULL;
}

static BtlSetup *EventPokewood_LoadBlockSetup(GameSystem *gsys, Field *field, HeapID heapId) {
    GameData *gameData = GSYS_GetGameData(gsys);
    BtlSetup *setup;

    loadPokewoodAndCheck(GameData_GetSaveControl(gameData), heapId, 0);
    setup = BtlSetup_Create(heapId);
    BtlSetup_Reset(setup);
    BtlSetup_InitForRecord(setup, gameData, heapId);
    BattleRec_LoadToolOverlay();
    BattleRecTool_LoadSetup(setup, 1, heapId);
    BattleRec_UnloadToolOverlay();
    EventPokewood_FinishWatchSetup(gameData, setup, setup->unkDF, heapId);
    return setup;
}

static BtlSetup *EventPokewood_LoadWatchSetup(GameSystem *gsys, Field *field, HeapID heapId, u8 slot) {
    BtlSetup *setup;

    if (slot == POKEWOOD_BLOCK_SLOT) {
        return EventPokewood_LoadBlockSetup(gsys, field, heapId);
    }
    setup = EventPokewood_LoadDownloadedSetup(gsys, field, heapId, slot);
    GameBeacon_BroadcastPokestarWatching(setup->unkDF);
    return setup;
}

// Loads the lines the movie opens with
static void EventPokewood_LoadIntroText(PokewoodMovieParam *param, BtlSetup *setup, u32 movie, u32 kind,
                                        HeapID heapId) {
    u32 gender = getTrainerGender(setup->unk34[0]);
    u32 isMale = TRUE;
    MsgData *msgData;

    if (gender != 0) {
        isMale = FALSE;
    }
    msgData = GFL_MsgSysLoadData(FALSE, ARCID_SYSTEM_MESSAGE,
                                 TEXT_BANK_PKWD_BOOK_MOVIE_00_GIRL + movie * 2 + (u8)isMale, heapId);
    if (kind == POKEWOOD_KIND_FILM) {
        param->introText = GFL_MsgDataLoadStrbufNew(msgData, 0);
    } else if (kind == POKEWOOD_KIND_WATCH) {
        param->introText = GFL_MsgDataLoadStrbufNew(msgData, 1);
    }
    setup->trainerNames[1] = param->introText;
    GFL_MsgDataFree(msgData);
}

// Sets up the battle of the movie that the event runs
static BtlSetup *EventPokewood_CreateSetup(PokewoodMovieParam *param, GameSystem *gsys, GameData *gameData,
                                           Field *field, HeapID heapId) {
    PokewoodSave *save = PokewoodSave_Get(gameData);
    BtlSetup *setup;
    PokewoodMovieFile *file;

    switch (param->kind) {
    case 0:
    case POKEWOOD_KIND_FILM:
        setup = EventPokewood_CreateFilmSetup(gsys, field, heapId, param->movie);
        setup->fieldSituation.bgm = SEQ_BGM_PW_FILMING;
        setup->fieldSituation.unk12 = SEQ_BGM_WIN1;
        EventPokewood_LoadIntroText(param, setup, param->movie, POKEWOOD_KIND_FILM, heapId);
        break;
    case POKEWOOD_KIND_WATCH:
        setup = EventPokewood_LoadWatchSetup(gsys, field, heapId, param->slot);
        file = GFL_ArcSysReadHeapNew(ARCID_POKEWOOD_MOVIE, setup->unkDF, HEAPID_TAIL(heapId));
        setup->fieldSituation.bgm = file->bgm;
        setup->fieldSituation.unk12 = file->bgm2;
        setup->unk138 = file->unk2a;
        setup->unk13a = file->unk2c;
        EventPokewood_LoadIntroText(param, setup, setup->unkDF, POKEWOOD_KIND_WATCH, heapId);
        GFL_HeapFree(file);
        Config_SetBattleScene(setup->config, 0);
        setup->unkDD_7 = 1;
        if (setup->unkDF < POKEWOOD_MOVIE_COUNT && func_020111a0(save, param->slot) != 0) {
            setup->unkDD_7 = 0;
        }
        break;
    }
    setup->unk98 = 8;
    return setup;
}

static void EventPokewood_FreeSetup(BtlSetup *setup) {
    if (setup->unkDD_3 == 2) {
        GFL_HeapFree(setup->unk34[0]);
        BtlSetup_FreeForRecord(setup);
    }
    BtlSetup_Free(setup);
}

// Keeps the battle of the film as a recording that can be watched
static void EventPokewood_StoreRecordedSetup(GameData *gameData, BtlSetup *setup, HeapID heapId) {
    GameData_GetPokewoodSystemPtr(gameData);
    BattleRec_LoadToolOverlay();
    setup->trainers[0] = TrainerBattleSetup_Create(heapId);
    EventPokewood_SetupRecordTrainer(setup->unkDF, setup->trainers[0], setup->unk34[0], heapId);
    BattleRecTool_StoreSetup(setup);
    BattleRec_UnloadToolOverlay();
}

static void EventPokewood_InitTitleParam(PokewoodTitleParam *title, BtlSetup *setup) {
    StrBuf *name = GFL_StrBufCreate(16, HEAPID_GAMEEVENT);
    PartyPkm *pkm = GFL_HeapAllocate(HEAPID_GAMEEVENT, PokeParty_GetPkmRawSize(), FALSE, "event_pokewood.c", 694);
    u32 species;
    MsgData *msgData;

    BattleRec_LoadToolOverlay();
    Pokewood_GetLoadedMovieField(2, (uptr)pkm);
    species = PokeParty_GetParam(pkm, PKM_PARAM_SPECIES, NULL);
    msgData = GFL_MsgSysLoadData(FALSE, ARCID_SYSTEM_MESSAGE, TEXT_BANK_SPECIES_NAMES, HEAPID_TAIL(HEAPID_GAMEEVENT));
    GFL_MsgDataLoadStrbuf(msgData, species, name);
    GFL_MsgDataFree(msgData);
    title->title = name;
    BattleRec_UnloadToolOverlay();
    GFL_HeapFree(pkm);
    title->movie = setup->unkDF;
    title->gender = getTrainerGender(setup->unk34[0]);
}

static void EventPokewood_FreeTitleParam(PokewoodTitleParam *title) {
    GFL_StrBufFree(title->title);
}

static void EventPokewood_InitBriefingParam(PkwdBriefingParam *briefing, BtlSetup *setup, PlayerInfo *playerInfo,
                                            GameData *gameData) {
    briefing->movie = setup->unkDF;
    briefing->audienceCount = setup->unk128;
    briefing->setup = setup;
    briefing->playerInfo = playerInfo;
    briefing->noSkip = setup->unkDD_7;
}

static void EventPokewood_InitResultParam(PkwdBriefingParam *result, BtlSetup *setup, PlayerInfo *playerInfo,
                                          GameData *gameData) {
    result->saveControl = GameData_GetSaveControl(gameData);
    result->gameData = gameData;
    result->playerInfo = playerInfo;
    result->audienceCount = setup->unk128;
    result->audience = setup->unk12C;
    result->setup = setup;
    result->movie = setup->unkDF;
    result->noSkip = setup->unkDD_7;
}

static void EventPokewood_OnBriefingDone(PkwdBriefingParam *briefing) {
}

static void EventPokewood_ResumeFieldMusic(GameSystem *gsys, GameData *gameData, GameEvent *event, Field *field,
                                           FieldSound *fieldSound) {
    u8 season = GameData_GetSeason(gameData);
    u32 bgm = GetMapBGMIDByPlayerState2(gameData, Field_GetPlayerStateZoneID(field), season);

    GameEvent_ChainNext(event, EventBGMChange_Create(gsys, bgm, 30, 60));
    FieldSnd_ResumeAmbience(fieldSound, 2);
}

// Runs a movie that is watched: the title card, the briefing, the battle, and the screen after it
static GameEventReturnCode EventPokewood_RunWatch(GameEvent *event, u32 *state, void *data) {
    PokewoodMovieEvent *wk = data;
    PokewoodMovieParam *param = wk->param;
    GameSystem *gsys = param->gsys;
    Field *field = GSYS_GetField(gsys);
    GameData *gameData = GSYS_GetGameData(gsys);
    FieldSound *fieldSound = GameData_GetFieldSoundSystem(gameData);
    PlayerInfo *playerInfo = GetGameDataPlayerInfo(gameData);
    PokewoodSave *save = PokewoodSave_Get(gameData);

    switch (*state) {
    case 0:
        wk->setup = EventPokewood_CreateSetup(param, gsys, gameData, field, param->heapId);
        GameEvent_ChainNext(event, CreateBGMFadeOutEvent(gsys, 20));
        (*state)++;
        break;
    case 1:
        GameEvent_ChainNext(event, CreateFieldCloseEvent(gsys, field));
        (*state)++;
        break;
    case 2:
        EventPokewood_InitTitleParam(&param->title, wk->setup);
        GSYS_QueueProcAsEvent(event, OVERLAY_PKWD_COUNT, &PKWD_COUNT_PROC_FUNCTIONS, &param->title);
        (*state)++;
        break;
    case 3:
        EventPokewood_FreeTitleParam(&param->title);
        (*state)++;
        break;
    case 4:
        EventPokewood_InitBriefingParam(&param->briefing, wk->setup, playerInfo, gameData);
        GSYS_QueueProcAsEvent(event, OVERLAY_PKWD_BRIEFING, &PKWD_BRIEFING_PROC_FUNCTIONS, &param->briefing);
        (*state)++;
        break;
    case 5:
        wk->setup->unk12C = param->briefing.audience;
        if (param->briefing.skip != 0) {
            *state = 8;
        } else {
            (*state)++;
        }
        EventPokewood_OnBriefingDone(&param->briefing);
        break;
    case 6:
        GFL_SndBGMPlay(wk->setup->fieldSituation.bgm, 0xffff);
        GSYS_QueueProcAsEvent(event, OVERLAY_ID(167), &data_ov167_021d6ce0, wk->setup);
        (*state)++;
        break;
    case 7:
        if (wk->setup->unk134 == 0) {
            EventPokewood_InitResultParam(&param->result, wk->setup, playerInfo, gameData);
            GSYS_QueueProcAsEvent(event, OVERLAY_PKWD_BRIEFING, &PKWD_BRIEFING_CREDITS_PROC_FUNCTIONS, &param->result);
        }
        (*state)++;
        break;
    case 8:
        GameEvent_ChainNext(event, EventFieldOpen_CreateHeadless(gsys));
        (*state)++;
        break;
    case 9:
        EventPokewood_ResumeFieldMusic(gsys, gameData, event, field, fieldSound);
        (*state)++;
        break;
    case 10: {
        PokewoodSystem *sys;
        u16 *var;

        GFL_StrBufFree(param->introText);
        if (wk->setup->unkDF < POKEWOOD_MOVIE_COUNT) {
            sys = *GameData_GetPokewoodSystemPtr(gameData);
            var = PokewoodSystem_GetResultVar(sys);
            if (var != NULL) {
                *var = (wk->setup->unkA8 != 3) ? TRUE : FALSE;
                if (*var != 0) {
                    PokewoodSystem_SetResultFlag(sys, gameData, wk->setup->unkDF, wk->setup->unk124);
                    PokewoodSystem_AddGradePoints(gameData, sys, wk->setup->unk124, param->slot);
                    PokewoodSave_SetFlag(save, 5, wk->setup->unkDF);
                    Pokewood_CheckMedals(gameData);
                    EventPokewood_UnlockMovies(save, gameData, wk->setup->unkDF, wk->setup->unk124, wk->setup->unk130);
                }
            }
        }
        EventPokewood_FreeSetup(wk->setup);
        (*state)++;
        break;
    }
    case 11:
        return GAMEEVENT_DONE;
    }
    return GAMEEVENT_CONTINUE;
}

// Looks up which of the downloaded movies of the slots load, and what each is shown as
static void EventPokewood_LoadSlotInfo(GameData *gameData, HeapID heapId, PokewoodMovieParam *param) {
    PokewoodSave *save = PokewoodSave_Get(gameData);
    SaveControl *saveControl = GameData_GetSaveControl(gameData);
    int slot;

    BattleRec_LoadToolOverlay();
    for (slot = 0; slot < 8; slot++) {
        if (Pokewood_LoadDownloadedMovie(saveControl, heapId, slot) == 1) {
            param->slotInfo[slot] = sSlotInfo[Pokewood_GetLoadedMovieField(0, 0)] + 1;
        } else {
            param->slotInfo[slot] = 0;
        }
    }
    BattleRec_UnloadToolOverlay();
}

// Runs a movie that the player films: the battle, and what it records
static GameEventReturnCode EventPokewood_RunFilm(GameEvent *event, u32 *state, void *data) {
    PokewoodMovieEvent *wk = data;
    PokewoodMovieParam *param = wk->param;
    GameSystem *gsys = param->gsys;
    Field *field = GSYS_GetField(gsys);
    GameData *gameData = GSYS_GetGameData(gsys);
    FieldSound *fieldSound = GameData_GetFieldSoundSystem(gameData);

    switch (*state) {
    case 0:
        wk->setup = EventPokewood_CreateSetup(param, gsys, gameData, field, param->heapId);
        GameEvent_ChainNext(event, EventBattleBGMPlay_Create(gsys, wk->setup->fieldSituation.bgm));
        (*state)++;
        break;
    case 1:
        EncEff_StartEvent(Field_GetEncEff(field), event, 29);
        (*state)++;
        break;
    case 2:
        GameEvent_ChainNext(event, CreateFieldCloseEvent(gsys, field));
        (*state)++;
        break;
    case 3:
        FieldSnd_SetPlayerVolumeFade(fieldSound, 127, 1);
        GSYS_QueueProcAsEvent(event, OVERLAY_ID(167), &data_ov167_021d6ce0, wk->setup);
        (*state)++;
        break;
    case 4:
        if (wk->setup->unkA8 != 3) {
            EventPokewood_LoadSlotInfo(gameData, param->heapId, param);
            EventPokewood_StoreRecordedSetup(gameData, wk->setup, param->heapId);
        }
        (*state)++;
        break;
    case 5:
        GameEvent_ChainNext(event, EventBGMFadePop_Create(gsys));
        (*state)++;
        break;
    case 6:
        GameEvent_ChainNext(event, EventFieldOpen_CreateHeadless(gsys));
        (*state)++;
        break;
    case 7:
        GameEvent_ChainNext(event, EventBGMFadeWait_Create(gsys));
        (*state)++;
        break;
    case 8:
        GameEvent_ChainNext(event, CallFieldMapEntranceInTransition(gsys, field, 0, 0, 1, 0, 0));
        (*state)++;
        break;
    case 9: {
        PokewoodSystem *sys;
        u16 *var;

        GFL_StrBufFree(param->introText);
        sys = *GameData_GetPokewoodSystemPtr(gameData);
        GameData_GetRecords(gameData);
        var = PokewoodSystem_GetResultVar(sys);
        if (var != NULL) {
            *var = (wk->setup->unkA8 != 3) ? TRUE : FALSE;
        }
        PokewoodSystem_SetRecord(sys, wk->setup, wk->setup->unkDF);
        PokewoodSystem_SetSlotInfo(sys, param->slotInfo);
        EventPokewood_FreeSetup(wk->setup);
        (*state)++;
        break;
    }
    case 10:
        return GAMEEVENT_DONE;
    }
    return GAMEEVENT_CONTINUE;
}

static GameEvent *EventPokewood_CreateRunEvent(GameSystem *gsys, PokewoodMovieParam *param) {
    GameEvent *event;
    PokewoodMovieEvent *wk;

    if (param->kind == POKEWOOD_KIND_FILM) {
        event = GameEvent_Create(gsys, NULL, EventPokewood_RunFilm, sizeof(PokewoodMovieEvent));
    } else {
        event = GameEvent_Create(gsys, NULL, EventPokewood_RunWatch, sizeof(PokewoodMovieEvent));
    }
    wk = GameEvent_GetData(event);
    wk->param = param;
    return event;
}

// Runs a movie, keeping its data in the Pokewood block meanwhile
static GameEventReturnCode EventPokewood_RunMovie(GameEvent *event, u32 *state, void *data) {
    PokewoodMovieParam *param = data;
    GameSystem *gsys = param->gsys;
    PokewoodSystem *sys = *GameData_GetPokewoodSystemPtr(GSYS_GetGameData(gsys));

    switch (*state) {
    case 0:
        allocatePokewoodBlk(param->heapId);
        GameEvent_ChainNext(event, EventPokewood_CreateRunEvent(gsys, param));
        *state = 1;
        break;
    case 1:
        savePokewoodBlk(PokewoodSystem_GetPokewoodBlk(sys));
        freeAndClearPokewoodBlk();
        return GAMEEVENT_DONE;
    }
    return GAMEEVENT_CONTINUE;
}

// Films a movie: args are the slot and the movie
GameEvent *EventPokewood_CreateFilmEvent(GameSystem *gsys, void *args) {
    u32 *values = args;
    GameEvent *event = GameEvent_Create(gsys, NULL, EventPokewood_RunMovie, sizeof(PokewoodMovieParam));
    PokewoodMovieParam *param = GameEvent_GetData(event);

    param->gsys = gsys;
    param->slot = values[0];
    param->movie = values[1];
    param->heapId = HEAPID_GAMEEVENT;
    param->kind = POKEWOOD_KIND_FILM;
    return event;
}

// Watches the downloaded movie of the slot args points to
GameEvent *EventPokewood_CreateWatchSlotEvent(GameSystem *gsys, void *args) {
    u32 slot = *(u32 *)args;
    GameEvent *event = GameEvent_Create(gsys, NULL, EventPokewood_RunMovie, sizeof(PokewoodMovieParam));
    PokewoodMovieParam *param = GameEvent_GetData(event);

    param->gsys = gsys;
    param->heapId = HEAPID_GAMEEVENT;
    param->slot = slot;
    param->kind = POKEWOOD_KIND_WATCH;
    return event;
}

// Watches the movie that was just filmed
GameEvent *EventPokewood_CreateWatchBlockEvent(GameSystem *gsys, void *args) {
    GameEvent *event = GameEvent_Create(gsys, NULL, EventPokewood_RunMovie, sizeof(PokewoodMovieParam));
    PokewoodMovieParam *param = GameEvent_GetData(event);

    param->gsys = gsys;
    param->heapId = HEAPID_GAMEEVENT;
    param->slot = POKEWOOD_BLOCK_SLOT;
    param->kind = POKEWOOD_KIND_WATCH;
    return event;
}

// Puts up the screen of overlay 322 that picks a movie
GameEvent *EventPokewood_CreateMoviePickerEvent(GameSystem *gsys, void *args) {
    Field *field = GSYS_GetField(gsys);
    GameEvent *event = GameEvent_Create(gsys, NULL, EventPokewood_RunPicker, sizeof(PokewoodPickerEvent));
    PokewoodPickerEvent *wk = GameEvent_GetData(event);

    MI_CpuFill8(wk, 0, sizeof(PokewoodPickerEvent));
    wk->gsys = gsys;
    wk->field = field;
    wk->param = *(PkwdBookParam *)args;
    return event;
}

static GameEventReturnCode EventPokewood_RunPicker(GameEvent *event, u32 *state, void *data) {
    PokewoodPickerEvent *wk = data;

    switch (*state) {
    case 0:
        GameEvent_ChainNext(event, EventFieldSubprocessTransition_Create(wk->gsys, wk->field, OVERLAY_ID(322),
                                                                         &PKWD_BOOK_PROC_FUNCTIONS, &wk->param));
        *state = 1;
        break;
    case 1:
        if (wk->param.result != NULL && *wk->param.result == -1) {
            *wk->param.result = 41;
        } else {
            PokewoodSystem **sys = GameData_GetPokewoodSystemPtr(GSYS_GetGameData(wk->gsys));

            PokewoodSystem_SetMovie(*sys, *wk->param.result);
        }
        return GAMEEVENT_DONE;
    }
    return GAMEEVENT_CONTINUE;
}

GameEvent *EventPokewood_CreateMenuEvent(GameSystem *gsys, void *args) {
    Field *field = GSYS_GetField(gsys);
    GameEvent *event = GameEvent_Create(gsys, NULL, EventPokewood_RunMenu, sizeof(PokewoodMenuEvent));
    PokewoodMenuEvent *wk = GameEvent_GetData(event);

    MI_CpuFill8(wk, 0, sizeof(PokewoodMenuEvent));
    wk->gsys = gsys;
    wk->field = field;
    wk->param = *(PkwdSelectScreenParam *)args;
    return event;
}

static GameEventReturnCode EventPokewood_RunMenu(GameEvent *event, u32 *state, void *data) {
    PokewoodMenuEvent *wk = data;

    switch (*state) {
    case 0:
        GameEvent_ChainNext(event,
                            EventFieldSubprocessTransition_Create(wk->gsys, wk->field, OVERLAY_ID(323),
                                                                  &PKWD_SELECTSCREEN_PROC_FUNCTIONS, &wk->param));
        *state = 1;
        break;
    case 1:
        if (wk->param.result != NULL && *wk->param.result == PKWD_SELECTSCREEN_CANCEL) {
            *wk->param.result = PKWD_SELECTSCREEN_CANCEL;
        } else {
            GSYS_GetGameData(wk->gsys);
        }
        return GAMEEVENT_DONE;
    }
    return GAMEEVENT_CONTINUE;
}

// Picks the cast from the party
static GameEventReturnCode EventPokewood_RunCastSelect(GameEvent *event, u32 *state, void *data) {
    PokewoodCastEvent *wk = data;
    GameSystem *gsys = wk->gsys;

    switch (*state) {
    case 0:
        GameEvent_ChainNext(event, CallFieldMapEntranceOutTransitionDefault(gsys, wk->field, 0, 0));
        (*state)++;
        break;
    case 1:
        GameEvent_ChainNext(event, CreateFieldCloseEvent(gsys, wk->field));
        (*state)++;
        break;
    case 2:
        GameEvent_ChainNext(event, EventPokeList_Create(gsys, wk->field, &wk->pokeList, &wk->status));
        (*state)++;
        break;
    case 3:
        GameEvent_ChainNext(event, EventFieldOpen_CreateHeadless(gsys));
        (*state)++;
        break;
    case 4:
        GameEvent_ChainNext(event, CallFieldMapEntranceInTransition(gsys, wk->field, 0, 0, 1, 0, 0));
        (*state)++;
        break;
    case 5:
        PokewoodSystem_SetCastFromPokeList(wk->sys, &wk->pokeList);
        return GAMEEVENT_DONE;
    }
    return GAMEEVENT_CONTINUE;
}

GameEvent *EventPokewood_CreateCastSelectEvent(GameSystem *gsys, void *args) {
    PokewoodSystem *sys = args;
    GameData *gameData = GSYS_GetGameData(gsys);
    Field *field = GSYS_GetField(gsys);
    GameEvent *event = GameEvent_Create(gsys, NULL, EventPokewood_RunCastSelect, sizeof(PokewoodCastEvent));
    PokewoodCastEvent *wk = GameEvent_GetData(event);

    wk->gsys = gsys;
    wk->field = field;
    wk->sys = sys;
    PokewoodSystem_SetupPokeList(sys, gameData, &wk->pokeList);
    PokewoodSystem_SetupPStatus(sys, gameData, &wk->status);
    return event;
}

static void EventPokewood_OnMovieUnlocked(PokewoodSave *save, u32 movie) {
}

// Unlocks the movies whose conditions the player now meets
void EventPokewood_UnlockMovies(PokewoodSave *save, GameData *gameData, u32 movie, u32 result, s32 score) {
    int i;

    for (i = 0; i < POKEWOOD_MOVIE_COUNT; i++) {
        u32 count = 0;
        int j;

        if (PokewoodSave_CheckFlag(save, 0, i)) {
            continue;
        }
        switch (sMovieUnlocks[i].kind) {
        case 1:
            if (movie == sMovieUnlocks[i].param && result == 1) {
                PokewoodSave_SetFlag(save, 0, i);
                EventPokewood_OnMovieUnlocked(save, i);
            }
            break;
        case 3:
            if (PokewoodSave_GetLevelOfPoints(PokewoodSave_GetLevelPoints(save)) >= sMovieUnlocks[i].param) {
                PokewoodSave_SetFlag(save, 0, i);
                EventPokewood_OnMovieUnlocked(save, i);
            }
            break;
        case 4:
            if (score >= 5000) {
                PokewoodSave_SetFlag(save, 0, i);
                EventPokewood_OnMovieUnlocked(save, i);
            }
            break;
        case 2:
            if (EventWork_FlagGet(GameData_GetEventWork(gameData), EVENT_FLAG_0x0960)) {
                PokewoodSave_SetFlag(save, 0, i);
                EventPokewood_OnMovieUnlocked(save, i);
            }
            break;
        case 7:
            if (PokewoodSave_CheckFlag(save, 2, sMovieUnlocks[i].param) &&
                EventWork_FlagGet(GameData_GetEventWork(gameData), EVENT_FLAG_0x0960)) {
                PokewoodSave_SetFlag(save, 0, i);
                EventPokewood_OnMovieUnlocked(save, i);
            }
            break;
        case 5:
            for (j = 0; j < POKEWOOD_MOVIE_COUNT - 1; j++) {
                count += PokewoodSave_CheckFlag(save, 2, j);
            }
            if (count == POKEWOOD_MOVIE_COUNT - 1) {
                PokewoodSave_SetFlag(save, 0, i);
                EventPokewood_OnMovieUnlocked(save, i);
            }
            break;
        case 6:
            if (PokewoodSave_CheckFlag(save, 5, 0)) {
                PokewoodSave_SetFlag(save, 0, i);
                EventPokewood_OnMovieUnlocked(save, i);
            }
            break;
        }
    }
}

// How the movie unlocks: 1 to 7, as the switch of EventPokewood_UnlockMovies has it
u8 EventPokewood_GetUnlockKind(u32 movie) {
    return sMovieUnlocks[movie].kind;
}

// Puts up the screen of overlay 325
GameEvent *EventPokewood_CreateScreenEvent(GameSystem *gsys, void *args) {
    Field *field = GSYS_GetField(gsys);
    GameEvent *event = GameEvent_Create(gsys, NULL, EventPokewood_RunScreen, sizeof(PokewoodScreenEvent));
    PokewoodScreenEvent *wk = GameEvent_GetData(event);

    MI_CpuFill8(wk, 0, sizeof(PokewoodScreenEvent));
    wk->gsys = gsys;
    wk->field = field;
    wk->param = *(PkwdRecordMonitorParam *)args;
    return event;
}

static GameEventReturnCode EventPokewood_RunScreen(GameEvent *event, u32 *state, void *data) {
    PokewoodScreenEvent *wk = data;

    switch (*state) {
    case 0:
        GameEvent_ChainNext(event,
                            EventFieldSubprocessTransition_Create(wk->gsys, wk->field, OVERLAY_PKWD_RECORD_MONITOR,
                                                                  &PKWD_RECORD_MONITOR_PROC_FUNCTIONS, &wk->param));
        *state = 1;
        break;
    case 1:
        return GAMEEVENT_DONE;
    }
    return GAMEEVENT_CONTINUE;
}

// Gives the opponent of a movie that is watched its class and name
static void EventPokewood_SetupRecordedOpponent(BtlSetup *setup, HeapID heapId) {
    EventPokewood_SetupMovieTrainer(setup->unkDF, POKEWOOD_KIND_WATCH, setup->trainers[1], setup->unk34[0], heapId);
    setup->trainers[1]->trainerId = 1;
}

// Shows the cast that has Pokéstar-only species under their names
static void EventPokewood_ShowCastNames(BtlSetup *setup, HeapID heapId) {
    MsgData *msgData =
        GFL_MsgSysLoadData(FALSE, ARCID_SYSTEM_MESSAGE, TEXT_BANK_EVENT_PASS_POWER_STARTMENU, HEAPID_TAIL(heapId));
    PokewoodCastFile *file = GFL_ArcSysReadHeapNew(ARCID_POKEWOOD_CAST, setup->unkDF, HEAPID_TAIL(heapId));
    PokeParty *party = setup->party[1];
    int i;

    for (i = 0; i < PokeParty_GetPkmCount(party); i++) {
        PartyPkm *pkm = PokeParty_GetPkm(party, i);

        if (PokeParty_GetParam(pkm, PKM_PARAM_SPECIES_VALID, NULL) != 0) {
            PokewoodCastMember *member = &file->members[file->count[0] + i];

            if (member->species == PokeParty_GetParam(pkm, PKM_PARAM_SPECIES, NULL)) {
                PokeParty_SetParam(pkm, PKM_PARAM_SPECIES, member->shownSpecies);
                PokeParty_RecalcStats(pkm);
                if (member->shownSpecies >= POKEWOOD_CAST_SPECIES_FIRST &&
                    member->shownSpecies <= POKEWOOD_CAST_SPECIES_LAST) {
                    StrBuf *name = GFL_MsgDataLoadStrbufNew(msgData, member->shownSpecies - POKEWOOD_CAST_MSG_OFFSET);

                    PokeParty_SetParam(pkm, PKM_PARAM_NICKNAME, (uptr)name);
                    GFL_StrBufFree(name);
                }
            }
        }
    }
    GFL_HeapFree(file);
    GFL_MsgDataFree(msgData);
}
