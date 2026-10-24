#ifndef POKEBW2_APP_WIFI_LOGIN_H
#define POKEBW2_APP_WIFI_LOGIN_H

#include "types.h"
#include "gfl/overlay.h"
#include "gfl/proc.h"
#include "struct_decls.h"
// The login's messages, for the callbacks of WifiLoginParam: WifiLoginMessage_PrintFrom prints one and
// WifiLoginMessage_IsDone waits for it
#include "app/wifilogin_message.h"

#define OVERLAY_WIFILOGIN OVERLAY_ID(190)

// The callback of the login, called each frame once the connection is made, with the login's message work and the
// data of WifiLoginParam. It returns 2 while it goes on, 0 once it is done, and 1 to log out with an error
typedef int (*WifiLoginCallback)(void *msgWork, void *data);

typedef struct {
    GameData *gameData;
    // The display's style, 0 for the Dream World account's and 1 for Game Sync's; with 0 the login prints a title on
    // the top screen and ignores the touch screen
    u32 unk4;
    // The style of the messages and menus, as unk4 numbers them
    u32 unk8;
    // The game service ID, which the Wi-Fi library and the beacons identify the game with
    u32 unkC;
    void *buffer;
    // Set to log in again after an error
    u32 unk14;
    // Set to leave the music alone
    u32 unk18;
    // 0 once logged in, 1 if the player gave up or the login failed
    u32 result;
    // Called each frame once logged in, with unk24
    WifiLoginCallback unk20;
    void *unk24;
} WifiLoginParam;

typedef struct {
    GameData *gameData;
    u32 unk4;
    u32 unk8;
    u32 unkC;
    u32 unk10;
    uptr unk14;
    uptr unk18;
} WifiLogoutParam;

extern const GameProcFunctions WIFILOGIN_PROC_FUNCTIONS;
extern const GameProcFunctions WIFILOGOUT_PROC_FUNCTIONS;

#endif // POKEBW2_APP_WIFI_LOGIN_H
