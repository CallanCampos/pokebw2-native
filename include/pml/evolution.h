#ifndef POKEBW2_PML_EVOLUTION_H
#define POKEBW2_PML_EVOLUTION_H

#include "types.h"
#include "gfl/heap.h"
#include "struct_decls.h"

// The check of whether a Pokémon evolves (shinka_check.c, named by the ROM's string). CheckEvolveSpecies,
// loadEvolutionFile, getEvolutionInfoFile and CheckPokeMoveLearned are swan's names
// (https://github.com/ds-pokemon-hacking/swan, GPL-3.0). loadEvolutionFile and getEvolutionInfoParam are declared in
// pml/personal.h

// What the check is made for, and so which of the evolution methods it looks at
#define EVO_CHECK_LEVEL_UP 0
#define EVO_CHECK_TRADE 1
#define EVO_CHECK_ITEM_UNUSED 2
#define EVO_CHECK_ITEM 3

// The evolutions of a species, and one of them: a method, EVO_METHOD_*, its parameter and the species it gives
#define EVOLUTION_COUNT 7

typedef struct {
    u16 method;
    u16 param;
    u16 species;
} Evolution;

// The fields of an evolution, for getEvolutionInfoParam
#define EVOLUTION_FIELD_METHOD 0
#define EVOLUTION_FIELD_PARAM 1
#define EVOLUTION_FIELD_SPECIES 2

// The species the Pokémon evolves into, or 0. a3 depends on the mode: the zone for EVO_CHECK_LEVEL_UP, the partner
// PartyPkm for EVO_CHECK_TRADE, the item for the two item modes. method, if not NULL, gets the evolution's method.
// An Everstone stops the check in every mode but EVO_CHECK_ITEM; no caller uses EVO_CHECK_ITEM_UNUSED
u16 CheckEvolveSpecies(PokeParty *party, PartyPkm *pkm, u32 mode, uptr a3, u8 season, u32 *method, HeapID heapId);

#endif // POKEBW2_PML_EVOLUTION_H
