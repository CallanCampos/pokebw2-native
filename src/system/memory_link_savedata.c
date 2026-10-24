#include "types.h"
#include "gfl/std.h"
#include "save/memory_link_savedata.h"
#include "save/musical_savedata.h"
#include "system/str_tool.h"

// The Memory Link's data in the save: the players of the Black and White saves that were linked and what those saves
// gave, which unlocks the memories. The ROM has no string for the file, so the name is a guess. The data is not a
// save block of its own.

// The parts of the memories, each with the player whose save gave it
enum {
    MEMORY_LINK_PART_EVENTS,
    MEMORY_LINK_PART_MUSICAL,
    MEMORY_LINK_PART_UNK2C,
};

// What unlocks a memory: a flag of the memories, or a player's slot being linked
typedef struct {
    int index;
    BOOL (*func)(MemoryLinkSave *link, int index);
} MemoryLinkCondition;

static u32 mergeData(MemoryLinkSave *link, const MemoryLinkData *data, SaveControl *save, BOOL apply);
static void setFlag(MemoryLinkSave *link, int index, BOOL value);
static void setVar(MemoryLinkSave *link, u32 index, u16 value);
static u32 getUnlockedMemories(MemoryLinkSave *link);

// The event work that marks each memory seen
static const u32 sMemoryEventWorks[MEMORY_LINK_MEMORY_COUNT] = {
    0x4045, 0x4046, 0x4047, 0x4048, 0x4049, 0x404a, 0x404b, 0x404c,
};

// clang-format off
static const MemoryLinkCondition sMemoryConditions[MEMORY_LINK_MEMORY_COUNT] = {
    { 8, MemoryLinkSave_IsMemoryFlagSet },
    { 13, MemoryLinkSave_IsMemoryFlagSet },
    { 0, MemoryLinkSave_IsMemoryFlagSet },
    { 15, MemoryLinkSave_IsMemoryFlagSet },
    { 12, MemoryLinkSave_IsMemoryFlagSet },
    { 0, MemoryLinkSave_IsMemoryFlagSet },
    { 1, MemoryLinkSave_IsPlayerLinked },
    { 0, MemoryLinkSave_IsMemoryFlagSet },
};

// clang-format on

void zero476Bytes(MemoryLinkSave *link) {
    MI_CpuFill8(link, 0, sizeof(MemoryLinkSave));
}

BOOL MemoryLinkSave_Merge(MemoryLinkSave *link, const MemoryLinkData *data, SaveControl *save) {
    u32 before = getUnlockedMemories(link);
    BOOL unlocked;
    u32 after;

    mergeData(link, data, save, TRUE);
    unlocked = TRUE;
    after = getUnlockedMemories(link);
    if (!(after & (before ^ after))) {
        unlocked = FALSE;
    }
    return unlocked;
}

BOOL MemoryLinkSave_CanMerge(MemoryLinkSave *link, const MemoryLinkData *data, SaveControl *save) {
    int linked = 0;
    u32 changed = mergeData(link, data, save, FALSE);
    BOOL result = TRUE;
    int i;

    for (i = 0; i < MEMORY_LINK_PLAYER_COUNT; i++) {
        if (!link->players[i].linked) {
            linked++;
        }
    }
    if (linked == MEMORY_LINK_PLAYER_COUNT) {
        return TRUE;
    }
    for (i = 0; i < MEMORY_LINK_PLAYER_COUNT; i++) {
        if (changed & (1 << i)) {
            const MemoryLinkPlayer *player = &link->players[i];

            if (data->player.id != player->id || !wcharscmp(player->name, data->player.name) ||
                data->player.gender != player->gender || data->player.version != player->version) {
                result = FALSE;
                break;
            }
        }
    }
    return result;
}

