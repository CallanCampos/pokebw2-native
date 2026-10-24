#include "types.h"
#include "constants/arc.h"
#include "constants/hold_effects.h"
#include "constants/pokemon.h"
#include "constants/species.h"
#include "constants/zones.h"
#include "gfl/arc.h"
#include "gfl/heap.h"
#include "pml/evolution.h"
#include "pml/item.h"
#include "pml/personal.h"
#include "pml/poke_party.h"
#include "system/rtc.h"

// Whether a Pokémon evolves, by its species' evolution table. The ROM's string names the file. The functions' and
// the zone lists' names are swan's (https://github.com/ds-pokemon-hacking/swan, GPL-3.0)

// The friendship the friendship methods take
#define EVO_FRIENDSHIP 220

static void getEvolutionInfoFile(u16 species, Evolution *dest);
static BOOL CheckPokeMoveLearned(PartyPkm *pkm, u16 move);

// The zones of the level-up methods that take a place. No species uses the fourth list's method, 28
static u16 EVOLVE_ICEROCK_ZONES[] = { ZONE_TWIST_MOUNTAIN_6 };
static u16 EVOLVE_MOSSROCK_ZONES[] = { ZONE_PINWHEEL_FOREST_2 };
static u16 EVOLVE_ELEC_ZONES[] = { ZONE_CHARGESTONE_CAVE_2, ZONE_CHARGESTONE_CAVE_3, ZONE_CHARGESTONE_CAVE_4 };
static u16 EVOLVE_ELEC2_ZONES[] = { ZONE_CHARGESTONE_CAVE_2, ZONE_CHARGESTONE_CAVE_3, ZONE_CHARGESTONE_CAVE_4 };

