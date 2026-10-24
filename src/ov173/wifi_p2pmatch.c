#include "types.h"
#include "app/ui/touchbar.h"
#include "app/wifi_p2pmatch.h"
#include "app/wifi_p2pmatch_net.h"
#include "app/wifi_p2pmatchroom.h"
#include "app/wifi_status.h"
#include "app/wificlub.h"
#include "battle/regulation.h"
#include "constants/arc.h"
#include "constants/language.h"
#include "constants/pokemon.h"
#include "constants/sound.h"
#include "constants/species.h"
#include "constants/text_banks.h"
#include "constants/version.h"
#include "field/union_msg.h"
#include "field/unity_tower.h"
#include "gfl/arc.h"
#include "gfl/arc_util.h"
#include "gfl/bg_sys.h"
#include "gfl/bmp.h"
#include "gfl/bmpwin.h"
#include "gfl/button_man.h"
#include "gfl/clact.h"
#include "gfl/dwc_rap.h"
#include "gfl/dwc_rapcommon.h"
#include "gfl/gx_layers.h"
#include "gfl/heap.h"
#include "gfl/key.h"
#include "gfl/msg.h"
#include "gfl/net.h"
#include "gfl/net_handle.h"
#include "gfl/net_state.h"
#include "gfl/proc.h"
#include "gfl/random.h"
#include "gfl/sound.h"
#include "gfl/std.h"
#include "gfl/str.h"
#include "gfl/tcb.h"
#include "gfl/tcbl.h"
#include "gfl/touchpanel.h"
#include "nitro/gx.h"
#include "nitro/os.h"
#include "nnsys/g2d.h"
#include "pml/poke_graphic.h"
#include "pml/poke_party.h"
#include "save/box.h"
#include "save/player_info.h"
#include "save/save_control.h"
#include "save/trainer_data_savedata.h"
#include "save/wifi_list.h"
#include "struct_decls.h"
#include "system/app_keycursor.h"
#include "system/app_printsys_common.h"
#include "system/bmp_menu.h"
#include "system/bmp_menulist.h"
#include "system/bmp_menuwork.h"
#include "system/bmp_winframe.h"
#include "system/dsi.h"
#include "system/game_data.h"
#include "system/gf_font.h"
#include "system/net_define.h"
#include "system/poke_icon.h"
#include "system/printsys.h"
#include "system/text_speed.h"
#include "system/time_icon.h"
#include "system/union_view.h"
#include "system/wipe.h"
#include "system/wordset.h"
#include "text/system/wifi_error.h"
#include "text/system/wifi_p2pmatch.h"

// The Wi-Fi Club's match board: the room where the player meets the friends who are online, the friend list of the
// lower screen, and the states of calling a friend, of choosing what to play and of leaving for the game. The ROM has
// no name for this file: it is named after the older games' wifi_p2pmatch.c, and its names are ours

#define WIFIP2PMATCH_HEAP 0x21
// How many friends the friend list holds
#define WIFIP2PMATCH_MEMBER_MAX 32
// The characters of the room: the friends and the player
#define WIFIP2PMATCH_OBJ_MAX 33
#define WIFIP2PMATCH_MENU_CURSOR_NUM 6
// How many friends a page of the lower screen shows, in two columns
#define WIFIP2PMATCH_VIEW_FRIEND_NUM 8

// The status a player shows the friends (WIFI_STATUS_GetWifiStatus)
enum {
    // Not in the Wi-Fi Club
    WIFIP2PMATCH_STATUS_NONE,
    WIFIP2PMATCH_STATUS_WAIT,
    // Waits for a friend to join the game mode
    WIFIP2PMATCH_STATUS_RECRUIT,
    WIFIP2PMATCH_STATUS_CALL,
    WIFIP2PMATCH_STATUS_PLAYING,
};

// The timing number both sides wait for before the state they agreed on
#define WIFIP2PMATCH_TIMING_SEQ 0x1b

// The list menus, which index of WIFIP2PMATCH_WORK.menuCursor keeps their cursor
enum {
    WIFIP2PMATCH_MENU_BATTLE_RULES,
    WIFIP2PMATCH_MENU_BATTLE_MODE,
    WIFIP2PMATCH_MENU_BATTLE_RULE,
    WIFIP2PMATCH_MENU_SHOOTER,
    WIFIP2PMATCH_MENU_STATUS,
    // Opens lower on the screen, under the party window
    WIFIP2PMATCH_MENU_PARTY,
};

// What WifiP2PMatch_DrawRulesWindow prints above the rules
enum {
    WIFIP2PMATCH_RULES_BATTLE_TYPE,
    // Nothing, in a window two rows shorter
    WIFIP2PMATCH_RULES_SHORT,
    WIFIP2PMATCH_RULES_PARTY,
    WIFIP2PMATCH_RULES_BATTLE_BOX,
};

// What a button of the lower screen shows (the column of its screen data)
enum {
    WIFIP2PMATCH_VIEW_BUTTON_MALE,
    WIFIP2PMATCH_VIEW_BUTTON_FEMALE,
    WIFIP2PMATCH_VIEW_BUTTON_NONE,
};

// The states, which are indices of sWifiP2PMatchSeqFuncs
enum {
    WIFIP2PMATCH_SEQ_INIT,
    WIFIP2PMATCH_SEQ_RECONNECT_KEY_WAIT,
    WIFIP2PMATCH_SEQ_RECONNECT_YES_NO_START,
    WIFIP2PMATCH_SEQ_RECONNECT_YES_NO,
    WIFIP2PMATCH_SEQ_CONNECT_START,
    WIFIP2PMATCH_SEQ_BG_PRIORITY,
    WIFIP2PMATCH_SEQ_CONNECT_WAIT,
    WIFIP2PMATCH_SEQ_USER_INFO_NOTICE,
    WIFIP2PMATCH_SEQ_USER_INFO_NOTICE_KEY_WAIT,
    WIFIP2PMATCH_SEQ_LOGIN_WAIT,
    WIFIP2PMATCH_SEQ_ROOM_START,
    WIFIP2PMATCH_SEQ_ROOM,
    WIFIP2PMATCH_SEQ_VCHAT_TOGGLE_KEY_WAIT,
    WIFIP2PMATCH_SEQ_VCHAT_CALL_WAIT,
    WIFIP2PMATCH_SEQ_VCHAT_INVITED,
    WIFIP2PMATCH_SEQ_VCHAT_INVITED_WAIT,
    WIFIP2PMATCH_SEQ_VCHAT_START,
    WIFIP2PMATCH_SEQ_VCHAT_END_YES_NO_START,
    WIFIP2PMATCH_SEQ_VCHAT_END_YES_NO,
    WIFIP2PMATCH_SEQ_CONNECT_FAILED,
    WIFIP2PMATCH_SEQ_CONNECT_FAILED_KEY_WAIT,
    WIFIP2PMATCH_SEQ_CONNECT_ERROR,
    WIFIP2PMATCH_SEQ_NONE16,
    WIFIP2PMATCH_SEQ_NONE17,
    WIFIP2PMATCH_SEQ_NONE18,
    WIFIP2PMATCH_SEQ_RETURN_TO_ROOM,
    WIFIP2PMATCH_SEQ_END,
    WIFIP2PMATCH_SEQ_NO_FRIEND_KEY_WAIT,
    WIFIP2PMATCH_SEQ_RECRUIT_MENU_START,
    WIFIP2PMATCH_SEQ_RECRUIT_MENU,
    WIFIP2PMATCH_SEQ_RECRUIT_MENU_OLD,
    WIFIP2PMATCH_SEQ_RECRUIT_STOP_ASK,
    WIFIP2PMATCH_SEQ_RECRUIT_STOP_YES_NO_START,
    WIFIP2PMATCH_SEQ_RECRUIT_STOP_YES_NO,
    WIFIP2PMATCH_SEQ_FRIEND_SELECT,
    WIFIP2PMATCH_SEQ_FRIEND_MENU_INIT,
    WIFIP2PMATCH_SEQ_RECRUITING_FRIEND_MENU,
    WIFIP2PMATCH_SEQ_JOIN_WAIT,
    WIFIP2PMATCH_SEQ_DUMMY26,
    WIFIP2PMATCH_SEQ_DUMMY27,
    WIFIP2PMATCH_SEQ_FRIEND_CONNECTED,
    WIFIP2PMATCH_SEQ_NEGOTIATION_WAIT,
    WIFIP2PMATCH_SEQ_SEND_PLAYER_INFO,
    WIFIP2PMATCH_SEQ_CONNECT_DATA_WAIT,
    WIFIP2PMATCH_SEQ_CONNECT_TIMING,
    WIFIP2PMATCH_SEQ_CONNECT_END,
    WIFIP2PMATCH_SEQ_FRIEND_DETAIL_INIT,
    WIFIP2PMATCH_SEQ_FRIEND_DETAIL_WAIT,
    WIFIP2PMATCH_SEQ_FRIEND_DETAIL_END,
    WIFIP2PMATCH_SEQ_EXIT_YES_NO_INIT,
    WIFIP2PMATCH_SEQ_EXIT_YES_NO,
    WIFIP2PMATCH_SEQ_DISCONNECT,
    WIFIP2PMATCH_SEQ_DISCONNECT_END,
    WIFIP2PMATCH_SEQ_RETRY_YES_NO_INIT,
    WIFIP2PMATCH_SEQ_RETRY_YES_NO,
    WIFIP2PMATCH_SEQ_VCHAT_CHANGE_YES_NO_INIT,
    WIFIP2PMATCH_SEQ_VCHAT_CHANGE_YES_NO,
    WIFIP2PMATCH_SEQ_DISCONNECT_WAIT,
    WIFIP2PMATCH_SEQ_DUMMY3_A,
    WIFIP2PMATCH_SEQ_DUMMY3_B,
    WIFIP2PMATCH_SEQ_USER_DATA_SAVE_START,
    WIFIP2PMATCH_SEQ_JOIN_TIMEOUT,
    WIFIP2PMATCH_SEQ_USER_DATA_SAVE,
    WIFIP2PMATCH_SEQ_VCHAT_CALL_START,
    WIFIP2PMATCH_SEQ_WAITING_FRIEND_MENU,
    WIFIP2PMATCH_SEQ_CONNECTED,
    WIFIP2PMATCH_SEQ_PLAY_YES_NO_START,
    WIFIP2PMATCH_SEQ_PLAY_YES_NO,
    WIFIP2PMATCH_SEQ_GAME_MODE_MESSAGE,
    WIFIP2PMATCH_SEQ_GAME_MODE_MENU_OPEN,
    WIFIP2PMATCH_SEQ_GAME_MODE_MENU,
    WIFIP2PMATCH_SEQ_GAME_MODE_KEY_WAIT,
    WIFIP2PMATCH_SEQ_SYNC_SEQ_WAIT,
    WIFIP2PMATCH_SEQ_SYNC_SEQ_TIMING_WAIT,
    WIFIP2PMATCH_SEQ_CANCEL_RESET,
    WIFIP2PMATCH_SEQ_ASK_VCHAT,
    WIFIP2PMATCH_SEQ_ASK_TVT,
    WIFIP2PMATCH_SEQ_ASK_TRADE,
    WIFIP2PMATCH_SEQ_BATTLE_SETUP_MENU_START,
    WIFIP2PMATCH_SEQ_ANSWER_YES_NO_START,
    WIFIP2PMATCH_SEQ_ANSWER_YES_NO,
    WIFIP2PMATCH_SEQ_START_GAME,
    WIFIP2PMATCH_SEQ_SEND_START_GAME,
    WIFIP2PMATCH_SEQ_SEND_CANCEL_RESET,
    WIFIP2PMATCH_SEQ_BATTLE_SETUP_MENU,
    WIFIP2PMATCH_SEQ_BATTLE_MODE_MENU_START,
    WIFIP2PMATCH_SEQ_BATTLE_MODE_MENU,
    WIFIP2PMATCH_SEQ_BATTLE_RULE_MENU_START,
    WIFIP2PMATCH_SEQ_BATTLE_RULE_MENU,
    WIFIP2PMATCH_SEQ_SHOOTER_MENU_START,
    WIFIP2PMATCH_SEQ_SHOOTER_MENU,
    WIFIP2PMATCH_SEQ_BATTLE_SETUP_DECIDE,
    WIFIP2PMATCH_SEQ_RULES_SHOW,
    WIFIP2PMATCH_SEQ_RULES_SHOW_WAIT,
    WIFIP2PMATCH_SEQ_BATTLE_OFFER_SHOW,
    WIFIP2PMATCH_SEQ_UNUSED5_F,
    WIFIP2PMATCH_SEQ_BATTLE_OFFER_YES_NO_START,
    WIFIP2PMATCH_SEQ_BATTLE_OFFER_YES_NO,
    WIFIP2PMATCH_SEQ_BATTLE_OFFER_CHECK_PARTY,
    WIFIP2PMATCH_SEQ_REGULATION_FAIL_PARTY,
    WIFIP2PMATCH_SEQ_PARTY_SELECT_START,
    WIFIP2PMATCH_SEQ_PARTY_MENU_START,
    WIFIP2PMATCH_SEQ_PARTY_MENU,
    WIFIP2PMATCH_SEQ_PARTY_SEND,
    WIFIP2PMATCH_SEQ_PARTY_SEND_WAIT,
    WIFIP2PMATCH_SEQ_BATTLE_START_WAIT,
    WIFIP2PMATCH_SEQ_BATTLE_START,
    WIFIP2PMATCH_SEQ_PARTY_MENU_FAIL_PARTY,
    WIFIP2PMATCH_SEQ_PARTY_MENU_FAIL_BATTLE_BOX,
    WIFIP2PMATCH_SEQ_PARTY_MENU_FAIL_END,
    WIFIP2PMATCH_SEQ_PLAY_AGAIN_ASK,
    WIFIP2PMATCH_SEQ_PLAY_AGAIN_YES_NO_START,
    WIFIP2PMATCH_SEQ_PLAY_AGAIN_YES_NO,
    WIFIP2PMATCH_SEQ_MESSAGE_KEY_WAIT_TO_ROOM,
    WIFIP2PMATCH_SEQ_BACK_TO_ROOM,
    WIFIP2PMATCH_SEQ_GAME_START,
    WIFIP2PMATCH_SEQ_GAME_START_FAIL_KEY_WAIT,
    WIFIP2PMATCH_SEQ_BATTLE_RECRUIT_START,
    WIFIP2PMATCH_SEQ_JOIN_FRIEND,
    WIFIP2PMATCH_SEQ_REGULATION_FAIL_BATTLE_BOX,
    WIFIP2PMATCH_SEQ_REGULATION_FAIL_END,
    WIFIP2PMATCH_SEQ_RETURN_FROM_GAME,
    WIFIP2PMATCH_SEQ_BATTLE_OFFER_MSG_WAIT,
    WIFIP2PMATCH_SEQ_BATTLE_OFFER_KEY_WAIT,
    WIFIP2PMATCH_SEQ_BATTLE_OFFER_YES_NO_GO,
    WIFIP2PMATCH_SEQ_VCHAT_DIFF_MSG,
    WIFIP2PMATCH_SEQ_VCHAT_DIFF_YES_NO_START,
    WIFIP2PMATCH_SEQ_VCHAT_DIFF_YES_NO,
    WIFIP2PMATCH_SEQ_VCHAT_SEND,
    WIFIP2PMATCH_SEQ_REGULATION_SEND,
    WIFIP2PMATCH_SEQ_REGULATION_SEND_END,
    WIFIP2PMATCH_SEQ_RECRUIT_MENU_MESSAGE,
    WIFIP2PMATCH_SEQ_REFUSED_MSG,
    WIFIP2PMATCH_SEQ_SEND_REFUSED_MSG,
    WIFIP2PMATCH_SEQ_VCHAT_DIFF_NO,
    WIFIP2PMATCH_SEQ_SEND_CONNECT_DATA,
    WIFIP2PMATCH_SEQ_CALL_WAIT,
    WIFIP2PMATCH_SEQ_CALL_CANCEL_YES_NO,
    WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING,
    WIFIP2PMATCH_SEQ_SEND_SEQ,
    WIFIP2PMATCH_SEQ_PLAY_AGAIN_KEY_WAIT,
    WIFIP2PMATCH_SEQ_GO_DISCONNECT,
    WIFIP2PMATCH_SEQ_DISCONNECT_START,
    WIFIP2PMATCH_SEQ_DECLINED_WAIT,
    WIFIP2PMATCH_SEQ_DECLINED,
    WIFIP2PMATCH_SEQ_CUT_CONNECTION,
    WIFIP2PMATCH_SEQ_VCHAT_END_SEND,
    WIFIP2PMATCH_SEQ_VCHAT_END_TIMING,
    WIFIP2PMATCH_SEQ_VCHAT_ENDED_MESSAGE,
    WIFIP2PMATCH_SEQ_VCHAT,
    WIFIP2PMATCH_SEQ_TRADE_REFUSED_KEY_WAIT,
    WIFIP2PMATCH_SEQ_UNABLE_MSG,
    WIFIP2PMATCH_SEQ_PARTY_SEND_TIMING,
    WIFIP2PMATCH_SEQ_CANCEL_MSG,
    WIFIP2PMATCH_SEQ_VCHAT_STOP_WAIT,
    WIFIP2PMATCH_SEQ_TALK_END_TIMING,
    WIFIP2PMATCH_SEQ_TALK_END_TIMING_WAIT,
    WIFIP2PMATCH_SEQ_REGULATION_FAIL_KEY_WAIT,
    WIFIP2PMATCH_SEQ_ROOM_MESSAGE,
    WIFIP2PMATCH_SEQ_VCHAT_DISCONNECT_TIMING,
    WIFIP2PMATCH_SEQ_CAMERA_NOTICE_YES_NO_START,
    WIFIP2PMATCH_SEQ_CAMERA_NOTICE_YES_NO,
    WIFIP2PMATCH_SEQ_CONNECT_FRIEND_YES_NO_START,
    WIFIP2PMATCH_SEQ_CONNECT_FRIEND_YES_NO,
    WIFIP2PMATCH_SEQ_NO_CAMERA_YES_NO_START,
    WIFIP2PMATCH_SEQ_NO_CAMERA_YES_NO,
    WIFIP2PMATCH_SEQ_ANSWER_NO_CAMERA_YES_NO_START,
    WIFIP2PMATCH_SEQ_ANSWER_NO_CAMERA_YES_NO,
    WIFIP2PMATCH_SEQ_DECLINE_RESET,
    WIFIP2PMATCH_SEQ_DECLINE_MSG,
    WIFIP2PMATCH_SEQ_RETURN_YES_NO_START,
    WIFIP2PMATCH_SEQ_RETURN_YES_NO,
    WIFIP2PMATCH_SEQ_RETURN_TIMING_WAIT,
    WIFIP2PMATCH_SEQ_RECRUIT_START,
    WIFIP2PMATCH_SEQ_MAX = 0xb5,
};

// An option of a list menu: its message in the match board's bank and its value
typedef struct {
    u32 msg;
    s32 value;
} WIFIP2PMATCH_MENU_OPTION;

// What the child sends the parent when the connection is made (command 0xa03)
typedef struct {
    u16 gameMode;
    // Whether the sender uses voice chat
    u16 vchat;
    u32 regulationCode;
} WIFIP2PMATCH_CONNECT_DATA;

// The graphics of the status icons
typedef struct {
    void *scrnBuf;
    NNSG2dScreenData *scrn;
    void *charBuf;
    NNSG2dCharacterData *chr;
    // The icons' characters on BG 1 and, for the lower screen, on BG 6
    u32 bg1Chars;
    u32 bg6Chars;
} WIFIP2PMATCH_ICON;

// The friend list of the lower screen: a page of eight buttons, and a friend's details when a button is touched
typedef struct {
    WordSet *wordSet;
    void *buttonScrnBuf;
    NNSG2dScreenData *buttonScrn;
    void *backScrnBuf;
    NNSG2dScreenData *backScrn;
    u32 unk14;
    // NULL while the view does not exist
    void *buttonMan;
    // What is known of each friend, by friend number - 1: 0 nothing, 1 registered, 2 in the room
    u8 friendState[WIFIP2PMATCH_MEMBER_MAX];
    u8 page;
    // The friend number of the button that was touched, 0 if none
    u8 touchFriendNo;
    u8 buttonFrame;
    // 0 for the list, 1 when a friend's details were asked for, 2 and 3 while they are shown
    u8 detailMode;
    u8 unk40;
    u8 redrawButtons;
    u8 buttonAnimCount;
    u8 lastTouchFriendNo;
    BOOL redrawAll;
    BmpWin *nameWin[WIFIP2PMATCH_VIEW_FRIEND_NUM];
    BmpWin *statusWin[WIFIP2PMATCH_VIEW_FRIEND_NUM][2];
    BmpWin *detailWin[2];
    u8 unkB0[0x44];
} WIFIP2PMATCH_VIEW;

typedef struct {
    TCB *vblankTcb;
    WifiList *wifiList;
    // The player's status, which event_wificlub.c keeps (WifiClubData.buffer)
    WIFI_STATUS *myStatus;
    u8 unk00C[4];
    // The friend numbers (index + 1) of the friends who are in the room, packed from the start and ended by 0, and
    // what it was a frame ago
    u8 index2No[WIFIP2PMATCH_MEMBER_MAX];
    u8 index2NoBackup[WIFIP2PMATCH_MEMBER_MAX];
    // What each friend's status last said
    u8 matchStatus[WIFIP2PMATCH_MEMBER_MAX];
    u8 matchGameMode[WIFIP2PMATCH_MEMBER_MAX];
    u8 matchVChat[WIFIP2PMATCH_MEMBER_MAX];
    // Nothing in this file creates these two: they are only freed
    ListMenuOption *subMenuOptions;
    BmpMenuList *subList;
    ListMenuOption *menuOptions;
    BmpMenuList *list;
    SaveControl *save;
    GameData *gameData;
    PokeParty *party;
    // The Battle Box as a party, NULL if it is empty
    PokeParty *battleBoxParty;
    u8 unk0D0[8];
    void *trainerData;
    WordSet *wordSet;
    MsgData *msgData;
    // The messages about Nintendo WFC
    MsgData *sysMsgData;
    Font *font;
    Font *smallFont;
    // Where a message is loaded, and where it is expanded for printing
    StrBuf *expStrBuf;
    StrBuf *talkString;
    StrBuf *titleString;
    StrBuf *unk0FC;
    WaitIcon *waitIcon;
    u32 unk104;
    BmpMenu *yesNoMenu;
    u32 unk10C;
    ClActUnit *clActUnit;
    ClActRenderer *clActRenderer;
    // The icons of the party's Pokémon, then of the Battle Box's
    u32 pokeIconChars[12];
    u32 pokeIconPalette;
    u32 pokeIconCellAnims;
    ClActor *pokeIcons[12];
    // The friend's trainer on the details page
    ClActor *trainerActor;
    u32 trainerChars;
    u32 trainerPalette;
    u32 trainerCellAnims;
    BmpWin *msgWin;
    // The player's name and status
    BmpWin *myInfoWin;
    BmpWin *titleWin;
    // The window of the messages about Nintendo WFC, and of a battle's rules
    BmpWin *sysMsgWin;
    BmpWin *subWin;
    BmpWin *listWin;
    BmpWin *unk1A8;
    PrintWindow listPrintWindow;
    PrintQueue *listPrintQueue;
    PrintQueue *printQueue;
    PrintStream *printStream;
    TCBExManager *tcbManager;
    // The frames left for a called friend to answer
    s32 callTimer;
    // The frames the player has been in the room since the last connection
    s32 unk1C8;
    // The state: an index of sWifiP2PMatchSeqFuncs
    int seq;
    // What the proc returns to event_wificlub.c (WifiClubData.mode)
    u32 endSeq;
    u32 unk1D4;
    u32 initMode;
    s32 unk1DC;
    s32 timer;
    u32 unk1E4;
    u32 unk1E8;
    u32 unk1EC;
    // The net ID of the friend who is connecting, -1 if none
    s32 preConnect;
    // The game mode each machine sent on connecting, by net ID
    u16 connectGameMode[0x20];
    // Each list menu's cursor
    u16 menuCursor[WIFIP2PMATCH_MENU_CURSOR_NUM];
    u16 menuCursorType;
    // The friend number (index + 1) of the partner
    u16 friendNo;
    u16 unk244;
    // What the player chose in the game mode menu
    u16 unk246;
    WifiClubData *parent;
    // A copy of the partner's status
    WIFI_STATUS targetStatus;
    AppPrintsysCommon printCommon;
    WIFI_MATCHROOM room;
    MCR_MOVEOBJ *moveObj[WIFIP2PMATCH_OBJ_MAX];
    WIFIP2PMATCH_ICON icon;
    WIFIP2PMATCH_VIEW view;
    u32 unk9A4;
    u32 viewBgChars;
    TouchBar *touchBar;
    KeyCursor *keyCursor;
    // Whether the player's character is kept from moving
    BOOL roomPaused;
    // Nothing in this file reaches these bytes
    u8 unk9B8[0x970];
    u16 unk1328;
    // How many friends the friend list has
    u16 friendNum;
    // Which of the three states that ask for a game mode ran
    u16 modeSeq;
    u16 unk132E;
    u32 listFrameChars;
    u32 talkFrameChars;
    u32 subFrameChars;
    u32 unk133C;
    // The fade of the music's volume
    u8 bgmVolume;
    u8 bgmVolumeStart;
    u8 bgmVolumeTarget;
    u8 bgmFadeFrames;
    RegulationPrintMsg *regulationPrintMsg;
    Regulation *regulation;
    // The battle chosen: single, double, triple or rotation, and no restrictions or flat
    u16 battleMode;
    u16 battleRule;
    u8 unk1350;
    u8 unk1351;
    u8 unk1352;
    // The state both sides go to next, which state 0x8b sends, and the one the partner sent
    u8 sendSeq;
    u8 unk1354;
    u8 unk1355;
    u8 unk1356;
    u8 recvSeq;
    u8 unk1358;
    u8 unk1359;
    // Whether the main loop has seen a connection
    u8 unk135A;
    u8 unk135B[5];
    // How the connection was made: 3 and 4 for the two sides of a pair that chooses what to play
    u32 result;
} WIFIP2PMATCH_WORK;

typedef int (*WifiP2PMatchSeqFunc)(WIFIP2PMATCH_WORK *wk, int seq);

static void WifiP2PMatch_YesNoStart(WIFIP2PMATCH_WORK *wk);
static u32 WifiP2PMatch_YesNoMain(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_SetFriendNameWord(WIFIP2PMATCH_WORK *wk, int friendIndex);
static BmpWin *WifiP2PMatch_FreeWindow(BmpWin *window);
static void WifiP2PMatch_FreeWaitIcon(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_EndMessage(WIFIP2PMATCH_WORK *wk);
static BOOL WifiP2PMatch_IsMessageEnd(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_PrintMessageCore(WIFIP2PMATCH_WORK *wk, u32 msgId, BOOL sysMsg, BOOL stream);
static void WifiP2PMatch_PrintMessage(WIFIP2PMATCH_WORK *wk, u32 msgId, BOOL sysMsg);
static void WifiP2PMatch_PrintMessageNow(WIFIP2PMATCH_WORK *wk, u32 msgId, BOOL sysMsg);
static void WifiP2PMatch_PrintSystemMessage(WIFIP2PMATCH_WORK *wk, u32 msgId);
static void WifiP2PMatch_GameModeMenuPrintCallback(BmpMenuList *list, s32 value, u8 y);
static void WifiP2PMatch_OpenListMenu(WIFIP2PMATCH_WORK *wk, BmpMenuListHeader *header,
                                      WIFIP2PMATCH_MENU_OPTION *options, int count, int type, int width);
static void WifiP2PMatch_OpenGameModeMenu(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_OpenFriendMenu(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_DrawMyInfo(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_DrawTitle(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_FreeWindows(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_InitMessages(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_ExitMessages(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_RedrawFriendDetail(WIFIP2PMATCH_WORK *wk, HeapID heapId);
static void WifiP2PMatch_OpenFriendDetail(WIFIP2PMATCH_WORK *wk, HeapID heapId);
static void WifiP2PMatch_CreateTrainerActor(WIFIP2PMATCH_WORK *wk, int friendIndex, u32 heapId);
static void WifiP2PMatch_FreeTrainerActor(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_CreateTouchBar(WIFIP2PMATCH_WORK *wk, HeapID heapId);
static void WifiP2PMatch_FreeTouchBar(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_ReloadViewScreen(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_DrawFriendDetail(WIFIP2PMATCH_WORK *wk, u32 heapId);
static int WifiP2PMatch_GetStatusMessage(u32 status, u32 gameMode, u32 *color);
static void WifiP2PMatch_OpenBattleRulesMenu(WIFIP2PMATCH_WORK *wk, BOOL fromParent);
static void WifiP2PMatch_OpenBattleModeMenu(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_OpenBattleRuleMenu(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_OpenShooterMenu(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_DrawPartyWindow(WIFIP2PMATCH_WORK *wk, PokeParty *party);
static void WifiP2PMatch_FreePartyWindow(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_CreatePartyIcons(WIFIP2PMATCH_WORK *wk, PokeParty *party, int base, ArcTool *arc);
static void WifiP2PMatch_CreatePokeIcons(WIFIP2PMATCH_WORK *wk, PokeParty *party, PokeParty *battleBox);
static void WifiP2PMatch_FreePokeIcons(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_ShowPokeIcons(WIFIP2PMATCH_WORK *wk, BOOL first);
static void WifiP2PMatch_HidePokeIcons(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_OpenPartyMenu(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_DrawRulesWindow(WIFIP2PMATCH_WORK *wk, u32 failBits, BOOL shooter, int kind);
static void WifiP2PMatch_FreeRulesWindow(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_StartWaitIcon(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_ClosePartyMenu(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_StartVChat(WIFIP2PMATCH_WORK *wk);
static int WifiP2PMatch_StateConnected(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StatePlayYesNoStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StatePlayYesNo(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateGameModeMessage(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateGameModeMenuOpen(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateGameModeMenu(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateNoCameraYesNoStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateNoCameraYesNo(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateAnswerNoCameraYesNoStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateAnswerNoCameraYesNo(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateGameModeKeyWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_AskGameMode(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateAskVChat(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateAskTVT(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateAskTrade(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateAnswerYesNoStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateAnswerYesNo(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateTradeRefusedKeyWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateDeclined(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateDeclinedWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateSendStartGame(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateSendCancelReset(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateSendRefusedMsg(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateStartGame(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateBattleSetupMenuStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateBattleSetupMenu(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateBattleModeMenuStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateBattleModeMenu(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateBattleRuleMenuStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateBattleRuleMenu(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateShooterMenuStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateShooterMenu(WIFIP2PMATCH_WORK *wk, int seq);
static u32 WifiP2PMatch_CheckParty(Regulation *regulation, GameData *gameData, u32 *flags);
static PokeParty *WifiP2PMatch_CreateBattleBoxParty(GameData *gameData);
static void WifiP2PMatch_FreeBattleBoxParty(PokeParty *party);
static u32 WifiP2PMatch_CheckBattleBoxParty(Regulation *regulation, PokeParty *party, u32 *flags);
static u32 WifiP2PMatch_LoadRegulation(WIFIP2PMATCH_WORK *wk, int which);
static u32 WifiP2PMatch_LoadMyRegulation(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_SetRegulationCode(WIFIP2PMATCH_WORK *wk, u32 regulationCode);
static int WifiP2PMatch_StateBattleSetupDecide(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateRegulationFailParty(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateRegulationFailBattleBox(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateRegulationFailEnd(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateRulesShow(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateRulesShowWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateBattleOfferShow(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateBattleOfferMsgWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateBattleOfferKeyWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateBattleOfferYesNoGo(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateUnused5F(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateBattleOfferYesNoStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateBattleOfferYesNo(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StatePlayAgainKeyWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateBattleOfferCheckParty(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateRegulationFailKeyWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StatePartySelectStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StatePartyMenuStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StatePartyMenu(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StatePartySend(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StatePartySendTiming(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StatePartySendWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateBattleStartWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateBattleStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StatePartyMenuFailParty(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StatePartyMenuFailBattleBox(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StatePartyMenuFailEnd(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StatePlayAgainAsk(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StatePlayAgainYesNoStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StatePlayAgainYesNo(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateSyncSeqWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateSyncSeqTimingWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateTalkEndTiming(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateTalkEndTimingWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateDeclineReset(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateDeclineMsg(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateCancelReset(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateCancelMsg(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateGoDisconnect(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateRefusedMsg(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateDisconnectStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateVChatDiffMsg(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateVChatDiffYesNoStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateVChatDiffYesNo(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateVChatDiffNo(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateVChatSend(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateRegulationSend(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateRegulationSendEnd(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateSendSeqTiming(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateSendSeq(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateUnableMsg(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateReturnFromGame(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateReturnYesNoStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateReturnYesNo(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateReturnTimingWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateGameStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateGameStartFailKeyWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateBattleRecruitStart(WIFIP2PMATCH_WORK *wk, int seq);
static sptr WifiP2PMatch_GetUserData(void *work);
static sptr WifiP2PMatch_GetFriendData(void *work);
static void WifiP2PMatch_OnFriendCode(int code, int param, void *work);
static void WifiP2PMatch_NetInit(WIFIP2PMATCH_WORK *wk, BOOL init);
static u32 WifiP2PMatch_GetStatus(WIFIP2PMATCH_WORK *wk, const WIFI_STATUS *status);
static u32 WifiP2PMatch_GetGameMode(WIFIP2PMATCH_WORK *wk, const WIFI_STATUS *status);
static BOOL WifiP2PMatch_IsBattleGameMode(u32 gameMode);
static BOOL WifiP2PMatch_IsPlayGameMode(int gameMode);
static BOOL WifiP2PMatch_IsRecruitStatus(u32 status);
static BOOL WifiP2PMatch_IsBattlePlaying(u32 status, u32 gameMode);
static BOOL WifiP2PMatch_HasTwoPokemon(WIFIP2PMATCH_WORK *wk);
static BOOL WifiP2PMatch_PassesRegulation(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_ChangeNetStatus(WIFIP2PMATCH_WORK *wk, u32 status, u32 gameMode);
static void WifiP2PMatch_GraphicInit(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_GraphicExit(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_VBlankTask(TCB *tcb, void *data);
static void WifiP2PMatch_SetVRAMBanks(void);
static void WifiP2PMatch_BGInit(HeapID heapId);
static void WifiP2PMatch_AddMoveObj(WIFIP2PMATCH_WORK *wk, MCR_MOVEOBJ *obj);
static void WifiP2PMatch_RemoveMoveObj(WIFIP2PMATCH_WORK *wk, MCR_MOVEOBJ *obj);
static void WifiP2PMatch_RoomExit(WIFIP2PMATCH_WORK *wk);
static MCR_MOVEOBJ *WifiP2PMatch_GetMoveObj(WIFIP2PMATCH_WORK *wk, u32 friendNo);
static void WifiP2PMatch_IconLoad(WIFIP2PMATCH_ICON *icon, ArcTool *arc, u32 heapId);
static void WifiP2PMatch_IconFree(WIFIP2PMATCH_ICON *icon);
static void WifiP2PMatch_IconWrite(WIFIP2PMATCH_ICON *icon, u32 bg, u8 x, u8 y, u32 iconNo, u32 unused);
static u32 WifiP2PMatch_GetIconNo(u32 gameMode, u32 status);
static void WifiP2PMatch_IconWriteStatus(WIFIP2PMATCH_ICON *icon, u32 bg, u8 x, u8 y, u32 status, u32 gameMode,
                                         u32 add);
static u32 WifiP2PMatch_GetIconPalette(u32 bg);
static int WifiP2PMatch_LeaveConnection(WIFIP2PMATCH_WORK *wk, BOOL wipe);
static void WifiP2PMatch_BGExit(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_BGGraphicLoad(WIFIP2PMATCH_WORK *wk, ArcTool *arc);
static void WifiP2PMatch_CharInit(void);
static void WifiP2PMatch_ClActInit(WIFIP2PMATCH_WORK *wk, ArcTool *arc);
static int WifiP2PMatch_StateInit(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateBGPriority(WIFIP2PMATCH_WORK *wk, int seq);
static void WifiP2PMatch_SendMyStatus(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_InitMyStatus(WIFIP2PMATCH_WORK *wk, u32 status, u32 gameMode);
static int WifiP2PMatch_UpdateRoomFriendList(WIFIP2PMATCH_WORK *wk);
static int WifiP2PMatch_CheckFriendStatusChange(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_OnNetError(WIFIP2PMATCH_WORK *wk);
static int WifiP2PMatch_StateReconnectKeyWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateReconnectYesNoStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateReconnectYesNo(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateConnectStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateConnectWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateUserDataSaveStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateUserDataSave(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateDisconnectWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateUserInfoNotice(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateUserInfoNoticeKeyWait(WIFIP2PMATCH_WORK *wk, int seq);
static void WifiP2PMatch_CreateRoomWindows(WIFIP2PMATCH_WORK *wk);
static BOOL WifiP2PMatch_ConnectRequestFunc(int friendIndex, void *work);
static int WifiP2PMatch_StateLoginWait(WIFIP2PMATCH_WORK *wk, int seq);
static void WifiP2PMatch_StartRoom(WIFIP2PMATCH_WORK *wk);
static int WifiP2PMatch_StateRoomStart(WIFIP2PMATCH_WORK *wk, int seq);
static BOOL WifiP2PMatch_IsConnected(WIFIP2PMATCH_WORK *wk);
static BOOL WifiP2PMatch_HasNewConnection(WIFIP2PMATCH_WORK *wk);
static BOOL WifiP2PMatch_IsConnectFailed(WIFIP2PMATCH_WORK *wk);
static int WifiP2PMatch_UpdateFriendsInRoom(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_AddWaitingFriends(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_UpdateFriendView(WIFIP2PMATCH_WORK *wk, HeapID heapId);
static void WifiP2PMatch_UpdateFriendList(WIFIP2PMATCH_WORK *wk, HeapID heapId);
static void WifiP2PMatch_ShowAllShadows(WIFIP2PMATCH_WORK *wk);
static int WifiP2PMatch_StateRoom(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateRoomMessage(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateVChatCallStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateVChatToggleKeyWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateVChatCallWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateVChatInvited(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateVChatInvitedWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateVChatStart(WIFIP2PMATCH_WORK *wk, int seq);
static void WifiP2PMatch_DrawVChatTalkIcon(WIFIP2PMATCH_WORK *wk);
static int WifiP2PMatch_StateVChat(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateVChatEndYesNoStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateVChatEndYesNo(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateVChatEndSend(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateVChatStopWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateVChatEndTiming(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateVChatDisconnectTiming(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateVChatEndedMessage(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateConnectFailed(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateConnectFailedKeyWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateCutConnection(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateConnectError(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateNone16(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateNone17(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateNone18(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateNoFriendKeyWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateRecruitStopAsk(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateRecruitStopYesNoStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateRecruitStopYesNo(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateRecruitMenuStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateRecruitMenuMessage(WIFIP2PMATCH_WORK *wk, int seq);
static void WifiP2PMatch_FreeRecruitMenu(WIFIP2PMATCH_WORK *wk);
static int WifiP2PMatch_StateRecruitMenu(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateRecruitStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateCameraNoticeYesNoStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateCameraNoticeYesNo(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateConnectFriendYesNoStart(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateConnectFriendYesNo(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateMessageKeyWaitToRoom(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateBackToRoom(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateRecruitMenuOld(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateFriendSelect(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateFriendMenuInit(WIFIP2PMATCH_WORK *wk, int seq);
static BOOL WifiP2PMatch_CallFriend(WIFIP2PMATCH_WORK *wk);
static int WifiP2PMatch_StateCallWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateCallCancelYesNo(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateWaitingFriendMenu(WIFIP2PMATCH_WORK *wk, int seq);
static void WifiP2PMatch_ResetVChatVolume(void);
static int WifiP2PMatch_StateRecruitingFriendMenu(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateJoinFriend(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateJoinWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateDummy3A(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateDummy3B(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateJoinTimeout(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateDummy26(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateDummy27(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateFriendConnected(WIFIP2PMATCH_WORK *wk, int seq);
static BOOL WifiP2PMatch_CheckDisconnect(WIFIP2PMATCH_WORK *wk);
static int WifiP2PMatch_StateNegotiationWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateSendPlayerInfo(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateSendConnectData(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateConnectDataWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateConnectTiming(WIFIP2PMATCH_WORK *wk, int seq);
static BOOL WifiP2PMatch_IsMessageEndOrNone(WIFIP2PMATCH_WORK *wk);
static int WifiP2PMatch_StateConnectEnd(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateReturnToRoom(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateFriendDetailInit(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateFriendDetailWait(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateFriendDetailEnd(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateExitYesNoInit(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateExitYesNo(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateDisconnect(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateDisconnectEnd(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateRetryYesNoInit(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateRetryYesNo(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateVChatChangeYesNoInit(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateVChatChangeYesNo(WIFIP2PMATCH_WORK *wk, int seq);
static int WifiP2PMatch_StateEnd(WIFIP2PMATCH_WORK *wk, int seq);
static void WifiP2PMatch_ChangeMyStatus(WIFIP2PMATCH_WORK *wk, u32 status, int gameMode);
static void WifiP2PMatch_SetMyStatus(WIFIP2PMATCH_WORK *wk, u32 status, u32 gameMode);
static BOOL WifiP2PMatch_ToggleVChat(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_ResetVChat(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_ViewCloseDetail(WIFIP2PMATCH_WORK *wk, HeapID heapId);
static void WifiP2PMatch_ViewCloseDetailOf(WIFIP2PMATCH_WORK *wk, u32 friendNo, HeapID heapId);
static void WifiP2PMatch_ViewCloseDetailIfOpen(WIFIP2PMATCH_WORK *wk, HeapID heapId);
static BOOL WifiP2PMatch_ViewExists(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_ViewInit(WIFIP2PMATCH_WORK *wk, ArcTool *arc, u32 heapId);
static void WifiP2PMatch_ViewExit(WIFIP2PMATCH_WORK *wk);
static u32 WifiP2PMatch_ViewMain(WIFIP2PMATCH_WORK *wk, HeapID heapId);
static void WifiP2PMatch_ViewRedrawButtons(WIFIP2PMATCH_WORK *wk);
static BOOL WifiP2PMatch_ViewDetailCloseInput(WIFIP2PMATCH_WORK *wk);
static u32 WifiP2PMatch_ViewGetTouchFriendNo(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_ViewResetTouch(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_ViewButtonCallback(u32 button, u32 event, void *work);
static void WifiP2PMatch_ViewButtonAnime(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_ViewGraphicInit(WIFIP2PMATCH_WORK *wk, ArcTool *arc, u32 heapId);
static void WifiP2PMatch_ViewGraphicExit(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_ViewScreenAddCharOffset(NNSG2dScreenData *scrn, u16 add);
static void WifiP2PMatch_ViewSetFriendState(WIFIP2PMATCH_WORK *wk, u32 friendNo, u32 state);
static void WifiP2PMatch_ViewClearFriendState(WIFIP2PMATCH_WORK *wk, u32 friendNo);
static u8 WifiP2PMatch_ViewGetFriendState(WIFIP2PMATCH_WORK *wk, u32 friendNo);
static void WifiP2PMatch_ViewRedraw(WIFIP2PMATCH_WORK *wk, HeapID heapId);
static void WifiP2PMatch_ViewRequestRedrawAll(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_ViewPageChanged(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_ViewDrawButtons(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_ViewDrawButton(WIFIP2PMATCH_WORK *wk, u8 x, u8 y, u8 type, u8 frame);
static void WifiP2PMatch_ViewDrawFriendName(WIFIP2PMATCH_WORK *wk, BmpWin *window, u32 friendNo, u32 frame, u32 index);
static void WifiP2PMatch_ViewDrawFriendIcons(WIFIP2PMATCH_WORK *wk, BmpWin **windows, u32 friendNo, u32 frame, u32 x,
                                             u32 y);
static void WifiP2PMatch_ViewSetFriendNameWord(WIFIP2PMATCH_WORK *wk, int friendIndex);
static WIFI_STATUS *WifiP2PMatch_GetFriendStatusData(int friendIndex);
static u8 WifiP2PMatch_GetFriendStatus(int friendIndex);
static BOOL WifiP2PMatch_StartConnect(WIFIP2PMATCH_WORK *wk, int a1);
static void WifiP2PMatch_RoomPause(WIFIP2PMATCH_WORK *wk, BOOL message);
static void WifiP2PMatch_RoomResume(WIFIP2PMATCH_WORK *wk);
static BOOL WifiP2PMatch_BgmVolumeFadeMain(WIFIP2PMATCH_WORK *wk);
static void WifiP2PMatch_BgmVolumeFadeStart(WIFIP2PMATCH_WORK *wk, u8 volume);
static void WifiP2PMatch_BgmVolumeInit(WIFIP2PMATCH_WORK *wk, u32 mode);
static BOOL WifiP2PMatch_ProcInit(GameProc *proc, u32 *state, void *param, void *work);
static BOOL WifiP2PMatch_ProcMain(GameProc *proc, u32 *state, void *param, void *work);
static BOOL WifiP2PMatch_ProcExit(GameProc *proc, u32 *state, void *param, void *work);

// MWCC lays out data objects of one size by where every object of the file is defined, so the order of these
// definitions, and the places of the ones further down (each before its first user), are what gives the ROM's layout

static const WIFIP2PMATCH_MENU_OPTION sWifiP2PMatchFriendMenuOptions[] = {
    { WifiP2pmatch_Text_Connect, 1 },
    { WifiP2pmatch_Text_Cancel, BMPMENULIST_CANCEL },
};

static const BmpMenuListHeader sWifiP2PMatchFriendMenuHeader = {
    NULL, NULL, NULL, 2,    2,    0,    16,   0,  0, 1, 15, 2, 0, 0, BMPMENULIST_SKIP_NONE, 0, BMPMENULIST_CURSOR_SHOW,
    NULL, 16,   16,   NULL, NULL, NULL, NULL, 20,
};

// The values are the states the options lead to; WifiP2PMatch_OpenBattleRulesMenu sets the first three messages
// to the mode, the rule and the Wonder Launcher setting chosen so far
static WIFIP2PMATCH_MENU_OPTION sWifiP2PMatchBattleRulesOptions[] = {
    { WifiP2pmatch_Text_SingleBattle, WIFIP2PMATCH_SEQ_BATTLE_MODE_MENU_START },
    { WifiP2pmatch_Text_NoRestrictions, WIFIP2PMATCH_SEQ_BATTLE_RULE_MENU_START },
    { WifiP2pmatch_Text_WonderLauncher, WIFIP2PMATCH_SEQ_SHOOTER_MENU_START },
    { WifiP2pmatch_Text_Confirm, WIFIP2PMATCH_SEQ_BATTLE_SETUP_DECIDE },
    { WifiP2pmatch_Text_Rules, WIFIP2PMATCH_SEQ_RULES_SHOW },
    { WifiP2pmatch_Text_Cancel, BMPMENULIST_CANCEL },
};

static WIFIP2PMATCH_MENU_OPTION sWifiP2PMatchBattleRuleOptions[] = {
    { WifiP2pmatch_Text_NoRestrictions, 0 },
    { WifiP2pmatch_Text_FlatRule, 1 },
    { WifiP2pmatch_Text_Cancel, BMPMENULIST_CANCEL },
};

// The values are game modes
static WIFIP2PMATCH_MENU_OPTION sWifiP2PMatchGameModeOptions[] = {
    { WifiP2pmatch_Text_VoiceChat, WIFIP2PMATCH_GAMEMODE_VCHAT },
    { WifiP2pmatch_Text_Xtransceiver, WIFIP2PMATCH_GAMEMODE_TVT },
    { WifiP2pmatch_Text_Battle, WIFIP2PMATCH_GAMEMODE_BATTLE },
    { WifiP2pmatch_Text_Trade, WIFIP2PMATCH_GAMEMODE_TRADE },
    { WifiP2pmatch_Text_Cancel, BMPMENULIST_CANCEL },
};

static const ConfirmDialogSetup sWifiP2PMatchYesNoSetup = { 2, 24, 13, 15, 0x115 };

static WIFIP2PMATCH_MENU_OPTION sWifiP2PMatchPartyMenuOptions[] = {
    { WifiP2pmatch_Text_Party, 0 },
    { WifiP2pmatch_Text_BattleBox, 1 },
};

static WIFIP2PMATCH_MENU_OPTION sWifiP2PMatchBattleModeOptions[] = {
    { WifiP2pmatch_Text_SingleBattle, 0 },
    { WifiP2pmatch_Text_DoubleBattle, 1 },
    { WifiP2pmatch_Text_TripleBattle, 2 },
    { WifiP2pmatch_Text_RotationBattle, 3 },
    { WifiP2pmatch_Text_Cancel, BMPMENULIST_CANCEL },
};

static WIFIP2PMATCH_MENU_OPTION sWifiP2PMatchShooterOptions[] = {
    { WifiP2pmatch_Text_WonderLauncher, 0 },
    { WifiP2pmatch_Text_WonderLauncherOff, 1 },
    { WifiP2pmatch_Text_Cancel, BMPMENULIST_CANCEL },
};

static BmpMenuListHeader sWifiP2PMatchGameModeMenuHeader = {
    NULL,
    NULL,
    WifiP2PMatch_GameModeMenuPrintCallback,
    5,
    5,
    0,
    16,
    0,
    0,
    1,
    15,
    2,
    0,
    0,
    BMPMENULIST_SKIP_NONE,
    0,
    BMPMENULIST_CURSOR_SHOW,
    NULL,
    16,
    16,
    NULL,
    NULL,
    NULL,
    NULL,
    20,
};

// The menus that use it set its count
static BmpMenuListHeader sWifiP2PMatchSubMenuHeader = {
    NULL, NULL, NULL, 5,    5,    0,    16,   0,  0, 1, 15, 2, 0, 0, BMPMENULIST_SKIP_NONE, 0, BMPMENULIST_CURSOR_SHOW,
    NULL, 16,   16,   NULL, NULL, NULL, NULL, 20,
};

// The regulation file of each battle, by battleMode + battleRule * 4
static u32 sWifiP2PMatchRegulationFiles[8] = { 5, 6, 7, 8, 0x18, 0x19, 0x1a, 0x1b };

// Opens the yes/no menu; WifiP2PMatch_YesNoMain gives the answer
static void WifiP2PMatch_YesNoStart(WIFIP2PMATCH_WORK *wk) {
    GFL_TextRndResetColorIndexLUT();
    wk->yesNoMenu = ShopUI_CreateConfirmDialog(&sWifiP2PMatchYesNoSetup, wk->talkFrameChars, 14, 0, WIFIP2PMATCH_HEAP);
}

// Returns 0 for yes, BMPMENU_CANCEL for no and BMPMENU_NULL while the menu is open
static u32 WifiP2PMatch_YesNoMain(WIFIP2PMATCH_WORK *wk) {
    u32 ret = ConfirmDialog_Update(wk->yesNoMenu);

    if (ret != BMPMENU_NULL) {
        wk->yesNoMenu = NULL;
    }
    return ret;
}

// Puts a friend's name in the first word of the messages
static void WifiP2PMatch_SetFriendNameWord(WIFIP2PMATCH_WORK *wk, int friendIndex) {
    if (friendIndex != -1) {
        u32 gender = WifiList_GetFriendParam(wk->wifiList, friendIndex, WIFI_LIST_FRIEND_GENDER);

        WifiList_LoadFriendName(wk->wifiList, friendIndex, wk->expStrBuf);
        GFL_WordSetSetStrbuf(wk->wordSet, 0, wk->expStrBuf, gender, 1, 2);
    }
}

// Frees a window and its frame. Returns NULL, for the window's pointer
static BmpWin *WifiP2PMatch_FreeWindow(BmpWin *window) {
    if (window != NULL) {
        BmpWin_ClearFrame(window, WINFRAME_TRANSFER_NOW);
        BmpWin_Free(window);
    }
    return NULL;
}

static void WifiP2PMatch_FreeWaitIcon(WIFIP2PMATCH_WORK *wk) {
    if (wk->waitIcon != NULL) {
        WaitIcon_Free(wk->waitIcon);
        wk->waitIcon = NULL;
    }
}

// Ends the message and frees its window
static void WifiP2PMatch_EndMessage(WIFIP2PMATCH_WORK *wk) {
    WifiP2PMatch_FreeWaitIcon(wk);
    if (!PrintQueue_IsEmpty(wk->printQueue)) {
        PrintQueue_Clear(wk->printQueue);
    }
    if (wk->printStream != NULL) {
        PrintStream_Delete(wk->printStream);
        wk->printStream = NULL;
    }
    wk->msgWin = WifiP2PMatch_FreeWindow(wk->msgWin);
}

// Runs the message. Returns whether it has been printed, which is TRUE when there is none
static BOOL WifiP2PMatch_IsMessageEnd(WIFIP2PMATCH_WORK *wk) {
    BOOL ret = TRUE;

    if (wk->printStream != NULL) {
        if (wk->msgWin != NULL) {
            KeyCursor_Update(wk->keyCursor, wk->printStream, wk->msgWin);
        }
        ret = AppPrintsysCommon_Update(&wk->printCommon, wk->printStream);
        if (ret) {
            if (wk->printStream != NULL) {
                PrintStream_Delete(wk->printStream);
                wk->printStream = NULL;
            }
        }
    }
    return ret;
}

// Prints a message of the match board's bank, or of the Wi-Fi error bank, in a new message window, a character at a
// time or at once
static void WifiP2PMatch_PrintMessageCore(WIFIP2PMATCH_WORK *wk, u32 msgId, BOOL sysMsg, BOOL stream) {
    TextSpeed_GetCurrentWait();
    WifiP2PMatch_FreeWaitIcon(wk);
    wk->msgWin = WifiP2PMatch_FreeWindow(wk->msgWin);
    if (!PrintQueue_IsEmpty(wk->printQueue)) {
        PrintQueue_Clear(wk->printQueue);
    }
    if (wk->printStream != NULL) {
        PrintStream_Delete(wk->printStream);
        wk->printStream = NULL;
    }
    wk->msgWin = BmpWin_CreateDynamic(2, 2, 19, 27, 4, 15, 1);
    if (sysMsg) {
        GFL_MsgDataLoadStrbuf(wk->sysMsgData, msgId, wk->expStrBuf);
    } else {
        GFL_MsgDataLoadStrbuf(wk->msgData, msgId, wk->expStrBuf);
    }
    GFL_WordSetFormatStrbuf(wk->wordSet, wk->talkString, wk->expStrBuf);
    GFL_BitmapFill(BmpWin_GetBitmap(wk->msgWin), 0xff);
    BmpWin_DrawFrame(wk->msgWin, WINFRAME_TRANSFER_VBLANK, wk->talkFrameChars, 14);
    if (!stream) {
        GFL_TextRendererDrawToBitmap(BmpWin_GetBitmap(wk->msgWin), 0, 0, wk->talkString, wk->font);
    } else {
        wk->printStream = PrintStream_Create(wk->msgWin, 0, 0, wk->talkString, wk->font, TextSpeed_GetCurrentWait(),
                                             wk->tcbManager, 2, WIFIP2PMATCH_HEAP, 15);
        AppPrintsysCommon_Init(&wk->printCommon, 2);
    }
    BmpWin_FlushChar(wk->msgWin);
    BmpWin_FlushMap(wk->msgWin);
    GFL_BGSysQueueScrLoad(2);
}

// Prints a message a character at a time; WifiP2PMatch_IsMessageEnd runs it
static void WifiP2PMatch_PrintMessage(WIFIP2PMATCH_WORK *wk, u32 msgId, BOOL sysMsg) {
    wk->sysMsgWin = WifiP2PMatch_FreeWindow(wk->sysMsgWin);
    WifiP2PMatch_PrintMessageCore(wk, msgId, sysMsg, TRUE);
}

// Prints a message at once
static void WifiP2PMatch_PrintMessageNow(WIFIP2PMATCH_WORK *wk, u32 msgId, BOOL sysMsg) {
    wk->sysMsgWin = WifiP2PMatch_FreeWindow(wk->sysMsgWin);
    GFL_TextRndResetColorIndexLUT();
    WifiP2PMatch_PrintMessageCore(wk, msgId, sysMsg, FALSE);
}

// Prints a message of the Wi-Fi error bank in the large window
static void WifiP2PMatch_PrintSystemMessage(WIFIP2PMATCH_WORK *wk, u32 msgId) {
    WifiP2PMatch_FreeWaitIcon(wk);
    wk->sysMsgWin = WifiP2PMatch_FreeWindow(wk->sysMsgWin);
    wk->msgWin = WifiP2PMatch_FreeWindow(wk->msgWin);
    wk->sysMsgWin = BmpWin_CreateDynamic(2, 4, 4, 23, 16, 15, 1);
    GFL_BGSysLoadNCLRDefault(ARCID_FONT, 5, 0, 15 * 0x20, 0x20, WIFIP2PMATCH_HEAP);
    GFL_BitmapFill(BmpWin_GetBitmap(wk->sysMsgWin), 0xff);
    BmpWin_ClearScreen(wk->sysMsgWin);
    BmpWin_FlushMap(wk->sysMsgWin);
    BmpWin_DrawFrame(wk->sysMsgWin, WINFRAME_TRANSFER_VBLANK, wk->talkFrameChars, 14);
    GFL_MsgDataLoadStrbuf(wk->sysMsgData, msgId, wk->expStrBuf);
    GFL_WordSetFormatStrbuf(wk->wordSet, wk->talkString, wk->expStrBuf);
    GFL_TextRndResetColorIndexLUT();
    GFL_TextRendererDrawToBitmap(BmpWin_GetBitmap(wk->sysMsgWin), 4, 4, wk->talkString, wk->font);
    BmpWin_FlushChar(wk->sysMsgWin);
}

// The status menu's print callback: voice chat and the Xtransceiver are in another color while the player's voice
// chat is off
static void WifiP2PMatch_GameModeMenuPrintCallback(BmpMenuList *list, s32 value, u8 y) {
    WIFIP2PMATCH_WORK *wk = BmpMenuList_GetWork(list);

    if (!WIFI_STATUS_GetVChatStatus(wk->myStatus)) {
        switch (value) {
        case WIFIP2PMATCH_GAMEMODE_VCHAT:
        case WIFIP2PMATCH_GAMEMODE_TVT:
            BmpMenuList_SetColors(list, 2, 15, 2);
            break;
        default:
            BmpMenuList_SetColors(list, 1, 15, 2);
            break;
        }
    } else {
        BmpMenuList_SetColors(list, 1, 15, 2);
    }
}

// Opens a list menu of count options at the right edge of the screen, width characters wide, with the cursor where
// the menu of this type last had it
static void WifiP2PMatch_OpenListMenu(WIFIP2PMATCH_WORK *wk, BmpMenuListHeader *header,
                                      WIFIP2PMATCH_MENU_OPTION *options, int count, int type, int width) {
    int i;
    int y;

    PrintQueue_Clear(wk->listPrintQueue);
    GFL_TextRndResetColorIndexLUT();
    wk->menuOptions = ListMenuCore_CreateOptionList(count, WIFIP2PMATCH_HEAP);
    for (i = 0; i < count; i++) {
        ListMenuCore_AppendMsgOption(wk->menuOptions, wk->msgData, options[i].msg, options[i].value, WIFIP2PMATCH_HEAP);
    }
    wk->listWin = WifiP2PMatch_FreeWindow(wk->listWin);
    y = 5;
    if (type == WIFIP2PMATCH_MENU_PARTY) {
        y = 13;
    }
    wk->listWin = BmpWin_CreateDynamic(2, 31 - width, y, width, count * 2, 15, 1);
    BmpWin_FlushMap(wk->listWin);
    BmpWin_DrawFrame(wk->listWin, WINFRAME_TRANSFER_NONE, wk->listFrameChars, 14);
    PrintWindow_Init(&wk->listPrintWindow, wk->listWin);
    header->options = wk->menuOptions;
    header->queue = wk->listPrintQueue;
    header->printWindow = &wk->listPrintWindow;
    header->font = wk->font;
    header->work = wk;
    wk->list = BmpMenuList_Create(header, 0, wk->menuCursor[type], WIFIP2PMATCH_HEAP);
    while (!PrintQueue_IsEmpty(wk->listPrintQueue)) {
        PrintQueue_Main(wk->listPrintQueue);
    }
    BmpMenuList_LoadCursor(wk->list, WIFIP2PMATCH_HEAP);
    GFL_BGSysQueueScrLoad(2);
    PrintWindow_Flush(&wk->listPrintWindow, wk->printQueue);
}

// The menu of what to do with the friend the player is connected to
static void WifiP2PMatch_OpenGameModeMenu(WIFIP2PMATCH_WORK *wk) {
    WifiP2PMatch_OpenListMenu(wk, &sWifiP2PMatchGameModeMenuHeader, sWifiP2PMatchGameModeOptions, 5,
                              WIFIP2PMATCH_MENU_STATUS, 17);
}

// The menu of a friend in the room: connecting is left out unless the friend is waiting or recruiting
static void WifiP2PMatch_OpenFriendMenu(WIFIP2PMATCH_WORK *wk) {
    int count = 2;
    BmpMenuListHeader header = sWifiP2PMatchFriendMenuHeader;
    WIFI_STATUS *status;
    u16 wifiStatus;
    u32 i;

    status = WifiP2PMatch_GetFriendStatusData(WIFI_MCR_PlayerSelect(&wk->room) - 1);
    wifiStatus = WifiP2PMatch_GetStatus(wk, status);
    WIFI_STATUS_GetVChatStatus(status);
    WIFI_STATUS_GetVChatStatus(wk->myStatus);
    wk->menuOptions = ListMenuCore_CreateOptionList(2, WIFIP2PMATCH_HEAP);
    for (i = 0; i < 2; i++) {
        if (i == 0 && wifiStatus != WIFIP2PMATCH_STATUS_RECRUIT && wifiStatus != 5 &&
            wifiStatus != WIFIP2PMATCH_STATUS_WAIT) {
            header.maxShown--;
            header.count--;
            count--;
        } else {
            ListMenuCore_AppendMsgOption(wk->menuOptions, wk->msgData, sWifiP2PMatchFriendMenuOptions[i].msg,
                                         sWifiP2PMatchFriendMenuOptions[i].value, WIFIP2PMATCH_HEAP);
        }
    }
    wk->listWin = BmpWin_CreateDynamic(2, 16, (3 - count) * 2 + 11, 15, count * 2, 15, 1);
    BmpWin_DrawFrame(wk->listWin, WINFRAME_TRANSFER_NONE, wk->talkFrameChars, 14);
    PrintWindow_Init(&wk->listPrintWindow, wk->listWin);
    header.options = wk->menuOptions;
    header.queue = wk->listPrintQueue;
    header.printWindow = &wk->listPrintWindow;
    header.font = wk->font;
    wk->list = BmpMenuList_Create(&header, 0, 0, WIFIP2PMATCH_HEAP);
    while (!PrintQueue_IsEmpty(wk->listPrintQueue)) {
        PrintQueue_Main(wk->listPrintQueue);
    }
    BmpMenuList_LoadCursor(wk->list, WIFIP2PMATCH_HEAP);
    BmpWin_FlushMap(wk->listWin);
    GFL_BGSysQueueScrLoad(2);
    PrintWindow_Flush(&wk->listPrintWindow, wk->printQueue);
}

// Draws the player's name, in the color of their gender, and what they are doing: the status, or the name of the
// friend they are in a voice chat with
static void WifiP2PMatch_DrawMyInfo(WIFIP2PMATCH_WORK *wk) {
    PlayerInfo *info = SaveControl_GetPlayerInfo(wk->save);
    StrBuf *name;
    u32 status;
    u32 gameMode;
    int msgId;
    u32 color;
    u32 icon;

    if (wk->myInfoWin != NULL) {
        GFL_TextRndResetColorIndexLUT();
        name = copyTrainerNameToNewStrbuf(info->name, WIFIP2PMATCH_HEAP);
        GFL_BitmapFill(BmpWin_GetBitmap(wk->myInfoWin), 0xff);
        if (getTrainerGender(info) == 0) {
            GFL_TextRndUpdateColorIndexLUT(5, 6, 0);
        } else {
            GFL_TextRndUpdateColorIndexLUT(3, 4, 0);
        }
        GFL_TextRendererDrawToBitmap(BmpWin_GetBitmap(wk->myInfoWin), 32, 0, name, wk->font);
        GFL_StrBufFree(name);
        status = WifiP2PMatch_GetStatus(wk, wk->myStatus);
        gameMode = WifiP2PMatch_GetGameMode(wk, wk->myStatus);
        msgId = WifiP2PMatch_GetStatusMessage(status, gameMode, &color);
        if (gameMode == WIFIP2PMATCH_GAMEMODE_VCHAT && status == WIFIP2PMATCH_STATUS_PLAYING) {
            if (DWCRap_GetFriendIndex() == -1) {
                GFL_MsgDataLoadStrbuf(wk->msgData, msgId, wk->expStrBuf);
            } else {
                u32 gender = WifiList_GetFriendParam(wk->wifiList, DWCRap_GetFriendIndex(), WIFI_LIST_FRIEND_GENDER);

                if (gender == 0) {
                    GFL_TextRndUpdateColorIndexLUT(5, 6, 0);
                } else if (gender == 1) {
                    GFL_TextRndUpdateColorIndexLUT(3, 4, 0);
                }
                WifiList_LoadFriendName(wk->wifiList, DWCRap_GetFriendIndex(), wk->expStrBuf);
            }
        } else {
            GFL_MsgDataLoadStrbuf(wk->msgData, msgId, wk->expStrBuf);
        }
        GFL_TextRendererDrawToBitmap(BmpWin_GetBitmap(wk->myInfoWin), 102, 0, wk->expStrBuf, wk->font);
        WifiP2PMatch_IconWriteStatus(&wk->icon, 1, 2, 1, status, gameMode, 0x30);
        if (WIFI_STATUS_GetVChatStatus(wk->myStatus)) {
            icon = 15;
        } else {
            icon = 11;
        }
        WifiP2PMatch_IconWrite(&wk->icon, 1, 26, 1, icon + 0x30, 0);
        BmpWin_ClearScreen(wk->myInfoWin);
        BmpWin_FlushMap(wk->myInfoWin);
        BmpWin_FlushChar(wk->myInfoWin);
        GFL_BGSysLoadScr(3);
    }
}

// The title of the upper screen
static void WifiP2PMatch_DrawTitle(WIFIP2PMATCH_WORK *wk) {
    WifiP2PMatch_FreeWindow(wk->titleWin);
    wk->titleWin = BmpWin_CreateDynamic(3, 5, 1, 22, 2, 15, 1);
    BmpWin_FlushMap(wk->titleWin);
    GFL_MsgDataLoadStrbuf(wk->msgData, WifiP2pmatch_Text_Empty_6, wk->titleString);
    GFL_TextRendererDrawToBitmap(BmpWin_GetBitmap(wk->titleWin), 0, 0, wk->titleString, wk->font);
    BmpWin_FlushChar(wk->titleWin);
}

// Frees the message, every window, the menus and the yes/no menu
static void WifiP2PMatch_FreeWindows(WIFIP2PMATCH_WORK *wk) {
    WifiP2PMatch_EndMessage(wk);
    if (wk->listWin != NULL) {
        BmpWin_Free(wk->listWin);
        BmpMenuList_Free(wk->list, NULL, NULL);
        ListMenuCore_FreeOptionList(wk->menuOptions);
        wk->menuOptions = NULL;
    }
    if (wk->titleWin != NULL) {
        BmpWin_Free(wk->titleWin);
        wk->titleWin = NULL;
    }
    if (wk->msgWin != NULL) {
        BmpWin_Free(wk->msgWin);
        wk->msgWin = NULL;
    }
    if (wk->unk1A8 != NULL) {
        BmpWin_Free(wk->unk1A8);
        wk->unk1A8 = NULL;
    }
    if (wk->myInfoWin != NULL) {
        BmpWin_Free(wk->myInfoWin);
        wk->myInfoWin = NULL;
    }
    if (wk->subWin != NULL) {
        BmpWin_Free(wk->subWin);
        wk->subWin = NULL;
    }
    if (wk->sysMsgWin != NULL) {
        BmpWin_ClearScreen(wk->sysMsgWin);
        GFL_BGSysQueueScrLoad(2);
        WifiP2PMatch_FreeWindow(wk->sysMsgWin);
        wk->sysMsgWin = NULL;
    }
    if (wk->subList != NULL) {
        BmpMenuList_Free(wk->subList, NULL, NULL);
        wk->subList = NULL;
    }
    if (wk->subMenuOptions != NULL) {
        ListMenuCore_FreeOptionList(wk->subMenuOptions);
        wk->subMenuOptions = NULL;
    }
    if (wk->yesNoMenu != NULL) {
        ConfirmDialog_Free(wk->yesNoMenu);
        wk->yesNoMenu = NULL;
    }
}

// The strings, print queues and cursor of the messages
static void WifiP2PMatch_InitMessages(WIFIP2PMATCH_WORK *wk) {
    wk->talkString = GFL_StrBufCreate(380, WIFIP2PMATCH_HEAP);
    wk->unk0FC = GFL_StrBufCreate(380, WIFIP2PMATCH_HEAP);
    wk->expStrBuf = GFL_StrBufCreate(380, WIFIP2PMATCH_HEAP);
    wk->titleString = GFL_StrBufCreate(180, WIFIP2PMATCH_HEAP);
    wk->printQueue = PrintQueue_CreateDefault(WIFIP2PMATCH_HEAP);
    wk->listPrintQueue = PrintQueue_CreateDefault(WIFIP2PMATCH_HEAP);
    wk->tcbManager = GFL_TCBExMgrCreate(WIFIP2PMATCH_HEAP, WIFIP2PMATCH_HEAP, 2, 0);
    wk->keyCursor = KeyCursor_Create(15, TRUE, FALSE, WIFIP2PMATCH_HEAP);
}

static void WifiP2PMatch_ExitMessages(WIFIP2PMATCH_WORK *wk) {
    WifiP2PMatch_FreeTouchBar(wk);
    WifiP2PMatch_FreeTrainerActor(wk);
    KeyCursor_Free(wk->keyCursor);
    GFL_StrBufFree(wk->titleString);
    GFL_StrBufFree(wk->talkString);
    GFL_StrBufFree(wk->unk0FC);
    if (wk->printStream != NULL) {
        PrintStream_Delete(wk->printStream);
        wk->printStream = NULL;
    }
    GFL_StrBufFree(wk->expStrBuf);
    GFL_TCBExMgrFree(wk->tcbManager);
    PrintQueue_Clear(wk->printQueue);
    PrintQueue_Clear(wk->listPrintQueue);
    PrintQueue_Free(wk->printQueue);
    PrintQueue_Free(wk->listPrintQueue);
    wk->printQueue = NULL;
}

// Draws the detail page of the friend whose button was touched again
static void WifiP2PMatch_RedrawFriendDetail(WIFIP2PMATCH_WORK *wk, HeapID heapId) {
    WifiList_GetFriendParam(wk->wifiList, wk->view.touchFriendNo - 1, WIFI_LIST_FRIEND_GENDER);
    GFL_BitmapFill(BmpWin_GetBitmap(wk->view.detailWin[0]), 0);
    WifiP2PMatch_DrawFriendDetail(wk, heapId);
    GFL_BGSysSetBGEnabledB(GX_PLANEMASK_BG1, FALSE);
    GFL_BGSysSetBGEnabledB(GX_PLANEMASK_BG3, TRUE);
}

// Opens the detail page of the friend whose button was touched, in place of the buttons
static void WifiP2PMatch_OpenFriendDetail(WIFIP2PMATCH_WORK *wk, HeapID heapId) {
    WifiList_GetFriendParam(wk->wifiList, wk->view.touchFriendNo - 1, WIFI_LIST_FRIEND_GENDER);
    GFL_BGSysBufferScr(6, wk->view.backScrn->rawData, wk->view.backScrn->szByte, 0);
    GFL_BGSysClearBG(7);
    GFL_BitmapFill(BmpWin_GetBitmap(wk->view.detailWin[1]), 0);
    GFL_BitmapFill(BmpWin_GetBitmap(wk->view.detailWin[0]), 0);
    WifiP2PMatch_DrawFriendDetail(wk, heapId);
    GFL_BGSysQueueScrLoad(7);
    BmpWin_FlushChar(wk->view.detailWin[1]);
    BmpWin_FlushChar(wk->view.detailWin[0]);
    GFL_BGSysSetBGEnabledB(GX_PLANEMASK_BG1, FALSE);
    GFL_BGSysSetBGEnabledB(GX_PLANEMASK_BG3, TRUE);
}

// The sprite of a friend's trainer on the detail page
static void WifiP2PMatch_CreateTrainerActor(WIFIP2PMATCH_WORK *wk, int friendIndex, u32 heapId) {
    ClActorSetup setup = { 56, 80, 0, 0, 0 };
    u32 trainerClass;
    ArcTool *arc;

    if (wk->trainerActor != NULL) {
        WifiP2PMatch_FreeTrainerActor(wk);
    }
    trainerClass =
        UnionView_GetTrainerClass(WifiList_GetFriendParam(wk->wifiList, friendIndex, WIFI_LIST_FRIEND_TRAINER_VIEW));
    arc = MakeTrGraArcHandle(heapId);
    wk->trainerChars = TrGra_LoadClActChars(arc, trainerClass, 1, heapId);
    wk->trainerPalette = TrGra_LoadClActPalette(arc, trainerClass, 1, 0, heapId);
    wk->trainerCellAnims = TrGra_LoadClActCellAnims(trainerClass, 2, 1, heapId);
    GFL_ArcToolFree(arc);
    wk->trainerActor =
        ClActor_Create(wk->clActUnit, wk->trainerChars, wk->trainerPalette, wk->trainerCellAnims, &setup, 1, heapId);
}

static void WifiP2PMatch_FreeTrainerActor(WIFIP2PMATCH_WORK *wk) {
    if (wk->trainerActor != NULL) {
        ClActor_Delete(wk->trainerActor);
        ClActSys_FreeChars(wk->trainerChars);
        ClActSys_FreePltt(wk->trainerPalette);
        ClActSys_FreeCellAnims(wk->trainerCellAnims);
        wk->trainerActor = NULL;
    }
}

// Declared in app/wifi_p2pmatch.h, which says why. Nothing reads it
const u16 WIFIP2PMATCH_UNK_100 = 100;

// The detail page's touch bar, with the return button
static void WifiP2PMatch_CreateTouchBar(WIFIP2PMATCH_WORK *wk, HeapID heapId) {
    TouchBarItem items[] = {
        { TOUCHBAR_ICON_RETURN, { 232, 168 } },
    };
    TouchBarSetup setup;

    if (wk->touchBar == NULL) {
        MI_CpuFill8(&setup, 0, sizeof(setup));
        setup.items = items;
        setup.count = NELEMS(items);
        setup.unit = wk->clActUnit;
        setup.unk1C = FALSE;
        setup.bg = 4;
        setup.bgPalette = 12;
        setup.objPalette = 12;
        setup.vramType = 2;
        wk->touchBar = TouchBar_Create(&setup, heapId);
        TouchBar_SetIconKey(wk->touchBar, TOUCHBAR_ICON_RETURN, 0);
    }
}

static void WifiP2PMatch_FreeTouchBar(WIFIP2PMATCH_WORK *wk) {
    if (wk->touchBar != NULL) {
        TouchBar_Free(wk->touchBar);
        wk->touchBar = NULL;
    }
}

// Loads the friend buttons' screen again
static void WifiP2PMatch_ReloadViewScreen(WIFIP2PMATCH_WORK *wk) {
    ArcTool *arc = GFL_ArcSysCreateFileHandle(ARCID_WIFIP2PMATCH, WIFIP2PMATCH_HEAP);

    GFL_G2DIOLoadNSCRSync(arc, 22, 4, 0, (u16)wk->viewBgChars, 0, FALSE, WIFIP2PMATCH_HEAP);
    GFL_ArcToolFree(arc);
}

// Draws the detail page of the friend whose button was touched: the name, the status, the battle record, the number
// of trades, the date of the last game, the status icons, the trainer and the touch bar
static void WifiP2PMatch_DrawFriendDetail(WIFIP2PMATCH_WORK *wk, u32 heapId) {
    int friendIndex;
    WIFI_STATUS *friendStatus;
    u32 status;
    u32 gameMode;
    int msgId;
    u32 color;
    u32 day;
    u32 icon;

    GFL_TextRndResetColorIndexLUT();
    friendIndex = wk->view.touchFriendNo - 1;
    WifiList_GetFriendParam(wk->wifiList, friendIndex, WIFI_LIST_FRIEND_GENDER);
    GFL_TextRndUpdateColorIndexLUT(15, 2, 0);
    WifiP2PMatch_ViewSetFriendNameWord(wk, friendIndex);
    GFL_MsgDataLoadStrbuf(wk->msgData, WifiP2pmatch_Text_Empty_17, wk->expStrBuf);
    GFL_WordSetFormatStrbuf(wk->view.wordSet, wk->titleString, wk->expStrBuf);
    GFL_TextRendererDrawToBitmap(BmpWin_GetBitmap(wk->view.detailWin[0]), 25, 0, wk->titleString, wk->font);

    friendStatus = WifiP2PMatch_GetFriendStatusData(friendIndex);
    status = WifiP2PMatch_GetStatus(wk, friendStatus);
    gameMode = WifiP2PMatch_GetGameMode(wk, friendStatus);
    msgId = WifiP2PMatch_GetStatusMessage(status, gameMode, &color);
    // The battles' statuses have short names for this page
    if (msgId >= WifiP2pmatch_Text_Single && msgId <= WifiP2pmatch_Text_Rotation) {
        msgId -= WifiP2pmatch_Text_Single - WifiP2pmatch_Text_SingleBattle;
    }
    GFL_MsgDataLoadStrbuf(wk->msgData, msgId, wk->expStrBuf);
    GFL_TextRendererDrawToBitmapEx(BmpWin_GetBitmap(wk->view.detailWin[0]), 96, 0, wk->expStrBuf, wk->font,
                                   PRINT_COLOR(15, 2, 0));

    GFL_MsgDataLoadStrbuf(wk->msgData, WifiP2pmatch_Text_BattleRecord, wk->titleString);
    GFL_TextRendererDrawToBitmapEx(BmpWin_GetBitmap(wk->view.detailWin[1]), 104, 0, wk->titleString, wk->font,
                                   PRINT_COLOR(15, 2, 0));
    WordSetNumber(wk->view.wordSet, 0, WifiList_GetFriendParam(wk->wifiList, friendIndex, WIFI_LIST_FRIEND_WINS), 4, 1,
                  TRUE);
    GFL_MsgDataLoadStrbuf(wk->msgData, WifiP2pmatch_Text_W, wk->expStrBuf);
    GFL_WordSetFormatStrbuf(wk->view.wordSet, wk->titleString, wk->expStrBuf);
    GFL_TextRendererDrawToBitmapEx(BmpWin_GetBitmap(wk->view.detailWin[1]), 176, 24, wk->titleString, wk->font,
                                   PRINT_COLOR(15, 2, 0));
    WordSetNumber(wk->view.wordSet, 0, WifiList_GetFriendParam(wk->wifiList, friendIndex, WIFI_LIST_FRIEND_LOSSES), 4,
                  1, TRUE);
    GFL_MsgDataLoadStrbuf(wk->msgData, WifiP2pmatch_Text_L, wk->expStrBuf);
    GFL_WordSetFormatStrbuf(wk->view.wordSet, wk->titleString, wk->expStrBuf);
    GFL_TextRendererDrawToBitmapEx(BmpWin_GetBitmap(wk->view.detailWin[1]), 176, 40, wk->titleString, wk->font,
                                   PRINT_COLOR(15, 2, 0));

    GFL_MsgDataLoadStrbuf(wk->msgData, WifiP2pmatch_Text_PokemonTrades, wk->titleString);
    GFL_TextRendererDrawToBitmapEx(BmpWin_GetBitmap(wk->view.detailWin[1]), 104, 64, wk->titleString, wk->font,
                                   PRINT_COLOR(15, 2, 0));
    WordSetNumber(wk->view.wordSet, 0, WifiList_GetFriendParam(wk->wifiList, friendIndex, WIFI_LIST_FRIEND_DRAWS), 4, 1,
                  TRUE);
    GFL_MsgDataLoadStrbuf(wk->msgData, WifiP2pmatch_Text_Empty_19, wk->expStrBuf);
    GFL_WordSetFormatStrbuf(wk->view.wordSet, wk->titleString, wk->expStrBuf);
    GFL_TextRendererDrawToBitmapEx(BmpWin_GetBitmap(wk->view.detailWin[1]), 188, 64, wk->titleString, wk->font,
                                   PRINT_COLOR(15, 2, 0));

    GFL_MsgDataLoadStrbuf(wk->msgData, WifiP2pmatch_Text_DateLastPlayed, wk->titleString);
    GFL_TextRendererDrawToBitmapEx(BmpWin_GetBitmap(wk->view.detailWin[1]), 8, 96, wk->titleString, wk->font,
                                   PRINT_COLOR(15, 2, 0));
    day = WifiList_GetFriendParam(wk->wifiList, friendIndex, WIFI_LIST_FRIEND_LAST_DAY);
    if (day != 0) {
        WordSetNumber(wk->view.wordSet, 2, day, 2, 0, TRUE);
        WordSetNumber(wk->view.wordSet, 0,
                      WifiList_GetFriendParam(wk->wifiList, friendIndex, WIFI_LIST_FRIEND_LAST_YEAR), 4, 1, TRUE);
        WordSetNumber(wk->view.wordSet, 1,
                      WifiList_GetFriendParam(wk->wifiList, friendIndex, WIFI_LIST_FRIEND_LAST_MONTH), 2, 0, TRUE);
        GFL_MsgDataLoadStrbuf(wk->msgData, WifiP2pmatch_Text_Empty_23, wk->expStrBuf);
        GFL_WordSetFormatStrbuf(wk->view.wordSet, wk->titleString, wk->expStrBuf);
        GFL_TextRendererDrawToBitmapEx(BmpWin_GetBitmap(wk->view.detailWin[1]), 152, 96, wk->titleString, wk->font,
                                       PRINT_COLOR(15, 2, 0));
    }
    BmpWin_FlushMap(wk->view.detailWin[1]);
    BmpWin_FlushChar(wk->view.detailWin[1]);
    BmpWin_FlushMap(wk->view.detailWin[0]);
    BmpWin_FlushChar(wk->view.detailWin[0]);
    GFL_BGSysQueueScrLoad(7);

    WifiP2PMatch_IconWriteStatus(&wk->icon, 6, 2, 1, status, gameMode, 0x20);
    if (WIFI_STATUS_GetUseVChat(friendStatus)) {
        icon = 0x2f;
    } else {
        icon = 0x2b;
    }
    WifiP2PMatch_IconWrite(&wk->icon, 6, 28, 1, icon, 0);
    WifiP2PMatch_CreateTrainerActor(wk, friendIndex, heapId);
    WifiP2PMatch_CreateTouchBar(wk, heapId);
    GFL_BGSysSetBGEnabledB(GX_PLANEMASK_OBJ, TRUE);
}

// The message of a status, by its game mode. Sets the text colors
static int WifiP2PMatch_GetStatusMessage(u32 status, u32 gameMode, u32 *color) {
    GFL_TextRndUpdateColorIndexLUT(1, 2, 0);
    GFL_TextRndUpdateColorIndexLUT(1, 2, 0);
    {
        int msgs[] = {
            WifiP2pmatch_Text_StandingBy, WifiP2pmatch_Text_StandingBy, WifiP2pmatch_Text_StandingBy,
            WifiP2pmatch_Text_Talking,    WifiP2pmatch_Text_VoiceChat,  WifiP2pmatch_Text_Xtransceiver,
            WifiP2pmatch_Text_Trade,      WifiP2pmatch_Text_Single,     WifiP2pmatch_Text_Single,
            WifiP2pmatch_Text_Double,     WifiP2pmatch_Text_Double,     WifiP2pmatch_Text_Triple,
            WifiP2pmatch_Text_Triple,     WifiP2pmatch_Text_Rotation,   WifiP2pmatch_Text_Rotation,
            WifiP2pmatch_Text_Empty_30,
        };

        if (gameMode > WIFIP2PMATCH_GAMEMODE_MAX) {
            gameMode = WIFIP2PMATCH_GAMEMODE_MAX;
        }
        return msgs[gameMode];
    }
}

static BmpMenuListHeader sWifiP2PMatchBattleRulesMenuHeader = {
    NULL, NULL, NULL, 6,    6,    0,    16,   0,  0, 1, 15, 2, 0, 0, BMPMENULIST_SKIP_NONE, 0, BMPMENULIST_CURSOR_SHOW,
    NULL, 16,   16,   NULL, NULL, NULL, NULL, 20,
};

// The menu of a battle's rules, which shows the mode, the rule and the Wonder Launcher setting chosen: the player's,
// or those the friend sent
static void WifiP2PMatch_OpenBattleRulesMenu(WIFIP2PMATCH_WORK *wk, BOOL fromParent) {
    if (fromParent) {
        sWifiP2PMatchBattleRulesOptions[0].msg = WifiP2pmatch_Text_BattleFormatSingleBattle + wk->parent->battleMode;
        sWifiP2PMatchBattleRulesOptions[1].msg = WifiP2pmatch_Text_BattleRuleNoRestrictions + wk->parent->battleRule;
        sWifiP2PMatchBattleRulesOptions[2].msg = WifiP2pmatch_Text_WonderLauncherOff_2 - wk->parent->unk20;
    } else {
        sWifiP2PMatchBattleRulesOptions[0].msg = WifiP2pmatch_Text_BattleFormatSingleBattle + wk->battleMode;
        sWifiP2PMatchBattleRulesOptions[1].msg = WifiP2pmatch_Text_BattleRuleNoRestrictions + wk->battleRule;
        sWifiP2PMatchBattleRulesOptions[2].msg = WifiP2pmatch_Text_WonderLauncherOff_2 - wk->parent->unk48;
    }
    WifiP2PMatch_OpenListMenu(wk, &sWifiP2PMatchBattleRulesMenuHeader, sWifiP2PMatchBattleRulesOptions, 6,
                              WIFIP2PMATCH_MENU_BATTLE_RULES, 30);
}

static void WifiP2PMatch_OpenBattleModeMenu(WIFIP2PMATCH_WORK *wk) {
    sWifiP2PMatchSubMenuHeader.count = 5;
    sWifiP2PMatchSubMenuHeader.maxShown = 5;
    WifiP2PMatch_OpenListMenu(wk, &sWifiP2PMatchSubMenuHeader, sWifiP2PMatchBattleModeOptions, 5,
                              WIFIP2PMATCH_MENU_BATTLE_MODE, 17);
}

static void WifiP2PMatch_OpenBattleRuleMenu(WIFIP2PMATCH_WORK *wk) {
    sWifiP2PMatchSubMenuHeader.count = 3;
    sWifiP2PMatchSubMenuHeader.maxShown = 3;
    WifiP2PMatch_OpenListMenu(wk, &sWifiP2PMatchSubMenuHeader, sWifiP2PMatchBattleRuleOptions, 3,
                              WIFIP2PMATCH_MENU_BATTLE_RULE, 17);
}

static void WifiP2PMatch_OpenShooterMenu(WIFIP2PMATCH_WORK *wk) {
    sWifiP2PMatchSubMenuHeader.count = 3;
    sWifiP2PMatchSubMenuHeader.maxShown = 3;
    WifiP2PMatch_OpenListMenu(wk, &sWifiP2PMatchSubMenuHeader, sWifiP2PMatchShooterOptions, 3,
                              WIFIP2PMATCH_MENU_SHOOTER, 20);
}

// The window over the party's icons: each Pokémon's level and gender
static void WifiP2PMatch_DrawPartyWindow(WIFIP2PMATCH_WORK *wk, PokeParty *party) {
    StrBuf *levelString;
    StrBuf *string;
    int count;
    int i;

    if (wk->subWin == NULL) {
        wk->subWin = BmpWin_CreateDynamic(2, 1, 5, 30, 6, 15, 1);
    }
    BmpWin_DrawFrame(wk->subWin, WINFRAME_TRANSFER_NONE, wk->talkFrameChars, 14);
    levelString = GFL_StrBufCreate(64, WIFIP2PMATCH_HEAP);
    string = GFL_StrBufCreate(64, WIFIP2PMATCH_HEAP);
    GFL_BitmapFill(BmpWin_GetBitmap(wk->subWin), 0xff);
    BmpWin_ClearScreen(wk->subWin);
    count = PokeParty_GetPkmCount(party);
    for (i = 0; i < count; i++) {
        PartyPkm *pkm = PokeParty_GetPkm(party, i);
        u32 species;
        u32 sex;
        u32 level;
        u32 nidoranNickname;

        if (PokeParty_GetParam(pkm, PKM_PARAM_IS_EGG, NULL)) {
            continue;
        }
        // BUG: the species is meant, and the personality value is read, so the gender of a Nidoran whose name shows
        // it is printed as well
#ifdef BUGFIX
        species = PokeParty_GetParam(pkm, PKM_PARAM_SPECIES, NULL);
#else
        species = PokeParty_GetParam(pkm, PKM_PARAM_PID, NULL);
#endif
        nidoranNickname = PokeParty_GetParam(pkm, PKM_PARAM_NIDORAN_NICKNAME, NULL);
        level = PokeParty_GetParam(pkm, PKM_PARAM_LEVEL, NULL);
        sex = PokeParty_GetParam(pkm, PKM_PARAM_SEX, NULL);
        GFL_MsgDataLoadStrbuf(wk->msgData, WifiP2pmatch_Text_L_2, string);
        WordSetNumber(wk->wordSet, 0, level, 3, 0, TRUE);
        GFL_WordSetFormatStrbuf(wk->wordSet, levelString, string);
        GFL_TextRendererDrawToBitmap(BmpWin_GetBitmap(wk->subWin), i * 40 + 1, 32, levelString, wk->smallFont);
        if (sex == GENDER_UNKNOWN) {
            continue;
        }
        if ((species == SPECIES_NIDORAN_F || species == SPECIES_NIDORAN_M) && !nidoranNickname) {
            continue;
        }
        if (sex == GENDER_MALE) {
            GFL_MsgDataLoadStrbuf(wk->msgData, WifiP2pmatch_Text_Empty_44, string);
            GFL_TextRendererDrawToBitmap(BmpWin_GetBitmap(wk->subWin), i * 40 + 30, 16, string, wk->font);
        } else {
            GFL_MsgDataLoadStrbuf(wk->msgData, WifiP2pmatch_Text_Empty_45, string);
            GFL_TextRendererDrawToBitmap(BmpWin_GetBitmap(wk->subWin), i * 40 + 30, 16, string, wk->font);
        }
    }
    GFL_StrBufFree(levelString);
    GFL_StrBufFree(string);
    BmpWin_FlushMap(wk->subWin);
    BmpWin_FlushChar(wk->subWin);
    GFL_BGSysQueueScrLoad(2);
}

static void WifiP2PMatch_FreePartyWindow(WIFIP2PMATCH_WORK *wk) {
    if (wk->subWin != NULL) {
        BmpWin_ClearScreen(wk->subWin);
        BmpWin_ClearFrame(wk->subWin, WINFRAME_TRANSFER_VBLANK);
        BmpWin_Free(wk->subWin);
        wk->subWin = NULL;
    }
}

// The icons of a party's Pokémon, from icon base on
static void WifiP2PMatch_CreatePartyIcons(WIFIP2PMATCH_WORK *wk, PokeParty *party, int base, ArcTool *arc) {
    int count = PokeParty_GetPkmCount(party);
    int i;

    for (i = 0; i < count; i++) {
        BoxPkm *pkm = PokeParty_ToBoxPkm(PokeParty_GetPkm(party, i));
        ClActorSetup setup;

        setup.x = i * 40 + 22;
        setup.y = 54;
        setup.sequence = 0;
        setup.priority = 0;
        setup.bgPriority = 0;
        wk->pokeIconChars[base + i] = ClActSys_LoadChars(arc, PokeIcon_GetFileOfPkm(pkm), FALSE, 0, WIFIP2PMATCH_HEAP);
        wk->pokeIcons[base + i] = ClActor_Create(wk->clActUnit, wk->pokeIconChars[base + i], wk->pokeIconPalette,
                                                 wk->pokeIconCellAnims, &setup, 0, WIFIP2PMATCH_HEAP);
        ClActor_SetPalette(wk->pokeIcons[base + i], PokeIcon_GetPaletteOfPkm(pkm), 1);
    }
}

// The icons of the party and of the Battle Box, hidden
static void WifiP2PMatch_CreatePokeIcons(WIFIP2PMATCH_WORK *wk, PokeParty *party, PokeParty *battleBox) {
    ArcTool *arc = GFL_ArcSysCreateFileHandle(ARCID_POKEICON, WIFIP2PMATCH_HEAP);

    wk->pokeIconPalette = ClActSys_LoadPlttEx(arc, PokeIcon_GetPaletteFile(), 0, 11 * 0x20, WIFIP2PMATCH_HEAP);
    wk->pokeIconCellAnims =
        ClActSys_LoadCellAnims(arc, PokeIcon_GetCellFile_Sub(), getOBJTileMapping_MainEng(), WIFIP2PMATCH_HEAP);
    WifiP2PMatch_CreatePartyIcons(wk, party, 0, arc);
    WifiP2PMatch_CreatePartyIcons(wk, battleBox, 6, arc);
    WifiP2PMatch_HidePokeIcons(wk);
    GFL_ArcToolFree(arc);
}

static void WifiP2PMatch_FreePokeIcons(WIFIP2PMATCH_WORK *wk) {
    int i;

    if (wk->pokeIcons[0] != NULL) {
        for (i = 0; i < 12; i++) {
            if (wk->pokeIcons[i] != NULL) {
                ClActor_Delete(wk->pokeIcons[i]);
                ClActSys_FreeChars(wk->pokeIconChars[i]);
            }
            wk->pokeIcons[i] = NULL;
        }
        ClActSys_FreePltt(wk->pokeIconPalette);
        ClActSys_FreeCellAnims(wk->pokeIconCellAnims);
    }
}

// Shows the first six icons and hides the other six, or the other way around
static void WifiP2PMatch_ShowPokeIcons(WIFIP2PMATCH_WORK *wk, BOOL first) {
    int i;

    for (i = 0; i < 6; i++) {
        if (wk->pokeIcons[i] != NULL) {
            if (first) {
                ClActor_SetAutoAnim(wk->pokeIcons[i], TRUE);
                ClActor_SetVisible(wk->pokeIcons[i], TRUE);
            } else {
                ClActor_SetAutoAnim(wk->pokeIcons[i], FALSE);
                ClActor_SetVisible(wk->pokeIcons[i], FALSE);
            }
        }
    }
    for (i = 6; i < 12; i++) {
        if (wk->pokeIcons[i] != NULL) {
            if (first) {
                ClActor_SetAutoAnim(wk->pokeIcons[i], FALSE);
                ClActor_SetVisible(wk->pokeIcons[i], FALSE);
            } else {
                ClActor_SetAutoAnim(wk->pokeIcons[i], TRUE);
                ClActor_SetVisible(wk->pokeIcons[i], TRUE);
            }
        }
    }
}

static void WifiP2PMatch_HidePokeIcons(WIFIP2PMATCH_WORK *wk) {
    int i;

    for (i = 0; i < 12; i++) {
        if (wk->pokeIcons[i] != NULL) {
            ClActor_SetAutoAnim(wk->pokeIcons[i], FALSE);
            ClActor_SetVisible(wk->pokeIcons[i], FALSE);
        }
    }
}

// The menu of the party and the Battle Box, which B doesn't close
static void WifiP2PMatch_OpenPartyMenu(WIFIP2PMATCH_WORK *wk) {
    sWifiP2PMatchSubMenuHeader.count = 2;
    sWifiP2PMatchSubMenuHeader.maxShown = 2;
    WifiP2PMatch_OpenListMenu(wk, &sWifiP2PMatchSubMenuHeader, sWifiP2PMatchPartyMenuOptions, 2,
                              WIFIP2PMATCH_MENU_PARTY, 18);
    BmpMenuList_SetCancelDisabled(wk->list, TRUE);
}

// The window of the regulation's six rules, under a title (a WIFIP2PMATCH_RULES_*). The rules whose bit of failBits
// is set, those the party breaks, are in another color
static void WifiP2PMatch_DrawRulesWindow(WIFIP2PMATCH_WORK *wk, u32 failBits, BOOL shooter, int kind) {
    int top = 4;
    int i;

    if (shooter == TRUE) {
        shooter = TRUE;
    } else {
        shooter = FALSE;
    }
    wk->sysMsgWin = WifiP2PMatch_FreeWindow(wk->sysMsgWin);
    if (kind == WIFIP2PMATCH_RULES_SHORT) {
        wk->sysMsgWin = BmpWin_CreateDynamic(2, 2, 5, 26, 12, 15, 1);
    } else {
        wk->sysMsgWin = BmpWin_CreateDynamic(2, 2, 5, 26, 16, 15, 1);
    }
    GFL_BitmapFill(BmpWin_GetBitmap(wk->sysMsgWin), 0xff);
    BmpWin_ClearScreen(wk->sysMsgWin);
    BmpWin_FlushMap(wk->sysMsgWin);
    BmpWin_DrawFrame(wk->sysMsgWin, WINFRAME_TRANSFER_VBLANK, wk->talkFrameChars, 14);
    GFL_TextRndResetColorIndexLUT();
    wk->regulationPrintMsg = PokeRegulation_CreatePrintMsg(wk->regulation, wk->wordSet, WIFIP2PMATCH_HEAP, shooter);
    if (kind == WIFIP2PMATCH_RULES_BATTLE_TYPE) {
        StrBuf *title = makeStrbufLoadBattleTypeText(wk->regulation, WIFIP2PMATCH_HEAP);

        GFL_TextRendererDrawToBitmap(BmpWin_GetBitmap(wk->sysMsgWin), 0, 0, title, wk->font);
        GFL_StrBufFree(title);
    } else if (kind != WIFIP2PMATCH_RULES_SHORT) {
        u32 msgId = WifiP2pmatch_Text_Party;

        if (kind != WIFIP2PMATCH_RULES_PARTY) {
            msgId = WifiP2pmatch_Text_BattleBox;
        }
        GFL_MsgDataLoadStrbuf(wk->msgData, msgId, wk->expStrBuf);
        GFL_TextRendererDrawToBitmap(BmpWin_GetBitmap(wk->sysMsgWin), 0, 0, wk->expStrBuf, wk->font);
    }
    if (kind == WIFIP2PMATCH_RULES_SHORT) {
        top = 0;
    }
    for (i = 0; i < 6; i++) {
        u16 color;

        if (failBits & (1 << i)) {
            color = PRINT_COLOR(4, 2, 0);
        } else {
            color = PRINT_COLOR(1, 2, 0);
        }
        GFL_TextRendererDrawToBitmapEx(BmpWin_GetBitmap(wk->sysMsgWin), 0, (top + i * 2) * 8,
                                       wk->regulationPrintMsg->rule[i], wk->font, color);
        GFL_TextRendererDrawToBitmapEx(BmpWin_GetBitmap(wk->sysMsgWin), 112, (top + i * 2) * 8,
                                       wk->regulationPrintMsg->value[i], wk->font, color);
    }
    BmpWin_FlushChar(wk->sysMsgWin);
}

static void WifiP2PMatch_FreeRulesWindow(WIFIP2PMATCH_WORK *wk) {
    PokeRegulation_FreePrintMsg(wk->regulationPrintMsg);
    wk->regulationPrintMsg = NULL;
    wk->sysMsgWin = WifiP2PMatch_FreeWindow(wk->sysMsgWin);
}

// Shows the wait icon in the message window
static void WifiP2PMatch_StartWaitIcon(WIFIP2PMATCH_WORK *wk) {
    if (wk->waitIcon != NULL) {
        WaitIcon_Free(wk->waitIcon);
        wk->waitIcon = NULL;
    }
    wk->waitIcon = WaitIcon_CreateTCBEx(wk->tcbManager, wk->msgWin, 15, 16, WIFIP2PMATCH_HEAP);
}

// Closes the party menu: the icons, the party window and the list
static void WifiP2PMatch_ClosePartyMenu(WIFIP2PMATCH_WORK *wk) {
    if (wk->listWin != NULL) {
        WifiP2PMatch_FreePokeIcons(wk);
        WifiP2PMatch_FreePartyWindow(wk);
        wk->listWin = WifiP2PMatch_FreeWindow(wk->listWin);
        BmpMenuList_Free(wk->list, NULL, NULL);
        ListMenuCore_FreeOptionList(wk->menuOptions);
        wk->menuOptions = NULL;
    }
}

// Starts the voice chat if both sides have it on, and tells the friends whether the player is using it
static void WifiP2PMatch_StartVChat(WIFIP2PMATCH_WORK *wk) {
    if (wk->parent->vchat[0] && wk->parent->vchat[1]) {
        DWCRap_SetVoiceChatEnabled(1);
        DWCRap_SetVoiceChatReady();
        DWCRap_StartVoiceChat();
        WifiP2PMatch_BgmVolumeFadeStart(wk, 42);
        WIFI_STATUS_SetUseVChat(wk->myStatus, TRUE);
    } else {
        WIFI_STATUS_SetUseVChat(wk->myStatus, FALSE);
    }
}

// State 0x41: connected. The side that called asks what to do; the other waits
static int WifiP2PMatch_StateConnected(WIFIP2PMATCH_WORK *wk, int seq) {
    GFL_NetSetAcceptConnect(NetHandle_GetOwn(), 0);
    if (!GFL_NetIsParent()) {
        wk->result = 3;
        WifiP2PMatch_SetFriendNameWord(wk, wk->friendNo - 1);
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_WantPlay, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_PLAY_YES_NO_START;
    } else {
        wk->result = 4;
        WifiP2PMatch_SetFriendNameWord(wk, wk->friendNo - 1);
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_AwaitingSResponse, FALSE);
        WifiP2PMatch_StartWaitIcon(wk);
        NetHandle_SendTimingPair(NetHandle_GetOwn(), WIFIP2PMATCH_TIMING_SEQ, 10);
        wk->seq = WIFIP2PMATCH_SEQ_SYNC_SEQ_TIMING_WAIT;
    }
    return seq;
}

// State 0x42
static int WifiP2PMatch_StatePlayYesNoStart(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    WifiP2PMatch_YesNoStart(wk);
    wk->seq = WIFIP2PMATCH_SEQ_PLAY_YES_NO;
    return seq;
}

// State 0x43: play with the friend, or leave them
static int WifiP2PMatch_StatePlayYesNo(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 ret;

    GFL_TextRndResetColorIndexLUT();
    ret = WifiP2PMatch_YesNoMain(wk);
    if (ret == BMPMENU_NULL) {
        return seq;
    }
    if (ret == 0) {
        wk->sendSeq = WIFIP2PMATCH_SEQ_GAME_MODE_MESSAGE;
    } else {
        wk->sendSeq = WIFIP2PMATCH_SEQ_CANCEL_RESET;
    }
    WifiP2PMatch_EndMessage(wk);
    wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
    return seq;
}

// State 0x44: the parent asks what to play; the child waits
static int WifiP2PMatch_StateGameModeMessage(WIFIP2PMATCH_WORK *wk, int seq) {
    GFL_NetSetAcceptConnect(NetHandle_GetOwn(), 0);
    if (GFL_NetIsParent()) {
        wk->result = 3;
        WifiP2PMatch_SetFriendNameWord(wk, wk->friendNo - 1);
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_WhatWouldLike, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_GAME_MODE_MENU_OPEN;
    } else {
        wk->result = 4;
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_AwaitingFriendsDecisionPlease, FALSE);
        WifiP2PMatch_StartWaitIcon(wk);
        NetHandle_SendTimingPair(NetHandle_GetOwn(), WIFIP2PMATCH_TIMING_SEQ, 10);
        wk->seq = WIFIP2PMATCH_SEQ_SYNC_SEQ_TIMING_WAIT;
    }
    return seq;
}

// State 0x45
static int WifiP2PMatch_StateGameModeMenuOpen(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    WifiP2PMatch_OpenGameModeMenu(wk);
    wk->seq = WIFIP2PMATCH_SEQ_GAME_MODE_MENU;
    return seq;
}

// State 0x46: the menu of what to play. Voice chat and the Xtransceiver can't be chosen while the player's voice
// chat is off, and need the friend's to be on
static int WifiP2PMatch_StateGameModeMenu(WIFIP2PMATCH_WORK *wk, int seq) {
    s32 ret;

    PrintWindow_Flush(&wk->listPrintWindow, wk->printQueue);
    ret = BmpMenuList_Update(wk->list);
    switch (ret) {
    case WIFIP2PMATCH_GAMEMODE_VCHAT:
    case WIFIP2PMATCH_GAMEMODE_TVT:
        if (!wk->parent->unk46) {
            return seq;
        }
        break;
    }
    if (ret != BMPMENULIST_NULL) {
        WifiP2PMatch_EndMessage(wk);
    }
    switch (ret) {
    case BMPMENULIST_NULL:
        return seq;
    case BMPMENULIST_CANCEL:
        wk->sendSeq = WIFIP2PMATCH_SEQ_TALK_END_TIMING;
        wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
        break;
    case WIFIP2PMATCH_GAMEMODE_VCHAT:
        if (!wk->parent->vchat[1]) {
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_LooksLikeCantTalk, FALSE);
            wk->seq = WIFIP2PMATCH_SEQ_GAME_MODE_KEY_WAIT;
        } else {
            wk->sendSeq = WIFIP2PMATCH_SEQ_ASK_VCHAT;
            wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
        }
        break;
    case WIFIP2PMATCH_GAMEMODE_TVT:
        if (!wk->parent->vchat[1]) {
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_LooksLikeCantTalk, FALSE);
            wk->seq = WIFIP2PMATCH_SEQ_GAME_MODE_KEY_WAIT;
        } else if (!canPlayerExchangePhotos() && isRunningOnDSi()) {
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_UsesCameraFunctionSend, FALSE);
            wk->seq = WIFIP2PMATCH_SEQ_NO_CAMERA_YES_NO_START;
        } else {
            wk->sendSeq = WIFIP2PMATCH_SEQ_ASK_TVT;
            wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
        }
        break;
    case WIFIP2PMATCH_GAMEMODE_TRADE:
        if (!WifiP2PMatch_HasTwoPokemon(wk)) {
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_NeedLeastTwoPokemon, FALSE);
            wk->seq = WIFIP2PMATCH_SEQ_GAME_MODE_KEY_WAIT;
        } else if (wk->parent->vchat[0] != wk->parent->vchat[1]) {
            wk->unk1352 = 6;
            wk->seq = WIFIP2PMATCH_SEQ_VCHAT_DIFF_MSG;
        } else {
            wk->sendSeq = WIFIP2PMATCH_SEQ_ASK_TRADE;
            wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
        }
        break;
    case WIFIP2PMATCH_GAMEMODE_BATTLE:
        wk->seq = WIFIP2PMATCH_SEQ_BATTLE_SETUP_MENU_START;
        break;
    }
    wk->listWin = WifiP2PMatch_FreeWindow(wk->listWin);
    BmpMenuList_Free(wk->list, NULL, &wk->menuCursor[WIFIP2PMATCH_MENU_STATUS]);
    ListMenuCore_FreeOptionList(wk->menuOptions);
    return seq;
}

// State 0xa4
static int WifiP2PMatch_StateNoCameraYesNoStart(WIFIP2PMATCH_WORK *wk, int seq) {
    WIFI_MCR_PCAnmMain(&wk->room);
    if (WifiP2PMatch_IsMessageEnd(wk)) {
        WifiP2PMatch_YesNoStart(wk);
        wk->seq = WIFIP2PMATCH_SEQ_NO_CAMERA_YES_NO;
    }
    return seq;
}

// State 0xa5: use the Xtransceiver without the camera?
static int WifiP2PMatch_StateNoCameraYesNo(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 ret = WifiP2PMatch_YesNoMain(wk);

    WIFI_MCR_PCAnmMain(&wk->room);
    if (ret == BMPMENU_NULL) {
        return seq;
    }
    if (ret == 0) {
        wk->sendSeq = WIFIP2PMATCH_SEQ_ASK_TVT;
        wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
    } else {
        wk->sendSeq = WIFIP2PMATCH_SEQ_TALK_END_TIMING;
        wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
    }
    return seq;
}

// State 0xa6
static int WifiP2PMatch_StateAnswerNoCameraYesNoStart(WIFIP2PMATCH_WORK *wk, int seq) {
    WIFI_MCR_PCAnmMain(&wk->room);
    if (WifiP2PMatch_IsMessageEnd(wk)) {
        WifiP2PMatch_YesNoStart(wk);
        wk->seq = WIFIP2PMATCH_SEQ_ANSWER_NO_CAMERA_YES_NO;
    }
    return seq;
}

// State 0xa7: the same question for the side that was asked
static int WifiP2PMatch_StateAnswerNoCameraYesNo(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 ret = WifiP2PMatch_YesNoMain(wk);

    WIFI_MCR_PCAnmMain(&wk->room);
    if (ret == BMPMENU_NULL) {
        return seq;
    }
    if (ret == 0) {
        wk->seq = WIFIP2PMATCH_SEQ_SEND_START_GAME;
        WifiP2PMatch_EndMessage(wk);
    } else {
        wk->sendSeq = WIFIP2PMATCH_SEQ_DECLINED;
        wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
    }
    return seq;
}

// State 0x47: a key after a message, then the question of what to play again
static int WifiP2PMatch_StateGameModeKeyWait(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    if (GCTX_HIDGetPressedKeys()) {
        wk->seq = WIFIP2PMATCH_SEQ_GAME_MODE_MESSAGE;
    }
    return seq;
}

// States 0x4b to 0x4d: the child is asked whether to play what the parent chose; the parent waits
static int WifiP2PMatch_AskGameMode(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 msgs[] = { WifiP2pmatch_Text_WouldLikeTalkUsing, WifiP2pmatch_Text_WouldLikeTalkUsing_2,
                   WifiP2pmatch_Text_Trade_2 };

    if (!GFL_NetIsParent()) {
        WifiP2PMatch_SetFriendNameWord(wk, wk->friendNo - 1);
        WifiP2PMatch_PrintMessage(wk, msgs[wk->modeSeq - WIFIP2PMATCH_SEQ_ASK_VCHAT], FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_ANSWER_YES_NO_START;
    } else {
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_AwaitingFriendsDecisionPlease, FALSE);
        WifiP2PMatch_StartWaitIcon(wk);
        NetHandle_SendTimingPair(NetHandle_GetOwn(), WIFIP2PMATCH_TIMING_SEQ, 10);
        wk->seq = WIFIP2PMATCH_SEQ_SYNC_SEQ_TIMING_WAIT;
    }
    return seq;
}

// State 0x4b: voice chat
static int WifiP2PMatch_StateAskVChat(WIFIP2PMATCH_WORK *wk, int seq) {
    wk->modeSeq = WIFIP2PMATCH_SEQ_ASK_VCHAT;
    return WifiP2PMatch_AskGameMode(wk, seq);
}

// State 0x4c: the Xtransceiver
static int WifiP2PMatch_StateAskTVT(WIFIP2PMATCH_WORK *wk, int seq) {
    wk->modeSeq = WIFIP2PMATCH_SEQ_ASK_TVT;
    return WifiP2PMatch_AskGameMode(wk, seq);
}

// State 0x4d: a trade
static int WifiP2PMatch_StateAskTrade(WIFIP2PMATCH_WORK *wk, int seq) {
    wk->modeSeq = WIFIP2PMATCH_SEQ_ASK_TRADE;
    return WifiP2PMatch_AskGameMode(wk, seq);
}

// State 0x4f
static int WifiP2PMatch_StateAnswerYesNoStart(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    WifiP2PMatch_YesNoStart(wk);
    wk->seq = WIFIP2PMATCH_SEQ_ANSWER_YES_NO;
    return seq;
}

// State 0x50: the child's answer. A trade needs two Pokémon, and the Xtransceiver asks first on a DSi that can't
// send photos
static int WifiP2PMatch_StateAnswerYesNo(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 ret;

    GFL_TextRndResetColorIndexLUT();
    ret = WifiP2PMatch_YesNoMain(wk);
    if (ret == BMPMENU_NULL) {
        return seq;
    }
    if (ret == 0) {
        if (!WifiP2PMatch_HasTwoPokemon(wk) && wk->modeSeq == WIFIP2PMATCH_SEQ_ASK_TRADE) {
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_NeedLeastTwoPokemon, FALSE);
            wk->seq = WIFIP2PMATCH_SEQ_TRADE_REFUSED_KEY_WAIT;
        } else if (!canPlayerExchangePhotos() && isRunningOnDSi() && wk->modeSeq == WIFIP2PMATCH_SEQ_ASK_TVT) {
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_UsesCameraFunctionSend, FALSE);
            wk->seq = WIFIP2PMATCH_SEQ_ANSWER_NO_CAMERA_YES_NO_START;
        } else {
            wk->seq = WIFIP2PMATCH_SEQ_SEND_START_GAME;
            WifiP2PMatch_EndMessage(wk);
        }
    } else {
        wk->sendSeq = WIFIP2PMATCH_SEQ_DECLINED;
        wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
    }
    return seq;
}

// State 0x96: a key after the message that a trade needs two Pokémon
static int WifiP2PMatch_StateTradeRefusedKeyWait(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    if (!GCTX_HIDGetPressedKeys()) {
        return seq;
    }
    if (wk->result == 3 || wk->result == 4) {
        wk->sendSeq = WIFIP2PMATCH_SEQ_DECLINED;
    } else {
        wk->sendSeq = WIFIP2PMATCH_SEQ_UNABLE_MSG;
    }
    wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
    return seq;
}

// State 0x90: the offer was declined
static int WifiP2PMatch_StateDeclined(WIFIP2PMATCH_WORK *wk, int seq) {
    if (GFL_NetIsParent()) {
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_FriendApparentlyUnableParticipate, FALSE);
        NetHandle_SendTimingPair(NetHandle_GetOwn(), WIFIP2PMATCH_TIMING_SEQ, 10);
        wk->seq = WIFIP2PMATCH_SEQ_SYNC_SEQ_TIMING_WAIT;
    } else {
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_DeclinedOffer, FALSE);
        wk->timer = 120;
        wk->seq = WIFIP2PMATCH_SEQ_DECLINED_WAIT;
    }
    return seq;
}

// State 0x8f
static int WifiP2PMatch_StateDeclinedWait(WIFIP2PMATCH_WORK *wk, int seq) {
    wk->timer--;
    WifiP2PMatch_IsMessageEnd(wk);
    if (wk->timer < 0) {
        wk->sendSeq = WIFIP2PMATCH_SEQ_RETURN_FROM_GAME;
        wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
    }
    return seq;
}

// State 0x52
static int WifiP2PMatch_StateSendStartGame(WIFIP2PMATCH_WORK *wk, int seq) {
    wk->sendSeq = WIFIP2PMATCH_SEQ_START_GAME;
    wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
    return seq;
}

// State 0x53
static int WifiP2PMatch_StateSendCancelReset(WIFIP2PMATCH_WORK *wk, int seq) {
    wk->sendSeq = WIFIP2PMATCH_SEQ_CANCEL_RESET;
    wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
    return seq;
}

// State 0x85
static int WifiP2PMatch_StateSendRefusedMsg(WIFIP2PMATCH_WORK *wk, int seq) {
    wk->sendSeq = WIFIP2PMATCH_SEQ_REFUSED_MSG;
    wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
    return seq;
}

// State 0x51: both agreed. The voice chat goes on in the room; the Xtransceiver and a trade end the proc with their
// game mode as the Wi-Fi Club's next mode
static int WifiP2PMatch_StateStartGame(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 gameMode;

    WIFI_STATUS_ResetVChatMac(wk->myStatus);
    wk->unk1351 = 0;
    switch (wk->modeSeq) {
    case WIFIP2PMATCH_SEQ_ASK_VCHAT:
        gameMode = WIFIP2PMATCH_GAMEMODE_VCHAT;
        break;
    case WIFIP2PMATCH_SEQ_ASK_TVT:
        gameMode = WIFIP2PMATCH_GAMEMODE_TVT;
        break;
    case WIFIP2PMATCH_SEQ_ASK_TRADE:
        gameMode = WIFIP2PMATCH_GAMEMODE_TRADE;
        break;
    }
    switch (wk->modeSeq) {
    case WIFIP2PMATCH_SEQ_ASK_VCHAT:
        WifiP2PMatch_StartVChat(wk);
        WifiP2PMatch_PrintMessageNow(wk, WifiP2pmatch_Text_PressBButtonEnd, FALSE);
        WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_PLAYING, WIFIP2PMATCH_GAMEMODE_VCHAT);
        WifiP2PMatch_BgmVolumeFadeStart(wk, 0);
        wk->unk1355 = 0;
        wk->seq = WIFIP2PMATCH_SEQ_VCHAT_START;
        break;
    case WIFIP2PMATCH_SEQ_ASK_TVT:
        WifiP2PMatch_EndMessage(wk);
        wk->endSeq = gameMode;
        seq = 2;
        break;
    case WIFIP2PMATCH_SEQ_ASK_TRADE:
        WifiP2PMatch_StartVChat(wk);
        WifiP2PMatch_EndMessage(wk);
        wk->endSeq = gameMode;
        seq = 2;
        break;
    }
    WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_PLAYING, gameMode);
    return seq;
}

// State 0x4e: opens the battle rule menu
static int WifiP2PMatch_StateBattleSetupMenuStart(WIFIP2PMATCH_WORK *wk, int seq) {
    WifiP2PMatch_OpenBattleRulesMenu(wk, TRUE);
    wk->seq = WIFIP2PMATCH_SEQ_BATTLE_SETUP_MENU;
    return seq;
}

// State 0x54: the battle rule menu. Its options' values are the states to go to
static int WifiP2PMatch_StateBattleSetupMenu(WIFIP2PMATCH_WORK *wk, int seq) {
    s32 ret;

    PrintWindow_Flush(&wk->listPrintWindow, wk->printQueue);
    ret = BmpMenuList_Update(wk->list);
    if (ret == BMPMENULIST_NULL) {
        return seq;
    }
    WifiP2PMatch_EndMessage(wk);
    wk->listWin = WifiP2PMatch_FreeWindow(wk->listWin);
    BmpMenuList_Free(wk->list, NULL, &wk->menuCursor[0]);
    ListMenuCore_FreeOptionList(wk->menuOptions);
    if (ret == BMPMENULIST_CANCEL) {
        if (wk->result == 3 || wk->result == 4) {
            WifiP2PMatch_SetFriendNameWord(wk, wk->friendNo - 1);
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_WhatWouldLike, FALSE);
            wk->seq = WIFIP2PMATCH_SEQ_GAME_MODE_MENU_OPEN;
        } else {
            wk->seq = WIFIP2PMATCH_SEQ_RECRUIT_MENU_START;
        }
    } else {
        if (wk->result == 3 || wk->result == 4) {
            wk->seq = ret;
        } else if (ret == WIFIP2PMATCH_SEQ_BATTLE_SETUP_DECIDE) {
            wk->seq = WIFIP2PMATCH_SEQ_BATTLE_RECRUIT_START;
        } else {
            wk->seq = ret;
        }
    }
    return seq;
}

// State 0x55: opens the battle mode menu
static int WifiP2PMatch_StateBattleModeMenuStart(WIFIP2PMATCH_WORK *wk, int seq) {
    WifiP2PMatch_OpenBattleModeMenu(wk);
    wk->seq = WIFIP2PMATCH_SEQ_BATTLE_MODE_MENU;
    return seq;
}

// State 0x56: the battle mode menu
static int WifiP2PMatch_StateBattleModeMenu(WIFIP2PMATCH_WORK *wk, int seq) {
    s32 ret;

    PrintWindow_Flush(&wk->listPrintWindow, wk->printQueue);
    ret = BmpMenuList_Update(wk->list);
    switch (ret) {
    case BMPMENULIST_NULL:
        return seq;
    default:
        wk->battleMode = ret;
        wk->parent->battleMode = ret;
        break;
    case BMPMENULIST_CANCEL:
        break;
    }
    wk->seq = WIFIP2PMATCH_SEQ_BATTLE_SETUP_MENU_START;
    WifiP2PMatch_EndMessage(wk);
    wk->listWin = WifiP2PMatch_FreeWindow(wk->listWin);
    BmpMenuList_Free(wk->list, NULL, &wk->menuCursor[1]);
    ListMenuCore_FreeOptionList(wk->menuOptions);
    return seq;
}

// State 0x57: opens the battle rule (no restrictions or flat) menu
static int WifiP2PMatch_StateBattleRuleMenuStart(WIFIP2PMATCH_WORK *wk, int seq) {
    WifiP2PMatch_OpenBattleRuleMenu(wk);
    wk->seq = WIFIP2PMATCH_SEQ_BATTLE_RULE_MENU;
    return seq;
}

// State 0x58: the no restrictions or flat menu
static int WifiP2PMatch_StateBattleRuleMenu(WIFIP2PMATCH_WORK *wk, int seq) {
    s32 ret;

    PrintWindow_Flush(&wk->listPrintWindow, wk->printQueue);
    ret = BmpMenuList_Update(wk->list);
    switch (ret) {
    case BMPMENULIST_NULL:
        return seq;
    default:
        wk->battleRule = ret;
        wk->parent->battleRule = ret;
        break;
    case BMPMENULIST_CANCEL:
        break;
    }
    wk->seq = WIFIP2PMATCH_SEQ_BATTLE_SETUP_MENU_START;
    WifiP2PMatch_EndMessage(wk);
    wk->listWin = WifiP2PMatch_FreeWindow(wk->listWin);
    BmpMenuList_Free(wk->list, NULL, &wk->menuCursor[2]);
    ListMenuCore_FreeOptionList(wk->menuOptions);
    return seq;
}

// State 0x59: opens the Wonder Launcher menu
static int WifiP2PMatch_StateShooterMenuStart(WIFIP2PMATCH_WORK *wk, int seq) {
    WifiP2PMatch_OpenShooterMenu(wk);
    wk->seq = WIFIP2PMATCH_SEQ_SHOOTER_MENU;
    return seq;
}

// State 0x5a: the Wonder Launcher menu
static int WifiP2PMatch_StateShooterMenu(WIFIP2PMATCH_WORK *wk, int seq) {
    s32 ret;

    PrintWindow_Flush(&wk->listPrintWindow, wk->printQueue);
    ret = BmpMenuList_Update(wk->list);
    switch (ret) {
    case BMPMENULIST_NULL:
        return seq;
    default:
        wk->parent->unk20 = 1 - ret;
        break;
    case BMPMENULIST_CANCEL:
        break;
    }
    wk->seq = WIFIP2PMATCH_SEQ_BATTLE_SETUP_MENU_START;
    WifiP2PMatch_EndMessage(wk);
    wk->listWin = WifiP2PMatch_FreeWindow(wk->listWin);
    BmpMenuList_Free(wk->list, NULL, &wk->menuCursor[3]);
    ListMenuCore_FreeOptionList(wk->menuOptions);
    return seq;
}

// Checks the player's party against a regulation
static u32 WifiP2PMatch_CheckParty(Regulation *regulation, GameData *gameData, u32 *flags) {
    *flags = 0;
    return PokeRegulation_CheckParty(regulation, GameData_GetParty(gameData), flags);
}

// The Battle Box as a new party, or NULL when it is empty
static PokeParty *WifiP2PMatch_CreateBattleBoxParty(GameData *gameData) {
    BattleBoxSave *battleBox = getBattleBox(GameData_GetSaveControl(gameData));

    if (BattleBoxSave_HasPkm(battleBox) == FALSE) {
        return NULL;
    }
    return convertBoxedPokeSetToParty(battleBox, WIFIP2PMATCH_HEAP);
}

static void WifiP2PMatch_FreeBattleBoxParty(PokeParty *party) {
    if (party != NULL) {
        GFL_HeapFree(party);
    }
}

// Checks the Battle Box's party against a regulation; every flag is set when there is none
static u32 WifiP2PMatch_CheckBattleBoxParty(Regulation *regulation, PokeParty *party, u32 *flags) {
    u32 ret = 4;

    *flags = 0;
    if (party != NULL) {
        ret = PokeRegulation_CheckParty(regulation, party, flags);
    } else {
        *flags = 0xffffffff;
    }
    return ret;
}

// Loads the regulation of the battle chosen here (0) or of the one the partner chose (1), and returns its code: the
// file, with the top bit for the Wonder Launcher
static u32 WifiP2PMatch_LoadRegulation(WIFIP2PMATCH_WORK *wk, int which) {
    u32 battleMode;
    u32 battleRule;
    u32 shooter;
    u32 code;

    switch (which) {
    case 0:
    default:
        battleMode = wk->battleMode;
        battleRule = wk->battleRule;
        shooter = wk->parent->unk48;
        break;
    case 1:
        battleMode = wk->parent->battleMode;
        battleRule = wk->parent->battleRule;
        shooter = wk->parent->unk20;
        break;
    }
    code = sWifiP2PMatchRegulationFiles[battleMode + battleRule * 4];
    if (wk->regulation != NULL) {
        GFL_HeapFree(wk->regulation);
    }
    wk->regulation = PokeRegulation_LoadNew(code, WIFIP2PMATCH_HEAP);
    if (shooter) {
        code |= 0x80000000;
    }
    return code;
}

static u32 WifiP2PMatch_LoadMyRegulation(WIFIP2PMATCH_WORK *wk) {
    return WifiP2PMatch_LoadRegulation(wk, 0);
}

// Sets the battle mode, the rule and the Wonder Launcher flag from a regulation code
static void WifiP2PMatch_SetRegulationCode(WIFIP2PMATCH_WORK *wk, u32 regulationCode) {
    int i;

    wk->parent->unk48 = (regulationCode & 0x80000000) ? TRUE : FALSE;
    regulationCode &= 0xfffffff;
    for (i = 0; i < NELEMS(sWifiP2PMatchRegulationFiles); i++) {
        if (regulationCode == sWifiP2PMatchRegulationFiles[i]) {
            wk->battleMode = i % 4;
            wk->battleRule = i / 4;
        }
    }
}

// State 0x5b: the rules are decided. Loads the regulation and checks the parties against it
static int WifiP2PMatch_StateBattleSetupDecide(WIFIP2PMATCH_WORK *wk, int seq) {
    if (wk->result == 3 || wk->result == 1) {
        WifiP2PMatch_LoadRegulation(wk, 1);
    } else {
        WifiP2PMatch_LoadRegulation(wk, 0);
    }
    if (!WifiP2PMatch_PassesRegulation(wk)) {
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_NotMeetConditionsCannot, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_REGULATION_FAIL_PARTY;
        return seq;
    }
    wk->unk1352 = 7;
    if (wk->parent->vchat[0] != wk->parent->vchat[1]) {
        wk->seq = WIFIP2PMATCH_SEQ_VCHAT_DIFF_MSG;
    } else {
        wk->seq = WIFIP2PMATCH_SEQ_REGULATION_SEND;
    }
    WifiP2PMatch_SetFriendNameWord(wk, wk->friendNo - 1);
    if (wk->result == 3 || wk->result == 4) {
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_Communicating, FALSE);
    } else {
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_AwaitingFriendsDecisionPlease, FALSE);
    }
    WifiP2PMatch_StartWaitIcon(wk);
    return seq;
}

// State 0x63: shows what of the party breaks the rules
static int WifiP2PMatch_StateRegulationFailParty(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 flags;

    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    if (GCTX_HIDGetPressedKeys()) {
        WifiP2PMatch_EndMessage(wk);
        WifiP2PMatch_CheckParty(wk->regulation, wk->gameData, &flags);
        if (wk->result == 3 || wk->result == 1) {
            WifiP2PMatch_DrawRulesWindow(wk, flags, wk->parent->unk20, 2);
        } else {
            WifiP2PMatch_DrawRulesWindow(wk, flags, wk->parent->unk48, 2);
        }
        wk->seq = WIFIP2PMATCH_SEQ_REGULATION_FAIL_BATTLE_BOX;
    }
    return seq;
}

// State 0x77: shows what of the Battle Box breaks the rules, if there is one
static int WifiP2PMatch_StateRegulationFailBattleBox(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 flags;

    if (GCTX_HIDGetPressedKeys()) {
        WifiP2PMatch_FreeRulesWindow(wk);
        if (wk->battleBoxParty != NULL) {
            WifiP2PMatch_CheckBattleBoxParty(wk->regulation, wk->battleBoxParty, &flags);
            if (wk->result == 3 || wk->result == 1) {
                WifiP2PMatch_DrawRulesWindow(wk, flags, wk->parent->unk20, 3);
            } else {
                WifiP2PMatch_DrawRulesWindow(wk, flags, wk->parent->unk48, 3);
            }
            wk->seq = WIFIP2PMATCH_SEQ_REGULATION_FAIL_END;
        } else {
            switch (wk->result) {
            case 3:
                wk->seq = WIFIP2PMATCH_SEQ_BATTLE_SETUP_MENU_START;
                break;
            case 2:
                wk->sendSeq = WIFIP2PMATCH_SEQ_UNABLE_MSG;
                wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
                break;
            case 4:
                wk->sendSeq = WIFIP2PMATCH_SEQ_DECLINED;
                wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
                break;
            default:
                wk->seq = WIFIP2PMATCH_SEQ_BACK_TO_ROOM;
                break;
            }
        }
    }
    return seq;
}

// State 0x78: closes the rules window and goes back by who asked
static int WifiP2PMatch_StateRegulationFailEnd(WIFIP2PMATCH_WORK *wk, int seq) {
    if (GCTX_HIDGetPressedKeys()) {
        WifiP2PMatch_FreeRulesWindow(wk);
        WifiP2PMatch_EndMessage(wk);
        switch (wk->result) {
        case 3:
            wk->seq = WIFIP2PMATCH_SEQ_BATTLE_SETUP_MENU_START;
            break;
        case 2:
            wk->sendSeq = WIFIP2PMATCH_SEQ_UNABLE_MSG;
            wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
            break;
        case 4:
            wk->sendSeq = WIFIP2PMATCH_SEQ_DECLINED;
            wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
            break;
        default:
            wk->seq = WIFIP2PMATCH_SEQ_BACK_TO_ROOM;
            break;
        }
    }
    return seq;
}

// State 0x5c: shows the rules of the battle
static int WifiP2PMatch_StateRulesShow(WIFIP2PMATCH_WORK *wk, int seq) {
    WifiP2PMatch_LoadRegulation(wk, 1);
    WifiP2PMatch_DrawRulesWindow(wk, 0, wk->parent->unk20, 0);
    wk->seq = WIFIP2PMATCH_SEQ_RULES_SHOW_WAIT;
    return seq;
}

// State 0x5d
static int WifiP2PMatch_StateRulesShowWait(WIFIP2PMATCH_WORK *wk, int seq) {
    if (GCTX_HIDGetPressedKeys()) {
        WifiP2PMatch_FreeRulesWindow(wk);
        wk->seq = WIFIP2PMATCH_SEQ_BATTLE_SETUP_MENU_START;
    }
    return seq;
}

// State 0x5e: the child sees the battle the parent chose, the parent waits
static int WifiP2PMatch_StateBattleOfferShow(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!GFL_NetIsParent()) {
        WifiP2PMatch_LoadMyRegulation(wk);
        WifiP2PMatch_DrawRulesWindow(wk, 0, wk->parent->unk48, 1);
        WifiP2PMatch_SetFriendNameWord(wk, wk->friendNo - 1);
        WifiP2PMatch_PrintMessageCore(wk, WifiP2pmatch_Text_WillHaveSingleBattle + wk->battleMode + wk->battleRule * 4,
                                      FALSE, TRUE);
        wk->seq = WIFIP2PMATCH_SEQ_BATTLE_OFFER_MSG_WAIT;
    } else {
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_AwaitingFriendsDecisionPlease, FALSE);
        WifiP2PMatch_StartWaitIcon(wk);
        NetHandle_SendTimingPair(NetHandle_GetOwn(), 0x1b, 10);
        wk->seq = WIFIP2PMATCH_SEQ_SYNC_SEQ_TIMING_WAIT;
    }
    return seq;
}

// State 0x7a
static int WifiP2PMatch_StateBattleOfferMsgWait(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    wk->seq = WIFIP2PMATCH_SEQ_BATTLE_OFFER_KEY_WAIT;
    return seq;
}

// State 0x7b
static int WifiP2PMatch_StateBattleOfferKeyWait(WIFIP2PMATCH_WORK *wk, int seq) {
    if (GCTX_HIDGetPressedKeys()) {
        WifiP2PMatch_FreeRulesWindow(wk);
        wk->seq = WIFIP2PMATCH_SEQ_BATTLE_OFFER_YES_NO_GO;
    }
    return seq;
}

// State 0x7c
static int WifiP2PMatch_StateBattleOfferYesNoGo(WIFIP2PMATCH_WORK *wk, int seq) {
    wk->seq = WIFIP2PMATCH_SEQ_BATTLE_OFFER_YES_NO_START;
    return seq;
}

// State 0x5f
static int WifiP2PMatch_StateUnused5F(WIFIP2PMATCH_WORK *wk, int seq) {
    return seq;
}

// State 0x60
static int WifiP2PMatch_StateBattleOfferYesNoStart(WIFIP2PMATCH_WORK *wk, int seq) {
    WifiP2PMatch_YesNoStart(wk);
    wk->seq = WIFIP2PMATCH_SEQ_BATTLE_OFFER_YES_NO;
    return seq;
}

// State 0x61: whether the child accepts the battle
static int WifiP2PMatch_StateBattleOfferYesNo(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 ret;

    GFL_TextRndResetColorIndexLUT();
    ret = WifiP2PMatch_YesNoMain(wk);
    if (ret == BMPMENU_NULL) {
        return seq;
    }
    if (ret == 0) {
        wk->seq = WIFIP2PMATCH_SEQ_BATTLE_OFFER_CHECK_PARTY;
        WifiP2PMatch_EndMessage(wk);
    } else {
        if (wk->result == 1 || wk->result == 2) {
            wk->sendSeq = WIFIP2PMATCH_SEQ_CANCEL_RESET;
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_RequestHasBeenSuspended, FALSE);
        } else {
            wk->sendSeq = WIFIP2PMATCH_SEQ_DECLINED;
        }
        wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
    }
    return seq;
}

// State 0x8c
static int WifiP2PMatch_StatePlayAgainKeyWait(WIFIP2PMATCH_WORK *wk, int seq) {
    if (GCTX_HIDGetPressedKeys()) {
        wk->sendSeq = WIFIP2PMATCH_SEQ_PLAY_AGAIN_ASK;
        wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
    }
    return seq;
}

// State 0x62: the child's parties against the rules
static int WifiP2PMatch_StateBattleOfferCheckParty(WIFIP2PMATCH_WORK *wk, int seq) {
    WifiP2PMatch_LoadMyRegulation(wk);
    if (!WifiP2PMatch_PassesRegulation(wk)) {
        switch (wk->result) {
        case 3:
            wk->seq = WIFIP2PMATCH_SEQ_REGULATION_FAIL_KEY_WAIT;
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_NotMeetConditionsCannot, FALSE);
            return seq;
        case 2:
        case 4:
            wk->seq = WIFIP2PMATCH_SEQ_REGULATION_FAIL_PARTY;
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_NotMeetConditionsCannot, FALSE);
            return seq;
        default:
            wk->seq = WIFIP2PMATCH_SEQ_TRADE_REFUSED_KEY_WAIT;
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_NotMeetConditionsCannot, FALSE);
            return seq;
        }
    }
    wk->sendSeq = WIFIP2PMATCH_SEQ_PARTY_SELECT_START;
    wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
    return seq;
}

// State 0x9d
static int WifiP2PMatch_StateRegulationFailKeyWait(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    if (!GCTX_HIDGetPressedKeys()) {
        return seq;
    }
    wk->sendSeq = WIFIP2PMATCH_SEQ_PLAY_AGAIN_ASK;
    wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
    return seq;
}

// State 0x64: the party is chosen at once when only it passes the rules and there is no Battle Box
static int WifiP2PMatch_StatePartySelectStart(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 flags;
    int id = 0;

    if (!GFL_NetIsParent()) {
        id = 1;
    }
    if (WifiP2PMatch_CheckBattleBoxParty(wk->regulation, wk->battleBoxParty, &flags) != 0 &&
        wk->battleBoxParty == NULL) {
        PokeParty_Copy(GameData_GetParty(wk->gameData), wk->parent->parties[id]);
        wk->seq = WIFIP2PMATCH_SEQ_PARTY_SEND;
    } else {
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_PleaseSelectWhetherUse, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_PARTY_MENU_START;
    }
    return seq;
}

// State 0x65: opens the party or Battle Box menu
static int WifiP2PMatch_StatePartyMenuStart(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    wk->unk1354 = 2;
    WifiP2PMatch_OpenPartyMenu(wk);
    WifiP2PMatch_CreatePokeIcons(wk, GameData_GetParty(wk->gameData), wk->battleBoxParty);
    wk->seq = WIFIP2PMATCH_SEQ_PARTY_MENU;
    return seq;
}

// State 0x66: the party or Battle Box menu
static int WifiP2PMatch_StatePartyMenu(WIFIP2PMATCH_WORK *wk, int seq) {
    s32 ret;
    u32 flags;
    u16 listTop;
    u16 cursorRow;
    int id = 0;

    PrintWindow_Flush(&wk->listPrintWindow, wk->printQueue);
    ret = BmpMenuList_Update(wk->list);
    BmpMenuList_GetPos(wk->list, &listTop, &cursorRow);
    if (cursorRow != wk->unk1354) {
        wk->unk1354 = cursorRow;
        if (cursorRow == 0) {
            WifiP2PMatch_DrawPartyWindow(wk, GameData_GetParty(wk->gameData));
            WifiP2PMatch_ShowPokeIcons(wk, TRUE);
        } else {
            WifiP2PMatch_DrawPartyWindow(wk, wk->battleBoxParty);
            WifiP2PMatch_ShowPokeIcons(wk, FALSE);
        }
    }
    if (!GFL_NetIsParent()) {
        id = 1;
    }
    switch (ret) {
    case BMPMENULIST_NULL:
        return seq;
    case BMPMENULIST_CANCEL:
        return seq;
    case 0:
        if (WifiP2PMatch_CheckParty(wk->regulation, wk->gameData, &flags) != 0) {
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_NotMeetConditionsCannot, FALSE);
            wk->seq = WIFIP2PMATCH_SEQ_PARTY_MENU_FAIL_PARTY;
            WifiP2PMatch_ClosePartyMenu(wk);
            return seq;
        }
        PokeParty_Copy(GameData_GetParty(wk->gameData), wk->parent->parties[id]);
        wk->seq = WIFIP2PMATCH_SEQ_PARTY_SEND;
        break;
    case 1:
        if (WifiP2PMatch_CheckBattleBoxParty(wk->regulation, wk->battleBoxParty, &flags) != 0) {
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_NotMeetConditionsCannot, FALSE);
            wk->seq = WIFIP2PMATCH_SEQ_PARTY_MENU_FAIL_BATTLE_BOX;
            WifiP2PMatch_ClosePartyMenu(wk);
            return seq;
        }
        PokeParty_Copy(wk->battleBoxParty, wk->parent->parties[id]);
        wk->seq = WIFIP2PMATCH_SEQ_PARTY_SEND;
        break;
    }
    WifiP2PMatch_ClosePartyMenu(wk);
    WifiP2PMatch_EndMessage(wk);
    return seq;
}

// State 0x67: sends the party
static int WifiP2PMatch_StatePartySend(WIFIP2PMATCH_WORK *wk, int seq) {
    int id = 0;

    if (!GFL_NetIsParent()) {
        id = 1;
    }
    if (GFL_NetSendCommandToMachine(NetHandle_GetOwn(), 0xff, 0xa07, PokeParty_GetSaveDataSize(),
                                    wk->parent->parties[id], 0, FALSE, TRUE)) {
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_AwaitingFriendsDecisionPlease, FALSE);
        WifiP2PMatch_StartWaitIcon(wk);
        wk->seq = WIFIP2PMATCH_SEQ_PARTY_SEND_TIMING;
    }
    return seq;
}

// State 0x98
static int WifiP2PMatch_StatePartySendTiming(WIFIP2PMATCH_WORK *wk, int seq) {
    if (WifiP2PMatch_IsMessageEnd(wk)) {
        NetHandle_SendTimingPair(NetHandle_GetOwn(), 0x11, 10);
        wk->seq = WIFIP2PMATCH_SEQ_PARTY_SEND_WAIT;
    }
    return seq;
}

// State 0x68: both parties are sent. Says how many Pokémon to choose
static int WifiP2PMatch_StatePartySendWait(WIFIP2PMATCH_WORK *wk, int seq) {
    if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 0x11, 10)) {
        u32 min = Regulation_GetParam(wk->regulation, REGULATION_PARAM_MIN_COUNT);
        u32 max = Regulation_GetParam(wk->regulation, REGULATION_PARAM_MAX_COUNT);

        u32 msgId;

        if (min == 3 && max == 3) {
            msgId = WifiP2pmatch_Text_PleaseSelectThreePokemon;
        } else if (min == 4 && max == 4) {
            msgId = WifiP2pmatch_Text_PleaseSelectFourPokemon;
        } else if (min == 6 && max == 6) {
            msgId = WifiP2pmatch_Text_PleaseSelectSixPokemon;
        } else {
            msgId = WifiP2pmatch_Text_PleaseChoosePokemonWill;
        }
        WifiP2PMatch_PrintMessage(wk, msgId, FALSE);
        WifiP2PMatch_StartWaitIcon(wk);
        wk->timer = 120;
        wk->seq = WIFIP2PMATCH_SEQ_BATTLE_START_WAIT;
    }
    return seq;
}

// State 0x69
static int WifiP2PMatch_StateBattleStartWait(WIFIP2PMATCH_WORK *wk, int seq) {
    if (WifiP2PMatch_IsMessageEnd(wk)) {
        if (wk->timer-- < 0) {
            wk->seq = WIFIP2PMATCH_SEQ_BATTLE_START;
        }
    }
    return seq;
}

// State 0x6a: leaves for the battle. Ends the proc
static int WifiP2PMatch_StateBattleStart(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 gameMode;

    WIFI_STATUS_ResetVChatMac(wk->myStatus);
    wk->unk1351 = 0;
    {
        u32 gameModes[8] = { 7, 8, 9, 10, 11, 12, 13, 14 };

        gameMode = gameModes[wk->battleRule + wk->battleMode * 2];
    }
    wk->endSeq = gameMode;
    WifiP2PMatch_StartVChat(wk);
    WifiP2PMatch_EndMessage(wk);
    WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_PLAYING, gameMode);
    return 2;
}

// State 0x6b: shows what of the party breaks the rules, then the menu again
static int WifiP2PMatch_StatePartyMenuFailParty(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 flags;

    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    if (GCTX_HIDGetPressedKeys()) {
        WifiP2PMatch_EndMessage(wk);
        WifiP2PMatch_CheckParty(wk->regulation, wk->gameData, &flags);
        WifiP2PMatch_DrawRulesWindow(wk, flags, wk->parent->unk48, 2);
        wk->seq = WIFIP2PMATCH_SEQ_PARTY_MENU_FAIL_END;
    }
    return seq;
}

// State 0x6c: the same for the Battle Box
static int WifiP2PMatch_StatePartyMenuFailBattleBox(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 flags;

    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    if (GCTX_HIDGetPressedKeys()) {
        WifiP2PMatch_EndMessage(wk);
        WifiP2PMatch_CheckBattleBoxParty(wk->regulation, wk->battleBoxParty, &flags);
        WifiP2PMatch_DrawRulesWindow(wk, flags, wk->parent->unk48, 3);
        wk->seq = WIFIP2PMATCH_SEQ_PARTY_MENU_FAIL_END;
    }
    return seq;
}

// State 0x6d
static int WifiP2PMatch_StatePartyMenuFailEnd(WIFIP2PMATCH_WORK *wk, int seq) {
    if (GCTX_HIDGetPressedKeys()) {
        WifiP2PMatch_FreeRulesWindow(wk);
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_PleaseSelectWhetherUse, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_PARTY_MENU_START;
    }
    return seq;
}

// State 0x6e: after a battle, the child is asked to play again and the parent waits
static int WifiP2PMatch_StatePlayAgainAsk(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    if (!GFL_NetIsParent()) {
        WifiP2PMatch_SetFriendNameWord(wk, wk->friendNo - 1);
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_WouldLikePlayAgain, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_PLAY_AGAIN_YES_NO_START;
    } else {
        NetHandle_SendTimingPair(NetHandle_GetOwn(), 0x1b, 10);
        wk->seq = WIFIP2PMATCH_SEQ_SYNC_SEQ_TIMING_WAIT;
    }
    return seq;
}

// State 0x6f
static int WifiP2PMatch_StatePlayAgainYesNoStart(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    WifiP2PMatch_YesNoStart(wk);
    wk->seq = WIFIP2PMATCH_SEQ_PLAY_AGAIN_YES_NO;
    return seq;
}

// State 0x70: whether to play again
static int WifiP2PMatch_StatePlayAgainYesNo(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 ret;

    GFL_TextRndResetColorIndexLUT();
    ret = WifiP2PMatch_YesNoMain(wk);
    if (ret == BMPMENU_NULL) {
        return seq;
    }
    if (ret == 0) {
        wk->sendSeq = WIFIP2PMATCH_SEQ_GAME_MODE_MESSAGE;
    } else {
        wk->sendSeq = WIFIP2PMATCH_SEQ_CANCEL_RESET;
    }
    WifiP2PMatch_EndMessage(wk);
    wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
    return seq;
}

// State 0x48: both sides go to the state that was sent
static int WifiP2PMatch_StateSyncSeqWait(WIFIP2PMATCH_WORK *wk, int seq) {
    WifiP2PMatch_IsMessageEnd(wk);
    if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 0x1a, 10)) {
        if (wk->recvSeq != 0) {
            wk->seq = wk->recvSeq;
        } else {
            wk->seq = WIFIP2PMATCH_SEQ_CONNECT_ERROR;
        }
        wk->recvSeq = 0;
    }
    return seq;
}

// State 0x49
static int WifiP2PMatch_StateSyncSeqTimingWait(WIFIP2PMATCH_WORK *wk, int seq) {
    WifiP2PMatch_IsMessageEnd(wk);
    if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 0x1b, 10)) {
        wk->seq = WIFIP2PMATCH_SEQ_SYNC_SEQ_WAIT;
    }
    return seq;
}

// State 0x9b
static int WifiP2PMatch_StateTalkEndTiming(WIFIP2PMATCH_WORK *wk, int seq) {
    WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
    NetHandle_SendTimingPair(NetHandle_GetOwn(), 0x19, 10);
    wk->result = 0;
    wk->seq = WIFIP2PMATCH_SEQ_TALK_END_TIMING_WAIT;
    return seq;
}

// State 0x9c
static int WifiP2PMatch_StateTalkEndTimingWait(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 0x19, 10)) {
        return seq;
    }
    func_02042e94(0);
    GFL_NetSetDisconnectError(FALSE);
    if (GFL_NetIsParent()) {
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_EndedConversation, FALSE);
    } else {
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_FriendBusyEndingCommunication, FALSE);
    }
    wk->seq = WIFIP2PMATCH_SEQ_DISCONNECT_START;
    return seq;
}

// State 0xa8
static int WifiP2PMatch_StateDeclineReset(WIFIP2PMATCH_WORK *wk, int seq) {
    WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
    func_02042e94(0);
    GFL_NetSetDisconnectError(FALSE);
    wk->seq = WIFIP2PMATCH_SEQ_DECLINE_MSG;
    return seq;
}

// State 0xa9
static int WifiP2PMatch_StateDeclineMsg(WIFIP2PMATCH_WORK *wk, int seq) {
    if (wk->result == 2 || wk->result == 3) {
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_FriendBusyEndingCommunication, FALSE);
    } else {
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_EndedConversation, FALSE);
    }
    wk->result = 0;
    wk->seq = WIFIP2PMATCH_SEQ_DISCONNECT_START;
    return seq;
}

// State 0x4a
static int WifiP2PMatch_StateCancelReset(WIFIP2PMATCH_WORK *wk, int seq) {
    WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
    func_02042e94(0);
    GFL_NetSetDisconnectError(FALSE);
    wk->seq = WIFIP2PMATCH_SEQ_CANCEL_MSG;
    return seq;
}

// State 0x99
static int WifiP2PMatch_StateCancelMsg(WIFIP2PMATCH_WORK *wk, int seq) {
    if (wk->result == 2 || wk->result == 3) {
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_EndedConversation, FALSE);
    } else {
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_FriendBusyEndingCommunication, FALSE);
    }
    wk->result = 0;
    wk->seq = WIFIP2PMATCH_SEQ_DISCONNECT_START;
    return seq;
}

// State 0x8d
static int WifiP2PMatch_StateGoDisconnect(WIFIP2PMATCH_WORK *wk, int seq) {
    wk->seq = WIFIP2PMATCH_SEQ_CONNECT_ERROR;
    return seq;
}

// State 0x84
static int WifiP2PMatch_StateRefusedMsg(WIFIP2PMATCH_WORK *wk, int seq) {
    WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
    if (GFL_NetIsParent()) {
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_FriendApparentlyUnableParticipate, FALSE);
    } else {
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_DeclinedOffer, FALSE);
    }
    wk->seq = WIFIP2PMATCH_SEQ_DISCONNECT_START;
    return seq;
}

// State 0x8e
static int WifiP2PMatch_StateDisconnectStart(WIFIP2PMATCH_WORK *wk, int seq) {
    func_02042e94(0);
    GFL_NetSetDisconnectError(FALSE);
    wk->timer = 0;
    wk->seq = WIFIP2PMATCH_SEQ_CUT_CONNECTION;
    return seq;
}

// State 0x7d: the two sides' voice chat settings differ
static int WifiP2PMatch_StateVChatDiffMsg(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 msgId = WifiP2pmatch_Text_VoiceChatWillTurned;

    if (wk->parent->vchat[1] == 0) {
        msgId = WifiP2pmatch_Text_VoiceChatWillTurned_2;
    }
    WifiP2PMatch_PrintMessage(wk, msgId, FALSE);
    wk->seq = WIFIP2PMATCH_SEQ_VCHAT_DIFF_YES_NO_START;
    return seq;
}

// State 0x7e
static int WifiP2PMatch_StateVChatDiffYesNoStart(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    WifiP2PMatch_YesNoStart(wk);
    wk->seq = WIFIP2PMATCH_SEQ_VCHAT_DIFF_YES_NO;
    return seq;
}

// State 0x7f
static int WifiP2PMatch_StateVChatDiffYesNo(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 ret;

    GFL_TextRndResetColorIndexLUT();
    ret = WifiP2PMatch_YesNoMain(wk);
    if (ret == BMPMENU_NULL) {
        return seq;
    }
    if (ret == 0) {
        wk->seq = WIFIP2PMATCH_SEQ_VCHAT_SEND;
    } else {
        wk->seq = WIFIP2PMATCH_SEQ_VCHAT_DIFF_NO;
    }
    WifiP2PMatch_EndMessage(wk);
    return seq;
}

// State 0x86
static int WifiP2PMatch_StateVChatDiffNo(WIFIP2PMATCH_WORK *wk, int seq) {
    wk->sendSeq = WIFIP2PMATCH_SEQ_CANCEL_RESET;
    wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
    return seq;
}

// State 0x80: sends the game mode and the changed voice chat setting
static int WifiP2PMatch_StateVChatSend(WIFIP2PMATCH_WORK *wk, int seq) {
    u16 data[2];

    data[0] = WifiP2PMatch_GetGameMode(wk, wk->myStatus);
    data[1] = 1 - wk->parent->unk46;
    if (GFL_NetSendCommand(NetHandle_GetOwn(), 0xa03, sizeof(data), data)) {
        wk->seq = WIFIP2PMATCH_SEQ_REGULATION_SEND;
        WIFI_STATUS_SetUseVChat(wk->myStatus, data[1]);
    }
    return seq;
}

// State 0x81: sends the regulation code
static int WifiP2PMatch_StateRegulationSend(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 code;

    if (wk->unk1352 == 6) {
        wk->sendSeq = WIFIP2PMATCH_SEQ_ASK_TRADE;
        wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
    } else {
        if (wk->result == 3 || wk->result == 1) {
            code = WifiP2PMatch_LoadRegulation(wk, 1);
        } else {
            code = WifiP2PMatch_LoadRegulation(wk, 0);
        }
        if (GFL_NetSendCommand(NetHandle_GetOwn(), 0xa06, sizeof(code), &code)) {
            wk->seq = WIFIP2PMATCH_SEQ_REGULATION_SEND_END;
        }
    }
    return seq;
}

// State 0x82
static int WifiP2PMatch_StateRegulationSendEnd(WIFIP2PMATCH_WORK *wk, int seq) {
    wk->sendSeq = WIFIP2PMATCH_SEQ_BATTLE_OFFER_SHOW;
    wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
    return seq;
}

// State 0x8a: the state in sendSeq is sent to the partner, and both sides go to it
static int WifiP2PMatch_StateSendSeqTiming(WIFIP2PMATCH_WORK *wk, int seq) {
    NetHandle_SendTimingPair(NetHandle_GetOwn(), 0x1b, 10);
    wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ;
    return seq;
}

// State 0x8b
static int WifiP2PMatch_StateSendSeq(WIFIP2PMATCH_WORK *wk, int seq) {
    if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 0x1b, 10)) {
        if (GFL_NetSendCommand(NetHandle_GetOwn(), 0xa05, sizeof(wk->sendSeq), &wk->sendSeq)) {
            wk->seq = WIFIP2PMATCH_SEQ_SYNC_SEQ_WAIT;
        }
    }
    return seq;
}

// State 0x97
static int WifiP2PMatch_StateUnableMsg(WIFIP2PMATCH_WORK *wk, int seq) {
    WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
    wk->result = 0;
    func_02042e94(0);
    GFL_NetSetDisconnectError(FALSE);
    if (GFL_NetIsParent()) {
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_FriendApparentlyUnableParticipate, FALSE);
    } else {
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_EndedConversation, FALSE);
    }
    wk->seq = WIFIP2PMATCH_SEQ_DISCONNECT_START;
    return seq;
}

// State 0x79: back from a game. The child is asked to play again and the parent waits
static int WifiP2PMatch_StateReturnFromGame(WIFIP2PMATCH_WORK *wk, int seq) {
    GFL_NetSetAcceptConnect(NetHandle_GetOwn(), 0);
    if (!GFL_NetIsParent()) {
        WifiP2PMatch_SetFriendNameWord(wk, wk->friendNo - 1);
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_WouldLikePlayAgain, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_RETURN_YES_NO_START;
    } else {
        WifiP2PMatch_SetFriendNameWord(wk, wk->friendNo - 1);
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_AwaitingFriendsDecisionPlease, FALSE);
        WifiP2PMatch_StartWaitIcon(wk);
        NetHandle_SendTimingPair(NetHandle_GetOwn(), 0x1b, 10);
        wk->seq = WIFIP2PMATCH_SEQ_SYNC_SEQ_TIMING_WAIT;
    }
    return seq;
}

// State 0xaa
static int WifiP2PMatch_StateReturnYesNoStart(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    WifiP2PMatch_YesNoStart(wk);
    wk->seq = WIFIP2PMATCH_SEQ_RETURN_YES_NO;
    return seq;
}

// State 0xab
static int WifiP2PMatch_StateReturnYesNo(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 ret;

    GFL_TextRndResetColorIndexLUT();
    ret = WifiP2PMatch_YesNoMain(wk);
    if (ret == BMPMENU_NULL) {
        return seq;
    }
    if (ret == 0) {
        wk->sendSeq = WIFIP2PMATCH_SEQ_GAME_MODE_MESSAGE;
    } else {
        wk->sendSeq = WIFIP2PMATCH_SEQ_DECLINE_RESET;
    }
    WifiP2PMatch_EndMessage(wk);
    wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
    return seq;
}

// State 0xac
static int WifiP2PMatch_StateReturnTimingWait(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 0x18, 10)) {
        return seq;
    }
    wk->seq = WIFIP2PMATCH_SEQ_RETURN_FROM_GAME;
    return seq;
}

// State 0x73: both sides are ready. Starts what was chosen: the voice chat here, the trade and the Xtransceiver by
// ending the proc, a battle by the rules
static int WifiP2PMatch_StateGameStart(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 flags;
    u16 gameMode = WifiP2PMatch_GetGameMode(wk, wk->myStatus);

    switch (gameMode) {
    case WIFIP2PMATCH_GAMEMODE_VCHAT:
        GFL_NetGetInitData();
        WifiP2PMatch_StartVChat(wk);
        WifiP2PMatch_BgmVolumeFadeStart(wk, 0);
        WifiP2PMatch_PrintMessageNow(wk, WifiP2pmatch_Text_PressBButtonEnd, FALSE);
        wk->unk1355 = 0;
        wk->seq = WIFIP2PMATCH_SEQ_VCHAT_START;
        return seq;
    case WIFIP2PMATCH_GAMEMODE_TVT:
        wk->endSeq = gameMode;
        return 2;
    case WIFIP2PMATCH_GAMEMODE_TRADE:
        WifiP2PMatch_StartVChat(wk);
        wk->endSeq = gameMode;
        return 2;
    default:
        GFL_NetSetAcceptConnect(NetHandle_GetOwn(), 0);
        if (GFL_NetIsParent()) {
            if (WifiP2PMatch_CheckParty(wk->regulation, wk->gameData, &flags) != 0 &&
                WifiP2PMatch_CheckBattleBoxParty(wk->regulation, wk->battleBoxParty, &flags) != 0) {
                WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_NotMeetConditionsCannot, FALSE);
                wk->seq = WIFIP2PMATCH_SEQ_GAME_START_FAIL_KEY_WAIT;
                return seq;
            }
            wk->seq = WIFIP2PMATCH_SEQ_BATTLE_SETUP_DECIDE;
        } else {
            NetHandle_SendTimingPair(NetHandle_GetOwn(), 0x1b, 10);
            wk->seq = WIFIP2PMATCH_SEQ_SYNC_SEQ_TIMING_WAIT;
        }
        return seq;
    }
}

// State 0x74
static int WifiP2PMatch_StateGameStartFailKeyWait(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    if (GCTX_HIDGetPressedKeys()) {
        WifiP2PMatch_EndMessage(wk);
        wk->sendSeq = WIFIP2PMATCH_SEQ_CANCEL_RESET;
        wk->seq = WIFIP2PMATCH_SEQ_SEND_SEQ_TIMING;
    }
    return seq;
}

// State 0x75: "Confirm" in the battle rule menu. Starts recruiting for the battle
static int WifiP2PMatch_StateBattleRecruitStart(WIFIP2PMATCH_WORK *wk, int seq) {
    WifiP2PMatch_LoadRegulation(wk, 1);
    if (!WifiP2PMatch_PassesRegulation(wk)) {
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_NotMeetConditionsCannot, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_REGULATION_FAIL_PARTY;
        return seq;
    }
    {
        u32 gameModes[8] = { 7, 8, 9, 10, 11, 12, 13, 14 };

        WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_RECRUIT,
                                    (u16)gameModes[wk->battleRule + wk->battleMode * 2]);
    }
    WifiP2PMatch_StartConnect(wk, -1);
    wk->seq = WIFIP2PMATCH_SEQ_ROOM_MESSAGE;
    WifiP2PMatch_RoomPause(wk, TRUE);
    return seq;
}

static BGSysVRAMConfig sWifiP2PMatchVRAMConfig = {
    GX_VRAM_BG_128_A,  GX_VRAM_BGEXTPLTT_NONE,  GX_VRAM_SUB_BG_128_C,        GX_VRAM_SUB_BGEXTPLTT_NONE,
    GX_VRAM_OBJ_128_B, GX_VRAM_OBJEXTPLTT_NONE, GX_VRAM_SUB_OBJ_128_D,       GX_VRAM_SUB_OBJEXTPLTT_NONE,
    GX_VRAM_TEX_NONE,  GX_VRAM_TEXPLTT_NONE,    GX_OBJVRAMMODE_CHAR_1D_128K, GX_OBJVRAMMODE_CHAR_1D_128K,
};

// wifiHeapSize is set for the machine before each use
static GFLNetInitData sWifiP2PMatchNetInit = {
    NULL,
    0,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    { .wifiError = { { 0 }, WifiP2PMatch_OnFriendCode } },
    WifiP2PMatch_GetFriendData,
    WifiP2PMatch_GetUserData,
    0x5e400,
    { 1, 0, 0, 0, 0x80, 0x13, 0, 0 },
    HEAPID_USER,
    0xd,
    0xf,
    0xd,
    0xf0,
    0,
    2,
    0x50,
    0x10,
    1,
    0,
    GFL_NET_TYPE_WIFI,
    1,
    0xa,
    { 0x2c, 1, 0, 0 },
    0,
    0,
};

static sptr WifiP2PMatch_GetUserData(void *work) {
    WIFIP2PMATCH_WORK *wk = work;

    return (sptr)WifiList_GetMyUserData(GameData_GetWifiList(wk->gameData));
}

static sptr WifiP2PMatch_GetFriendData(void *work) {
    WIFIP2PMATCH_WORK *wk = work;

    return (sptr)WifiList_GetFriendData(GameData_GetWifiList(wk->gameData), 0);
}

static void WifiP2PMatch_OnFriendCode(int code, int param, void *work) {
    WIFIP2PMATCH_WORK *wk = work;

    WifiList_MergeFriendRecord(GameData_GetWifiList(wk->gameData), code, param);
}

// Starts the network with the Wi-Fi heap of the machine, or only gives it the init data again
static void WifiP2PMatch_NetInit(WIFIP2PMATCH_WORK *wk, BOOL init) {
    if (OS_IsRunOnTwl()) {
        sWifiP2PMatchNetInit.wifiHeapSize = 0x79000;
    } else {
        sWifiP2PMatchNetInit.wifiHeapSize = 0x5e400;
    }
    if (init) {
        GFL_NetInit(&sWifiP2PMatchNetInit, NULL, wk);
    } else {
        GFL_NetSetInitData(&sWifiP2PMatchNetInit);
    }
}

static u32 WifiP2PMatch_GetStatus(WIFIP2PMATCH_WORK *wk, const WIFI_STATUS *status) {
    return WIFI_STATUS_GetWifiStatus(status);
}

static u32 WifiP2PMatch_GetGameMode(WIFIP2PMATCH_WORK *wk, const WIFI_STATUS *status) {
    return WIFI_STATUS_GetGameMode(status);
}

// Whether a game mode is one of the battles
static BOOL WifiP2PMatch_IsBattleGameMode(u32 gameMode) {
    switch (gameMode) {
    case WIFIP2PMATCH_GAMEMODE_BATTLE:
    case WIFIP2PMATCH_GAMEMODE_BATTLE + 1:
    case WIFIP2PMATCH_GAMEMODE_BATTLE + 2:
    case WIFIP2PMATCH_GAMEMODE_BATTLE + 3:
    case WIFIP2PMATCH_GAMEMODE_BATTLE + 4:
    case WIFIP2PMATCH_GAMEMODE_BATTLE + 5:
    case WIFIP2PMATCH_GAMEMODE_BATTLE + 6:
    case WIFIP2PMATCH_GAMEMODE_BATTLE + 7:
        return TRUE;
    }
    return FALSE;
}

static BOOL WifiP2PMatch_IsPlayGameMode(int gameMode) {
    if (gameMode < WIFIP2PMATCH_GAMEMODE_MAX && gameMode > 3) {
        return TRUE;
    }
    return FALSE;
}

static BOOL WifiP2PMatch_IsRecruitStatus(u32 status) {
    if (status == WIFIP2PMATCH_STATUS_RECRUIT) {
        return TRUE;
    }
    return FALSE;
}

// Whether a status is that of a battle being played
static BOOL WifiP2PMatch_IsBattlePlaying(u32 status, u32 gameMode) {
    if (WifiP2PMatch_IsBattleGameMode(gameMode) && status == WIFIP2PMATCH_STATUS_PLAYING) {
        return TRUE;
    }
    return FALSE;
}

// Whether the party has two Pokémon that are not Eggs
static BOOL WifiP2PMatch_HasTwoPokemon(WIFIP2PMATCH_WORK *wk) {
    GameData_GetBoxSaveAccessor(wk->gameData);
    if (howManyPartyPokesAreNotEggs(GameData_GetParty(wk->gameData)) >= 2) {
        return TRUE;
    }
    return FALSE;
}

// Whether the party or the Battle Box passes the regulation
static BOOL WifiP2PMatch_PassesRegulation(WIFIP2PMATCH_WORK *wk) {
    u32 flags;

    if (WifiP2PMatch_CheckParty(wk->regulation, wk->gameData, &flags) != 0 &&
        WifiP2PMatch_CheckBattleBoxParty(wk->regulation, wk->battleBoxParty, &flags) != 0) {
        return FALSE;
    }
    return TRUE;
}

// The network's side of a status change
static void WifiP2PMatch_ChangeNetStatus(WIFIP2PMATCH_WORK *wk, u32 status, u32 gameMode) {
    if (status == WIFIP2PMATCH_STATUS_WAIT) {
        GFL_NetSetMPSendEnabled(FALSE);
        WifiP2PMatch_NetInit(wk, FALSE);
    } else if (WifiP2PMatch_IsBattlePlaying(status, gameMode)) {
        GFL_NetSetMPSendEnabled(TRUE);
    }
    switch (status) {
    case 2:
    case 5:
        GFL_NetSetAcceptConnect(NetHandle_GetOwn(), 1);
        break;
    case 0:
    case 1:
    case 3:
    case 4:
    case 6:
    default:
        GFL_NetSetAcceptConnect(NetHandle_GetOwn(), 1);
        break;
    }
}

// The screen's graphics, messages and fonts
static void WifiP2PMatch_GraphicInit(WIFIP2PMATCH_WORK *wk) {
    ArcTool *arc;

    if (wk->vblankTcb != NULL) {
        GFL_TCBRemove(wk->vblankTcb);
    }
    GFL_BGSysSetEnabledBGsA(0);
    GFL_BGSysSetEnabledBGsB(0);
    GX_SetVisiblePlane(0);
    GXS_SetVisiblePlane(0);
    arc = GFL_ArcSysCreateFileHandle(ARCID_WIFIP2PMATCH, WIFIP2PMATCH_HEAP);
    wk->wordSet = GFL_WordSetSystemCreateDefault(WIFIP2PMATCH_HEAP);
    wk->msgData = GFL_MsgSysLoadData(FALSE, ARCID_SYSTEM_MESSAGE, TEXT_BANK_WIFI_P2PMATCH, WIFIP2PMATCH_HEAP);
    wk->sysMsgData = GFL_MsgSysLoadData(FALSE, ARCID_SYSTEM_MESSAGE, TEXT_BANK_WIFI_ERROR, WIFIP2PMATCH_HEAP);
    wk->font = GFL_FontCreate(ARCID_FONT, 0, 0, FALSE, WIFIP2PMATCH_HEAP);
    wk->smallFont = GFL_FontCreate(ARCID_FONT, 3, 0, FALSE, WIFIP2PMATCH_HEAP);
    GFL_BGSysSetDisplayLayout(1);
    WifiP2PMatch_SetVRAMBanks();
    WifiP2PMatch_BGInit(WIFIP2PMATCH_HEAP);
    BmpWin_InitAllocator(WIFIP2PMATCH_HEAP);
    WifiP2PMatch_BGGraphicLoad(wk, arc);
    wk->vblankTcb = GFL_VBlankTCBAdd(WifiP2PMatch_VBlankTask, wk, 1);
    WifiP2PMatch_CharInit();
    WifiP2PMatch_ClActInit(wk, arc);
    GFL_BGSysSetBGEnabledA(0x10, TRUE);
    GFL_ArcToolFree(arc);
}

static void WifiP2PMatch_GraphicExit(WIFIP2PMATCH_WORK *wk) {
    if (wk->vblankTcb != NULL) {
        GFL_TCBRemove(wk->vblankTcb);
    }
    WifiP2PMatch_FreeTrainerActor(wk);
    WifiP2PMatch_FreeTouchBar(wk);
    WifiP2PMatch_ReloadViewScreen(wk);
    if (WIFI_MCR_GetInitFlag(&wk->room) == TRUE) {
        WifiP2PMatch_RoomExit(wk);
    }
    if (WifiP2PMatch_ViewExists(wk) == TRUE) {
        WifiP2PMatch_ViewExit(wk);
    }
    WifiP2PMatch_IconFree(&wk->icon);
    ClActRenderer_Delete(wk->clActRenderer);
    ClActUnit_Delete(wk->clActUnit);
    ClActSys_Delete();
    WifiP2PMatch_FreeWindows(wk);
    WifiP2PMatch_BGExit(wk);
    BmpWin_FreeAllocator();
    GFL_BGSysFree();
    GFL_MsgDataFree(wk->sysMsgData);
    GFL_MsgDataFree(wk->msgData);
    GFL_WordSetSystemFree(wk->wordSet);
    GFL_FontFree(wk->smallFont);
    GFL_FontFree(wk->font);
}

static void WifiP2PMatch_VBlankTask(TCB *tcb, void *data) {
    ClActSys_VBlank();
}

static void WifiP2PMatch_SetVRAMBanks(void) {
    GFL_BGSysSetVRAMBanks(&sWifiP2PMatchVRAMConfig);
}

static void WifiP2PMatch_BGInit(HeapID heapId) {
    int i;

    GFL_BGSysCreate(heapId);
    {
        BGSysLCDConfig config = { 1, 0, 0, FALSE };

        GFL_BGSysSetLCDConfig(&config);
    }
    {
        BGSetup setup = {
            0,
            0,
            0x1000,
            0,
            BGRES_512x256,
            GX_BG_COLORMODE_16,
            GX_BG_SCRBASE(0xe000),
            GX_BG_CHARBASE(0x00000),
            0x10000,
            GX_BG_EXTPLTT_01,
            0,
            0,
            0,
        };

        GFL_BGSysCreateBG(0, &setup, BGMODE_TEXT);
        GFL_BGSysClearBG(0);
        GFL_BGSysLoadScr(0);
    }
    {
        BGSetup setup = {
            0,
            0,
            0x800,
            0,
            BGRES_256x256,
            GX_BG_COLORMODE_16,
            GX_BG_SCRBASE(0xd000),
            GX_BG_CHARBASE(0x10000),
            0x8000,
            GX_BG_EXTPLTT_01,
            1,
            0,
            0,
        };

        GFL_BGSysCreateBG(1, &setup, BGMODE_TEXT);
        GFL_BGSysClearBG(1);
        GFL_BGSysLoadScr(1);
    }
    {
        BGSetup setup = {
            0,
            0,
            0x800,
            0,
            BGRES_256x256,
            GX_BG_COLORMODE_16,
            GX_BG_SCRBASE(0xd800),
            GX_BG_CHARBASE(0x08000),
            0x8000,
            GX_BG_EXTPLTT_23,
            2,
            0,
            0,
        };

        GFL_BGSysCreateBG(2, &setup, BGMODE_TEXT);
        GFL_BGSysFillChar(2, 0, 1, 0);
        GFL_BGSysClearBG(2);
        GFL_BGSysLoadScr(2);
    }
    {
        BGSetup setup = {
            0,
            0,
            0x800,
            0,
            BGRES_256x256,
            GX_BG_COLORMODE_16,
            GX_BG_SCRBASE(0xf000),
            GX_BG_CHARBASE(0x18000),
            0x8000,
            GX_BG_EXTPLTT_23,
            1,
            0,
            0,
        };

        GFL_BGSysCreateBG(3, &setup, BGMODE_TEXT);
        GFL_BGSysClearBG(3);
        GFL_BGSysLoadScr(3);
    }
    {
        BGSetup setup = {
            0,
            0,
            0x800,
            0,
            BGRES_256x256,
            GX_BG_COLORMODE_16,
            GX_BG_SCRBASE(0xe000),
            GX_BG_CHARBASE(0x00000),
            0x8000,
            GX_BG_EXTPLTT_01,
            3,
            0,
            0,
        };

        GFL_BGSysCreateBG(4, &setup, BGMODE_TEXT);
        GFL_BGSysClearBG(4);
    }
    {
        BGSetup setup = {
            0,
            0,
            0x800,
            0,
            BGRES_256x256,
            GX_BG_COLORMODE_16,
            GX_BG_SCRBASE(0xe800),
            GX_BG_CHARBASE(0x08000),
            0x8000,
            GX_BG_EXTPLTT_01,
            0,
            0,
            0,
        };

        GFL_BGSysCreateBG(5, &setup, BGMODE_TEXT);
        GFL_BGSysClearBG(5);
    }
    {
        BGSetup setup = {
            0,
            0,
            0x800,
            0,
            BGRES_256x256,
            GX_BG_COLORMODE_16,
            GX_BG_SCRBASE(0xf000),
            GX_BG_CHARBASE(0x10000),
            0x8000,
            GX_BG_EXTPLTT_01,
            2,
            0,
            0,
        };

        GFL_BGSysCreateBG(6, &setup, BGMODE_TEXT);
        GFL_BGSysClearBG(6);
    }
    {
        BGSetup setup = {
            0,
            0,
            0x800,
            0,
            BGRES_256x256,
            GX_BG_COLORMODE_16,
            GX_BG_SCRBASE(0xf800),
            GX_BG_CHARBASE(0x18000),
            0x8000,
            GX_BG_EXTPLTT_01,
            1,
            0,
            0,
        };

        GFL_BGSysCreateBG(7, &setup, BGMODE_TEXT);
        GFL_BGSysClearBG(7);
    }
    for (i = 0; i < 7; i++) {
        GFL_BGSysClearChar(i);
    }
    GFL_BGSysSetBGPriority(0, 3);
    GFL_BGSysSetBGPriority(1, 1);
    GFL_BGSysSetBGPriority(3, 1);
    GFL_BGSysSetBGPriority(2, 0);
    G2_BlendNone();
    G2S_BlendNone();
}

// Stores a person of the room in the first free slot
static void WifiP2PMatch_AddMoveObj(WIFIP2PMATCH_WORK *wk, MCR_MOVEOBJ *obj) {
    int i;

    for (i = 0; i < WIFIP2PMATCH_OBJ_MAX; i++) {
        if (wk->moveObj[i] == NULL) {
            wk->moveObj[i] = obj;
            return;
        }
    }
}

static void WifiP2PMatch_RemoveMoveObj(WIFIP2PMATCH_WORK *wk, MCR_MOVEOBJ *obj) {
    int i;

    for (i = 0; i < WIFIP2PMATCH_OBJ_MAX; i++) {
        if (wk->moveObj[i] == obj) {
            wk->moveObj[i] = NULL;
            return;
        }
    }
}

// Frees the room and forgets who was in it
static void WifiP2PMatch_RoomExit(WIFIP2PMATCH_WORK *wk) {
    int i;

    WIFI_MCR_Dest(&wk->room);
    for (i = 0; i < WIFIP2PMATCH_OBJ_MAX; i++) {
        wk->moveObj[i] = NULL;
        WifiP2PMatch_ViewClearFriendState(wk, i + 1);
    }
    MI_CpuFill8(wk->matchStatus, 0, sizeof(wk->matchStatus));
}

// The person of the room who is a friend, or NULL
static MCR_MOVEOBJ *WifiP2PMatch_GetMoveObj(WIFIP2PMATCH_WORK *wk, u32 friendNo) {
    int i;

    for (i = 0; i < WIFIP2PMATCH_OBJ_MAX; i++) {
        if (wk->moveObj[i] != NULL) {
            if (friendNo == WIFI_MCR_GetFriendNo(wk->moveObj[i])) {
                return wk->moveObj[i];
            }
        }
    }
    return NULL;
}

// Loads the status icons: their palettes and characters for both screens, and the screen their tiles are read from
static void WifiP2PMatch_IconLoad(WIFIP2PMATCH_ICON *icon, ArcTool *arc, u32 heapId) {
    GFL_G2DIOLoadArcNCLRDefault(arc, 4, 0, 0x100, 0x80, heapId);
    GFL_G2DIOLoadArcNCLRDefault(arc, 4, 4, 0x100, 0x80, heapId);
    icon->bg1Chars = GFL_BGSysLoadArcNCGRDynamic(arc, 13, 1, 0, FALSE, heapId);
    icon->bg6Chars = GFL_BGSysLoadArcNCGRDynamic(arc, 13, 6, 0, FALSE, heapId);
    if (icon->charBuf == NULL) {
        icon->charBuf = GFL_G2DIOReadOBJNCGRArc(arc, 13, FALSE, &icon->chr, heapId);
    }
    if (icon->scrnBuf == NULL) {
        icon->scrnBuf = GFL_G2DIOReadNSCRArc(arc, 21, FALSE, &icon->scrn, heapId);
    }
}

static void WifiP2PMatch_IconFree(WIFIP2PMATCH_ICON *icon) {
    if (icon->scrnBuf != NULL) {
        GFL_HeapFree(icon->scrnBuf);
        icon->scrnBuf = NULL;
    }
    if (icon->charBuf != NULL) {
        GFL_HeapFree(icon->charBuf);
        icon->charBuf = NULL;
    }
    GFL_BGSysFreeCharMemory(1, CHAR_POS(icon->bg1Chars), CHAR_SIZE(icon->bg1Chars));
    GFL_BGSysFreeCharMemory(6, CHAR_POS(icon->bg6Chars), CHAR_SIZE(icon->bg6Chars));
}

// Writes an icon's four tiles into a BG's screen. The icons are 16 to a row in their screen
static void WifiP2PMatch_IconWrite(WIFIP2PMATCH_ICON *icon, u32 bg, u8 x, u8 y, u32 iconNo, u32 unused) {
    int i;
    int j;
    u16 *tiles = (u16 *)icon->scrn->rawData;
    u32 palette = WifiP2PMatch_GetIconPalette(bg);
    u32 srcY = (iconNo / 16) * 2;
    u32 srcX = (iconNo % 16) * 2;
    u32 width = icon->scrn->screenWidth / 8;

    for (i = 0; i < 2; i++) {
        for (j = 0; j < 2; j++) {
            GFL_BGSysSetScrTile(bg, j + x, i + y, (palette << 12) + tiles[(srcY + i) * width + srcX + j]);
        }
    }
    GFL_BGSysQueueScrLoad(bg);
}

// The icon of a game mode, which differs while the friend is recruiting for it
static u32 WifiP2PMatch_GetIconNo(u32 gameMode, u32 status) {
    u32 iconNo = 15;

    switch (gameMode) {
    case 4:
        if (status == WIFIP2PMATCH_STATUS_RECRUIT) {
            iconNo = 4;
        } else {
            iconNo = 3;
        }
        break;
    case 5:
        if (status == WIFIP2PMATCH_STATUS_RECRUIT) {
            iconNo = 6;
        } else {
            iconNo = 5;
        }
        break;
    case 6:
        if (status == WIFIP2PMATCH_STATUS_RECRUIT) {
            iconNo = 10;
        } else {
            iconNo = 9;
        }
        break;
    case 2:
        iconNo = 2;
        break;
    case 0:
        iconNo = 15;
        break;
    case 3:
        iconNo = 1;
        break;
    default:
        if (WifiP2PMatch_IsBattleGameMode(gameMode)) {
            if (status == WIFIP2PMATCH_STATUS_RECRUIT) {
                iconNo = 8;
            } else {
                iconNo = 7;
            }
        } else if (gameMode >= 15) {
            iconNo = 14;
        }
        break;
    }
    return iconNo;
}

static void WifiP2PMatch_IconWriteStatus(WIFIP2PMATCH_ICON *icon, u32 bg, u8 x, u8 y, u32 status, u32 gameMode,
                                         u32 add) {
    u32 iconNo = WifiP2PMatch_GetIconNo(gameMode, status);

    WifiP2PMatch_IconWrite(icon, bg, x, y, iconNo + add, 0);
}

// The palette of the icons
static u32 WifiP2PMatch_GetIconPalette(u32 bg) {
    return 8;
}

// Leaves a connection: puts the voice chat settings back in the status. Returns the state to go on with
static int WifiP2PMatch_LeaveConnection(WIFIP2PMATCH_WORK *wk, BOOL wipe) {
    if (wipe) {
        GFL_WipeSet(WIPE_MODE_BOTH, WIPE_TYPE_FADE_IN, WIPE_TYPE_FADE_IN, WIPE_COLOR_BLACK, 6, 1, WIFIP2PMATCH_HEAP);
    }
    WIFI_STATUS_SetVChatStatus(wk->myStatus, wk->parent->unk46);
    WIFI_STATUS_SetUseVChat(wk->myStatus, wk->parent->unk46);
    WifiP2PMatch_ResetVChatVolume();
    return WIFIP2PMATCH_SEQ_LOGIN_WAIT;
}

static void WifiP2PMatch_BGExit(WIFIP2PMATCH_WORK *wk) {
}

static void WifiP2PMatch_BGGraphicLoad(WIFIP2PMATCH_WORK *wk, ArcTool *arc) {
    GFL_BGSysSetBGEnabledA(0xf, TRUE);
    GFL_BGSysSetBGEnabledB(0xd, TRUE);
    WifiP2PMatch_IconLoad(&wk->icon, arc, WIFIP2PMATCH_HEAP);
    GFL_BGSysLoadNCLRDefault(ARCID_FONT, 5, 0, 15 * 0x20, 0x20, WIFIP2PMATCH_HEAP);
    wk->listFrameChars = LoadCursorImageEndOfHeap(2, 14, 0, WIFIP2PMATCH_HEAP);
    wk->talkFrameChars = LoadCursorImageEndOfHeap(2, 14, 0, WIFIP2PMATCH_HEAP);
    wk->subFrameChars = LoadCursorImageEndOfHeap(1, 14, 0, WIFIP2PMATCH_HEAP);
}

static void WifiP2PMatch_CharInit(void) {
}

static const ClActSurfaceSetup sWifiP2PMatchSurfaceSetups[2] = {
    { 0, 0, 256, 192, CLACT_VRAM_MAIN, FALSE },
    { 0, 256, 256, 192, CLACT_VRAM_SUB, FALSE },
};

static const ClActSysSetup sWifiP2PMatchClActSysSetup = { 0, 0, 0, 512, 4, 124, 4, 124, 0, 100, 100, 100, 100, 16, 16 };

static void WifiP2PMatch_ClActInit(WIFIP2PMATCH_WORK *wk, ArcTool *arc) {
    ClActSys_Create(&sWifiP2PMatchClActSysSetup, &sWifiP2PMatchVRAMConfig, WIFIP2PMATCH_HEAP);
    wk->clActUnit = ClActUnit_Create(100, 0, WIFIP2PMATCH_HEAP);
    wk->clActRenderer =
        ClActRenderer_Create(sWifiP2PMatchSurfaceSetups, NELEMS(sWifiP2PMatchSurfaceSetups), WIFIP2PMATCH_HEAP);
    ClActUnit_SetRenderer(wk->clActUnit, wk->clActRenderer);
}

// State 0: the proc's first. Starts the screen, or goes on with the connection after a game
static int WifiP2PMatch_StateInit(WIFIP2PMATCH_WORK *wk, int seq) {
    if (wk->parent->mode != 0) {
        WifiP2PMatch_BgmVolumeInit(wk, 4);
        WifiList_SetFriendLastDateToNow(wk->wifiList, DWCRap_GetFriendIndex());
        if ((wk->result == 3 || wk->result == 4) && !DWCRap_IsEnding()) {
            DWCRap_StopVoiceChat();
            WifiP2PMatch_StartRoom(wk);
            wk->friendNo = wk->parent->friendIndex;
            WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_PLAYING, 3);
            NetHandle_SendTimingPair(NetHandle_GetOwn(), 0x18, 10);
            wk->seq = WIFIP2PMATCH_SEQ_RETURN_TIMING_WAIT;
        } else {
            NetHandle_SendTimingPair(NetHandle_GetOwn(), 0x12, 10);
            wk->seq = WIFIP2PMATCH_SEQ_VCHAT_END_TIMING;
        }
    } else {
        wk->seq = WIFIP2PMATCH_SEQ_LOGIN_WAIT;
        WifiP2PMatch_BgmVolumeInit(wk, 1);
    }
    if (GFL_SndBGMGetRequestedID() != SEQ_BGM_POKECEN) {
        GFL_SndBGMPlay(SEQ_BGM_POKECEN, 0xffff);
    }
    return seq;
}

// State 5
static int WifiP2PMatch_StateBGPriority(WIFIP2PMATCH_WORK *wk, int seq) {
    GFL_BGSysSetBGPriority(0, 3);
    GFL_BGSysSetBGPriority(1, 1);
    GFL_BGSysSetBGPriority(3, 1);
    GFL_BGSysSetBGPriority(2, 0);
    wk->seq = WIFIP2PMATCH_SEQ_CONNECT_WAIT;
    return seq;
}

// Gives the status to the network
static void WifiP2PMatch_SendMyStatus(WIFIP2PMATCH_WORK *wk) {
    DWCRap_SetOwnStatusData((const char *)wk->myStatus, sizeof(WIFI_STATUS));
}

// Fills the player's status and gives it to the network
static void WifiP2PMatch_InitMyStatus(WIFIP2PMATCH_WORK *wk, u32 status, u32 gameMode) {
    PlayerInfo *info = GetGameDataPlayerInfo(wk->gameData);
    u8 gender;

    wk->myStatus->version = GAME_VERSION;
    wk->myStatus->language = GAME_LANGUAGE;
    gender = getTrainerGender(info);
    wk->myStatus->gender = gender;
    wk->myStatus->trainerView = PlayerInfo_GetUnionView(info);
    WifiP2PMatch_SetMyStatus(wk, status, gameMode);
    wk->myStatus->country = UnityTowerVisitor_GetCountry(info);
    wk->myStatus->province = UnityTowerVisitor_GetProvince(info);
    WIFI_STATUS_SetVChatStatus(wk->myStatus, wk->parent->unk46);
    WIFI_STATUS_SetUseVChat(wk->myStatus, wk->parent->unk46);
    // WIFI_STATUS.unk0 is the trainer ID after the GameSpy profile ID
    wk->myStatus->trainerId = getIDAsUInt(info);
    wk->myStatus->profileId = PlayerInfo_GetProfileId(info);
    WifiP2PMatch_SendMyStatus(wk);
}

// Lists the friends who are in the room. Returns how many
static int WifiP2PMatch_UpdateRoomFriendList(WIFIP2PMATCH_WORK *wk) {
    int i;
    int count = 0;

    for (i = 0; i < WIFIP2PMATCH_MEMBER_MAX; i++) {
        wk->index2No[i] = 0;
        if (wk->friendNum > i) {
            if (WifiP2PMatch_GetFriendStatus(i)) {
                u32 status = WifiP2PMatch_GetStatus(wk, WifiP2PMatch_GetFriendStatusData(i));

                if (status != WIFIP2PMATCH_STATUS_NONE && status < 6) {
                    wk->index2No[count] = i + 1;
                    count++;
                } else if (WifiP2PMatch_ViewGetFriendState(wk, i + 1)) {
                    WifiP2PMatch_ViewClearFriendState(wk, i + 1);
                }
            }
        }
    }
    return count;
}

// Looks for friends whose status, game mode or voice chat setting changed, makes their characters show it and
// records what the friend list keeps of them. Returns how many changed
static int WifiP2PMatch_CheckFriendStatusChange(WIFIP2PMATCH_WORK *wk) {
    int i;
    int count = 0;
    MCR_MOVEOBJ *obj;

    for (i = 0; i < WIFIP2PMATCH_MEMBER_MAX; i++) {
        WIFI_STATUS *status = WifiP2PMatch_GetFriendStatusData(i);
        u8 wifiStatus = WifiP2PMatch_GetStatus(wk, status);
        u8 gameMode = WifiP2PMatch_GetGameMode(wk, status);

        if (WifiP2PMatch_GetFriendStatus(i) == 0) {
            wifiStatus = WIFIP2PMATCH_STATUS_NONE;
        }
        if (wifiStatus != wk->matchStatus[i] || gameMode != wk->matchGameMode[i] ||
            wk->matchVChat[i] != WIFI_STATUS_GetVChatStatus(status)) {
            obj = WifiP2PMatch_GetMoveObj(wk, i + 1);
            if (obj != NULL) {
                BOOL wasRecruiting = WifiP2PMatch_IsRecruitStatus(wk->matchStatus[i]);
                BOOL isRecruiting = WifiP2PMatch_IsRecruitStatus(wifiStatus);

                if (wasRecruiting == TRUE && isRecruiting == FALSE) {
                    WIFI_MCR_NpcMoveSet(&wk->room, obj, WIFI_MCR_NPC_MOVE_NORMAL);
                } else if (wasRecruiting == FALSE && isRecruiting == TRUE) {
                    WIFI_MCR_NpcMoveSet(&wk->room, obj, WIFI_MCR_NPC_MOVE_JUMP);
                }
                WifiList_SetFriendParam(wk->wifiList, i, WIFI_LIST_FRIEND_TRAINER_VIEW, status->trainerView);
                WifiList_SetFriendParam(wk->wifiList, i, WIFI_LIST_FRIEND_GENDER, status->gender);
                WifiList_SetFriendParam(wk->wifiList, i, WIFI_LIST_FRIEND_PROFILE_ID, status->profileId);
                WifiList_SetFriendParam(wk->wifiList, i, WIFI_LIST_FRIEND_ID, status->trainerId);
            }
            wk->matchStatus[i] = wifiStatus;
            wk->matchGameMode[i] = gameMode;
            wk->matchVChat[i] = WIFI_STATUS_GetVChatStatus(status);
            count++;
        }
    }
    return count;
}

static void WifiP2PMatch_OnNetError(WIFIP2PMATCH_WORK *wk) {
}

static int WifiP2PMatch_StateReconnectKeyWait(WIFIP2PMATCH_WORK *wk, int seq) {
    if (wk->timer > 0) {
        wk->timer--;
        return seq;
    }
    if (GCTX_HIDGetPressedKeys() & (PAD_BUTTON_A | PAD_BUTTON_B)) {
        wk->sysMsgWin = WifiP2PMatch_FreeWindow(wk->sysMsgWin);
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_ReconnectNintendoWfc, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_RECONNECT_YES_NO_START;
    }
    return seq;
}

static int WifiP2PMatch_StateReconnectYesNoStart(WIFIP2PMATCH_WORK *wk, int seq) {
    if (WifiP2PMatch_IsMessageEnd(wk)) {
        WifiP2PMatch_YesNoStart(wk);
        wk->seq = WIFIP2PMATCH_SEQ_RECONNECT_YES_NO;
    }
    return seq;
}

static int WifiP2PMatch_StateReconnectYesNo(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 ret = WifiP2PMatch_YesNoMain(wk);

    if (ret == BMPMENU_NULL) {
        return seq;
    }
    if (ret == 0) {
        GFL_NetExit(NULL);
        WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
        wk->seq = WIFIP2PMATCH_SEQ_CONNECT_START;
    } else {
        WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
        GFL_NetExit(NULL);
        wk->endSeq = 0;
        wk->seq = WIFIP2PMATCH_SEQ_END;
    }
    WifiP2PMatch_EndMessage(wk);
    return seq;
}

// Takes the room and the friend page down, if they are up, and connects again
static int WifiP2PMatch_StateConnectStart(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!GFL_NetIsNotInitialized()) {
        ArcTool *arc;

        if (WIFI_MCR_GetInitFlag(&wk->room) == TRUE) {
            GFL_WipeSet(WIPE_MODE_BOTH, WIPE_TYPE_FADE_OUT, WIPE_TYPE_FADE_OUT, WIPE_COLOR_BLACK, 6, 1,
                        WIFIP2PMATCH_HEAP);
            WifiP2PMatch_RoomExit(wk);
            ClActSys_ResetOamBuffers();
        }
        if (WifiP2PMatch_ViewExists(wk) == TRUE) {
            WifiP2PMatch_ViewExit(wk);
        }
        wk->titleWin = WifiP2PMatch_FreeWindow(wk->titleWin);
        GFL_BGSysClearBG(3);
        arc = GFL_ArcSysCreateFileHandle(ARCID_WIFIP2PMATCH, WIFIP2PMATCH_HEAP);
        WifiP2PMatch_BGGraphicLoad(wk, arc);
        GFL_ArcToolFree(arc);
        wk->unk1328 = 1;
        if (wk->subMenuOptions != NULL) {
            BmpWin_Free(wk->subWin);
            BmpMenuList_Free(wk->subList, NULL, NULL);
            wk->subList = NULL;
            ListMenuCore_FreeOptionList(wk->subMenuOptions);
            wk->subMenuOptions = NULL;
        }
        MI_CpuFill8(wk->index2No, 0, sizeof(wk->index2No));
        MI_CpuFill8(wk->index2NoBackup, 0, sizeof(wk->index2NoBackup));
        MI_CpuFill8(wk->matchStatus, 0, sizeof(wk->matchStatus));
        MI_CpuFill8(wk->matchGameMode, 0, sizeof(wk->matchGameMode));
        MI_CpuFill8(wk->matchVChat, 0, sizeof(wk->matchVChat));
        WifiP2PMatch_DrawTitle(wk);
        WifiP2PMatch_NetInit(wk, FALSE);
        NetState_StartWifiLogin();
        WifiP2PMatch_PrintMessage(wk, WifiError_Text_ConnectingNintendoWiFi, TRUE);
        wk->seq = WIFIP2PMATCH_SEQ_BG_PRIORITY;
    }
    return seq;
}

static int WifiP2PMatch_StateConnectWait(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    if (DWCRap_IsUserDataDirty()) {
        wk->seq = WIFIP2PMATCH_SEQ_USER_DATA_SAVE_START;
        WifiP2PMatch_PrintMessage(wk, WifiError_Text_SavingDontTurnOff, TRUE);
    }
    if (NetState_IsWifiConnecting()) {
        if (wk->unk1E8 != 0) {
            wk->seq = WIFIP2PMATCH_SEQ_USER_INFO_NOTICE;
        } else {
            WifiP2PMatch_InitMyStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
            WifiP2PMatch_UpdateRoomFriendList(wk);
            wk->seq = WifiP2PMatch_LeaveConnection(wk, FALSE);
        }
    } else if (NetState_IsWifiBattleError() || NetState_GetWifiStatus() == 3) {
        WifiP2PMatch_OnNetError(wk);
    }
    return seq;
}

static int WifiP2PMatch_StateUserDataSaveStart(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    if (DWCRap_IsUserDataDirty()) {
        GameData_StartSave(wk->gameData);
        wk->seq = WIFIP2PMATCH_SEQ_USER_DATA_SAVE;
        return seq;
    }
    if (NetState_IsWifiConnecting()) {
        if (wk->unk1E8 != 0) {
            wk->seq = WIFIP2PMATCH_SEQ_USER_INFO_NOTICE;
        } else {
            WifiP2PMatch_InitMyStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
            WifiP2PMatch_UpdateRoomFriendList(wk);
            wk->seq = WifiP2PMatch_LeaveConnection(wk, FALSE);
        }
    } else if (NetState_IsWifiBattleError() || NetState_GetWifiStatus() == 3) {
        WifiP2PMatch_OnNetError(wk);
    }
    return seq;
}

static int WifiP2PMatch_StateUserDataSave(WIFIP2PMATCH_WORK *wk, int seq) {
    if (DWCRap_IsUserDataDirty()) {
        int result = GameData_UpdateSave(wk->gameData);

        if (result != 0 && result != 1) {
            DWCRap_ClearUserDataDirty();
        } else {
            return seq;
        }
    }
    if (NetState_IsWifiConnecting()) {
        if (wk->unk1E8 != 0) {
            wk->seq = WIFIP2PMATCH_SEQ_USER_INFO_NOTICE;
        } else {
            WifiP2PMatch_InitMyStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
            WifiP2PMatch_UpdateRoomFriendList(wk);
            wk->seq = WifiP2PMatch_LeaveConnection(wk, FALSE);
        }
    } else if (NetState_IsWifiBattleError() || NetState_GetWifiStatus() == 3) {
        WifiP2PMatch_OnNetError(wk);
    }
    return seq;
}

// Waits, then leaves the connection and goes back to the room
static int WifiP2PMatch_StateDisconnectWait(WIFIP2PMATCH_WORK *wk, int seq) {
    if (wk->timer > 0) {
        wk->timer--;
        return seq;
    }
    if (wk->regulationPrintMsg != NULL) {
        PokeRegulation_FreePrintMsg(wk->regulationPrintMsg);
        wk->regulationPrintMsg = NULL;
    }
    if (NetState_IsWifiBattleError() || NetState_GetWifiStatus() == 3) {
        WifiP2PMatch_OnNetError(wk);
    } else if (NetState_IsWifiReady()) {
        if (WIFI_MCR_GetInitFlag(&wk->room) == TRUE) {
            WifiP2PMatch_RoomExit(wk);
        }
        wk->seq = WifiP2PMatch_LeaveConnection(wk, FALSE);
    } else if (NetState_GetWifiStatus() < 4 && !NetState_IsDisconnectReceived() && GFL_NetIsNegotiated(0xff)) {
        // Still disconnecting
    } else {
        NetState_WifiDisconnect(TRUE);
        if (WIFI_MCR_GetInitFlag(&wk->room) == TRUE) {
            WifiP2PMatch_RoomExit(wk);
        }
        wk->seq = WifiP2PMatch_LeaveConnection(wk, FALSE);
    }
    return seq;
}

static int WifiP2PMatch_StateUserInfoNotice(WIFIP2PMATCH_WORK *wk, int seq) {
    WifiP2PMatch_EndMessage(wk);
    if (isRunningOnDSi()) {
        WifiP2PMatch_PrintSystemMessage(wk, WifiError_Text_WiFiUserInformation_4);
    } else {
        WifiP2PMatch_PrintSystemMessage(wk, WifiError_Text_WiFiUserInformation_2);
    }
    wk->seq = WIFIP2PMATCH_SEQ_USER_INFO_NOTICE_KEY_WAIT;
    wk->unk1E8 = 0;
    return seq;
}

static int WifiP2PMatch_StateUserInfoNoticeKeyWait(WIFIP2PMATCH_WORK *wk, int seq) {
    if (GCTX_HIDGetPressedKeys() & (PAD_BUTTON_A | PAD_BUTTON_B)) {
        if (WifiList_CountFriends(wk->wifiList) == 0) {
            WifiP2PMatch_PrintMessage(wk, WifiError_Text_DisconnectingFromNintendoWi, TRUE);
            wk->seq = WIFIP2PMATCH_SEQ_DISCONNECT;
            wk->timer = 1;
        } else {
            WifiP2PMatch_InitMyStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
            WifiP2PMatch_UpdateRoomFriendList(wk);
            wk->seq = WifiP2PMatch_LeaveConnection(wk, FALSE);
            wk->sysMsgWin = WifiP2PMatch_FreeWindow(wk->sysMsgWin);
        }
    }
    return seq;
}

// Makes the windows of the room's upper screen: the player's status and the message frame
static void WifiP2PMatch_CreateRoomWindows(WIFIP2PMATCH_WORK *wk) {
    if (wk->myInfoWin != NULL) {
        BmpWin_Free(wk->myInfoWin);
    }
    GFL_BGSysLoadNCLRDefault(ARCID_FONT, 5, 0, 0x1e0, 0x20, WIFIP2PMATCH_HEAP);
    wk->titleWin = WifiP2PMatch_FreeWindow(wk->titleWin);
    wk->myInfoWin = BmpWin_CreateDynamic(3, 1, 1, 28, 2, 15, 1);
    wk->titleWin = BmpWin_CreateDynamic(2, 1, 1, 28, 2, 15, 1);
    GFL_BitmapFill(BmpWin_GetBitmap(wk->myInfoWin), 0xff);
    BmpWin_FlushChar(wk->myInfoWin);
    GFL_BitmapFill(BmpWin_GetBitmap(wk->titleWin), 0xff);
    BmpWin_FlushChar(wk->titleWin);
    BmpWin_DrawFrame(wk->titleWin, TRUE, wk->talkFrameChars, 14);
}

// Whether a friend who asks to connect is in the game mode the player is in
static BOOL WifiP2PMatch_ConnectRequestFunc(int friendIndex, void *work) {
    WIFIP2PMATCH_WORK *wk = work;
    WIFI_STATUS *myStatus = wk->myStatus;
    WIFI_STATUS *friendStatus = WifiP2PMatch_GetFriendStatusData(friendIndex);

    WIFI_STATUS_GetWifiStatus(myStatus);
    WIFI_STATUS_GetWifiStatus(friendStatus);
    if (WIFI_STATUS_GetGameMode(myStatus) == WIFI_STATUS_GetGameMode(friendStatus)) {
        return TRUE;
    }
    return FALSE;
}

static int WifiP2PMatch_StateLoginWait(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!NetState_IsWifiBattleError() && !NetState_IsWifiReady()) {
        return seq;
    }
    wk->seq = WIFIP2PMATCH_SEQ_ROOM_START;
    return seq;
}

// Starts the room, with the player in it, and the friend page
static void WifiP2PMatch_StartRoom(WIFIP2PMATCH_WORK *wk) {
    ArcTool *arc;

    DWCRap_SetRequestFunc(WifiP2PMatch_ConnectRequestFunc);
    arc = GFL_ArcSysCreateFileHandle(ARCID_WIFIP2PMATCH, WIFIP2PMATCH_HEAP);
    WifiP2PMatch_ResetVChat(wk);
    WifiP2PMatch_FreeRecruitMenu(wk);
    MI_CpuFill8(wk->index2NoBackup, 0, sizeof(wk->index2NoBackup));
    if (wk->subMenuOptions != NULL) {
        BmpWin_Free(wk->subWin);
        BmpMenuList_Free(wk->subList, NULL, NULL);
        wk->subList = NULL;
        ListMenuCore_FreeOptionList(wk->subMenuOptions);
        wk->subMenuOptions = NULL;
    }
    if (!WIFI_MCR_GetInitFlag(&wk->room)) {
        u32 view;

        if (getTrainerGender(GetGameDataPlayerInfo(wk->gameData)) == 0) {
            view = 0x10;
        } else {
            view = 0x11;
        }
        wk->friendNum = WifiList_GetFriendSlotCount(wk->wifiList);
        WIFI_MCR_Init(&wk->room, WIFIP2PMATCH_HEAP, arc, view, wk->friendNum, ARCID_WIFIP2PMATCH, wk->clActUnit,
                      wk->clActRenderer);
        WifiP2PMatch_AddMoveObj(wk, WIFI_MCR_SetPlayer(&wk->room, view));
    }
    if (!WifiP2PMatch_ViewExists(wk)) {
        WifiP2PMatch_ViewInit(wk, arc, WIFIP2PMATCH_HEAP);
    }
    WIFI_STATUS_ResetVChatMac(wk->myStatus);
    wk->unk1351 = 0;
    WifiP2PMatch_ViewRedraw(wk, WIFIP2PMATCH_HEAP);
    WifiP2PMatch_EndMessage(wk);
    WifiP2PMatch_UpdateRoomFriendList(wk);
    WifiP2PMatch_CreateRoomWindows(wk);
    WifiP2PMatch_NetInit(wk, FALSE);
    wk->preConnect = -1;
    GFL_ArcToolFree(arc);
    GFL_WipeSet(WIPE_MODE_BOTH, WIPE_TYPE_FADE_IN, WIPE_TYPE_FADE_IN, WIPE_COLOR_BLACK, 6, 1, WIFIP2PMATCH_HEAP);
}

static int WifiP2PMatch_StateRoomStart(WIFIP2PMATCH_WORK *wk, int seq) {
    WifiP2PMatch_StartRoom(wk);
    WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
    WifiP2PMatch_ChangeNetStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
    wk->seq = WIFIP2PMATCH_SEQ_ROOM;
    WifiP2PMatch_RoomResume(wk);
    func_02042e94(0);
    GFL_NetSetDisconnectError(FALSE);
    return seq;
}

static BOOL WifiP2PMatch_IsConnected(WIFIP2PMATCH_WORK *wk) {
    if (NetState_GetWifiStatus() == 1) {
        return TRUE;
    }
    return FALSE;
}

static BOOL WifiP2PMatch_HasNewConnection(WIFIP2PMATCH_WORK *wk) {
    if (DWCRap_GetLastNetId() != -1) {
        return TRUE;
    }
    return FALSE;
}

// Whether the connection failed
static BOOL WifiP2PMatch_IsConnectFailed(WIFIP2PMATCH_WORK *wk) {
    switch (NetState_GetWifiStatus()) {
    case 3:
    case 4:
    case 5:
        return TRUE;
    }
    return FALSE;
}

// Takes the friends who left out of the room and puts those who came into it. Returns how many friends are there
static int WifiP2PMatch_UpdateFriendsInRoom(WIFIP2PMATCH_WORK *wk) {
    int i, j;
    MCR_MOVEOBJ *obj;
    int num = WifiP2PMatch_UpdateRoomFriendList(wk);
    BOOL in = FALSE;
    BOOL out = FALSE;

    if (GFL_STD_MemCmp(wk->index2No, wk->index2NoBackup, sizeof(wk->index2No)) != 0) {
        for (i = 0; i < WIFIP2PMATCH_MEMBER_MAX; i++) {
            BOOL found = FALSE;

            for (j = 0; j < WIFIP2PMATCH_MEMBER_MAX; j++) {
                if (wk->index2No[j] == wk->index2NoBackup[i]) {
                    found = TRUE;
                    break;
                }
            }
            if (!found && wk->index2NoBackup[i] != 0) {
                obj = WifiP2PMatch_GetMoveObj(wk, wk->index2NoBackup[i]);
                if (obj != NULL) {
                    WIFI_MCR_DelPeopleReq(&wk->room, obj);
                    WifiP2PMatch_RemoveMoveObj(wk, obj);
                    WifiP2PMatch_ViewClearFriendState(wk, wk->index2NoBackup[i]);
                    WifiP2PMatch_ViewCloseDetailOf(wk, wk->index2NoBackup[i], WIFIP2PMATCH_HEAP);
                    out = TRUE;
                } else {
                    WifiP2PMatch_ViewClearFriendState(wk, wk->index2NoBackup[i]);
                }
            }

            found = FALSE;
            for (j = 0; j < WIFIP2PMATCH_MEMBER_MAX; j++) {
                if (wk->index2No[i] == wk->index2NoBackup[j]) {
                    found = TRUE;
                    break;
                }
            }
            if (!found && wk->index2No[i] != 0) {
                WIFI_STATUS *status = WifiP2PMatch_GetFriendStatusData(wk->index2No[i] - 1);

                obj = WIFI_MCR_SetNpc(&wk->room, status->trainerView, wk->index2No[i]);
                if (obj != NULL) {
                    WifiP2PMatch_ViewSetFriendState(wk, wk->index2No[i], 2);
                    WifiP2PMatch_AddMoveObj(wk, obj);
                    in = TRUE;
                } else {
                    WifiP2PMatch_ViewSetFriendState(wk, wk->index2No[i], 1);
                }
            }
        }
        MI_CpuCopy8(wk->index2No, wk->index2NoBackup, sizeof(wk->index2No));
        if (in || out) {
            WifiP2PMatch_ViewRequestRedrawAll(wk);
        }
        if (in == TRUE) {
            GFL_SndSEPlay(SEQ_SE_FLD_05);
        } else if (out == TRUE) {
            GFL_SndSEPlay(SEQ_SE_FLD_05);
        }
    }
    return num;
}

// Puts the friends into the room who could not come in before, because the player stood on their seat
static void WifiP2PMatch_AddWaitingFriends(WIFIP2PMATCH_WORK *wk) {
    int i;
    BOOL in = FALSE;

    for (i = 0; i < WIFIP2PMATCH_MEMBER_MAX; i++) {
        if (wk->index2No[i] != 0 && WifiP2PMatch_GetMoveObj(wk, wk->index2No[i]) == NULL) {
            WIFI_STATUS *status = WifiP2PMatch_GetFriendStatusData(wk->index2No[i] - 1);
            MCR_MOVEOBJ *obj = WIFI_MCR_SetNpc(&wk->room, status->trainerView, wk->index2No[i]);

            if (obj != NULL) {
                WifiP2PMatch_ViewSetFriendState(wk, wk->index2No[i], 2);
                WifiP2PMatch_AddMoveObj(wk, obj);
                wk->matchStatus[i] = 0;
                wk->matchGameMode[i] = 0;
                wk->matchVChat[i] = 0;
                in = TRUE;
            }
        }
    }
    if (in == TRUE) {
        GFL_SndSEPlay(SEQ_SE_FLD_05);
    }
}

static void WifiP2PMatch_UpdateFriendView(WIFIP2PMATCH_WORK *wk, HeapID heapId) {
    if (WifiP2PMatch_CheckFriendStatusChange(wk) > 0) {
        if (wk->view.detailMode != 0) {
            WifiP2PMatch_RedrawFriendDetail(wk, heapId);
        } else {
            wk->view.redrawAll = TRUE;
            WifiP2PMatch_ViewDrawButtons(wk);
        }
    }
}

// The friend list's bookkeeping of a frame
static void WifiP2PMatch_UpdateFriendList(WIFIP2PMATCH_WORK *wk, HeapID heapId) {
    WifiP2PMatch_UpdateFriendsInRoom(wk);
    WifiP2PMatch_UpdateFriendView(wk, heapId);
    WifiP2PMatch_AddWaitingFriends(wk);
}

static void WifiP2PMatch_ShowAllShadows(WIFIP2PMATCH_WORK *wk) {
    int i;

    for (i = 0; i < WIFIP2PMATCH_OBJ_MAX; i++) {
        if (wk->moveObj[i] != NULL) {
            WIFI_MCR_ShadowOn(&wk->room, wk->moveObj[i]);
        }
    }
}

// The room: the player walks around while the friend list is kept up to date and friends may connect
static int WifiP2PMatch_StateRoom(WIFIP2PMATCH_WORK *wk, int seq) {
    int i;
    u32 status;
    u32 ret = WIFI_MCR_RET_NONE;

    wk->unk1356 = 0;
    if (!GFL_WipeIsFinished()) {
        return seq;
    }
    if (NetState_IsWifiBattleError()) {
        WifiP2PMatch_OnNetError(wk);
        return seq;
    }
    if (wk->preConnect != -1) {
        wk->unk1C8++;
        if (wk->unk1C8 > 60 * 60) {
            NetState_RecordError(0, 0, 0, 0x3f1);
            return seq;
        }
    }
    if (NetState_GetWifiStatus() == 4) {
        WifiP2PMatch_SetFriendNameWord(wk, DWCRap_GetFriendIndex());
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_FailedRespond, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_CONNECT_ERROR;
        WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
        NetState_WifiDisconnect(TRUE);
        return seq;
    }
    WifiP2PMatch_UpdateFriendList(wk, WIFIP2PMATCH_HEAP);
    WifiP2PMatch_ViewRedrawButtons(wk);
    WIFI_MCR_PCAnmOff(&wk->room);
    if (!WIFI_MCR_PlayerMovePauseGet(&wk->room)) {
        u32 friendNo;
        MCR_MOVEOBJ *obj;

        if (DWCRap_GetLastNetId() == -1 && (GCTX_HIDGetPressedKeys() & PAD_BUTTON_X)) {
            if (WifiP2PMatch_ToggleVChat(wk)) {
                WifiP2PMatch_PrintMessageNow(wk, WifiP2pmatch_Text_VoiceChatTurned, FALSE);
            } else {
                WifiP2PMatch_PrintMessageNow(wk, WifiP2pmatch_Text_VoiceChatTurnedOff, FALSE);
            }
            GFL_SndSEPlay(SEQ_SE_DECIDE1);
            WifiP2PMatch_DrawMyInfo(wk);
            wk->seq = WIFIP2PMATCH_SEQ_VCHAT_TOGGLE_KEY_WAIT;
            WifiP2PMatch_ViewCloseDetailIfOpen(wk, WIFIP2PMATCH_HEAP);
            return seq;
        }
        friendNo = WifiP2PMatch_ViewMain(wk, WIFIP2PMATCH_HEAP);
        obj = WifiP2PMatch_GetMoveObj(wk, friendNo);
        if (friendNo != 0) {
            if (obj != NULL) {
                WIFI_MCR_CursorOn(&wk->room, obj);
            } else {
                WifiP2PMatch_ShowAllShadows(wk);
                WIFI_MCR_CursorOff(&wk->room);
            }
        } else {
            WifiP2PMatch_ShowAllShadows(wk);
            WIFI_MCR_CursorOff(&wk->room);
        }
    }
    if (wk->unk135A != 0xff) {
        // A friend who set the player as the voice chat partner is calling
        for (i = 0; i < WIFIP2PMATCH_MEMBER_MAX; i++) {
            u32 friendIndex = GFL_RandomLCAlt(WIFIP2PMATCH_MEMBER_MAX);

            if (WIFI_STATUS_IsVChatMac(wk->myStatus, WifiP2PMatch_GetFriendStatusData(friendIndex))) {
                u32 callCounter = WIFI_STATUS_GetCallCounter(WifiP2PMatch_GetFriendStatusData(friendIndex));

                if (callCounter != wk->parent->callCounter[friendIndex]) {
                    WifiP2PMatch_GetStatus(wk, WifiP2PMatch_GetFriendStatusData(friendIndex));
                    WifiP2PMatch_GetGameMode(wk, WifiP2PMatch_GetFriendStatusData(friendIndex));
                    wk->friendNo = friendIndex + 1;
                    GFL_SEPlayKeepVol(SEQ_SE_SYS_101, 0);
                    wk->seq = WIFIP2PMATCH_SEQ_VCHAT_CALL_START;
                    return seq;
                }
            }
        }
    }
    if (WifiP2PMatch_IsConnected(wk) && wk->preConnect != -1) {
        GFL_SEPlayKeepVol(SEQ_SE_SYS_101, 0);
        func_02042e94(1);
        GFL_NetSetDisconnectError(TRUE);
        wk->seq = WIFIP2PMATCH_SEQ_FRIEND_CONNECTED;
        return seq;
    }
    if (wk->preConnect == -1 && DWCRap_GetLastNetId() != -1) {
        wk->preConnect = DWCRap_GetLastNetId();
        WifiP2PMatch_SetFriendNameWord(wk, wk->preConnect);
        if (wk->unk1351 == 0) {
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_WantsJoin, FALSE);
        }
        wk->unk1C8 = 0;
    } else if (wk->preConnect != -1 && DWCRap_GetLastNetId() == -1) {
        DWCRap_SetVoiceChatEnabled(0);
        WIFI_STATUS_SetUseVChat(wk->myStatus, WIFI_STATUS_GetVChatStatus(wk->myStatus));
        WifiP2PMatch_SetFriendNameWord(wk, wk->preConnect);
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_FailedRespond, FALSE);
        NetState_WifiDisconnect(TRUE);
        wk->seq = WIFIP2PMATCH_SEQ_CONNECT_FAILED;
        return seq;
    }
    if (WifiP2PMatch_IsConnectFailed(wk) && wk->preConnect != -1) {
        WifiP2PMatch_SetFriendNameWord(wk, DWCRap_GetLastIndex());
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_FailedRespond, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_CONNECT_FAILED;
        return seq;
    }
    if (DWCRap_IsVoiceChatEnabled() == 1 && DWCRap_GetLastNetId() == -1) {
        DWCRap_SetVoiceChatEnabled(0);
        WIFI_STATUS_SetUseVChat(wk->myStatus, WIFI_STATUS_GetVChatStatus(wk->myStatus));
    }

    status = WifiP2PMatch_GetStatus(wk, wk->myStatus);
    if (wk->preConnect == -1) {
        if ((GCTX_HIDGetPressedKeys() & PAD_BUTTON_B) && WifiP2PMatch_IsRecruitStatus(status)) {
            GFL_SndSEPlay(SEQ_SE_DECIDE1);
            wk->seq = WIFIP2PMATCH_SEQ_RECRUIT_STOP_ASK;
            WifiP2PMatch_ViewCloseDetail(wk, WIFIP2PMATCH_HEAP);
            return seq;
        }
        ret = WIFI_MCR_Main(&wk->room);
        WIFI_MCR_PCAnmMain(&wk->room);
    }
    switch (ret) {
    case WIFI_MCR_RET_NONE:
        return seq;
    case WIFI_MCR_RET_CANCEL:
        GFL_SndSEPlay(SEQ_SE_DECIDE1);
        if (WifiP2PMatch_IsRecruitStatus(status)) {
            wk->seq = WIFIP2PMATCH_SEQ_RECRUIT_STOP_ASK;
        } else {
            wk->endSeq = 0;
            wk->seq = WIFIP2PMATCH_SEQ_EXIT_YES_NO_INIT;
            WifiP2PMatch_PrintMessage(wk, WifiError_Text_OkDisconnectFromNintendo, TRUE);
        }
        WifiP2PMatch_ViewCloseDetailIfOpen(wk, WIFIP2PMATCH_HEAP);
        return seq;
    case WIFI_MCR_RET_MYSELECT:
        if (status == WIFIP2PMATCH_STATUS_WAIT) {
            wk->result = 1;
            GFL_SndSEPlay(SEQ_SE_DECIDE1);
            WIFI_MCR_PCAnmStart(&wk->room);
            wk->seq = WIFIP2PMATCH_SEQ_RECRUIT_MENU_START;
            WifiP2PMatch_ViewCloseDetailIfOpen(wk, WIFIP2PMATCH_HEAP);
            wk->unk1351 = 0;
            return seq;
        }
        break;
    case WIFI_MCR_RET_SELECT:
        if (status == WIFIP2PMATCH_STATUS_WAIT) {
            u32 friendNo = WIFI_MCR_PlayerSelect(&wk->room);

            GFL_SndSEPlay(SEQ_SE_MESSAGE);
            MI_CpuCopy8(WifiP2PMatch_GetFriendStatusData(friendNo - 1), &wk->targetStatus, sizeof(WIFI_STATUS));
            wk->seq = WIFIP2PMATCH_SEQ_FRIEND_SELECT;
            WifiP2PMatch_ViewCloseDetailIfOpen(wk, WIFIP2PMATCH_HEAP);
            wk->unk135A = 1;
            return seq;
        }
        break;
    }
    return seq;
}

// The room while a message is up: as WifiP2PMatch_StateRoom, without the friend page's touches and the voice chat calls
static int WifiP2PMatch_StateRoomMessage(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 status;
    u32 ret = WIFI_MCR_RET_NONE;

    if (wk->preConnect != -1) {
        wk->unk1C8++;
        if (wk->unk1C8 > 60 * 60) {
            NetState_RecordError(0, 0, 0, 0x3f1);
            return seq;
        }
    }
    WifiP2PMatch_IsMessageEnd(wk);
    wk->unk1356 = 0;
    WifiP2PMatch_UpdateFriendList(wk, WIFIP2PMATCH_HEAP);
    WifiP2PMatch_ViewRedrawButtons(wk);
    WIFI_MCR_PCAnmOff(&wk->room);
    if (WifiP2PMatch_IsConnected(wk) && wk->preConnect != -1) {
        GFL_SEPlayKeepVol(SEQ_SE_SYS_101, 0);
        func_02042e94(1);
        GFL_NetSetDisconnectError(TRUE);
        wk->seq = WIFIP2PMATCH_SEQ_FRIEND_CONNECTED;
        return seq;
    }
    if (WifiP2PMatch_IsConnectFailed(wk) && wk->preConnect != -1) {
        WifiP2PMatch_SetFriendNameWord(wk, DWCRap_GetLastIndex());
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_FailedRespond, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_CONNECT_FAILED;
        return seq;
    }
    if (wk->preConnect == -1 && DWCRap_GetLastNetId() != -1) {
        wk->preConnect = DWCRap_GetLastNetId();
        WifiP2PMatch_SetFriendNameWord(wk, wk->preConnect);
        if (wk->unk1351 == 0) {
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_WantsJoin, FALSE);
        }
        wk->unk1C8 = 0;
    } else if (DWCRap_GetLastIndex() != -1) {
        WifiP2PMatch_SetFriendNameWord(wk, DWCRap_GetLastIndex());
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_FailedRespond, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_CONNECT_FAILED;
        return seq;
    }

    status = WifiP2PMatch_GetStatus(wk, wk->myStatus);
    if (wk->preConnect == -1) {
        if ((GCTX_HIDGetPressedKeys() & PAD_BUTTON_B) && WifiP2PMatch_IsRecruitStatus(status)) {
            GFL_SndSEPlay(SEQ_SE_DECIDE1);
            wk->seq = WIFIP2PMATCH_SEQ_RECRUIT_STOP_ASK;
            WifiP2PMatch_ViewCloseDetailIfOpen(wk, WIFIP2PMATCH_HEAP);
            return seq;
        }
        ret = WIFI_MCR_Main(&wk->room);
        WIFI_MCR_PCAnmMain(&wk->room);
    }
    switch (ret) {
    case WIFI_MCR_RET_NONE:
        return seq;
    case WIFI_MCR_RET_CANCEL:
        GFL_SndSEPlay(SEQ_SE_DECIDE1);
        if (WifiP2PMatch_IsRecruitStatus(status)) {
            wk->seq = WIFIP2PMATCH_SEQ_RECRUIT_STOP_ASK;
        } else {
            wk->endSeq = 0;
            wk->seq = WIFIP2PMATCH_SEQ_EXIT_YES_NO_INIT;
            WifiP2PMatch_PrintMessage(wk, WifiError_Text_OkDisconnectFromNintendo, TRUE);
        }
        WifiP2PMatch_ViewCloseDetailIfOpen(wk, WIFIP2PMATCH_HEAP);
        return seq;
    case WIFI_MCR_RET_MYSELECT:
        if (status == WIFIP2PMATCH_STATUS_WAIT) {
            wk->result = 1;
            GFL_SndSEPlay(SEQ_SE_DECIDE1);
            WIFI_MCR_PCAnmStart(&wk->room);
            wk->seq = WIFIP2PMATCH_SEQ_RECRUIT_MENU_START;
            WifiP2PMatch_ViewCloseDetailIfOpen(wk, WIFIP2PMATCH_HEAP);
            return seq;
        }
        break;
    case WIFI_MCR_RET_SELECT:
        if (status == WIFIP2PMATCH_STATUS_WAIT) {
            u32 friendNo = WIFI_MCR_PlayerSelect(&wk->room);

            GFL_SndSEPlay(SEQ_SE_MESSAGE);
            MI_CpuCopy8(WifiP2PMatch_GetFriendStatusData(friendNo - 1), &wk->targetStatus, sizeof(WIFI_STATUS));
            wk->seq = WIFIP2PMATCH_SEQ_FRIEND_SELECT;
            WifiP2PMatch_ViewCloseDetailIfOpen(wk, WIFIP2PMATCH_HEAP);
            wk->unk135A = 1;
            return seq;
        }
        break;
    }
    return seq;
}

// Calls the friend who set the player as the voice chat partner
static int WifiP2PMatch_StateVChatCallStart(WIFIP2PMATCH_WORK *wk, int seq) {
    int friendIndex = wk->friendNo - 1;

    if (GFL_SndPlayerIsActive(0)) {
        return seq;
    }
    wk->parent->callCounter[friendIndex] = WIFI_STATUS_GetCallCounter(WifiP2PMatch_GetFriendStatusData(friendIndex));
    if (friendIndex >= 0 && friendIndex < WIFIP2PMATCH_MEMBER_MAX && WifiP2PMatch_StartConnect(wk, friendIndex)) {
        wk->callTimer = 60 * 60;
        WifiP2PMatch_ChangeNetStatus(wk, WIFIP2PMATCH_STATUS_PLAYING, 3);
        WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_PLAYING, 3);
        WifiP2PMatch_ViewCloseDetail(wk, WIFIP2PMATCH_HEAP);
        WifiP2PMatch_SetFriendNameWord(wk, friendIndex);
        wk->callTimer = 60 * 60;
        WifiP2PMatch_PrintMessageNow(wk, WifiP2pmatch_Text_HasContacted, FALSE);
        WifiP2PMatch_StartWaitIcon(wk);
        wk->seq = WIFIP2PMATCH_SEQ_JOIN_WAIT;
        wk->unk1350 = wk->friendNo;
        return seq;
    }
    wk->seq = WIFIP2PMATCH_SEQ_ROOM;
    return seq;
}

static int WifiP2PMatch_StateVChatToggleKeyWait(WIFIP2PMATCH_WORK *wk, int seq) {
    if (GCTX_HIDGetPressedKeys()) {
        wk->seq = WIFIP2PMATCH_SEQ_ROOM;
        WifiP2PMatch_RoomResume(wk);
        WifiP2PMatch_EndMessage(wk);
    }
    return seq;
}

static int WifiP2PMatch_StateVChatCallWait(WIFIP2PMATCH_WORK *wk, int seq) {
    WIFI_STATUS *status = WifiP2PMatch_GetFriendStatusData(wk->friendNo - 1);
    u32 wifiStatus;

    WIFI_STATUS_GetVChatStatus(status);
    wifiStatus = WifiP2PMatch_GetStatus(wk, status);
    wk->callTimer--;
    if (wk->callTimer < 0) {
        wk->seq = WIFIP2PMATCH_SEQ_JOIN_TIMEOUT;
    } else if (wifiStatus != WIFIP2PMATCH_STATUS_WAIT && wifiStatus != WIFIP2PMATCH_STATUS_RECRUIT) {
        WifiP2PMatch_SetFriendNameWord(wk, DWCRap_GetFriendIndex());
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_CantConnect, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_CONNECT_FAILED;
    } else if (NetState_GetWifiStatus() == 1) {
        WifiP2PMatch_EndMessage(wk);
        WIFI_STATUS_ResetVChatMac(wk->myStatus);
        wk->unk1351 = 0;
        WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_PLAYING, 4);
        wk->unk1355 = 0;
        wk->seq = WIFIP2PMATCH_SEQ_VCHAT_START;
        WifiP2PMatch_BgmVolumeFadeStart(wk, 0);
        WifiP2PMatch_PrintMessageNow(wk, WifiP2pmatch_Text_PressBButtonEnd, FALSE);
    }
    return seq;
}

static int WifiP2PMatch_StateVChatInvited(WIFIP2PMATCH_WORK *wk, int seq) {
    WifiP2PMatch_SetFriendNameWord(wk, DWCRap_GetFriendIndex());
    WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_InvitesVoiceChat, FALSE);
    WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_PLAYING, 4);
    wk->seq = WIFIP2PMATCH_SEQ_VCHAT_INVITED_WAIT;
    return seq;
}

static int WifiP2PMatch_StateVChatInvitedWait(WIFIP2PMATCH_WORK *wk, int seq) {
    if (NetState_IsWifiBattleError()) {
        WifiP2PMatch_OnNetError(wk);
        return seq;
    }
    if (DWCRap_GetFriendIndex() < 0) {
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_EndedVoiceChat, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_CONNECT_FAILED;
        return seq;
    }
    if (WifiP2PMatch_IsMessageEnd(wk)) {
        if (GCTX_HIDGetPressedKeys() & (PAD_BUTTON_A | PAD_BUTTON_B)) {
            WifiP2PMatch_EndMessage(wk);
            WifiP2PMatch_PrintMessageNow(wk, WifiP2pmatch_Text_PressBButtonEnd, FALSE);
            WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_PLAYING, 4);
            WifiP2PMatch_BgmVolumeFadeStart(wk, 0);
            wk->unk1355 = 0;
            wk->seq = WIFIP2PMATCH_SEQ_VCHAT_START;
        } else {
            WifiP2PMatch_StateVChatStart(wk, seq);
        }
    }
    return seq;
}

// The voice chat starts
static int WifiP2PMatch_StateVChatStart(WIFIP2PMATCH_WORK *wk, int seq) {
    NetHandle_SendTimingPair(NetHandle_GetOwn(), 0x13, 10);
    WifiList_SetFriendLastDateToNow(wk->wifiList, DWCRap_GetFriendIndex());
    wk->seq = WIFIP2PMATCH_SEQ_VCHAT;
    return seq;
}

// Shows whether the friend is talking
static void WifiP2PMatch_DrawVChatTalkIcon(WIFIP2PMATCH_WORK *wk) {
    if (DWCRap_GetVoiceChatSpeaking()) {
        WifiP2PMatch_IconWrite(&wk->icon, 1, 2, 1, 0x3c, 0);
    } else {
        WifiP2PMatch_IconWrite(&wk->icon, 1, 2, 1, 0x33, 0);
    }
}

// The voice chat
static int WifiP2PMatch_StateVChat(WIFIP2PMATCH_WORK *wk, int seq) {
    WIFI_STATUS *status = WifiP2PMatch_GetFriendStatusData(DWCRap_GetFriendIndex());

    WifiP2PMatch_GetStatus(wk, status);
    WifiP2PMatch_GetGameMode(wk, status);
    WifiP2PMatch_UpdateFriendList(wk, WIFIP2PMATCH_HEAP);
    WifiP2PMatch_ViewRedrawButtons(wk);
    if (GCTX_HIDGetPressedKeys() & PAD_BUTTON_B) {
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_WouldLikeEndVoice, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_VCHAT_END_YES_NO_START;
    } else if (wk->unk1355 != 0) {
        WifiP2PMatch_EndMessage(wk);
        WifiP2PMatch_SetFriendNameWord(wk, DWCRap_GetFriendIndex());
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_EndedVoiceChat, FALSE);
        wk->timer = 60;
        wk->seq = WIFIP2PMATCH_SEQ_VCHAT_ENDED_MESSAGE;
    } else {
        WifiP2PMatch_DrawVChatTalkIcon(wk);
    }
    return seq;
}

static int WifiP2PMatch_StateVChatEndYesNoStart(WIFIP2PMATCH_WORK *wk, int seq) {
    WifiP2PMatch_DrawVChatTalkIcon(wk);
    if (WifiP2PMatch_IsMessageEnd(wk)) {
        if (wk->unk1355 != 0) {
            WifiP2PMatch_EndMessage(wk);
            WifiP2PMatch_SetFriendNameWord(wk, DWCRap_GetFriendIndex());
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_EndedVoiceChat, FALSE);
            wk->timer = 60;
            wk->seq = WIFIP2PMATCH_SEQ_VCHAT_ENDED_MESSAGE;
        } else {
            WifiP2PMatch_YesNoStart(wk);
            wk->seq = WIFIP2PMATCH_SEQ_VCHAT_END_YES_NO;
        }
    }
    return seq;
}

static int WifiP2PMatch_StateVChatEndYesNo(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 ret;

    WifiP2PMatch_DrawVChatTalkIcon(wk);
    ret = WifiP2PMatch_YesNoMain(wk);
    if (ret == BMPMENU_NULL) {
        if (wk->unk1355 != 0) {
            WifiP2PMatch_EndMessage(wk);
            ConfirmDialog_Free(wk->yesNoMenu);
            wk->yesNoMenu = NULL;
            WifiP2PMatch_SetFriendNameWord(wk, DWCRap_GetFriendIndex());
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_EndedVoiceChat, FALSE);
            wk->timer = 60;
            wk->seq = WIFIP2PMATCH_SEQ_VCHAT_ENDED_MESSAGE;
        }
    } else if (ret == 0) {
        wk->seq = WIFIP2PMATCH_SEQ_VCHAT_END_SEND;
    } else {
        WifiP2PMatch_PrintMessageNow(wk, WifiP2pmatch_Text_PressBButtonEnd, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_VCHAT_START;
    }
    return seq;
}

// Tells the friend that the voice chat ends
static int WifiP2PMatch_StateVChatEndSend(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    if (GFL_NetSendCommand(NetHandle_GetOwn(), 0xa08, 0, NULL)) {
        NetHandle_SendTimingPair(NetHandle_GetOwn(), 0x12, 10);
        wk->seq = WIFIP2PMATCH_SEQ_VCHAT_END_TIMING;
    }
    return seq;
}

static int WifiP2PMatch_StateVChatStopWait(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!DWCRap_IsVoiceChatActive()) {
        NetHandle_SendTimingPair(NetHandle_GetOwn(), 0x18, 10);
        wk->seq = WIFIP2PMATCH_SEQ_RETURN_TIMING_WAIT;
    }
    return seq;
}

static int WifiP2PMatch_StateVChatEndTiming(WIFIP2PMATCH_WORK *wk, int seq) {
    wk->unk1355 = 0;
    if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 0x12, 10) || NetHandle_CountNegotiated() != 2) {
        if (wk->result == 3 || wk->result == 4) {
            DWCRap_StopVoiceChat();
            WifiP2PMatch_BgmVolumeFadeStart(wk, 0x7f);
            WifiP2PMatch_EndMessage(wk);
            WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_CALL, 3);
            wk->seq = WIFIP2PMATCH_SEQ_VCHAT_STOP_WAIT;
        } else {
            WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
            func_02042e94(0);
            GFL_NetSetDisconnectError(FALSE);
            NetHandle_SendTimingPair(NetHandle_GetOwn(), 0x15, 10);
            wk->seq = WIFIP2PMATCH_SEQ_VCHAT_DISCONNECT_TIMING;
        }
    }
    return seq;
}

static int WifiP2PMatch_StateVChatDisconnectTiming(WIFIP2PMATCH_WORK *wk, int seq) {
    if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 0x15, 10) || NetHandle_CountNegotiated() != 2) {
        NetState_WifiDisconnect(TRUE);
        wk->preConnect = -1;
        wk->timer = 20;
        WifiP2PMatch_EndMessage(wk);
        wk->seq = WIFIP2PMATCH_SEQ_DISCONNECT_WAIT;
    }
    return seq;
}

static int WifiP2PMatch_StateVChatEndedMessage(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    wk->timer--;
    if (wk->timer < 0) {
        NetHandle_SendTimingPair(NetHandle_GetOwn(), 0x12, 10);
        wk->seq = WIFIP2PMATCH_SEQ_VCHAT_END_TIMING;
    }
    return seq;
}

// The connection failed or was called off: shows the message, then goes back to the room
static int WifiP2PMatch_StateConnectFailed(WIFIP2PMATCH_WORK *wk, int seq) {
    if (NetState_IsWifiBattleError()) {
        WifiP2PMatch_OnNetError(wk);
        return seq;
    }
    if (WifiP2PMatch_GetStatus(wk, wk->myStatus) != WIFIP2PMATCH_STATUS_WAIT) {
        WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
    }
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        wk->timer = 30;
        return seq;
    }
    wk->timer--;
    if ((GCTX_HIDGetPressedKeys() & (PAD_BUTTON_A | PAD_BUTTON_B)) || wk->timer == 0) {
        WifiP2PMatch_EndMessage(wk);
        NetState_WifiDisconnect(TRUE);
        WifiP2PMatch_NetInit(wk, FALSE);
        if (!WIFI_MCR_GetInitFlag(&wk->room)) {
            wk->timer = 20;
            wk->seq = WIFIP2PMATCH_SEQ_DISCONNECT_WAIT;
        } else {
            WifiP2PMatch_ResetVChat(wk);
            WifiP2PMatch_DrawMyInfo(wk);
            wk->preConnect = -1;
            wk->seq = WIFIP2PMATCH_SEQ_ROOM;
            WIFI_MCR_NpcPauseOff(&wk->room, WifiP2PMatch_GetMoveObj(wk, WIFI_MCR_PlayerSelect(&wk->room)));
            WifiP2PMatch_RoomResume(wk);
        }
    }
    return seq;
}

static int WifiP2PMatch_StateConnectFailedKeyWait(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    if (GCTX_HIDGetPressedKeys() & (PAD_BUTTON_A | PAD_BUTTON_B)) {
        WifiP2PMatch_EndMessage(wk);
        WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
        NetState_WifiDisconnect(TRUE);
        wk->timer = 20;
        wk->seq = WIFIP2PMATCH_SEQ_DISCONNECT_WAIT;
    }
    return seq;
}

static int WifiP2PMatch_StateCutConnection(WIFIP2PMATCH_WORK *wk, int seq) {
    wk->timer++;
    if (wk->timer <= 240) {
        return seq;
    }
    WIFI_MCR_NpcPauseOff(&wk->room, WifiP2PMatch_GetMoveObj(wk, WIFI_MCR_PlayerSelect(&wk->room)));
    WifiP2PMatch_EndMessage(wk);
    WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
    NetState_WifiDisconnect(TRUE);
    WifiP2PMatch_NetInit(wk, FALSE);
    wk->unk135A = 0;
    if (!WIFI_MCR_GetInitFlag(&wk->room)) {
        wk->seq = WifiP2PMatch_LeaveConnection(wk, FALSE);
    } else {
        WifiP2PMatch_ResetVChat(wk);
        WifiP2PMatch_DrawMyInfo(wk);
        wk->preConnect = -1;
        wk->seq = WIFIP2PMATCH_SEQ_ROOM;
        WifiP2PMatch_RoomResume(wk);
    }
    return seq;
}

static int WifiP2PMatch_StateConnectError(WIFIP2PMATCH_WORK *wk, int seq) {
    if (NetState_IsWifiBattleError()) {
        WifiP2PMatch_OnNetError(wk);
        return seq;
    }
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        wk->timer = 30;
        return seq;
    }
    wk->timer--;
    if ((GCTX_HIDGetPressedKeys() & (PAD_BUTTON_A | PAD_BUTTON_B)) || wk->timer == 0) {
        WIFI_MCR_NpcPauseOff(&wk->room, WifiP2PMatch_GetMoveObj(wk, WIFI_MCR_PlayerSelect(&wk->room)));
        WifiP2PMatch_EndMessage(wk);
        WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
        NetState_WifiDisconnect(TRUE);
        WifiP2PMatch_NetInit(wk, FALSE);
        wk->unk135A = 0;
        if (!WIFI_MCR_GetInitFlag(&wk->room)) {
            wk->seq = WifiP2PMatch_LeaveConnection(wk, FALSE);
        } else {
            WifiP2PMatch_ResetVChat(wk);
            WifiP2PMatch_DrawMyInfo(wk);
            wk->preConnect = -1;
            wk->seq = WIFIP2PMATCH_SEQ_ROOM;
            WifiP2PMatch_RoomResume(wk);
        }
    }
    return seq;
}

static int WifiP2PMatch_StateNone16(WIFIP2PMATCH_WORK *wk, int seq) {
    return seq;
}

static int WifiP2PMatch_StateNone17(WIFIP2PMATCH_WORK *wk, int seq) {
    return seq;
}

static int WifiP2PMatch_StateNone18(WIFIP2PMATCH_WORK *wk, int seq) {
    return seq;
}

static int WifiP2PMatch_StateNoFriendKeyWait(WIFIP2PMATCH_WORK *wk, int seq) {
    if (wk->timer > 0) {
        wk->timer--;
        return seq;
    }
    if (GCTX_HIDGetPressedKeys() & (PAD_BUTTON_A | PAD_BUTTON_B)) {
        WifiP2PMatch_PrintMessage(wk, WifiError_Text_DisconnectingFromNintendoWi, TRUE);
        wk->seq = WIFIP2PMATCH_SEQ_DISCONNECT;
        wk->timer = 1;
    }
    return seq;
}

// B in the room while seeking: asks whether to stop
static int WifiP2PMatch_StateRecruitStopAsk(WIFIP2PMATCH_WORK *wk, int seq) {
    WIFI_MCR_PCAnmMain(&wk->room);
    WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_SuspendInvitation, FALSE);
    wk->seq = WIFIP2PMATCH_SEQ_RECRUIT_STOP_YES_NO_START;
    return seq;
}

static int WifiP2PMatch_StateRecruitStopYesNoStart(WIFIP2PMATCH_WORK *wk, int seq) {
    WIFI_MCR_PCAnmMain(&wk->room);
    if (WifiP2PMatch_IsMessageEnd(wk)) {
        WifiP2PMatch_YesNoStart(wk);
        wk->seq = WIFIP2PMATCH_SEQ_RECRUIT_STOP_YES_NO;
    }
    return seq;
}

static int WifiP2PMatch_StateRecruitStopYesNo(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 ret = WifiP2PMatch_YesNoMain(wk);

    WIFI_MCR_PCAnmMain(&wk->room);
    if (wk->preConnect == -1 && DWCRap_GetLastNetId() != -1) {
        // A friend connected meanwhile: the answer is no
        if (ret == BMPMENU_NULL) {
            ConfirmDialog_Free(wk->yesNoMenu);
            wk->yesNoMenu = NULL;
        }
        ret = 1;
    }
    if (ret == BMPMENU_NULL) {
        return seq;
    }
    if (ret == 0) {
        WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
        NetState_WifiDisconnect(TRUE);
        WifiP2PMatch_NetInit(wk, FALSE);
        WifiP2PMatch_RoomResume(wk);
        wk->seq = WIFIP2PMATCH_SEQ_ROOM;
    } else {
        WifiP2PMatch_PrintMessageNow(wk, WifiP2pmatch_Text_SeekingParticipants, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_ROOM_MESSAGE;
    }
    return seq;
}

// A in front of a PC: the menu of what to seek
static int WifiP2PMatch_StateRecruitMenuStart(WIFIP2PMATCH_WORK *wk, int seq) {
    WifiP2PMatch_OpenGameModeMenu(wk);
    wk->seq = WIFIP2PMATCH_SEQ_RECRUIT_MENU_MESSAGE;
    WIFI_MCR_PCAnmMain(&wk->room);
    return seq;
}

static int WifiP2PMatch_StateRecruitMenuMessage(WIFIP2PMATCH_WORK *wk, int seq) {
    WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_WhatWouldLikeSeek, FALSE);
    wk->seq = WIFIP2PMATCH_SEQ_RECRUIT_MENU;
    return seq;
}

// Frees the menu of what to seek
static void WifiP2PMatch_FreeRecruitMenu(WIFIP2PMATCH_WORK *wk) {
    if (wk->listWin != NULL) {
        wk->listWin = WifiP2PMatch_FreeWindow(wk->listWin);
        BmpMenuList_Free(wk->list, NULL, &wk->menuCursor[4]);
        ListMenuCore_FreeOptionList(wk->menuOptions);
        WifiP2PMatch_EndMessage(wk);
    }
}

static int WifiP2PMatch_StateRecruitMenu(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 ret;

    countActivePkms(wk->party);
    WIFI_MCR_PCAnmMain(&wk->room);
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    ret = BmpMenuList_Update(wk->list);
    switch (ret) {
    case BMPMENULIST_NULL:
        return seq;
    case BMPMENULIST_CANCEL:
        wk->seq = WIFIP2PMATCH_SEQ_ROOM;
        WifiP2PMatch_FreeRecruitMenu(wk);
        WifiP2PMatch_RoomResume(wk);
        return seq;
    case 7:
        WifiP2PMatch_FreeRecruitMenu(wk);
        wk->battleMode = wk->parent->battleMode;
        wk->battleRule = wk->parent->battleRule;
        wk->parent->unk48 = wk->parent->unk20;
        WifiP2PMatch_OpenBattleRulesMenu(wk, TRUE);
        wk->seq = WIFIP2PMATCH_SEQ_BATTLE_SETUP_MENU;
        return seq;
    case 4:
        if (!wk->parent->unk46) {
            return seq;
        }
        WifiP2PMatch_FreeRecruitMenu(wk);
        WifiP2PMatch_StartConnect(wk, -1);
        wk->seq = WIFIP2PMATCH_SEQ_ROOM_MESSAGE;
        break;
    case 6:
        if (!WifiP2PMatch_HasTwoPokemon(wk)) {
            wk->listWin = WifiP2PMatch_FreeWindow(wk->listWin);
            BmpMenuList_Free(wk->list, NULL, &wk->menuCursor[4]);
            ListMenuCore_FreeOptionList(wk->menuOptions);
            WifiP2PMatch_EndMessage(wk);
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_NeedLeastTwoPokemon, FALSE);
            wk->seq = WIFIP2PMATCH_SEQ_MESSAGE_KEY_WAIT_TO_ROOM;
            return seq;
        }
        WifiP2PMatch_FreeRecruitMenu(wk);
        WifiP2PMatch_StartConnect(wk, -1);
        wk->seq = WIFIP2PMATCH_SEQ_ROOM_MESSAGE;
        break;
    case 5:
        if (!wk->parent->unk46) {
            return seq;
        }
        if (!canPlayerExchangePhotos() && isRunningOnDSi()) {
            WifiP2PMatch_FreeRecruitMenu(wk);
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_UsesCameraFunctionSend, FALSE);
            wk->seq = WIFIP2PMATCH_SEQ_CAMERA_NOTICE_YES_NO_START;
            return seq;
        }
        WifiP2PMatch_FreeRecruitMenu(wk);
        WifiP2PMatch_StartConnect(wk, -1);
        wk->seq = WIFIP2PMATCH_SEQ_ROOM_MESSAGE;
        break;
    }
    WifiP2PMatch_RoomPause(wk, TRUE);
    wk->timer = 0;
    wk->unk246 = ret;
    wk->seq = WIFIP2PMATCH_SEQ_RECRUIT_START;
    return seq;
}

static int WifiP2PMatch_StateRecruitStart(WIFIP2PMATCH_WORK *wk, int seq) {
    wk->timer--;
    if (wk->timer < 0) {
        WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_RECRUIT, wk->unk246);
        wk->seq = WIFIP2PMATCH_SEQ_ROOM_MESSAGE;
    }
    return seq;
}

static int WifiP2PMatch_StateCameraNoticeYesNoStart(WIFIP2PMATCH_WORK *wk, int seq) {
    WIFI_MCR_PCAnmMain(&wk->room);
    if (WifiP2PMatch_IsMessageEnd(wk)) {
        WifiP2PMatch_YesNoStart(wk);
        wk->seq = WIFIP2PMATCH_SEQ_CAMERA_NOTICE_YES_NO;
    }
    return seq;
}

static int WifiP2PMatch_StateCameraNoticeYesNo(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 ret = WifiP2PMatch_YesNoMain(wk);

    WIFI_MCR_PCAnmMain(&wk->room);
    if (ret == BMPMENU_NULL) {
        return seq;
    }
    if (ret == 0) {
        WifiP2PMatch_StartConnect(wk, -1);
        WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_RECRUIT, 5);
        WifiP2PMatch_RoomPause(wk, TRUE);
        wk->seq = WIFIP2PMATCH_SEQ_ROOM_MESSAGE;
    } else {
        WifiP2PMatch_RoomResume(wk);
        WifiP2PMatch_EndMessage(wk);
        wk->seq = WIFIP2PMATCH_SEQ_ROOM;
    }
    return seq;
}

static int WifiP2PMatch_StateConnectFriendYesNoStart(WIFIP2PMATCH_WORK *wk, int seq) {
    WIFI_MCR_PCAnmMain(&wk->room);
    if (WifiP2PMatch_IsMessageEnd(wk)) {
        WifiP2PMatch_YesNoStart(wk);
        wk->seq = WIFIP2PMATCH_SEQ_CONNECT_FRIEND_YES_NO;
    }
    return seq;
}

static int WifiP2PMatch_StateConnectFriendYesNo(WIFIP2PMATCH_WORK *wk, int seq) {
    WIFI_STATUS *targetStatus = &wk->targetStatus;
    u32 friendNo = WIFI_MCR_PlayerSelect(&wk->room);
    u32 ret = WifiP2PMatch_YesNoMain(wk);
    MCR_MOVEOBJ *obj = WifiP2PMatch_GetMoveObj(wk, friendNo);

    WIFI_MCR_PCAnmMain(&wk->room);
    if (ret == BMPMENU_NULL) {
        return seq;
    }
    if (ret == 0) {
        wk->result = 2;
        if (WIFI_STATUS_GetVChatStatus(targetStatus) == WIFI_STATUS_GetVChatStatus(wk->myStatus)) {
            wk->seq = WIFIP2PMATCH_SEQ_JOIN_FRIEND;
        } else {
            if (WIFI_STATUS_GetVChatStatus(wk->myStatus)) {
                WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_VoiceChatWillTurned_2, FALSE);
            } else {
                WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_VoiceChatWillTurned, FALSE);
            }
            wk->seq = WIFIP2PMATCH_SEQ_VCHAT_CHANGE_YES_NO_INIT;
        }
    } else {
        WifiP2PMatch_EndMessage(wk);
        WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
        wk->preConnect = -1;
        WifiP2PMatch_RoomResume(wk);
        wk->seq = WIFIP2PMATCH_SEQ_ROOM;
        if (obj != NULL) {
            WIFI_MCR_NpcPauseOff(&wk->room, obj);
        }
    }
    return seq;
}

static int WifiP2PMatch_StateMessageKeyWaitToRoom(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    if (GCTX_HIDGetPressedKeys()) {
        WifiP2PMatch_EndMessage(wk);
        NetState_WifiDisconnect(TRUE);
        WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
        wk->preConnect = -1;
        WifiP2PMatch_RoomResume(wk);
        wk->seq = WIFIP2PMATCH_SEQ_ROOM;
    }
    return seq;
}

static int WifiP2PMatch_StateBackToRoom(WIFIP2PMATCH_WORK *wk, int seq) {
    WifiP2PMatch_EndMessage(wk);
    NetState_WifiDisconnect(TRUE);
    WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
    wk->preConnect = -1;
    WifiP2PMatch_RoomResume(wk);
    wk->seq = WIFIP2PMATCH_SEQ_ROOM;
    return seq;
}

static int WifiP2PMatch_StateRecruitMenuOld(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 ret;

    WIFI_MCR_PCAnmMain(&wk->room);
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    ret = BmpMenuList_Update(wk->list);
    switch (ret) {
    case BMPMENULIST_NULL:
        return seq;
    case BMPMENULIST_CANCEL:
        GFL_SndSEPlay(SEQ_SE_DECIDE1);
        wk->seq = WIFIP2PMATCH_SEQ_RECRUIT_MENU_START;
        break;
    default:
        GFL_SndSEPlay(SEQ_SE_DECIDE1);
        WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_RECRUIT, ret);
        wk->seq = WIFIP2PMATCH_SEQ_ROOM;
        WifiP2PMatch_EndMessage(wk);
        break;
    }
    if (wk->seq == WIFIP2PMATCH_SEQ_ROOM) {
        WifiP2PMatch_StartConnect(wk, -1);
        WifiP2PMatch_RoomPause(wk, TRUE);
    }
    wk->listWin = WifiP2PMatch_FreeWindow(wk->listWin);
    BmpMenuList_Free(wk->list, NULL, &wk->menuCursor[wk->menuCursorType]);
    ListMenuCore_FreeOptionList(wk->menuOptions);
    return seq;
}

// A in front of a friend: makes the friend face the player and says what the friend is doing, or calls the friend
static int WifiP2PMatch_StateFriendSelect(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 friendNo;
    MCR_MOVEOBJ *obj;
    u16 status, gameMode;
    u32 msgId;
    int game = 0;
    WIFI_STATUS *targetStatus = &wk->targetStatus;

    GFL_NetSetAcceptConnect(NetHandle_GetOwn(), 0);
    friendNo = WIFI_MCR_PlayerSelect(&wk->room);
    obj = WifiP2PMatch_GetMoveObj(wk, friendNo);
    WIFI_MCR_NpcPauseOn(&wk->room, obj, WIFI_MCR_GetRetWay(WifiP2PMatch_GetMoveObj(wk, 0)));
    status = WifiP2PMatch_GetStatus(wk, targetStatus);
    gameMode = WifiP2PMatch_GetGameMode(wk, targetStatus);
    WifiP2PMatch_SetFriendNameWord(wk, friendNo - 1);
    if (gameMode == 6) {
        game = 1;
    } else if (gameMode == 5) {
        game = 2;
    } else if (gameMode == 4) {
        game = 3;
    } else if (WifiP2PMatch_IsBattleGameMode(gameMode)) {
        game = 4;
    }
    switch (status) {
    case WIFIP2PMATCH_STATUS_NONE:
        msgId = WifiP2pmatch_Text_SeemsBusy;
        break;
    case WIFIP2PMATCH_STATUS_WAIT:
        wk->seq = WIFIP2PMATCH_SEQ_ROOM;
        WifiP2PMatch_CallFriend(wk);
        return seq;
    case WIFIP2PMATCH_STATUS_RECRUIT: {
        u32 msgIds[] = {
            WifiP2pmatch_Text_SeemsBusy,
            WifiP2pmatch_Text_SeekingSomeoneTrade,
            WifiP2pmatch_Text_LookingSomeoneChatXtransceiver,
            WifiP2pmatch_Text_WantsHaveVoiceChat,
            WifiP2pmatch_Text_SeekingSomeoneBattle,
        };

        msgId = msgIds[game];
        break;
    }
    case WIFIP2PMATCH_STATUS_PLAYING: {
        u32 msgIds[] = {
            WifiP2pmatch_Text_SeemsBusy,   WifiP2pmatch_Text_Trading,  WifiP2pmatch_Text_UsingXtransceiver,
            WifiP2pmatch_Text_VoiceChat_2, WifiP2pmatch_Text_Battle_2,
        };

        msgId = msgIds[game];
        break;
    }
    case 5:
        msgId = WifiP2pmatch_Text_SeemsBusy;
        break;
    default:
        msgId = WifiP2pmatch_Text_SeemsBusy;
        break;
    }
    WifiP2PMatch_PrintMessage(wk, msgId, FALSE);
    wk->seq = WIFIP2PMATCH_SEQ_FRIEND_MENU_INIT;
    return seq;
}

// State 0x23: the message about the friend is done: opens the friend menu if the friend can still be joined
static int WifiP2PMatch_StateFriendMenuInit(WIFIP2PMATCH_WORK *wk, int seq) {
    WIFI_STATUS *targetStatus = &wk->targetStatus;
    MCR_MOVEOBJ *obj;
    u32 status;
    u32 gameMode;

    if (NetState_IsWifiBattleError()) {
        WifiP2PMatch_OnNetError(wk);
        return seq;
    }
    obj = WifiP2PMatch_GetMoveObj(wk, WIFI_MCR_PlayerSelect(&wk->room));
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    status = WifiP2PMatch_GetStatus(wk, targetStatus);
    gameMode = WifiP2PMatch_GetGameMode(wk, targetStatus);
    switch (status) {
    case WIFIP2PMATCH_STATUS_WAIT:
        WifiP2PMatch_OpenFriendMenu(wk);
        wk->seq = WIFIP2PMATCH_SEQ_WAITING_FRIEND_MENU;
        break;
    case WIFIP2PMATCH_STATUS_RECRUIT:
    case 5:
        if (WifiP2PMatch_IsPlayGameMode(gameMode)) {
            WifiP2PMatch_OpenFriendMenu(wk);
            wk->seq = WIFIP2PMATCH_SEQ_RECRUITING_FRIEND_MENU;
            break;
        }
        // fallthrough
    case WIFIP2PMATCH_STATUS_NONE:
    case WIFIP2PMATCH_STATUS_CALL:
    case WIFIP2PMATCH_STATUS_PLAYING:
    default:
        WIFI_MCR_NpcPauseOff(&wk->room, obj);
        wk->seq = WIFIP2PMATCH_SEQ_MESSAGE_KEY_WAIT_TO_ROOM;
        break;
    }
    return seq;
}

// Calls the friend the player stands in front of. Returns FALSE when the friend cannot be called
static BOOL WifiP2PMatch_CallFriend(WIFIP2PMATCH_WORK *wk) {
    int friendNo = WIFI_MCR_PlayerSelect(&wk->room);
    int friendIndex;
    WIFI_STATUS *status;
    u32 wifiStatus;

    wk->friendNo = friendNo;
    if (friendNo != 0) {
        countActivePkms(wk->party);
        friendIndex = friendNo - 1;
        status = WifiP2PMatch_GetFriendStatusData(friendIndex);
        wifiStatus = WifiP2PMatch_GetStatus(wk, status);
        WIFI_STATUS_GetVChatStatus(status);
        if (wifiStatus == 6) {
            return FALSE;
        }
        if (WifiP2PMatch_StartConnect(wk, -1)) {
            WIFI_STATUS_SetVChatMac(wk->myStatus, WifiP2PMatch_GetFriendStatusData(friendIndex));
            wk->callTimer = 3600;
            WifiP2PMatch_ChangeNetStatus(wk, WIFIP2PMATCH_STATUS_CALL, 3);
            WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_CALL, 3);
            WifiP2PMatch_SetFriendNameWord(wk, friendNo - 1);
            wk->unk1351 = friendNo;
            WifiP2PMatch_PrintMessageNow(wk, WifiP2pmatch_Text_Communicating, FALSE);
            WifiP2PMatch_StartWaitIcon(wk);
            wk->seq = WIFIP2PMATCH_SEQ_CALL_WAIT;
        } else {
            WifiP2PMatch_SetFriendNameWord(wk, friendNo - 1);
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_CantConnect, FALSE);
            wk->seq = WIFIP2PMATCH_SEQ_CONNECT_ERROR;
        }
    }
    return TRUE;
}

// State 0x88: waits for the friend called from the menu to connect, until B is pressed or a minute has passed
static int WifiP2PMatch_StateCallWait(WIFIP2PMATCH_WORK *wk, int seq) {
    WifiP2PMatch_IsMessageEnd(wk);
    if (DWCRap_GetLastNetId() != -1) {
        wk->seq = WIFIP2PMATCH_SEQ_ROOM;
    }
    if (WifiP2PMatch_GetFriendStatus(wk->friendNo - 1) == 0) {
        WifiP2PMatch_SetFriendNameWord(wk, wk->friendNo - 1);
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_FailedRespond, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_CONNECT_FAILED;
        return seq;
    }
    wk->timer++;
    if (wk->timer % 900 == 0) {
        WIFI_STATUS_SetVChatMac(wk->myStatus, WifiP2PMatch_GetFriendStatusData(wk->friendNo - 1));
        WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_CALL, 3);
    }
    if (wk->timer > 3600) {
        WifiP2PMatch_SetFriendNameWord(wk, wk->friendNo - 1);
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_FailedRespond, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_CONNECT_FAILED;
        return seq;
    }
    if (GCTX_HIDGetPressedKeys() == PAD_BUTTON_B) {
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_SuspendRequest, FALSE);
        WifiP2PMatch_YesNoStart(wk);
        wk->seq = WIFIP2PMATCH_SEQ_CALL_CANCEL_YES_NO;
    }
    return seq;
}

// State 0x89: whether to stop calling the friend
static int WifiP2PMatch_StateCallCancelYesNo(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 ret;

    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    ret = WifiP2PMatch_YesNoMain(wk);
    if (ret == BMPMENU_NULL) {
        return seq;
    }
    if (ret == 0) {
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_RequestHasBeenSuspended, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_CONNECT_ERROR;
    } else {
        WifiP2PMatch_EndMessage(wk);
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_Communicating, FALSE);
        WifiP2PMatch_StartWaitIcon(wk);
        wk->seq = WIFIP2PMATCH_SEQ_CALL_WAIT;
    }
    return seq;
}

// State 0x40: the menu of a friend who is waiting
static int WifiP2PMatch_StateWaitingFriendMenu(WIFIP2PMATCH_WORK *wk, int seq) {
    BOOL keepMessage = FALSE;
    s32 ret = BmpMenuList_Update(wk->list);
    int friendNo = WIFI_MCR_PlayerSelect(&wk->room);
    MCR_MOVEOBJ *obj = WifiP2PMatch_GetMoveObj(wk, friendNo);
    WIFI_STATUS *status;
    BOOL vchat;
    u32 gameMode;

    if (NetState_IsWifiBattleError()) {
        wk->listWin = WifiP2PMatch_FreeWindow(wk->listWin);
        BmpMenuList_Free(wk->list, NULL, NULL);
        ListMenuCore_FreeOptionList(wk->menuOptions);
        if (obj != NULL) {
            WIFI_MCR_NpcPauseOff(&wk->room, obj);
        }
        WifiP2PMatch_OnNetError(wk);
        return seq;
    }
    switch (ret) {
    case BMPMENULIST_NULL:
        // The friend has left the room or is doing something else by now
        if (obj == NULL) {
            BmpMenuList_Free(wk->list, NULL, NULL);
            WifiP2PMatch_SetFriendNameWord(wk, friendNo - 1);
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_CantConnect, FALSE);
            wk->seq = WIFIP2PMATCH_SEQ_CONNECT_ERROR;
            keepMessage = TRUE;
        } else {
            status = WifiP2PMatch_GetFriendStatusData(friendNo - 1);
            WifiP2PMatch_GetStatus(wk, status);
            vchat = WIFI_STATUS_GetVChatStatus(status);
            gameMode = WifiP2PMatch_GetGameMode(wk, status);
            if (gameMode != WifiP2PMatch_GetGameMode(wk, &wk->targetStatus) ||
                vchat != WIFI_STATUS_GetVChatStatus(&wk->targetStatus)) {
                WifiP2PMatch_SetFriendNameWord(wk, friendNo - 1);
                WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_CantConnect, FALSE);
                wk->seq = WIFIP2PMATCH_SEQ_CONNECT_ERROR;
                keepMessage = TRUE;
            } else {
                return seq;
            }
        }
        break;
    case BMPMENULIST_CANCEL:
        GFL_SndSEPlay(SEQ_SE_CANCEL1);
        wk->seq = WIFIP2PMATCH_SEQ_ROOM;
        break;
    default:
        GFL_SndSEPlay(SEQ_SE_DECIDE1);
        if (ret == 1) {
            wk->seq = WIFIP2PMATCH_SEQ_ROOM;
            WifiP2PMatch_CallFriend(wk);
        } else if (ret == 2) {
            wk->seq = WIFIP2PMATCH_SEQ_FRIEND_DETAIL_INIT;
        }
        break;
    }
    if (!keepMessage) {
        WifiP2PMatch_EndMessage(wk);
    }
    wk->listWin = WifiP2PMatch_FreeWindow(wk->listWin);
    BmpMenuList_Free(wk->list, NULL, NULL);
    ListMenuCore_FreeOptionList(wk->menuOptions);
    if (obj != NULL) {
        WIFI_MCR_NpcPauseOff(&wk->room, obj);
    }
    return seq;
}

static void WifiP2PMatch_ResetVChatVolume(void) {
    DWCRap_SetVoiceChatEnabled(0);
}

// State 0x24: the menu of a friend who is looking for someone to play with
static int WifiP2PMatch_StateRecruitingFriendMenu(WIFIP2PMATCH_WORK *wk, int seq) {
    WIFI_STATUS *targetStatus = &wk->targetStatus;
    s32 ret = BmpMenuList_Update(wk->list);
    u32 friendNo;
    MCR_MOVEOBJ *obj;
    int gameMode;

    if (WifiP2PMatch_HasNewConnection(wk)) {
        ret = BMPMENULIST_CANCEL;
    }
    friendNo = WIFI_MCR_PlayerSelect(&wk->room);
    wk->friendNo = friendNo;
    obj = WifiP2PMatch_GetMoveObj(wk, friendNo);
    if (NetState_IsWifiBattleError()) {
        wk->listWin = WifiP2PMatch_FreeWindow(wk->listWin);
        BmpMenuList_Free(wk->list, NULL, NULL);
        ListMenuCore_FreeOptionList(wk->menuOptions);
        if (obj != NULL) {
            WIFI_MCR_NpcPauseOff(&wk->room, obj);
        }
        WifiP2PMatch_OnNetError(wk);
        return seq;
    }
    if (ret != BMPMENULIST_NULL) {
        WifiP2PMatch_EndMessage(wk);
        wk->listWin = WifiP2PMatch_FreeWindow(wk->listWin);
        BmpMenuList_Free(wk->list, NULL, NULL);
        ListMenuCore_FreeOptionList(wk->menuOptions);
    }
    switch (ret) {
    case BMPMENULIST_NULL:
        return seq;
    case BMPMENULIST_CANCEL:
        GFL_SndSEPlay(SEQ_SE_CANCEL1);
        wk->seq = WIFIP2PMATCH_SEQ_ROOM;
        break;
    default:
        GFL_SndSEPlay(SEQ_SE_DECIDE1);
        gameMode = WifiP2PMatch_GetGameMode(wk, targetStatus);
        if (gameMode == 5 && !canPlayerExchangePhotos() && isRunningOnDSi()) {
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_UsesCameraFunctionSend, FALSE);
            wk->seq = WIFIP2PMATCH_SEQ_CONNECT_FRIEND_YES_NO_START;
            return seq;
        }
        if (gameMode == 6 && !WifiP2PMatch_HasTwoPokemon(wk)) {
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_NeedLeastTwoPokemon, FALSE);
            wk->seq = WIFIP2PMATCH_SEQ_MESSAGE_KEY_WAIT_TO_ROOM;
            break;
        }
        if (WifiP2PMatch_IsBattleGameMode(gameMode)) {
            wk->battleMode = (gameMode - 7) / 2;
            wk->battleRule = (gameMode - 7) % 2;
            WifiP2PMatch_LoadMyRegulation(wk);
            if (!WifiP2PMatch_PassesRegulation(wk)) {
                WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_NotMeetConditionsCannot, FALSE);
                wk->seq = WIFIP2PMATCH_SEQ_MESSAGE_KEY_WAIT_TO_ROOM;
            }
        }
        wk->result = 2;
        if (WIFI_STATUS_GetVChatStatus(targetStatus) == WIFI_STATUS_GetVChatStatus(wk->myStatus)) {
            wk->seq = WIFIP2PMATCH_SEQ_JOIN_FRIEND;
            break;
        }
        if (WIFI_STATUS_GetVChatStatus(wk->myStatus)) {
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_VoiceChatWillTurned_2, FALSE);
        } else {
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_VoiceChatWillTurned, FALSE);
        }
        wk->seq = WIFIP2PMATCH_SEQ_VCHAT_CHANGE_YES_NO_INIT;
        break;
    }
    if (obj != NULL) {
        WIFI_MCR_NpcPauseOff(&wk->room, obj);
    }
    return seq;
}

// State 0x76: connects to the friend who is looking for someone, if nothing has changed since
static int WifiP2PMatch_StateJoinFriend(WIFIP2PMATCH_WORK *wk, int seq) {
    int friendNo = wk->friendNo;
    WIFI_STATUS *status;
    u32 wifiStatus;
    u32 gameMode;
    BOOL vchat;

    countActivePkms(wk->party);
    if (wk->friendNo == 0) {
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_RequestHasBeenSuspended, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_CONNECT_ERROR;
    }
    status = WifiP2PMatch_GetFriendStatusData(friendNo - 1);
    wifiStatus = WifiP2PMatch_GetStatus(wk, status);
    gameMode = WifiP2PMatch_GetGameMode(wk, status);
    vchat = WIFI_STATUS_GetVChatStatus(status);
    if (wifiStatus != WifiP2PMatch_GetStatus(wk, &wk->targetStatus) ||
        gameMode != WifiP2PMatch_GetGameMode(wk, &wk->targetStatus) ||
        vchat != WIFI_STATUS_GetVChatStatus(&wk->targetStatus) || wifiStatus == 6) {
        WifiP2PMatch_SetFriendNameWord(wk, friendNo - 1);
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_CantConnect, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_CONNECT_ERROR;
        return seq;
    }
    WifiP2PMatch_GetFriendStatus(friendNo - 1);
    if (WifiP2PMatch_StartConnect(wk, friendNo - 1)) {
        wk->callTimer = 3600;
        WifiP2PMatch_ChangeNetStatus(wk, WIFIP2PMATCH_STATUS_CALL, gameMode);
        WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_CALL, gameMode);
        WifiP2PMatch_SetFriendNameWord(wk, friendNo - 1);
        WifiP2PMatch_PrintMessageNow(wk, WifiP2pmatch_Text_Communicating, FALSE);
        WifiP2PMatch_StartWaitIcon(wk);
        wk->friendNo = friendNo;
        wk->seq = WIFIP2PMATCH_SEQ_JOIN_WAIT;
    } else {
        WifiP2PMatch_SetFriendNameWord(wk, friendNo - 1);
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_CantConnect, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_CONNECT_ERROR;
    }
    return seq;
}

// State 0x25: waits for the connection to the friend
static int WifiP2PMatch_StateJoinWait(WIFIP2PMATCH_WORK *wk, int seq) {
    MCR_MOVEOBJ *obj;

    WifiP2PMatch_IsMessageEnd(wk);
    wk->callTimer--;
    if (wk->callTimer < 0) {
        obj = WifiP2PMatch_GetMoveObj(wk, WIFI_MCR_PlayerSelect(&wk->room));
        if (obj != NULL) {
            WIFI_MCR_NpcPauseOff(&wk->room, obj);
        }
        wk->seq = WIFIP2PMATCH_SEQ_JOIN_TIMEOUT;
    } else if (NetState_GetWifiStatus() == 5) {
        WifiP2PMatch_SetFriendNameWord(wk, DWCRap_GetFriendIndex());
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_FailedRespond, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_CONNECT_FAILED;
    } else if (DWCRap_GetStatus() == 5) {
        WifiP2PMatch_SetFriendNameWord(wk, DWCRap_GetFriendIndex());
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_AppearsBusy, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_CONNECT_FAILED;
    } else if (DWCRap_GetStatus() == 4 || NetState_IsDisconnectReceived()) {
        WifiP2PMatch_SetFriendNameWord(wk, DWCRap_GetFriendIndex());
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_FailedRespond, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_CONNECT_FAILED;
    } else if (NetState_GetWifiStatus() == 1) {
        WIFI_STATUS_ResetVChatMac(wk->myStatus);
        WifiP2PMatch_SendMyStatus(wk);
        WifiP2PMatch_GetStatus(wk, wk->myStatus);
        wk->seq = WIFIP2PMATCH_SEQ_NEGOTIATION_WAIT;
        WifiP2PMatch_EndMessage(wk);
        WifiP2PMatchNet_CommandInit(wk);
        wk->timer = 30;
        func_02042e94(1);
        GFL_NetSetDisconnectError(TRUE);
    }
    return seq;
}

// State 0x3a
static int WifiP2PMatch_StateDummy3A(WIFIP2PMATCH_WORK *wk, int seq) {
    return seq;
}

// State 0x3b
static int WifiP2PMatch_StateDummy3B(WIFIP2PMATCH_WORK *wk, int seq) {
    return seq;
}

// State 0x3d: the friend did not answer: back to the room
static int WifiP2PMatch_StateJoinTimeout(WIFIP2PMATCH_WORK *wk, int seq) {
    WifiP2PMatch_ResetVChat(wk);
    WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
    wk->unk135A = 0;
    NetState_WifiDisconnect(TRUE);
    wk->preConnect = -1;
    wk->seq = WIFIP2PMATCH_SEQ_ROOM;
    WifiP2PMatch_RoomResume(wk);
    WifiP2PMatch_EndMessage(wk);
    return seq;
}

// State 0x26
static int WifiP2PMatch_StateDummy26(WIFIP2PMATCH_WORK *wk, int seq) {
    return seq;
}

// State 0x27
static int WifiP2PMatch_StateDummy27(WIFIP2PMATCH_WORK *wk, int seq) {
    return seq;
}

// State 0x28: a friend has connected to the player
static int WifiP2PMatch_StateFriendConnected(WIFIP2PMATCH_WORK *wk, int seq) {
    WIFI_STATUS *status;

    if (GFL_SndPlayerIsActive(0)) {
        return seq;
    }
    if (WifiP2PMatch_GetFriendStatus(DWCRap_GetFriendIndex()) == 0) {
        WifiP2PMatch_SetFriendNameWord(wk, wk->preConnect);
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_FailedRespond, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_CONNECT_FAILED;
        return seq;
    }
    status = WifiP2PMatch_GetFriendStatusData(DWCRap_GetFriendIndex());
    WifiP2PMatch_GetStatus(wk, wk->myStatus);
    WifiP2PMatch_GetStatus(wk, status);
    WIFI_STATUS_GetVChatStatus(wk->myStatus);
    wk->friendNo = DWCRap_GetFriendIndex() + 1;
    if (!wk->unk1351) {
        WifiP2PMatch_SetFriendNameWord(wk, DWCRap_GetFriendIndex());
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_CommunicatingPleaseStandBy, FALSE);
    }
    WifiP2PMatchNet_CommandInit(wk);
    wk->seq = WIFIP2PMATCH_SEQ_NEGOTIATION_WAIT;
    wk->timer = 30;
    return seq;
}

// Whether the connection to the friend was lost, which ends the state
static BOOL WifiP2PMatch_CheckDisconnect(WIFIP2PMATCH_WORK *wk) {
    if (NetState_GetWifiStatus() >= 3) {
        WifiP2PMatch_SetFriendNameWord(wk, wk->preConnect);
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_FailedRespond, FALSE);
        wk->seq = WIFIP2PMATCH_SEQ_CONNECT_FAILED;
    } else if (NetState_IsWifiBattleError()) {
        WifiP2PMatch_OnNetError(wk);
    } else {
        return FALSE;
    }
    wk->unk244 = 0;
    return TRUE;
}

// State 0x29: waits for the negotiation with the friend, then starts the exchange of the players' data
static int WifiP2PMatch_StateNegotiationWait(WIFIP2PMATCH_WORK *wk, int seq) {
    WifiP2PMatch_IsMessageEnd(wk);
    if (NetState_GetWifiStatus() >= 4 || NetState_IsDisconnectReceived()) {
        if (wk->unk244) {
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_FriendEndedCommunicationBattle, FALSE);
        } else {
            WifiP2PMatch_SetFriendNameWord(wk, wk->preConnect);
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_FailedRespond, FALSE);
        }
        wk->unk244 = 0;
        wk->seq = WIFIP2PMATCH_SEQ_CONNECT_FAILED;
    } else if (!WifiP2PMatch_CheckDisconnect(wk)) {
        if (wk->timer > 1) {
            wk->timer--;
        }
        if (wk->timer == 0) {
            if (NetHandle_IsNegotiated(NetHandle_GetOwn())) {
                MI_CpuFill8(wk->connectGameMode, 0, sizeof(wk->connectGameMode));
                NetHandle_SendTimingPair(NetHandle_GetOwn(), 0xe, 10);
                wk->unk244 = 0;
                WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_CommunicatingPleaseStandBy_2, FALSE);
                WifiP2PMatch_StartWaitIcon(wk);
                wk->seq = WIFIP2PMATCH_SEQ_SEND_PLAYER_INFO;
            }
        } else if (wk->timer == 1) {
            if (GFL_NetGetHandleNetId(NetHandle_GetOwn())) {
                if (NetHandle_SendNegotiation() == TRUE) {
                    wk->timer = 0;
                }
            } else if (NetHandle_CountNegotiated()) {
                if (NetHandle_SendNegotiation() == TRUE) {
                    wk->timer = 0;
                }
            }
        }
    }
    return seq;
}

// The partner's player data: adds the partner to the trainers the player has met
void WifiP2PMatch_RecvPlayerInfo(int netId, int size, const void *data, void *work, NetHandle *handle) {
    WIFIP2PMATCH_WORK *wk = work;
    PlayerInfo *info;

    if (handle == NetHandle_GetOwn()) {
        info = GameData_GetPlayerInfoByNetID(wk->gameData, netId);
        PlayerInfo_Copy(data, info);
        Misc_AddMetTrainer(getHollow_RivalData(wk->save), getIDAsUInt(info));
    }
}

// The partner's party, which is received in the parent work's buffer
void WifiP2PMatch_RecvParty(int netId, int size, const void *data, void *work, NetHandle *handle) {
}

void WifiP2PMatch_RecvCancel(int netId, int size, const void *data, void *work, NetHandle *handle) {
    WIFIP2PMATCH_WORK *wk = work;

    if (handle == NetHandle_GetOwn()) {
        wk->unk1355 = 1;
    }
}

void *WifiP2PMatch_GetPartyRecvBuffer(int netId, void *work, int size) {
    WIFIP2PMATCH_WORK *wk = work;

    return wk->parent->parties[netId];
}

// The state both sides go to next, which state 0x48 is waiting for
void WifiP2PMatch_RecvNextSeq(int netId, int size, const void *data, void *work, NetHandle *handle) {
    WIFIP2PMATCH_WORK *wk = work;
    const u8 *recvSeq = data;

    if (handle == NetHandle_GetOwn()) {
        if (wk->seq == WIFIP2PMATCH_SEQ_SYNC_SEQ_WAIT) {
            wk->recvSeq = *recvSeq;
            NetHandle_SendTimingPair(NetHandle_GetOwn(), 0x1a, 10);
        } else {
            wk->seq = WIFIP2PMATCH_SEQ_CONNECT_ERROR;
        }
    }
}

// The regulation the partner chose
void WifiP2PMatch_RecvRegulation(int netId, int size, const void *data, void *work, NetHandle *handle) {
    WIFIP2PMATCH_WORK *wk = work;
    const u32 *regulationCode = data;

    if (handle == NetHandle_GetOwn()) {
        WifiP2PMatch_SetRegulationCode(wk, *regulationCode);
    }
}

// What the partner is connecting for
void WifiP2PMatch_RecvConnectData(int netId, int size, const void *data, void *work, NetHandle *handle) {
    WIFIP2PMATCH_WORK *wk = work;
    const WIFIP2PMATCH_CONNECT_DATA *connect = data;

    wk->connectGameMode[netId] = connect->gameMode;
    wk->parent->vchat[netId] = connect->vchat;
    WifiP2PMatch_SetRegulationCode(wk, connect->regulationCode);
}

// State 0x2a: sends the player's data
static int WifiP2PMatch_StateSendPlayerInfo(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!WifiP2PMatch_CheckDisconnect(wk) && NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 0xe, 10)) {
        if (GFL_NetSendCommand(NetHandle_GetOwn(), WIFIP2PMATCH_NET_CMD_BASE + 4, PlayerInfo_GetSize(),
                               GetGameDataPlayerInfo(wk->gameData))) {
            WifiP2PMatch_EndMessage(wk);
            wk->seq = WIFIP2PMATCH_SEQ_SEND_CONNECT_DATA;
        }
    }
    return seq;
}

// State 0x87: sends what the player is connecting for
static int WifiP2PMatch_StateSendConnectData(WIFIP2PMATCH_WORK *wk, int seq) {
    WIFIP2PMATCH_CONNECT_DATA connect;

    WifiP2PMatch_GetStatus(wk, wk->myStatus);
    connect.gameMode = WifiP2PMatch_GetGameMode(wk, wk->myStatus);
    if (wk->unk1356) {
        connect.vchat = wk->unk1356 - 1;
        wk->unk1356 = 0;
    } else {
        connect.vchat = wk->parent->unk46;
    }
    connect.regulationCode = WifiP2PMatch_LoadRegulation(wk, 1);
    if (GFL_NetSendCommand(NetHandle_GetOwn(), WIFIP2PMATCH_NET_CMD_BASE + 3, sizeof(connect), &connect)) {
        wk->seq = WIFIP2PMATCH_SEQ_CONNECT_DATA_WAIT;
    }
    return seq;
}

// State 0x2b: waits for what the partner is connecting for, which has to be what the player is
static int WifiP2PMatch_StateConnectDataWait(WIFIP2PMATCH_WORK *wk, int seq) {
    int netId = 0;
    u16 gameMode;

    if (GFL_NetIsParent()) {
        netId = 1;
    }
    if (!WifiP2PMatch_CheckDisconnect(wk) && wk->connectGameMode[netId] != 0) {
        gameMode = WifiP2PMatch_GetGameMode(wk, wk->myStatus);
        if (gameMode == wk->connectGameMode[netId]) {
            NetHandle_SendTimingPair(NetHandle_GetOwn(), 0xf, 10);
            wk->seq = WIFIP2PMATCH_SEQ_CONNECT_TIMING;
        } else {
            WifiP2PMatch_SetFriendNameWord(wk, DWCRap_GetFriendIndex());
            WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_CantConnect, FALSE);
            wk->seq = WIFIP2PMATCH_SEQ_CONNECT_FAILED;
        }
    }
    return seq;
}

// State 0x2c
static int WifiP2PMatch_StateConnectTiming(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!WifiP2PMatch_CheckDisconnect(wk) && NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 0xf, 10)) {
        func_02042e94(1);
        GFL_NetSetDisconnectError(TRUE);
        NetHandle_SendTimingPair(NetHandle_GetOwn(), 0x12, 10);
        wk->seq = WIFIP2PMATCH_SEQ_CONNECT_END;
    }
    return seq;
}

// Whether the message is done or there is none
static BOOL WifiP2PMatch_IsMessageEndOrNone(WIFIP2PMATCH_WORK *wk) {
    if (WifiP2PMatch_IsMessageEnd(wk)) {
        return TRUE;
    }
    if (wk->msgWin == NULL) {
        return TRUE;
    }
    return FALSE;
}

// State 0x2d: both sides are ready: goes on to what the partner is doing
static int WifiP2PMatch_StateConnectEnd(WIFIP2PMATCH_WORK *wk, int seq) {
    WIFI_STATUS *status;
    u32 gameMode;

    if (!WifiP2PMatch_CheckDisconnect(wk) && NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 0x12, 10) &&
        WifiP2PMatch_IsMessageEndOrNone(wk) == TRUE) {
        DWCFriend_UpdateFriendData(WifiList_GetFriendData(GameData_GetWifiList(wk->gameData), 0));
        WifiP2PMatch_EndMessage(wk);
        status = WifiP2PMatch_GetFriendStatusData(DWCRap_GetFriendIndex());
        WifiP2PMatch_GetStatus(wk, status);
        gameMode = WifiP2PMatch_GetGameMode(wk, status);
        if (gameMode == 3) {
            wk->seq = WIFIP2PMATCH_SEQ_CONNECTED;
        } else {
            WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_PLAYING, gameMode);
            wk->seq = WIFIP2PMATCH_SEQ_GAME_START;
        }
    }
    return seq;
}

// State 0x19
static int WifiP2PMatch_StateReturnToRoom(WIFIP2PMATCH_WORK *wk, int seq) {
    GFL_BGSysClearBG(3);
    wk->unk1358 = 0;
    wk->seq = WifiP2PMatch_LeaveConnection(wk, FALSE);
    return seq;
}

// State 0x2e
static int WifiP2PMatch_StateFriendDetailInit(WIFIP2PMATCH_WORK *wk, int seq) {
    wk->seq = WIFIP2PMATCH_SEQ_FRIEND_DETAIL_WAIT;
    return seq;
}

// State 0x2f: the friend's details, until a key is pressed
static int WifiP2PMatch_StateFriendDetailWait(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    if (WifiP2PMatch_IsConnected(wk)) {
        wk->seq = WIFIP2PMATCH_SEQ_FRIEND_DETAIL_END;
    }
    if (GCTX_HIDGetPressedKeys() & (PAD_BUTTON_A | PAD_BUTTON_B)) {
        wk->seq = WIFIP2PMATCH_SEQ_FRIEND_DETAIL_END;
    }
    return seq;
}

// State 0x30
static int WifiP2PMatch_StateFriendDetailEnd(WIFIP2PMATCH_WORK *wk, int seq) {
    WifiP2PMatch_EndMessage(wk);
    wk->seq = WIFIP2PMATCH_SEQ_ROOM;
    return seq;
}

// State 0x31: asks whether to leave the Wi-Fi Club
static int WifiP2PMatch_StateExitYesNoInit(WIFIP2PMATCH_WORK *wk, int seq) {
    if (WifiP2PMatch_IsMessageEnd(wk)) {
        WifiP2PMatch_YesNoStart(wk);
        wk->seq = WIFIP2PMATCH_SEQ_EXIT_YES_NO;
    }
    return seq;
}

// State 0x32
static int WifiP2PMatch_StateExitYesNo(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 ret;

    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    ret = WifiP2PMatch_YesNoMain(wk);
    if (ret == BMPMENU_NULL) {
        return seq;
    }
    if (ret == 0) {
        wk->yesNoMenu = NULL;
        WifiP2PMatch_PrintMessage(wk, WifiError_Text_DisconnectingFromNintendoWi, TRUE);
        wk->seq = WIFIP2PMATCH_SEQ_DISCONNECT;
        wk->timer = 1;
    } else {
        wk->yesNoMenu = NULL;
        WifiP2PMatch_EndMessage(wk);
        wk->seq = WIFIP2PMATCH_SEQ_ROOM;
        WifiP2PMatch_RoomResume(wk);
    }
    return seq;
}

// State 0x33: disconnects from Nintendo WFC
static int WifiP2PMatch_StateDisconnect(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    if (wk->timer == 1) {
        wk->timer = 0;
        GFL_NetExit(NULL);
    }
    if (!GFL_NetIsInitialized()) {
        WifiList_CompactFriends(wk->wifiList);
        WifiP2PMatch_PrintMessage(wk, WifiError_Text_HaveBeenDisconnectedFrom, TRUE);
        wk->seq = WIFIP2PMATCH_SEQ_DISCONNECT_END;
        wk->timer = 30;
    }
    return seq;
}

// State 0x34
static int WifiP2PMatch_StateDisconnectEnd(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    wk->timer--;
    if (wk->timer == 0) {
        wk->endSeq = 0;
        wk->seq = WIFIP2PMATCH_SEQ_END;
        WifiP2PMatch_EndMessage(wk);
    }
    return seq;
}

// State 0x35
static int WifiP2PMatch_StateRetryYesNoInit(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 0x10, 10)) {
        return seq;
    }
    func_02042e94(0);
    GFL_NetSetDisconnectError(TRUE);
    if (WifiP2PMatch_IsMessageEnd(wk)) {
        WifiP2PMatch_YesNoStart(wk);
        wk->seq = WIFIP2PMATCH_SEQ_RETRY_YES_NO;
    }
    return seq;
}

// State 0x36
static int WifiP2PMatch_StateRetryYesNo(WIFIP2PMATCH_WORK *wk, int seq) {
    u32 ret;

    if (!WifiP2PMatch_IsMessageEnd(wk)) {
        return seq;
    }
    ret = WifiP2PMatch_YesNoMain(wk);
    if (ret == BMPMENU_NULL) {
        return seq;
    }
    if (ret == 0) {
        WifiP2PMatch_PrintMessage(wk, WifiP2pmatch_Text_AwaitingFriendsDecisionPlease, FALSE);
        WifiP2PMatch_StartWaitIcon(wk);
        wk->unk244 = 1;
        wk->seq = WIFIP2PMATCH_SEQ_NEGOTIATION_WAIT;
        wk->timer = 30;
    } else {
        WifiP2PMatch_EndMessage(wk);
        NetState_WifiDisconnect(TRUE);
        wk->timer = 20;
        wk->seq = WIFIP2PMATCH_SEQ_DISCONNECT_WAIT;
    }
    return seq;
}

// State 0x37: asks whether to switch voice chat to the friend's setting
static int WifiP2PMatch_StateVChatChangeYesNoInit(WIFIP2PMATCH_WORK *wk, int seq) {
    if (WifiP2PMatch_IsMessageEnd(wk)) {
        WifiP2PMatch_YesNoStart(wk);
        wk->seq = WIFIP2PMATCH_SEQ_VCHAT_CHANGE_YES_NO;
    }
    return seq;
}

// State 0x38
static int WifiP2PMatch_StateVChatChangeYesNo(WIFIP2PMATCH_WORK *wk, int seq) {
    MCR_MOVEOBJ *obj = WifiP2PMatch_GetMoveObj(wk, WIFI_MCR_PlayerSelect(&wk->room));
    u32 ret;

    GFL_TextRndResetColorIndexLUT();
    ret = WifiP2PMatch_YesNoMain(wk);
    if (ret == BMPMENU_NULL) {
        return seq;
    }
    if (ret == 0) {
        WIFI_STATUS_SetVChatStatus(wk->myStatus, 1 - WIFI_STATUS_GetVChatStatus(wk->myStatus));
        WIFI_STATUS_SetUseVChat(wk->myStatus, WIFI_STATUS_GetVChatStatus(wk->myStatus));
        wk->unk1356 = 2 - wk->parent->unk46;
        wk->seq = WIFIP2PMATCH_SEQ_JOIN_FRIEND;
    } else {
        wk->result = 0;
        WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
        wk->unk135A = 0;
        wk->preConnect = -1;
        if (obj != NULL) {
            WIFI_MCR_NpcPauseOff(&wk->room, obj);
        }
        WifiP2PMatch_ResetVChat(wk);
        WifiP2PMatch_DrawMyInfo(wk);
        wk->seq = WIFIP2PMATCH_SEQ_ROOM;
    }
    WifiP2PMatch_EndMessage(wk);
    return seq;
}

// State 0x1a: ends the proc once the connection is closed
static int WifiP2PMatch_StateEnd(WIFIP2PMATCH_WORK *wk, int seq) {
    if (!GFL_NetIsInitialized()) {
        seq = 2;
    }
    return seq;
}

// Changes the player's status and tells the friends
static void WifiP2PMatch_ChangeMyStatus(WIFIP2PMATCH_WORK *wk, u32 status, int gameMode) {
    if (wk->myStatus != NULL) {
        if (status == WIFIP2PMATCH_STATUS_WAIT && gameMode == 2) {
            WIFI_STATUS_ResetVChatMac(wk->myStatus);
        }
        if (status == WIFIP2PMATCH_STATUS_PLAYING && gameMode < 3) {
            WifiList_SetFriendLastDateToNow(wk->wifiList, DWCRap_GetFriendIndex());
        }
        WifiP2PMatch_SetMyStatus(wk, status, gameMode);
        WifiP2PMatch_SendMyStatus(wk);
    }
}

static void WifiP2PMatch_SetMyStatus(WIFIP2PMATCH_WORK *wk, u32 status, u32 gameMode) {
    u32 oldStatus;
    u32 oldGameMode;

    if (wk->myStatus != NULL) {
        oldStatus = WifiP2PMatch_GetStatus(wk, wk->myStatus);
        oldGameMode = WifiP2PMatch_GetGameMode(wk, wk->myStatus);
        if (oldStatus != status || oldGameMode != gameMode) {
            WifiP2PMatch_ChangeNetStatus(wk, status, gameMode);
            WIFI_STATUS_SetWifiStatus(wk->myStatus, status);
            WIFI_STATUS_SetGameMode(wk->myStatus, gameMode);
            // The volume stays low while playing
            if ((gameMode != 5 || status != WIFIP2PMATCH_STATUS_PLAYING) && status != WIFIP2PMATCH_STATUS_PLAYING) {
                WifiP2PMatch_BgmVolumeFadeStart(wk, 127);
            }
        }
        WifiP2PMatch_DrawMyInfo(wk);
    }
}

// Switches voice chat on or off. Returns the new setting
static BOOL WifiP2PMatch_ToggleVChat(WIFIP2PMATCH_WORK *wk) {
    wk->parent->unk46 = 1 - wk->parent->unk46;
    WIFI_STATUS_SetVChatStatus(wk->myStatus, wk->parent->unk46);
    WIFI_STATUS_SetUseVChat(wk->myStatus, wk->parent->unk46);
    WifiP2PMatch_SendMyStatus(wk);
    return WIFI_STATUS_GetVChatStatus(wk->myStatus);
}

// Sets the player's status back to the voice chat setting chosen
static void WifiP2PMatch_ResetVChat(WIFIP2PMATCH_WORK *wk) {
    WifiP2PMatch_BgmVolumeFadeStart(wk, 127);
    WIFI_STATUS_SetVChatStatus(wk->myStatus, wk->parent->unk46);
    WIFI_STATUS_SetUseVChat(wk->myStatus, wk->parent->unk46);
    WifiP2PMatch_SendMyStatus(wk);
    WIFI_STATUS_GetVChatStatus(wk->myStatus);
}

// Closes the friend's details on the lower screen
static void WifiP2PMatch_ViewCloseDetail(WIFIP2PMATCH_WORK *wk, HeapID heapId) {
    WifiP2PMatch_FreeTrainerActor(wk);
    WifiP2PMatch_FreeTouchBar(wk);
    WifiP2PMatch_ReloadViewScreen(wk);
    WifiP2PMatch_ViewResetTouch(wk);
    WifiP2PMatch_ShowAllShadows(wk);
    WIFI_MCR_CursorOff(&wk->room);
    WifiP2PMatch_ViewRedraw(wk, heapId);
}

static void WifiP2PMatch_ViewCloseDetailOf(WIFIP2PMATCH_WORK *wk, u32 friendNo, HeapID heapId) {
    if (friendNo == WifiP2PMatch_ViewGetTouchFriendNo(wk)) {
        WifiP2PMatch_ViewCloseDetail(wk, heapId);
    }
}

static void WifiP2PMatch_ViewCloseDetailIfOpen(WIFIP2PMATCH_WORK *wk, HeapID heapId) {
    if (wk->view.detailMode != 0) {
        WifiP2PMatch_ViewCloseDetail(wk, heapId);
    }
}

static BOOL WifiP2PMatch_ViewExists(WIFIP2PMATCH_WORK *wk) {
    if (wk->view.buttonMan != NULL) {
        return TRUE;
    }
    return FALSE;
}

// The frames of a pressed button's animation
static const u8 sWifiP2PMatchViewButtonAnimeFrames[16] = { 1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 2, 2, 2, 2 };

// The buttons of the lower screen: two columns of four friends
static const TouchRect sWifiP2PMatchViewButtonRects[WIFIP2PMATCH_VIEW_FRIEND_NUM + 1] = {
    { 0, 47, 0, 119 },     { 48, 95, 0, 119 },     { 96, 143, 0, 119 },
    { 144, 191, 0, 119 },  { 0, 47, 128, 255 },    { 48, 95, 128, 255 },
    { 96, 143, 128, 255 }, { 144, 191, 128, 255 }, { TOUCH_RECT_END, 0, 0, 0 },
};

// Creates the friends' page of the lower screen
static void WifiP2PMatch_ViewInit(WIFIP2PMATCH_WORK *wk, ArcTool *arc, u32 heapId) {
    MI_CpuFill8(&wk->view, 0, sizeof(WIFIP2PMATCH_VIEW));
    GFL_BGSysSetBGEnabledB(7, TRUE);
    GFL_BGSysSetBGEnabledB(8, FALSE);
    wk->view.buttonMan = GFL_BMN_Create(sWifiP2PMatchViewButtonRects, WifiP2PMatch_ViewButtonCallback, wk, heapId);
    wk->view.wordSet = GFL_WordSetSystemCreateDefault(heapId);
    WifiP2PMatch_ViewGraphicInit(wk, arc, heapId);
    wk->view.redrawAll = TRUE;
    WifiP2PMatch_ViewPageChanged(wk);
    WifiP2PMatch_ViewDrawButtons(wk);
}

static void WifiP2PMatch_ViewExit(WIFIP2PMATCH_WORK *wk) {
    if (WifiP2PMatch_ViewExists(wk)) {
        WifiP2PMatch_ViewGraphicExit(wk);
        GFL_BMN_Delete(wk->view.buttonMan);
        wk->view.buttonMan = NULL;
        GFL_WordSetSystemFree(wk->view.wordSet);
        wk->view.wordSet = NULL;
        GFL_BGSysSetBGEnabledB(1, TRUE);
        GFL_BGSysSetBGEnabledB(14, FALSE);
    }
}

// Runs the friends' page. Returns the number of the friend whose button was touched, or 0
static u32 WifiP2PMatch_ViewMain(WIFIP2PMATCH_WORK *wk, HeapID heapId) {
    u32 param = WIFI_MCR_GetPlayerOnMapParam(&wk->room);
    u32 underParam = WIFI_MCR_GetPlayerOnUnderMapParam(&wk->room);

    if (underParam >= WIFI_MCR_MAPPM_BLOCK0 && underParam <= WIFI_MCR_MAPPM_BLOCK3) {
        param = underParam;
    }
    // The page follows the block of the room the player is in
    if (param >= WIFI_MCR_MAPPM_BLOCK0 && param <= WIFI_MCR_MAPPM_BLOCK3) {
        if (wk->view.page != param - WIFI_MCR_MAPPM_BLOCK0) {
            wk->view.page = param - WIFI_MCR_MAPPM_BLOCK0;
            WifiP2PMatch_ViewPageChanged(wk);
            wk->view.redrawButtons = TRUE;
            wk->view.redrawAll = TRUE;
        }
    }
    if (wk->view.detailMode == 1) {
        wk->view.detailMode = 2;
        WifiP2PMatch_OpenFriendDetail(wk, heapId);
    }
    if (wk->view.detailMode == 0) {
        WifiP2PMatch_FreeTrainerActor(wk);
        WifiP2PMatch_FreeTouchBar(wk);
        WifiP2PMatch_ReloadViewScreen(wk);
        GFL_BMN_Main(wk->view.buttonMan);
        WifiP2PMatch_ViewButtonAnime(wk);
        if (wk->view.redrawButtons == TRUE) {
            WifiP2PMatch_ViewDrawButtons(wk);
            wk->view.redrawButtons = FALSE;
        }
    }
    if (wk->view.detailMode == 2 || wk->view.detailMode == 3) {
        if (WifiP2PMatch_ViewDetailCloseInput(wk) == TRUE) {
            wk->view.touchFriendNo = 0;
            wk->view.buttonFrame = 0;
            wk->view.redrawButtons = TRUE;
            wk->view.redrawAll = TRUE;
            WifiP2PMatch_ViewCloseDetail(wk, heapId);
            wk->view.detailMode = 0;
            wk->view.redrawAll = TRUE;
        }
    }
    return wk->view.touchFriendNo;
}

static void WifiP2PMatch_ViewRedrawButtons(WIFIP2PMATCH_WORK *wk) {
    if (wk->view.detailMode == 0 && wk->view.redrawButtons == TRUE) {
        WifiP2PMatch_ViewDrawButtons(wk);
        wk->view.redrawButtons = FALSE;
    }
}

// Whether the player closes the friend's details
static BOOL WifiP2PMatch_ViewDetailCloseInput(WIFIP2PMATCH_WORK *wk) {
    if (wk->view.detailMode == 3) {
        return FALSE;
    }
    if (wk->touchBar != NULL) {
        TouchBar_Main(wk->touchBar);
        if (TouchBar_GetDecided(wk->touchBar) == TOUCHBAR_ICON_RETURN) {
            return TRUE;
        }
    }
    if ((GCTX_HIDGetHeldKeys() & (PAD_KEY_RIGHT | PAD_KEY_LEFT | PAD_KEY_UP | PAD_KEY_DOWN)) ||
        (GCTX_HIDGetPressedKeys() & (PAD_BUTTON_A | PAD_BUTTON_B | PAD_BUTTON_X))) {
        return TRUE;
    }
    return FALSE;
}

static u32 WifiP2PMatch_ViewGetTouchFriendNo(WIFIP2PMATCH_WORK *wk) {
    return wk->view.touchFriendNo;
}

static void WifiP2PMatch_ViewResetTouch(WIFIP2PMATCH_WORK *wk) {
    if (WifiP2PMatch_ViewExists(wk) == TRUE) {
        wk->view.touchFriendNo = 0;
        wk->view.buttonFrame = 0;
        wk->view.detailMode = 0;
        wk->view.redrawButtons = TRUE;
        wk->view.redrawAll = TRUE;
    }
}

// A button of the friends' page was touched
static void WifiP2PMatch_ViewButtonCallback(u32 button, u32 event, void *work) {
    WIFIP2PMATCH_WORK *wk = work;
    u32 friendIndex = button + wk->view.page * WIFIP2PMATCH_VIEW_FRIEND_NUM;

    if (wk->view.friendState[friendIndex] == 2 && wk->view.touchFriendNo == 0 && event == 0) {
        wk->view.touchFriendNo = friendIndex + 1;
        GFL_SndSEPlay(SEQ_SE_DECIDE1);
    }
}

// Animates the touched button, after which the friend's details open
static void WifiP2PMatch_ViewButtonAnime(WIFIP2PMATCH_WORK *wk) {
    if (wk->view.touchFriendNo != 0) {
        wk->view.buttonFrame = sWifiP2PMatchViewButtonAnimeFrames[wk->view.buttonAnimCount];
        wk->view.buttonAnimCount++;
        wk->view.redrawButtons = TRUE;
        if (wk->view.buttonAnimCount >= 16) {
            wk->view.buttonAnimCount = 0;
            wk->view.buttonFrame = 0;
            wk->view.detailMode = 1;
        }
    }
}

// Loads the graphics of the friends' page and creates its windows
static void WifiP2PMatch_ViewGraphicInit(WIFIP2PMATCH_WORK *wk, ArcTool *arc, u32 heapId) {
    int i;
    int j;
    int x;
    int y;

    GFL_G2DIOLoadArcNCLRDefault(arc, 6, 4, 0, 0x100, heapId);
    wk->viewBgChars = GFL_BGSysLoadArcNCGRDynamic(arc, 14, 4, 0, FALSE, heapId);
    GFL_BGSysLoadArcNCGRStatic(arc, 15, 6, 0x80, 0, FALSE, heapId);
    GFL_G2DIOLoadNSCRSync(arc, 22, 4, 0, (u16)wk->viewBgChars, 0, FALSE, heapId);
    wk->view.buttonScrnBuf = GFL_G2DIOReadNSCRArc(arc, 23, FALSE, &wk->view.buttonScrn, heapId);
    WifiP2PMatch_ViewScreenAddCharOffset(wk->view.buttonScrn, 0x80);
    wk->view.backScrnBuf = GFL_G2DIOReadNSCRArc(arc, 25, FALSE, &wk->view.backScrn, heapId);
    WifiP2PMatch_ViewScreenAddCharOffset(wk->view.backScrn, 0x80);
    GFL_BGSysLoadNCLRDefault(23, 5, 4, 0x1e0, 0x20, heapId);
    for (i = 0; i < WIFIP2PMATCH_VIEW_FRIEND_NUM; i++) {
        y = i % 4;
        x = i / 4;
        wk->view.nameWin[i] = BmpWin_CreateDynamic(7, x * 16 + 4, y * 6 + 1, 9, 3, 15, TRUE);
        GFL_BitmapFill(BmpWin_GetBitmap(wk->view.nameWin[i]), 0);
        BmpWin_FlushChar(wk->view.nameWin[i]);
        for (j = 0; j < 2; j++) {
            wk->view.statusWin[i][j] = BmpWin_CreateDynamic(5, x * 16 + 1 + j * 12, y * 6 + 1, 2, 3, 15, TRUE);
            GFL_BitmapFill(BmpWin_GetBitmap(wk->view.statusWin[i][j]), 0);
            BmpWin_FlushChar(wk->view.statusWin[i][j]);
        }
    }
    wk->view.detailWin[0] = BmpWin_CreateDynamic(7, 1, 1, 30, 2, 15, TRUE);
    GFL_BitmapFill(BmpWin_GetBitmap(wk->view.detailWin[0]), 0);
    BmpWin_FlushMap(wk->view.detailWin[0]);
    BmpWin_FlushChar(wk->view.detailWin[0]);
    wk->view.detailWin[1] = BmpWin_CreateDynamic(7, 1, 5, 30, 17, 15, TRUE);
    GFL_BitmapFill(BmpWin_GetBitmap(wk->view.detailWin[1]), 0);
    BmpWin_FlushMap(wk->view.detailWin[1]);
    BmpWin_FlushChar(wk->view.detailWin[1]);
    GFL_BGSysLoadScr(4);
    GFL_BGSysLoadScr(7);
}

static void WifiP2PMatch_ViewGraphicExit(WIFIP2PMATCH_WORK *wk) {
    int i;
    int j;

    for (i = 0; i < WIFIP2PMATCH_VIEW_FRIEND_NUM; i++) {
        BmpWin_Free(wk->view.nameWin[i]);
        for (j = 0; j < 2; j++) {
            BmpWin_Free(wk->view.statusWin[i][j]);
        }
    }
    BmpWin_Free(wk->view.detailWin[0]);
    BmpWin_Free(wk->view.detailWin[1]);
    GFL_HeapFree(wk->view.buttonScrnBuf);
    GFL_HeapFree(wk->view.backScrnBuf);
}

// Adds to every tile of a screen, to use the characters loaded at an offset
static void WifiP2PMatch_ViewScreenAddCharOffset(NNSG2dScreenData *scrn, u16 add) {
    int y;
    int x;
    u16 *tiles = (u16 *)scrn->rawData;
    int width = scrn->screenWidth / 8;
    int height = scrn->screenHeight / 8;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            tiles[y * width + x] += add;
        }
    }
}

// state: 0 for a friend who is not online, 1 for one who is and 2 for one who is in the room
static void WifiP2PMatch_ViewSetFriendState(WIFIP2PMATCH_WORK *wk, u32 friendNo, u32 state) {
    wk->view.friendState[friendNo - 1] = state;
    wk->view.redrawButtons = TRUE;
}

static void WifiP2PMatch_ViewClearFriendState(WIFIP2PMATCH_WORK *wk, u32 friendNo) {
    wk->view.friendState[friendNo - 1] = 0;
    wk->view.redrawButtons = TRUE;
}

static u8 WifiP2PMatch_ViewGetFriendState(WIFIP2PMATCH_WORK *wk, u32 friendNo) {
    return wk->view.friendState[friendNo - 1];
}

static void WifiP2PMatch_ViewRedraw(WIFIP2PMATCH_WORK *wk, HeapID heapId) {
    if (wk->view.detailMode != 0) {
        WifiP2PMatch_OpenFriendDetail(wk, heapId);
    } else {
        wk->view.redrawAll = TRUE;
        WifiP2PMatch_ViewDrawButtons(wk);
    }
}

static void WifiP2PMatch_ViewRequestRedrawAll(WIFIP2PMATCH_WORK *wk) {
    wk->view.redrawAll = TRUE;
    wk->view.redrawButtons = TRUE;
}

static void WifiP2PMatch_ViewPageChanged(WIFIP2PMATCH_WORK *wk) {
}

// Draws the buttons of the page: all of them, or only the one that is animating
static void WifiP2PMatch_ViewDrawButtons(WIFIP2PMATCH_WORK *wk) {
    int i;
    int j;
    BOOL all;
    int friendNo;
    u32 type;
    int x;
    int y;
    u32 frame;
    BOOL redraw;

    if (wk->view.redrawAll == TRUE) {
        wk->view.redrawAll = FALSE;
        all = TRUE;
        GFL_BGSysClearBG(7);
    } else {
        all = FALSE;
    }
    for (i = 0; i < WIFIP2PMATCH_VIEW_FRIEND_NUM; i++) {
        friendNo = wk->view.page * WIFIP2PMATCH_VIEW_FRIEND_NUM + i + 1;
        if (wk->view.friendState[friendNo - 1] != 0) {
            if (WifiList_GetFriendParam(wk->wifiList, friendNo - 1, 9) == 0) {
                type = WIFIP2PMATCH_VIEW_BUTTON_MALE;
            } else {
                type = WIFIP2PMATCH_VIEW_BUTTON_FEMALE;
            }
        } else {
            type = WIFIP2PMATCH_VIEW_BUTTON_NONE;
        }
        x = i / 4;
        y = i % 4;
        if (friendNo == wk->view.touchFriendNo) {
            frame = wk->view.buttonFrame;
        } else {
            frame = 0;
        }
        if (friendNo == wk->view.touchFriendNo || friendNo == wk->view.lastTouchFriendNo) {
            redraw = TRUE;
        } else {
            redraw = FALSE;
        }
        if (all == TRUE || redraw == TRUE) {
            WifiP2PMatch_ViewDrawButton(wk, x * 16, y * 6, type, frame);
            if (type != WIFIP2PMATCH_VIEW_BUTTON_NONE) {
                WifiP2PMatch_ViewDrawFriendName(wk, wk->view.nameWin[i], friendNo, frame, i);
                BmpWin_FlushMap(wk->view.nameWin[i]);
                WifiP2PMatch_ViewDrawFriendIcons(wk, wk->view.statusWin[i], friendNo, frame, x * 16, y * 6);
            } else {
                GFL_BitmapFill(BmpWin_GetBitmap(wk->view.nameWin[i]), 0);
                BmpWin_FlushChar(wk->view.nameWin[i]);
                for (j = 0; j < 2; j++) {
                    GFL_BitmapFill(BmpWin_GetBitmap(wk->view.statusWin[i][j]), 0);
                    BmpWin_FlushChar(wk->view.statusWin[i][j]);
                }
            }
        }
    }
    GFL_BGSysQueueScrLoad(7);
    GFL_BGSysQueueScrLoad(6);
    GFL_BGSysSetBGEnabledB(2, TRUE);
    GFL_BGSysSetBGEnabledB(8, TRUE);
}

// Copies a button from the buttons' screen data: a column per type and a row per frame, and the empty one below
static void WifiP2PMatch_ViewDrawButton(WIFIP2PMATCH_WORK *wk, u8 x, u8 y, u8 type, u8 frame) {
    u16 srcX;
    u16 srcY;

    if (type == WIFIP2PMATCH_VIEW_BUTTON_NONE) {
        srcX = 0;
        srcY = 18;
    } else {
        srcX = type * 16;
        srcY = frame * 6;
    }
    GFL_BGSysLoadScrArea(6, x, y, 16, 6, wk->view.buttonScrn->rawData, srcX, srcY, 32, 24);
}

// Draws a friend's name
static void WifiP2PMatch_ViewDrawFriendName(WIFIP2PMATCH_WORK *wk, BmpWin *window, u32 friendNo, u32 frame, u32 index) {
    GFL_BitmapFill(BmpWin_GetBitmap(window), 0);
    GFL_TextRndUpdateColorIndexLUT(15, 2, 0);
    WifiP2PMatch_ViewSetFriendNameWord(wk, friendNo - 1);
    GFL_MsgDataLoadStrbuf(wk->msgData, WifiP2pmatch_Text_Empty_17, wk->expStrBuf);
    GFL_WordSetFormatStrbuf(wk->view.wordSet, wk->titleString, wk->expStrBuf);
    GFL_TextRendererDrawToBitmap(BmpWin_GetBitmap(window), 2, 8, wk->titleString, wk->font);
    BmpWin_FlushChar(window);
}

// Draws the icons of what a friend is doing and of the friend's voice chat setting
static void WifiP2PMatch_ViewDrawFriendIcons(WIFIP2PMATCH_WORK *wk, BmpWin **windows, u32 friendNo, u32 frame, u32 x,
                                             u32 y) {
    int add = 0;
    WIFI_STATUS *status;
    u32 wifiStatus;
    u32 gameMode;
    int vchatIcon;

    if (frame != 0) {
        add = 16;
    }
    status = WifiP2PMatch_GetFriendStatusData(friendNo - 1);
    wifiStatus = WifiP2PMatch_GetStatus(wk, status);
    gameMode = WifiP2PMatch_GetGameMode(wk, status);
    WifiP2PMatch_IconWriteStatus(&wk->icon, 6, x + 2, y + 2, wifiStatus, gameMode, add);
    if (WIFI_STATUS_GetUseVChat(status)) {
        vchatIcon = 15;
    } else {
        vchatIcon = 11;
    }
    WifiP2PMatch_IconWrite(&wk->icon, 6, x + 12, y + 2, vchatIcon + add, 0);
    GFL_BGSysQueueScrLoad(7);
}

// Puts a friend's name in the page's word set
static void WifiP2PMatch_ViewSetFriendNameWord(WIFIP2PMATCH_WORK *wk, int friendIndex) {
    u32 sex;

    if (friendIndex != -1) {
        sex = WifiList_GetFriendParam(wk->wifiList, friendIndex, 9);
        WifiList_LoadFriendName(wk->wifiList, friendIndex, wk->expStrBuf);
        GFL_WordSetSetStrbuf(wk->view.wordSet, 0, wk->expStrBuf, sex, 1, 2);
    }
}

static WIFI_STATUS *WifiP2PMatch_GetFriendStatusData(int friendIndex) {
    return (WIFI_STATUS *)DWCRap_GetFriendStatusData(friendIndex);
}

static u8 WifiP2PMatch_GetFriendStatus(int friendIndex) {
    return DWCRap_GetFriendStatus(friendIndex);
}

static BOOL WifiP2PMatch_StartConnect(WIFIP2PMATCH_WORK *wk, int a1) {
    WifiP2PMatchNet_CommandInit(wk);
    return NetState_ConnectWifiFriend(a1);
}

// Stops the player in the room
static void WifiP2PMatch_RoomPause(WIFIP2PMATCH_WORK *wk, BOOL message) {
    if (!wk->roomPaused) {
        wk->roomPaused = TRUE;
        WIFI_MCR_PlayerMovePause(&wk->room, TRUE);
        if (message == TRUE) {
            WifiP2PMatch_PrintMessageNow(wk, WifiP2pmatch_Text_SeekingParticipants, FALSE);
        }
    }
}

static void WifiP2PMatch_RoomResume(WIFIP2PMATCH_WORK *wk) {
    if (wk->roomPaused == TRUE) {
        wk->roomPaused = FALSE;
        WifiP2PMatch_EndMessage(wk);
        WIFI_MCR_PlayerMovePause(&wk->room, FALSE);
        wk->result = 0;
    }
}

// Fades the music's volume. Returns TRUE when the fade is over
static BOOL WifiP2PMatch_BgmVolumeFadeMain(WIFIP2PMATCH_WORK *wk) {
    int diff;

    if (wk->bgmFadeFrames != 0) {
        wk->bgmFadeFrames--;
        if (wk->bgmFadeFrames != 0) {
            diff = wk->bgmVolumeTarget - wk->bgmVolumeStart;
            if (diff < 0) {
                wk->bgmVolume = wk->bgmVolumeTarget + (-diff / 6) * wk->bgmFadeFrames;
            } else {
                wk->bgmVolume = wk->bgmVolumeTarget - (diff / 6) * wk->bgmFadeFrames;
            }
            GFL_SndBGMSetVolume(0xffff, wk->bgmVolume);
            return FALSE;
        }
        wk->bgmVolume = wk->bgmVolumeTarget;
        GFL_SndBGMSetVolume(0xffff, wk->bgmVolumeTarget);
    }
    return TRUE;
}

static void WifiP2PMatch_BgmVolumeFadeStart(WIFIP2PMATCH_WORK *wk, u8 volume) {
    if (wk->bgmVolumeTarget != volume) {
        wk->bgmVolumeStart = wk->bgmVolume;
        wk->bgmFadeFrames = 6;
        wk->bgmVolumeTarget = volume;
        GFL_SndPlayerSetVolumeEx(volume, 0x3f);
        PokeVoice_SetMasterVolume(volume);
    }
}

static void WifiP2PMatch_BgmVolumeInit(WIFIP2PMATCH_WORK *wk, u32 mode) {
    if (mode == 4) {
        wk->bgmVolumeTarget = 42;
    } else {
        wk->bgmVolumeTarget = 127;
    }
    wk->bgmVolumeStart = wk->bgmVolumeTarget;
}

static BOOL WifiP2PMatch_ProcInit(GameProc *proc, u32 *state, void *param, void *work) {
    WifiClubData *parent = param;
    WIFIP2PMATCH_WORK *wk;

    GFL_HeapCreateChild(1, WIFIP2PMATCH_HEAP | 0x8000, 0xa4000);
    wk = GFL_ProcInitSubsystem(proc, sizeof(WIFIP2PMATCH_WORK), WIFIP2PMATCH_HEAP);
    MI_CpuFill8(wk, 0, sizeof(WIFIP2PMATCH_WORK));
    GFL_NetSetWork(wk);
    wk->myStatus = parent->buffer;
    wk->save = parent->save;
    wk->gameData = parent->gameData;
    wk->battleBoxParty = WifiP2PMatch_CreateBattleBoxParty(wk->gameData);
    wk->party = GameData_GetParty(wk->gameData);
    wk->wifiList = GameData_GetWifiList(wk->gameData);
    wk->trainerData = getTrainerDataBlkAddress(parent->save);
    wk->initMode = parent->mode;
    wk->result = parent->unk49;
    wk->unk135A = 0;
    if (parent->mode == 1) {
        parent->mode = 0;
        WifiP2PMatch_BgmVolumeInit(wk, 4);
    } else if (parent->mode != 0) {
        WifiP2PMatch_BgmVolumeInit(wk, 4);
    } else {
        WifiP2PMatch_BgmVolumeInit(wk, 1);
    }
    wk->endSeq = 0;
    wk->preConnect = -1;
    wk->parent = parent;
    GFL_BGSysSetBGEnabledA(15, FALSE);
    GFL_BGSysSetBGEnabledB(15, FALSE);
    WifiP2PMatch_InitMessages(wk);
    wk->seq = WIFIP2PMATCH_SEQ_INIT;
    WifiP2PMatch_GraphicInit(wk);
    if (parent->mode == 0) {
        WifiP2PMatch_InitMyStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
    }
    if (GFL_NetIsInitialized()) {
        WifiP2PMatchNet_CommandInit(wk);
        GFL_NetSetIconScreen(TRUE, WIFIP2PMATCH_HEAP);
    }
    return TRUE;
}

// The states by number. The last seven have no function
static WifiP2PMatchSeqFunc sWifiP2PMatchSeqFuncs[WIFIP2PMATCH_SEQ_MAX] = {
    WifiP2PMatch_StateInit,                     // 0x00
    WifiP2PMatch_StateReconnectKeyWait,         // 0x01
    WifiP2PMatch_StateReconnectYesNoStart,      // 0x02
    WifiP2PMatch_StateReconnectYesNo,           // 0x03
    WifiP2PMatch_StateConnectStart,             // 0x04
    WifiP2PMatch_StateBGPriority,               // 0x05
    WifiP2PMatch_StateConnectWait,              // 0x06
    WifiP2PMatch_StateUserInfoNotice,           // 0x07
    WifiP2PMatch_StateUserInfoNoticeKeyWait,    // 0x08
    WifiP2PMatch_StateLoginWait,                // 0x09
    WifiP2PMatch_StateRoomStart,                // 0x0a
    WifiP2PMatch_StateRoom,                     // 0x0b
    WifiP2PMatch_StateVChatToggleKeyWait,       // 0x0c
    WifiP2PMatch_StateVChatCallWait,            // 0x0d
    WifiP2PMatch_StateVChatInvited,             // 0x0e
    WifiP2PMatch_StateVChatInvitedWait,         // 0x0f
    WifiP2PMatch_StateVChatStart,               // 0x10
    WifiP2PMatch_StateVChatEndYesNoStart,       // 0x11
    WifiP2PMatch_StateVChatEndYesNo,            // 0x12
    WifiP2PMatch_StateConnectFailed,            // 0x13
    WifiP2PMatch_StateConnectFailedKeyWait,     // 0x14
    WifiP2PMatch_StateConnectError,             // 0x15
    WifiP2PMatch_StateNone16,                   // 0x16
    WifiP2PMatch_StateNone17,                   // 0x17
    WifiP2PMatch_StateNone18,                   // 0x18
    WifiP2PMatch_StateReturnToRoom,             // 0x19
    WifiP2PMatch_StateEnd,                      // 0x1a
    WifiP2PMatch_StateNoFriendKeyWait,          // 0x1b
    WifiP2PMatch_StateRecruitMenuStart,         // 0x1c
    WifiP2PMatch_StateRecruitMenu,              // 0x1d
    WifiP2PMatch_StateRecruitMenuOld,           // 0x1e
    WifiP2PMatch_StateRecruitStopAsk,           // 0x1f
    WifiP2PMatch_StateRecruitStopYesNoStart,    // 0x20
    WifiP2PMatch_StateRecruitStopYesNo,         // 0x21
    WifiP2PMatch_StateFriendSelect,             // 0x22
    WifiP2PMatch_StateFriendMenuInit,           // 0x23
    WifiP2PMatch_StateRecruitingFriendMenu,     // 0x24
    WifiP2PMatch_StateJoinWait,                 // 0x25
    WifiP2PMatch_StateDummy26,                  // 0x26
    WifiP2PMatch_StateDummy27,                  // 0x27
    WifiP2PMatch_StateFriendConnected,          // 0x28
    WifiP2PMatch_StateNegotiationWait,          // 0x29
    WifiP2PMatch_StateSendPlayerInfo,           // 0x2a
    WifiP2PMatch_StateConnectDataWait,          // 0x2b
    WifiP2PMatch_StateConnectTiming,            // 0x2c
    WifiP2PMatch_StateConnectEnd,               // 0x2d
    WifiP2PMatch_StateFriendDetailInit,         // 0x2e
    WifiP2PMatch_StateFriendDetailWait,         // 0x2f
    WifiP2PMatch_StateFriendDetailEnd,          // 0x30
    WifiP2PMatch_StateExitYesNoInit,            // 0x31
    WifiP2PMatch_StateExitYesNo,                // 0x32
    WifiP2PMatch_StateDisconnect,               // 0x33
    WifiP2PMatch_StateDisconnectEnd,            // 0x34
    WifiP2PMatch_StateRetryYesNoInit,           // 0x35
    WifiP2PMatch_StateRetryYesNo,               // 0x36
    WifiP2PMatch_StateVChatChangeYesNoInit,     // 0x37
    WifiP2PMatch_StateVChatChangeYesNo,         // 0x38
    WifiP2PMatch_StateDisconnectWait,           // 0x39
    WifiP2PMatch_StateDummy3A,                  // 0x3a
    WifiP2PMatch_StateDummy3B,                  // 0x3b
    WifiP2PMatch_StateUserDataSaveStart,        // 0x3c
    WifiP2PMatch_StateJoinTimeout,              // 0x3d
    WifiP2PMatch_StateUserDataSave,             // 0x3e
    WifiP2PMatch_StateVChatCallStart,           // 0x3f
    WifiP2PMatch_StateWaitingFriendMenu,        // 0x40
    WifiP2PMatch_StateConnected,                // 0x41
    WifiP2PMatch_StatePlayYesNoStart,           // 0x42
    WifiP2PMatch_StatePlayYesNo,                // 0x43
    WifiP2PMatch_StateGameModeMessage,          // 0x44
    WifiP2PMatch_StateGameModeMenuOpen,         // 0x45
    WifiP2PMatch_StateGameModeMenu,             // 0x46
    WifiP2PMatch_StateGameModeKeyWait,          // 0x47
    WifiP2PMatch_StateSyncSeqWait,              // 0x48
    WifiP2PMatch_StateSyncSeqTimingWait,        // 0x49
    WifiP2PMatch_StateCancelReset,              // 0x4a
    WifiP2PMatch_StateAskVChat,                 // 0x4b
    WifiP2PMatch_StateAskTVT,                   // 0x4c
    WifiP2PMatch_StateAskTrade,                 // 0x4d
    WifiP2PMatch_StateBattleSetupMenuStart,     // 0x4e
    WifiP2PMatch_StateAnswerYesNoStart,         // 0x4f
    WifiP2PMatch_StateAnswerYesNo,              // 0x50
    WifiP2PMatch_StateStartGame,                // 0x51
    WifiP2PMatch_StateSendStartGame,            // 0x52
    WifiP2PMatch_StateSendCancelReset,          // 0x53
    WifiP2PMatch_StateBattleSetupMenu,          // 0x54
    WifiP2PMatch_StateBattleModeMenuStart,      // 0x55
    WifiP2PMatch_StateBattleModeMenu,           // 0x56
    WifiP2PMatch_StateBattleRuleMenuStart,      // 0x57
    WifiP2PMatch_StateBattleRuleMenu,           // 0x58
    WifiP2PMatch_StateShooterMenuStart,         // 0x59
    WifiP2PMatch_StateShooterMenu,              // 0x5a
    WifiP2PMatch_StateBattleSetupDecide,        // 0x5b
    WifiP2PMatch_StateRulesShow,                // 0x5c
    WifiP2PMatch_StateRulesShowWait,            // 0x5d
    WifiP2PMatch_StateBattleOfferShow,          // 0x5e
    WifiP2PMatch_StateUnused5F,                 // 0x5f
    WifiP2PMatch_StateBattleOfferYesNoStart,    // 0x60
    WifiP2PMatch_StateBattleOfferYesNo,         // 0x61
    WifiP2PMatch_StateBattleOfferCheckParty,    // 0x62
    WifiP2PMatch_StateRegulationFailParty,      // 0x63
    WifiP2PMatch_StatePartySelectStart,         // 0x64
    WifiP2PMatch_StatePartyMenuStart,           // 0x65
    WifiP2PMatch_StatePartyMenu,                // 0x66
    WifiP2PMatch_StatePartySend,                // 0x67
    WifiP2PMatch_StatePartySendWait,            // 0x68
    WifiP2PMatch_StateBattleStartWait,          // 0x69
    WifiP2PMatch_StateBattleStart,              // 0x6a
    WifiP2PMatch_StatePartyMenuFailParty,       // 0x6b
    WifiP2PMatch_StatePartyMenuFailBattleBox,   // 0x6c
    WifiP2PMatch_StatePartyMenuFailEnd,         // 0x6d
    WifiP2PMatch_StatePlayAgainAsk,             // 0x6e
    WifiP2PMatch_StatePlayAgainYesNoStart,      // 0x6f
    WifiP2PMatch_StatePlayAgainYesNo,           // 0x70
    WifiP2PMatch_StateMessageKeyWaitToRoom,     // 0x71
    WifiP2PMatch_StateBackToRoom,               // 0x72
    WifiP2PMatch_StateGameStart,                // 0x73
    WifiP2PMatch_StateGameStartFailKeyWait,     // 0x74
    WifiP2PMatch_StateBattleRecruitStart,       // 0x75
    WifiP2PMatch_StateJoinFriend,               // 0x76
    WifiP2PMatch_StateRegulationFailBattleBox,  // 0x77
    WifiP2PMatch_StateRegulationFailEnd,        // 0x78
    WifiP2PMatch_StateReturnFromGame,           // 0x79
    WifiP2PMatch_StateBattleOfferMsgWait,       // 0x7a
    WifiP2PMatch_StateBattleOfferKeyWait,       // 0x7b
    WifiP2PMatch_StateBattleOfferYesNoGo,       // 0x7c
    WifiP2PMatch_StateVChatDiffMsg,             // 0x7d
    WifiP2PMatch_StateVChatDiffYesNoStart,      // 0x7e
    WifiP2PMatch_StateVChatDiffYesNo,           // 0x7f
    WifiP2PMatch_StateVChatSend,                // 0x80
    WifiP2PMatch_StateRegulationSend,           // 0x81
    WifiP2PMatch_StateRegulationSendEnd,        // 0x82
    WifiP2PMatch_StateRecruitMenuMessage,       // 0x83
    WifiP2PMatch_StateRefusedMsg,               // 0x84
    WifiP2PMatch_StateSendRefusedMsg,           // 0x85
    WifiP2PMatch_StateVChatDiffNo,              // 0x86
    WifiP2PMatch_StateSendConnectData,          // 0x87
    WifiP2PMatch_StateCallWait,                 // 0x88
    WifiP2PMatch_StateCallCancelYesNo,          // 0x89
    WifiP2PMatch_StateSendSeqTiming,            // 0x8a
    WifiP2PMatch_StateSendSeq,                  // 0x8b
    WifiP2PMatch_StatePlayAgainKeyWait,         // 0x8c
    WifiP2PMatch_StateGoDisconnect,             // 0x8d
    WifiP2PMatch_StateDisconnectStart,          // 0x8e
    WifiP2PMatch_StateDeclinedWait,             // 0x8f
    WifiP2PMatch_StateDeclined,                 // 0x90
    WifiP2PMatch_StateCutConnection,            // 0x91
    WifiP2PMatch_StateVChatEndSend,             // 0x92
    WifiP2PMatch_StateVChatEndTiming,           // 0x93
    WifiP2PMatch_StateVChatEndedMessage,        // 0x94
    WifiP2PMatch_StateVChat,                    // 0x95
    WifiP2PMatch_StateTradeRefusedKeyWait,      // 0x96
    WifiP2PMatch_StateUnableMsg,                // 0x97
    WifiP2PMatch_StatePartySendTiming,          // 0x98
    WifiP2PMatch_StateCancelMsg,                // 0x99
    WifiP2PMatch_StateVChatStopWait,            // 0x9a
    WifiP2PMatch_StateTalkEndTiming,            // 0x9b
    WifiP2PMatch_StateTalkEndTimingWait,        // 0x9c
    WifiP2PMatch_StateRegulationFailKeyWait,    // 0x9d
    WifiP2PMatch_StateRoomMessage,              // 0x9e
    WifiP2PMatch_StateVChatDisconnectTiming,    // 0x9f
    WifiP2PMatch_StateCameraNoticeYesNoStart,   // 0xa0
    WifiP2PMatch_StateCameraNoticeYesNo,        // 0xa1
    WifiP2PMatch_StateConnectFriendYesNoStart,  // 0xa2
    WifiP2PMatch_StateConnectFriendYesNo,       // 0xa3
    WifiP2PMatch_StateNoCameraYesNoStart,       // 0xa4
    WifiP2PMatch_StateNoCameraYesNo,            // 0xa5
    WifiP2PMatch_StateAnswerNoCameraYesNoStart, // 0xa6
    WifiP2PMatch_StateAnswerNoCameraYesNo,      // 0xa7
    WifiP2PMatch_StateDeclineReset,             // 0xa8
    WifiP2PMatch_StateDeclineMsg,               // 0xa9
    WifiP2PMatch_StateReturnYesNoStart,         // 0xaa
    WifiP2PMatch_StateReturnYesNo,              // 0xab
    WifiP2PMatch_StateReturnTimingWait,         // 0xac
    WifiP2PMatch_StateRecruitStart,             // 0xad
};

static BOOL WifiP2PMatch_ProcMain(GameProc *proc, u32 *state, void *param, void *work) {
    WIFIP2PMATCH_WORK *wk = work;
    WifiP2PMatchSeqFunc func;
    u8 letter;
    u8 shadow;
    u8 background;

    if (GFL_NetIsInitialized()) {
        switch (DWCRapCommon_CheckError(1, 1)) {
        case DWCRAP_RESULT_TIMEOUT:
            // The partner was lost: back to the room
            WifiP2PMatch_ClosePartyMenu(wk);
            WifiP2PMatch_FreeWindows(wk);
            func_02042e94(0);
            GFL_NetSetDisconnectError(FALSE);
            WifiP2PMatch_ChangeMyStatus(wk, WIFIP2PMATCH_STATUS_WAIT, 2);
            wk->unk135A = 0;
            wk->timer = 20;
            wk->seq = WIFIP2PMATCH_SEQ_DISCONNECT_WAIT;
            return FALSE;
        case DWCRAP_RESULT_FAILED:
            Wipe_SetScreenCovered(0, 0);
            Wipe_SetScreenCovered(1, 0);
            wk->endSeq = 1;
            return TRUE;
        }
    }
    if (!GFL_NetErrCheck()) {
        func = sWifiP2PMatchSeqFuncs[wk->seq];
        if (func != NULL) {
            *state = func(wk, *state);
            if (*state == 2) {
                GFL_WipeSet(0, 0, 0, 0, 6, 1, WIFIP2PMATCH_HEAP);
                return TRUE;
            }
        }
    }
    if (wk->clActUnit != NULL) {
        ClActSys_Draw();
    }
    if (wk->printQueue != NULL) {
        GFL_TextRndGetGlobalColors(&letter, &shadow, &background);
        GFL_TextRndUpdateColorIndexLUT(1, 2, 15);
        GFL_TCBExMgrUpdate(wk->tcbManager);
        GFL_TextRndUpdateColorIndexLUT(letter, shadow, background);
        PrintQueue_Main(wk->printQueue);
    }
    if (wk->listPrintQueue != NULL) {
        PrintQueue_Main(wk->listPrintQueue);
    }
    WifiP2PMatch_BgmVolumeFadeMain(wk);
    return FALSE;
}

static BOOL WifiP2PMatch_ProcExit(GameProc *proc, u32 *state, void *param, void *work) {
    WIFIP2PMATCH_WORK *wk = work;
    WifiClubData *parent = param;

    if (!WifiP2PMatch_BgmVolumeFadeMain(wk)) {
        return FALSE;
    }
    if (!GFL_WipeIsFinished()) {
        return FALSE;
    }
    if (GFL_NetIsInitialized()) {
        WifiP2PMatchNet_CommandExit(wk);
    }
    parent->unk49 = wk->result;
    if (wk->regulation != NULL) {
        Regulation_Copy(wk->regulation, parent->regulation);
        GFL_HeapFree(wk->regulation);
        wk->regulation = NULL;
    }
    WifiP2PMatch_ClosePartyMenu(wk);
    WifiP2PMatch_GraphicExit(wk);
    parent->friendIndex = wk->friendNo;
    parent->mode = wk->endSeq;
    if (GFL_NetIsInitialized()) {
        parent->unk10 = DWCRap_GetFriendIndex();
    }
    WifiP2PMatch_FreeBattleBoxParty(wk->battleBoxParty);
    WifiP2PMatch_ExitMessages(wk);
    GFL_ProcReleaseSubsystem(proc);
    GFL_HeapDelete(WIFIP2PMATCH_HEAP);
    return TRUE;
}

const GameProcFunctions WIFICLUB_PROC_FUNCTIONS = {
    WifiP2PMatch_ProcInit,
    WifiP2PMatch_ProcMain,
    WifiP2PMatch_ProcExit,
};
