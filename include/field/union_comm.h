#ifndef POKEBW2_FIELD_UNION_COMM_H
#define POKEBW2_FIELD_UNION_COMM_H

#include "types.h"
#include "battle/regulation.h"
#include "dwc/dwc.h"
#include "field/field.h"
#include "field/trcard_sys.h"
#include "gfl/heap.h"
#include "save/join_avenue.h"
#include "save/player_info.h"
#include "struct_decls.h"
#include "system/pms_data.h"

// union_comm.c, overlay 28, which the ROM names: the Union Room's system work, its GameCommSys callbacks for
// GAME_COMM_NO_UNION (see game_comm.c), its beacon and the players it has met. The field side is union_main.c, and
// the Union Room's menus are in overlay 34. Field names marked unk are used by overlays not decompiled yet

// What UnionMain_Boot boots the communication with
struct UnionBootParam {
    PlayerInfo *playerInfo;
    GameCommSys *comm;
    GameData *gameData;
    GameSystem *gsys;
};

// A player met, in a group's list and in the beacon
typedef struct {
    u8 mac[6];
    u8 look; // PlayerInfo_GetUnionView's appearance
    u8 gender : 1;
    u8 used : 1;
} UnionMember;

#define UNION_MEMBER_MAX 5

// All the players of a list at once, to copy it
typedef struct {
    UnionMember list[UNION_MEMBER_MAX];
} UnionMemberList;

// The beacon this machine sends
typedef struct {
    u8 mac[6]; // its own, or the parent's when a child
    u8 memberCount : 3;
    u8 entryClosed : 1;
    u8 look : 4;
    u8 state : 3;
    u8 gender : 1;
    u8 valid : 1;
    u8 unk07_5 : 3;
    u8 version;
    u8 language;
    u8 unk0A;
    u8 activity;
    u16 name[8];
    u16 look2[4];
    u16 counter;
    u8 country;
    u8 province;
    union {
        UnionMember members[UNION_MEMBER_MAX];
        UnionMemberList memberList;
    };
    // The friend data token of the player, for the Union Room
    DWCFriendData unk50;
    u32 trainerId;
    u8 unk60[4];
} UnionBeacon;

// The field actor of one member of a heard beacon, which overlay 34 runs. Its actor ID is the entry's index << 8 | the
// member + 0x20
typedef struct {
    struct UnionBeaconEntry *entry;
    u8 look;
    u8 gender;
    u8 member; // in entry->beacon.members
    u8 used;
    u8 step;
    u8 nextStep;
    u8 phase;
    u8 seq;
} UnionActorSlot;

#define UNION_ACTOR_MAX 50

// A beacon heard from another machine
typedef struct UnionBeaconEntry {
    UnionBeacon beacon;
    u8 mac[6];
    u8 status; // 1 when new, 2 when heard again
    u8 index;
    u16 timeout; // frames until it is forgotten, reset on each beacon
    UnionActorSlot *actor[UNION_MEMBER_MAX];
    u8 unk84[8];
} UnionBeaconEntry;

// The players this machine is grouped with
struct UnionGroup {
    // The player spoken to, the one who answered, and the parent joined (the names of the third are the ROM's)
    UnionBeaconEntry *target;
    UnionBeaconEntry *unk4;
    UnionBeaconEntry *answer_pc;
    UnionBeaconEntry *parent;
    u8 unk10[8];
    u8 unk18;
    u8 unk19;
    u8 unk1A;
    u8 unk1B;
    union {
        UnionMember members[UNION_MEMBER_MAX];
        UnionMemberList memberList;
    };
    u16 unk44;
    u8 unk46_0 : 1;
    u8 unk46_1 : 1;
    u8 unk46_2 : 2;
    u8 unk46_4 : 4;
    u8 unk47;
    u8 unk48;
    u8 recvFlag;
    u8 doneMask; // a bit per net ID
    u8 step : 7;
    u8 active : 1;
    u8 unk4C;
};

// This machine's state, which its beacon is built from
typedef struct {
    u8 activity;
    u8 step; // 0x2a when idle; the beacon sends it
    u8 unk02;
    u16 look[4];
    u16 counter;
    u8 changed;
    u8 state;
    u8 unk10[4];
    UnionGroup mycomm; // the ROM's name
    u8 ruleSel[4];     // the battle format, two options and the menu kind
    s32 partyCheck;    // the regulation's checks of the party and of the Battle Box
    s32 boxCheck;
    s16 wait; // frames
    s16 result;
    u8 prevStep;
    u8 nextStep;
    u8 phase; // of the step: 0 entering, 1 running, 2 leaving
    u8 seq;
    u8 mac78[6]; // the player to show the focus effect on
} UnionSelf;

// A step of the system, run by overlay 34
typedef BOOL (*UnionSituFunc)(UnionSystem *unisys, UnionSelf *situ, Field *field, u8 *seq);

// A line of the Union Room's chat: what a player said, as the touch screen shows it, in a ring of lines
typedef struct {
    u16 name[8];
    // What the player said, when kind is UNION_CHAT_KIND_SENTENCE
    PMSData sentence;
    u16 unk18;
    u8 mac[6]; // of the player, who is looked up by it
    u8 gender;
    u8 kind;    // UNION_CHAT_KIND_*
    u8 subKind; // 0, or 1 for a friend and 2 for another player
    u8 unk23;
} UnionVisitor;

