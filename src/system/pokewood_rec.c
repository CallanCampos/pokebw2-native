#include "types.h"
#include "battle/battle_rec_tool.h"
#include "field/pokewood_system.h"
#include "gfl/arc.h"
#include "gfl/heap.h"
#include "gfl/rtc_cache.h"
#include "gfl/std.h"
#include "save/pokewood.h"
#include "save/save_control.h"
#include "system/game_data.h"

// pokewood_rec.c, the name its allocation passes: the movie of Pokéstar Studios that is loaded, which the save keeps
// in an extra block for each of the eight downloaded movies.

// The file of ARCID_POKEWOOD_MOVIE that loadPokewoodAndCheck loads
#define POKEWOOD_DEFAULT_MOVIE_FILE 41

static void setBlockField(u32 field, u32 value);
static u32 getSlotExtraBlock(int slot);

// The extra save block of each slot
static const u32 sSlotExtraBlocks[POKEWOOD_DOWNLOAD_SLOT_COUNT] = {
    9, 10, 11, 12, 13, 14, 15, 16,
};

// The movie that is loaded
static PokewoodBlock *sBlock;

void clearPokewoodBlock(PokewoodBlock *block) {
    MI_CpuFill8(block, 0, sizeof(PokewoodBlock));
}

u32 getSizeOfPokewoodBlock(void) {
    return sizeof(PokewoodBlock);
}

void allocatePokewoodBlk(HeapID heapId) {
    sBlock = GFL_HeapAllocate(heapId, POKEWOOD_EXTRA_BLOCK_SIZE, TRUE, "pokewood_rec.c", 102);
}

void freeAndClearPokewoodBlk(void) {
    GFL_HeapFree(sBlock);
    sBlock = NULL;
}

void savePokewoodBlk(void *dest) {
    MIi_CpuCopy32(sBlock, dest, sizeof(PokewoodBlock));
}

void setPokewoodBlk(const void *src) {
    MIi_CpuCopy32(src, sBlock, sizeof(PokewoodBlock));
}

u32 Pokewood_LoadDownloadedMovie(SaveControl *save, HeapID heapId, u32 slot) {
    u32 block = getSlotExtraBlock(slot);
    u32 result;

    MI_CpuFill8(sBlock, 0, POKEWOOD_EXTRA_BLOCK_SIZE);
    result = SaveControl_LoadExtraBlock(save, block, heapId, sBlock, POKEWOOD_EXTRA_BLOCK_SIZE);
    freeIntermediateSaveExtraBlksAfterLoad(save, block);
    if (result == 0) {
        return 0;
    }
    if (result != 1) {
        return 4;
    }
    if (sBlock->crc == getCRC16(sBlock, POKEWOOD_BLOCK_CRC_SIZE)) {
        return 1;
    }
    return 2;
}

u32 loadPokewoodAndCheck(SaveControl *save, HeapID heapId, u32 slot) {
    MI_CpuFill8(sBlock, 0, POKEWOOD_EXTRA_BLOCK_SIZE);
    GFL_ArcSysRead(sBlock, ARCID_POKEWOOD_MOVIE, POKEWOOD_DEFAULT_MOVIE_FILE);
    if (sBlock->crc == getCRC16(sBlock, POKEWOOD_BLOCK_CRC_SIZE)) {
        return 1;
    }
    return 2;
}

u32 Pokewood_SaveDownloadedMovie(GameData *gameData, HeapID heapId, u32 slot, u16 *state) {
    SaveControl *save = GameData_GetSaveControl(gameData);
    u32 block = getSlotExtraBlock(slot);
    RTCDate date;
    RTCTime time;
    u32 result;

    switch (*state) {
    case 0: {
        u32 dateValue;
        u32 timeValue;

        RTC_GetCachedDateTime(&date, &time);
        timeValue = ((time.hour & 0xff) << 16) | ((time.minute & 0xff) << 8) | time.second;
        dateValue = (date.year << 24) | ((date.month & 0xff) << 16) | ((date.day & 0xff) << 8) | date.week;
        setBlockField(POKEWOOD_BLOCK_FIELD_DATE, dateValue);
        setBlockField(POKEWOOD_BLOCK_FIELD_TIME, timeValue);
        sBlock->crc = getCRC16(sBlock, POKEWOOD_BLOCK_CRC_SIZE);
        SaveControl_InitExtraBlock(save, block, heapId, sBlock, POKEWOOD_EXTRA_BLOCK_SIZE);
        GameData_StartSaveBlock(gameData, block);
        (*state)++;
        break;
    }
    case 1:
        result = GameData_UpdateSaveBlock(gameData, block);
        if (result == 2 || result == 3) {
            freeIntermediateSaveExtraBlksAfterLoad2(save, block);
            return result;
        }
        break;
    }
    return 0;
}

BOOL Pokewood_IsLoadedMovieValid(void) {
    if (addUpDataStream((const u8 *)sBlock, POKEWOOD_EXTRA_BLOCK_SIZE) != 0 &&
        sBlock->crc == getCRC16(sBlock, POKEWOOD_BLOCK_CRC_SIZE)) {
        return TRUE;
    }
    return FALSE;
}

PokewoodBlock *getPokewoodBlock(void) {
    return sBlock;
}

u32 Pokewood_GetLoadedMovieField(u32 field, uptr a1) {
    switch (field) {
    case POKEWOOD_BLOCK_FIELD_UNKDF:
        return sBlock->info.unkDF;
    case POKEWOOD_BLOCK_FIELD_UNK124:
        return sBlock->info.unk124;
    case POKEWOOD_BLOCK_FIELD_PKM_0:
    case POKEWOOD_BLOCK_FIELD_PKM_0 + 1:
    case POKEWOOD_BLOCK_FIELD_PKM_0 + 2:
    case POKEWOOD_BLOCK_FIELD_PKM_0 + 3:
    case POKEWOOD_BLOCK_FIELD_PKM_0 + 4:
    case POKEWOOD_BLOCK_FIELD_PKM_5:
        BattleRecTool_LoadPkm(&sBlock->party.pkm[field - POKEWOOD_BLOCK_FIELD_PKM_0], (PartyPkm *)a1);
        return 1;
    case POKEWOOD_BLOCK_FIELD_UNK128:
        return sBlock->unk128;
    case POKEWOOD_BLOCK_FIELD_UNKE0:
        return sBlock->unkE0;
    case POKEWOOD_BLOCK_FIELD_DATE:
        return sBlock->date;
    case POKEWOOD_BLOCK_FIELD_TIME:
        return sBlock->time;
    case POKEWOOD_BLOCK_FIELD_UNK130:
        return sBlock->unk130;
    }
    return 0;
}

static void setBlockField(u32 field, u32 value) {
    switch (field) {
    case POKEWOOD_BLOCK_FIELD_DATE:
        sBlock->date = value;
        break;
    case POKEWOOD_BLOCK_FIELD_TIME:
        sBlock->time = value;
        break;
    }
}

static u32 getSlotExtraBlock(int slot) {
    if (slot >= NELEMS(sSlotExtraBlocks) || slot < 0) {
        slot = 0;
    }
    return sSlotExtraBlocks[slot];
}