u16 CheckEvolveSpecies(PokeParty *party, PartyPkm *pkm, u32 mode, uptr a3, u8 season, u32 *method, HeapID heapId) {
    Evolution *evo;
    u16 species;
    u16 item;
    u16 friendship;
    u16 pidHigh;
    u32 pid;
    u8 level;
    u8 beauty;
    u8 holdEffect;
    int i;
    u32 j;
    u16 result = 0;
    u32 dummy;

    if (PokeParty_GetParam(pkm, PKM_PARAM_IS_EGG, NULL)) {
        return 0;
    }
    species = PokeParty_GetParam(pkm, PKM_PARAM_SPECIES, NULL);
    item = PokeParty_GetParam(pkm, PKM_PARAM_ITEM, NULL);
    pid = PokeParty_GetParam(pkm, PKM_PARAM_PID, NULL);
    beauty = PokeParty_GetParam(pkm, PKM_PARAM_CONTEST_BEAUTY, NULL);
    pidHigh = (pid & 0xffff0000) >> 16;
    holdEffect = GetItemParam(item, ITEM_PARAM_HOLD_EFFECT, heapId);
    // Kadabra evolves even with an Everstone
    if (species != SPECIES_KADABRA && holdEffect == HOLD_EFFECT_NO_EVOLVE && mode != EVO_CHECK_ITEM) {
        return 0;
    }
    if (method == NULL) {
        method = &dummy;
    }
    evo = GFL_HeapAllocate(heapId, EVOLUTION_COUNT * sizeof(Evolution), FALSE, "shinka_check.c", 106);
    getEvolutionInfoFile(species, evo);
    switch (mode) {
    case EVO_CHECK_LEVEL_UP:
        level = PokeParty_GetParam(pkm, PKM_PARAM_LEVEL, NULL);
        friendship = PokeParty_GetParam(pkm, PKM_PARAM_HAPPINESS, NULL);
        for (i = 0; i < EVOLUTION_COUNT; i++) {
            switch (evo[i].method) {
            case EVO_METHOD_FRIENDSHIP:
                if (friendship >= EVO_FRIENDSHIP) {
                    result = evo[i].species;
                }
                break;
            case EVO_METHOD_FRIENDSHIP_DAY:
                if (IsDayPeriodNight(season) == FALSE && friendship >= EVO_FRIENDSHIP) {
                    result = evo[i].species;
                }
                break;
            case EVO_METHOD_FRIENDSHIP_NIGHT:
                if (IsDayPeriodNight(season) == TRUE && friendship >= EVO_FRIENDSHIP) {
                    result = evo[i].species;
                }
                break;
            case EVO_METHOD_LEVEL:
                if (evo[i].param <= level) {
                    result = evo[i].species;
                }
                break;
            case EVO_METHOD_TRADE:
            case EVO_METHOD_TRADE_WITH_ITEM:
            case EVO_METHOD_TRADE_WITH_SPECIES:
            case EVO_METHOD_ITEM:
                break;
            case EVO_METHOD_LEVEL_ATK_GT_DEF:
                if (evo[i].param <= level && PokeParty_GetParam(pkm, PKM_PARAM_ATTACK, NULL) >
                                                 PokeParty_GetParam(pkm, PKM_PARAM_DEFENSE, NULL)) {
                    result = evo[i].species;
                }
                break;
            case EVO_METHOD_LEVEL_ATK_EQ_DEF:
                if (evo[i].param <= level && PokeParty_GetParam(pkm, PKM_PARAM_ATTACK, NULL) ==
                                                 PokeParty_GetParam(pkm, PKM_PARAM_DEFENSE, NULL)) {
                    result = evo[i].species;
                }
                break;
            case EVO_METHOD_LEVEL_ATK_LT_DEF:
                if (evo[i].param <= level && PokeParty_GetParam(pkm, PKM_PARAM_ATTACK, NULL) <
                                                 PokeParty_GetParam(pkm, PKM_PARAM_DEFENSE, NULL)) {
                    result = evo[i].species;
                }
                break;
            case EVO_METHOD_LEVEL_PERSONALITY_LOW:
                if (evo[i].param <= level && pidHigh % 10 < 5) {
                    result = evo[i].species;
                }
                break;
            case EVO_METHOD_LEVEL_PERSONALITY_HIGH:
                if (evo[i].param <= level && pidHigh % 10 >= 5) {
                    result = evo[i].species;
                }
                break;
            case EVO_METHOD_SHEDINJA:
                if (evo[i].param <= level) {
                    result = evo[i].species;
                }
                break;
            case EVO_METHOD_SHEDINJA_MADE:
                *method = EVO_METHOD_SHEDINJA_MADE;
                break;
            case EVO_METHOD_BEAUTY:
                if (evo[i].param <= beauty) {
                    result = evo[i].species;
                }
                break;
            case EVO_METHOD_ITEM_MALE:
            case EVO_METHOD_ITEM_FEMALE:
                break;
            case EVO_METHOD_HELD_ITEM_DAY:
                if (IsDayPeriodNight(season) == FALSE && item == evo[i].param) {
                    result = evo[i].species;
                }
                break;
            case EVO_METHOD_HELD_ITEM_NIGHT:
                if (IsDayPeriodNight(season) == TRUE && item == evo[i].param) {
                    result = evo[i].species;
                }
                break;
            case EVO_METHOD_KNOWS_MOVE:
                if (CheckPokeMoveLearned(pkm, evo[i].param) == TRUE) {
                    result = evo[i].species;
                }
                break;
            case EVO_METHOD_SPECIES_IN_PARTY:
                if (party != NULL && CheckSpeciesInParty(party, evo[i].param) == TRUE) {
                    result = evo[i].species;
                }
                break;
            case EVO_METHOD_LEVEL_MALE:
                if (PokeParty_GetParam(pkm, PKM_PARAM_SEX, NULL) == 0 && evo[i].param <= level) {
                    result = evo[i].species;
                }
                break;
            case EVO_METHOD_LEVEL_FEMALE:
                if (PokeParty_GetParam(pkm, PKM_PARAM_SEX, NULL) == 1 && evo[i].param <= level) {
                    result = evo[i].species;
                }
                break;
            case EVO_METHOD_LEVEL_MAGNETIC_FIELD:
                for (j = 0; j < NELEMS(EVOLVE_ELEC_ZONES); j++) {
                    if (a3 == EVOLVE_ELEC_ZONES[j]) {
                        result = evo[i].species;
                        break;
                    }
                }
                break;
            case EVO_METHOD_LEVEL_MOSS_ROCK:
                for (j = 0; j < NELEMS(EVOLVE_MOSSROCK_ZONES); j++) {
                    if (a3 == EVOLVE_MOSSROCK_ZONES[j]) {
                        result = evo[i].species;
                        break;
                    }
                }
                break;
            case EVO_METHOD_LEVEL_ICE_ROCK:
                for (j = 0; j < NELEMS(EVOLVE_ICEROCK_ZONES); j++) {
                    if (a3 == EVOLVE_ICEROCK_ZONES[j]) {
                        result = evo[i].species;
                        break;
                    }
                }
                break;
            case 28:
                for (j = 0; j < NELEMS(EVOLVE_ELEC2_ZONES); j++) {
                    if (a3 == EVOLVE_ELEC2_ZONES[j]) {
                        result = evo[i].species;
                        break;
                    }
                }
                break;
            }
            if (result != 0) {
                *method = evo[i].method;
                break;
            }
        }
        break;
    case EVO_CHECK_TRADE:
        for (i = 0; i < EVOLUTION_COUNT; i++) {
            switch (evo[i].method) {
            case EVO_METHOD_TRADE:
                result = evo[i].species;
                break;
            case EVO_METHOD_TRADE_WITH_ITEM:
                if (item == evo[i].param) {
                    result = evo[i].species;
                }
                break;
            case EVO_METHOD_TRADE_WITH_SPECIES:
                // Karrablast and Shelmet evolve when traded for each other, unless the partner holds an Everstone
                if (GetItemParam(PokeParty_GetParam((PartyPkm *)a3, PKM_PARAM_ITEM, NULL), ITEM_PARAM_HOLD_EFFECT,
                                 heapId) != HOLD_EFFECT_NO_EVOLVE) {
                    if ((PokeParty_GetParam(pkm, PKM_PARAM_LEGAL_SPECIES, NULL) == SPECIES_KARRABLAST &&
                         PokeParty_GetParam((PartyPkm *)a3, PKM_PARAM_LEGAL_SPECIES, NULL) == SPECIES_SHELMET) ||
                        (PokeParty_GetParam((PartyPkm *)a3, PKM_PARAM_LEGAL_SPECIES, NULL) == SPECIES_KARRABLAST &&
                         PokeParty_GetParam(pkm, PKM_PARAM_LEGAL_SPECIES, NULL) == SPECIES_SHELMET)) {
                        result = evo[i].species;
                    }
                }
                break;
            }
            if (result != 0) {
                *method = evo[i].method;
                break;
            }
        }
        break;
    case EVO_CHECK_ITEM_UNUSED:
    case EVO_CHECK_ITEM:
        for (i = 0; i < EVOLUTION_COUNT; i++) {
            if (evo[i].method == EVO_METHOD_ITEM && a3 == evo[i].param) {
                result = evo[i].species;
            }
            if (evo[i].method == EVO_METHOD_ITEM_MALE && PokeParty_GetParam(pkm, PKM_PARAM_SEX, NULL) == 0 &&
                a3 == evo[i].param) {
                result = evo[i].species;
            }
            if (evo[i].method == EVO_METHOD_ITEM_FEMALE && PokeParty_GetParam(pkm, PKM_PARAM_SEX, NULL) == 1 &&
                a3 == evo[i].param) {
                result = evo[i].species;
            }
            if (result != 0) {
                *method = evo[i].method;
                break;
            }
        }
        break;
    }
    GFL_HeapFree(evo);
    return result;
}