// Adds what a Black or White save gives to the memories, if apply is set, and returns a bit for each part,
// MEMORY_LINK_PART_*, that the save changes
static u32 mergeData(MemoryLinkSave *link, const MemoryLinkData *data, SaveControl *save, BOOL apply) {
    int i;
    const MemoryLinkMemories *src = &data->memories;
    MemoryLinkMemories *dest = &link->memories;
    BOOL changed[MEMORY_LINK_PLAYER_COUNT] = { FALSE, FALSE, FALSE };

    for (i = 0; i < MEMORY_LINK_FLAG_COUNT; i++) {
        if (src->flags[i / 8] & (1 << (i % 8))) {
            if (!(dest->flags[i / 8] & (1 << (i % 8)))) {
                changed[MEMORY_LINK_PART_EVENTS] = TRUE;
            }
            if (apply) {
                setFlag(link, i, TRUE);
            }
        }
    }
    for (i = 0; i < MEMORY_LINK_VAR_COUNT; i++) {
        BOOL valid = TRUE;

        switch (i) {
        case 0:
            break;
        case 1:
            if (src->vars[i] == 0) {
                valid = FALSE;
            }
            break;
        case 2:
            break;
        }
        if (valid && src->vars[i] != dest->vars[i]) {
            if (dest->vars[i] == 0) {
                changed[MEMORY_LINK_PART_EVENTS] = TRUE;
            }
            if (apply) {
                setVar(link, i, src->vars[i]);
            }
        }
    }
    if (src->musicalShowCount != 0) {
        MusicalSave *musical = getAddressOfMusicalDataInfo(save);

        if (!MusicalSave_GetDownloadedProps(musical)) {
            BOOL differs = GFL_STD_MemCmp(src->props, MusicalSave_GetProps(musical), sizeof(src->props)) != 0;
            BOOL hasAll = MusicalSave_HasAllProps(musical);

            if (differs || (!differs && hasAll)) {
                changed[MEMORY_LINK_PART_MUSICAL] = TRUE;
                if (apply) {
                    dest->musicalTotalPoints = src->musicalTotalPoints;
                    dest->musicalShowCount = src->musicalShowCount;
                    MI_CpuCopy8(src->props, dest->props, sizeof(src->props));
                }
            }
        }
    }
    if (src->flags[0] & 1) {
        if (GFL_STD_MemCmp(src->unk2C, dest->unk2C, sizeof(src->unk2C)) != 0) {
            if (apply) {
                MI_CpuCopy8(src->unk2C, dest->unk2C, sizeof(src->unk2C));
            }
            changed[MEMORY_LINK_PART_UNK2C] = TRUE;
        }
    }
    if (apply) {
        for (i = 0; i < MEMORY_LINK_PLAYER_COUNT; i++) {
            if (changed[i]) {
                link->players[i] = data->player;
                link->players[i].linked = TRUE;
            }
        }
    }
    return changed[MEMORY_LINK_PART_EVENTS] | (changed[MEMORY_LINK_PART_MUSICAL] << 1) |
           (changed[MEMORY_LINK_PART_UNK2C] << 2);
}

uptr MemoryLinkSave_GetValue(MemoryLinkSave *link, u32 kind, void *dest) {
    switch (kind) {
    case MEMORY_LINK_KIND_MUSICAL_TOTAL_POINTS:
        return link->memories.musicalTotalPoints;
    case MEMORY_LINK_KIND_MUSICAL_SHOW_COUNT:
        return link->memories.musicalShowCount;
    case MEMORY_LINK_KIND_UNK2C_0:
    case MEMORY_LINK_KIND_UNK2C_0 + 1:
    case MEMORY_LINK_KIND_UNK2C_0 + 2:
    case MEMORY_LINK_KIND_UNK2C_0 + 3:
    case MEMORY_LINK_KIND_UNK2C_0 + 4:
    case MEMORY_LINK_KIND_UNK2C_5:
        MI_CpuCopy8(link->memories.unk2C[(u16)(kind - MEMORY_LINK_KIND_UNK2C_0)], dest,
                    sizeof(link->memories.unk2C[0]));
        return 0;
    case MEMORY_LINK_KIND_PLAYER_0:
    case MEMORY_LINK_KIND_PLAYER_0 + 1:
    case MEMORY_LINK_KIND_PLAYER_2:
        if (dest != NULL) {
            MI_CpuCopy8(&link->players[kind - MEMORY_LINK_KIND_PLAYER_0], dest, sizeof(MemoryLinkPlayer));
        }
        // Not the player's address but the first one's, whichever was asked for
        return (uptr)link;
    case MEMORY_LINK_KIND_PROPS:
        if (dest != NULL) {
            MI_CpuCopy8(link->memories.props, dest, sizeof(link->memories.props));
        }
        return (uptr)link->memories.props;
    }
    return 0;
}

BOOL MemoryLinkSave_IsMemoryFlagSet(MemoryLinkSave *link, int index) {
    return (link->memories.flags[index / 8] & (1 << (index % 8))) != 0;
}

u16 MemoryLinkSave_GetVar(MemoryLinkSave *link, u32 index) {
    return link->memories.vars[index];
}

BOOL MemoryLinkSave_IsPlayerLinked(MemoryLinkSave *link, int index) {
    return link->players[index].linked;
}

u8 MemoryLinkSave_GetPlayerGender(MemoryLinkSave *link, u32 index) {
    return link->players[index].gender;
}

const u16 *MemoryLinkSave_GetPlayerName(MemoryLinkSave *link, u32 index) {
    return link->players[index].name;
}

static void setFlag(MemoryLinkSave *link, int index, BOOL value) {
    if (value) {
        link->memories.flags[index / 8] |= 1 << (index % 8);
    } else {
        link->memories.flags[index / 8] &= ~(1 << (index % 8));
    }
}

static void setVar(MemoryLinkSave *link, u32 index, u16 value) {
    link->memories.vars[index] = value;
}

BOOL MemoryLinkSave_CheckMemoryCondition(MemoryLinkSave *link, u32 memory) {
    return sMemoryConditions[memory].func(link, sMemoryConditions[memory].index);
}

u32 MemoryLink_GetMemoryEventWork(u32 memory) {
    return sMemoryEventWorks[memory];
}

// A bit for each memory that is unlocked
static u32 getUnlockedMemories(MemoryLinkSave *link) {
    int i;
    u32 mask = 0;

    for (i = 0; i < MEMORY_LINK_MEMORY_COUNT; i++) {
        if (MemoryLinkSave_CheckMemoryCondition(link, i)) {
            mask |= 1 << i;
        }
    }
    return mask;
}