// What a line of the chat says: the player's sentence, or that it entered the Union Room, or what the player wants to
// do
#define UNION_CHAT_KIND_SENTENCE 0
#define UNION_CHAT_KIND_ENTERED 1
#define UNION_CHAT_KIND_BATTLE 2
#define UNION_CHAT_KIND_TRADE 3
#define UNION_CHAT_KIND_DRAW 4
#define UNION_CHAT_KIND_SPIN_TRADE 5

#define UNION_VISITOR_MAX 30

typedef struct {
    UnionVisitor visitors[UNION_VISITOR_MAX];
    u8 unk438[4];
    // How many lines were added, the newest line the touch screen shows at its bottom, and the one it showed before
    s32 count;
    int viewEnd;
    s32 drawnEnd;
} UnionVisitorList;

// The line of the ring that the index calls for, or NULL if it is empty
UnionVisitor *UnionChat_GetLine(UnionVisitorList *list, u32 index);

// The command the system is running: a row of union_main.c's sUnionCommands, or UNION_SUBPROC_ID_NULL
typedef struct {
    BOOL active;
    u32 id;
    void *arg;
    u8 step;
} UnionSubproc;

#define UNION_SUBPROC_ID_NULL 0

#define UNION_BEACON_ENTRY_MAX 10

struct UnionSystem {
    UnionBootParam *param;
    u8 recvBuf[5][0x800]; // a buffer per net ID
    // The names of these six are the ROM's, from overlay 34's asserts
    WordSet *wordset;
    MsgData *msgdata;
    void *fld_msgwin_stream;
    ListMenuUI *fldmenu_func;
    void *fldmenu_yesno_func;
    void *fldmsgwin;
    u32 unk281C;
    StrBuf *msgbuf; // the message, and with its words set
    StrBuf *fmtbuf;
    u8 msgPrinting;
    u8 beaconReady;
    u8 fieldActive;
    u8 entryReply;      // 1 when this machine's entry request was accepted, 2 when refused
    u8 entryAcceptedTo; // a bit per net ID
    u8 entryRepliedTo;
    u8 unk282E;
    u8 unk282F;
    UnionSelf situ; // the ROM's name for it
    UnionBeacon beacon;
    UnionBeaconEntry found[UNION_BEACON_ENTRY_MAX];
    UnionActorSlot actors[UNION_ACTOR_MAX];
    UnionVisitorList visitorList;
    // What the system allocates as players meet (the names are the ROM's, from its asserts). The trainer cards are sent
    // whole
    struct {
        UnionApp *uniapp;
        Regulation *regulation;
        void *rpm; // the regulation's rules printed, from PokeRegulation_CreatePrintMsg (the name is the ROM's)
        void *partySelect;
        PokeParty *boxParty; // the Battle Box as a party
        TrainerCardParam *cardParam;
        TrainerCardData *my_card;
        TrainerCardData *target_card;
    } alloc;
    JoinAvenuePerson joinPerson;
    UnionSubproc subproc;
    void *cmdArg; // what overlay 34 allocates for the command it starts
    UnionColosseum *colosseum_sys;
    u8 commState;
    u8 linkState; // 0 when idle, then the steps of UnionComm_UpdateLink's restart of the communication
    u8 linkMode;
    u8 updateDisabled;
    u32 unk362C;
    u32 unk3630;
    u32 unk3634;
};

// Allocates the system for the boot parameters
UnionSystem *UnionComm_Create(UnionBootParam *param);

// The GameCommSys callbacks for GAME_COMM_NO_UNION
void *UnionComm_Init(u32 *seq, void *param);
BOOL UnionComm_InitWait(u32 *seq, void *param, void *work);
// Called once the communication is up, and once it has ended
void UnionComm_OnNetReady(void *work);
BOOL UnionComm_Exit(u32 *seq, void *param, void *work);
BOOL UnionComm_ExitWait(u32 *seq, void *param, void *work);
void UnionComm_Main(u32 *seq, void *param, void *work);

// Asks for the communication to restart in a mode, and whether it was asked to
void UnionComm_RequestLink0(UnionSystem *sys);
void UnionComm_RequestLink1(UnionSystem *sys);
void UnionComm_RequestLink2(UnionSystem *sys);
BOOL UnionComm_IsLinkRequested(UnionSystem *sys);
void UnionComm_ActivateGroup(UnionSystem *sys);
// Clears the beacons found, and frees what the system holds
void UnionComm_ClearBeacons(UnionSystem *sys);
void UnionComm_FreeResources(UnionSystem *sys);
// Sets a part of this machine's state: a group's target 0 to 3, or the activity for 4
void UnionComm_SetSelf(UnionSystem *sys, u32 which, uptr value);
// Resets a group, with this machine as its first member
void UnionGroup_ResetTargets(UnionGroup *group);
void UnionGroup_Init(UnionSystem *sys, UnionGroup *group);
// Adds a player to the group's list, and removes one; the entry forms take the player from a beacon heard
int UnionGroup_AddMember(UnionGroup *group, const u8 *mac, u8 look, u8 gender);
void UnionGroup_AddBeaconMember(UnionGroup *group, const UnionBeaconEntry *entry);
void UnionGroup_RemoveMember(UnionGroup *group, const u8 *mac);
void UnionGroup_RemoveBeaconMember(UnionGroup *group, const UnionBeaconEntry *entry);
// Sets this machine's look, and its state; both make the beacon change
void UnionComm_SetLook(UnionSystem *sys, const u16 *look);
void UnionComm_SetState(UnionSystem *sys, u8 state);
// Sets the net game command base for an activity
void UnionComm_SetCommandBase(u32 activity);

#endif // POKEBW2_FIELD_UNION_COMM_H
