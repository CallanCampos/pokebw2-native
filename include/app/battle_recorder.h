#ifndef POKEBW2_APP_BATTLE_RECORDER_H
#define POKEBW2_APP_BATTLE_RECORDER_H

#include "types.h"
#include "gfl/proc.h"
#include "struct_decls.h"

// The Battle Recorder, whose proc is overlay 272's br_main.c. It runs overlay 271's core proc (app/battle_recorder/),
// which runs the screens of overlays 268, 269 and 270

// What the Battle Recorder is opened for
enum {
    // The Battle Recorder in the bag: the player's own videos and records
    BR_MODE_BROWSE,
    // The Global Link's Battle Videos, online
    BR_MODE_GLOBAL_BV,
    // The Global Link's musical photos, online
    BR_MODE_GLOBAL_MUSICAL,
};

// The parameter of br_main.c's proc
typedef struct {
    u32 mode;
    GameData *gameData;
    uptr unk8;
    // 2 when the online connection failed
    u32 result;
} BattleRecorderParam;

extern const GameProcFunctions BR_MAIN_PROC_FUNCTIONS;

#endif // POKEBW2_APP_BATTLE_RECORDER_H