ArcTool *loadEvolutionFile(HeapID heapId) {
    return GFL_ArcSysCreateFileHandle(ARCID_EVOLUTIONS, heapId);
}

// A field, EVOLUTION_FIELD_*, of one of a species' evolutions
BOOL getEvolutionInfoParam(ArcTool *evoFile, u16 species, u16 index, u16 field) {
    Evolution evo[EVOLUTION_COUNT];

    GFL_ArcToolRead(evoFile, species, evo);
    switch (field) {
    case EVOLUTION_FIELD_METHOD:
        return evo[index].method;
    case EVOLUTION_FIELD_PARAM:
        return evo[index].param;
    case EVOLUTION_FIELD_SPECIES:
        return evo[index].species;
    }
    return 0;
}

static void getEvolutionInfoFile(u16 species, Evolution *dest) {
    GFL_ArcSysRead(dest, ARCID_EVOLUTIONS, species);
}

static BOOL CheckPokeMoveLearned(PartyPkm *pkm, u16 move) {
    BOOL learned = FALSE;
    int i;

    for (i = PKM_PARAM_MOVE1; i < PKM_PARAM_MOVE1 + 4; i++) {
        if (move == PokeParty_GetParam(pkm, i, NULL)) {
            learned = TRUE;
            break;
        }
    }
    return learned;
}
