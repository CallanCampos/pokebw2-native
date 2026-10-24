#include "types.h"
#include "constants/arc.h"
#include "constants/text_banks.h"
#include "constants/version.h"
#include "field/beacon_view.h"
#include "field/beacon_view_common.h"
#include "field/event_beacon_detail.h"
#include "field/event_fest_mission.h"
#include "field/event_medal_info_beacon.h"
#include "field/event_pass_power.h"
#include "field/event_phrase_input.h"
#include "field/fest_mission_data.h"
#include "field/festival.h"
#include "field/field.h"
#include "field/field_menu.h"
#include "field/field_msgbg.h"
#include "field/field_pass_power.h"
#include "field/intrude_work.h"
#include "gfl/arc.h"
#include "gfl/arc_util.h"
#include "gfl/bg_sys.h"
#include "gfl/bmp.h"
#include "gfl/bmpwin.h"
#include "gfl/clact.h"
#include "gfl/graphics.h"
#include "gfl/gx_layers.h"
#include "gfl/heap.h"
#include "gfl/msg.h"
#include "gfl/net.h"
#include "gfl/overlay.h"
#include "gfl/std.h"
#include "gfl/str.h"
#include "gfl/tcb.h"
#include "gfl/tcbl.h"
#include "nitro/gx.h"
#include "nitro/hw.h"
#include "nitro/mi.h"
#include "nitro/os.h"
#include "nnsys/g2d.h"
#include "save/high_link.h"
#include "save/player_info.h"
#include "save/save_control.h"
#include "save/trainer_card.h"
#include "save/trainer_data_savedata.h"
#include "system/app_keycursor.h"
#include "system/app_taskmenu.h"
#include "system/beacon_status.h"
#include "system/bmp_oam.h"
#include "system/fest_mission.h"
#include "system/game_beacon.h"
#include "system/game_data.h"
#include "system/game_event.h"
#include "system/game_system.h"
#include "system/gf_font.h"
#include "system/palanm.h"
#include "system/printsys.h"
#include "system/wordset.h"

// The beacon view's work: what it takes from the game, its graphics, windows and actors, and the events that leave
// the view for another screen. Named by the ROM's "beacon_view_setup.c"; the functions' names are ours

// What the event waits for before it hands over
enum {
    EVENT_WAIT_BUTTON,
    EVENT_WAIT_TASKS,
    EVENT_WAIT_MENU_BUTTON,
    EVENT_WAIT_MEDAL,
};

typedef struct {
    u32 seq;
    GameSystem *gsys;
    Field *field;
    BeaconViewWork *work;
    GameEvent *next;
    u32 wait;
    uptr param;
} BeaconViewEvent;

// The files of a set of OAM resources: how many of each kind, and the first
typedef struct {
    u16 count;
    u16 fileId;
} BeaconViewResFiles;

typedef struct {
    BeaconViewResFiles lists[BEACON_VIEW_RES_KINDS];
} BeaconViewResSetup;

typedef struct {
    u8 x;
    u8 y;
    u8 sequence : 5;
    u8 bgPriority : 3;
} BeaconViewActorSetup;

static GameEvent *BeaconView_CreateWaitEvent(BeaconViewWork *work, GameEvent *next, u32 wait, uptr param);
static GameEventReturnCode BeaconView_WaitEvent(GameEvent *event, u32 *state, void *data);
static BOOL BeaconView_WaitButton(BeaconViewWork *work, BeaconViewEvent *event);
static BOOL BeaconView_WaitTasks(BeaconViewWork *work, BeaconViewEvent *event);
static BOOL BeaconView_WaitMenuButton(BeaconViewWork *work, BeaconViewEvent *event);
static BOOL BeaconView_WaitMedal(BeaconViewWork *work, BeaconViewEvent *event);
static void BeaconView_VBlankTask(TCB *tcb, void *data);
static void BeaconView_InitData(BeaconViewWork *work);
static void BeaconView_FreeData(BeaconViewWork *work);
static void BeaconView_InitSystems(BeaconViewWork *work);
static void BeaconView_FreeSystems(BeaconViewWork *work);
static void BeaconView_InitBG(BeaconViewWork *work, ArcTool *arc);
static void BeaconView_FreeBG(BeaconViewWork *work);
static void BeaconView_InitMenu(BeaconViewWork *work);
static void BeaconView_FreeMenu(BeaconViewWork *work);
static void BeaconView_LoadResources(BeaconViewWork *work, ArcTool *arc);
static void BeaconView_FreeResources(BeaconViewWork *work);
static void BeaconView_InitActors(BeaconViewWork *work, ArcTool *arc);
static void BeaconView_FreeActors(BeaconViewWork *work);
static void BeaconView_InitWin(BeaconViewWork *work, BeaconViewWinSlot *slot, u8 bg, u8 palette, u8 x, u8 y, u8 width,
                               u8 height);
static void BeaconView_InitWins(BeaconViewWork *work);
static void BeaconView_FreeWins(BeaconViewWork *work);
static void BeaconView_LoadRes(BeaconViewWork *work, BeaconViewRes *res, const BeaconViewResSetup *setup, ArcTool *arc);
static void BeaconView_FreeRes(BeaconViewWork *work, BeaconViewRes *res);
static ClActor *BeaconView_AddActor(BeaconViewWork *work, u32 chars, u32 palette, u32 cellAnims, s16 x, s16 y,
                                    u8 sequence, u8 bgPriority, u8 priority);
static void BeaconView_AddScreenActors(BeaconViewWork *work);
static void BeaconView_DeleteScreenActors(BeaconViewWork *work);
static void BeaconView_InitPanel(BeaconViewWork *work, u8 id);
static void BeaconView_FreePanel(BeaconViewWork *work, u8 id);

BeaconViewWork *BeaconView_Create(GameSystem *gsys, void *parent) {
    BeaconViewWork *work = GFL_HeapAllocate(HEAPID_FIELD_MENU, sizeof(BeaconViewWork), TRUE, "beacon_view_setup.c", 91);

    work->gsys = gsys;
    work->gameData = GSYS_GetGameData(gsys);
    work->field = GSYS_GetField(gsys);
    work->dispControl = FieldG2D_GetDispControl(work->field);
    work->festival = GSYS_GetLinkFestival(gsys);
    work->missionConfig = GetFestMissionCfg(work->festival);
    work->parent = parent;
    work->heapId = HEAPID_FIELD_MENU;
    work->resHeapId = HEAPID_FIELDMAP;
    work->tmpHeapId = HEAPID_FIELD_MENU | HEAPID_TAIL_BIT;
    work->arc = GFL_ArcSysCreateFileHandle(ARCID_BEACON, work->heapId);
    BeaconView_InitData(work);
    BeaconView_InitSystems(work);
    BeaconView_InitBG(work, work->arc);
    BeaconView_InitMenu(work);
    BeaconView_LoadResources(work, work->arc);
    BeaconView_InitActors(work, work->arc);
    BeaconView_InitWins(work);
    return work;
}

void BeaconView_Free(BeaconViewWork *work) {
    GFL_TCBExMgrFreeTasks(work->tcbManager);
    BeaconView_FreeWins(work);
    BeaconView_FreeActors(work);
    BeaconView_FreeResources(work);
    BeaconView_FreeMenu(work);
    BeaconView_FreeBG(work);
    BeaconView_FreeSystems(work);
    BeaconView_FreeData(work);
    GFL_ArcToolFree(work->arc);
    GFL_HeapFree(work);
}

void BeaconView_RequestEvent(BeaconViewWork *work, u16 request) {
    work->eventRequest = request;
}

void BeaconView_ClearEventRequest(BeaconViewWork *work) {
    work->eventRequest = BEACON_VIEW_EVENT_NONE;
}

GameEvent *BeaconView_CreateEvent(BeaconViewWork *work, BOOL enabled) {
    GameEvent *event;

    if (!enabled) {
        return NULL;
    }
    switch (work->eventRequest) {
    case BEACON_VIEW_EVENT_MENU_COMM:
        func_ov012_02153654(work->commSys);
        // fallthrough
    case BEACON_VIEW_EVENT_MENU:
    case BEACON_VIEW_EVENT_MENU_WIFI:
        BeaconStatus_SetLogTop(work->status, 0);
        GameData_SetCGearPowerRequest(work->gameData, 0);
        event = EventFieldMenuReturn_Create(work->gsys, work->field, 0);
        event = BeaconView_CreateWaitEvent(work, event, EVENT_WAIT_BUTTON, BEACON_VIEW_ACTOR_RETURN);
        break;
    case BEACON_VIEW_EVENT_DETAIL: {
        // The line's place in the log, and the line on the screen
        u32 args = work->selected + work->top;

        args |= work->selected << 16;

        event = GameEvent_CreateOverlayDelegate(work->gsys, OVERLAY_ID(13), (GameEventProvider)EventBeaconDetail_Create,
                                                &args);
        event = BeaconView_CreateWaitEvent(work, event, EVENT_WAIT_TASKS, 0);
        break;
    }
    case BEACON_VIEW_EVENT_MEDAL: {
        // The line's place in the log, and the line on the screen
        u32 args = work->selected + work->top;

        args |= work->selected << 16;

        event = GameEvent_CreateOverlayDelegate(work->gsys, OVERLAY_ID(15),
                                                (GameEventProvider)EventMedalInfoBeacon_Create, &args);
        event = BeaconView_CreateWaitEvent(work, event, EVENT_WAIT_MEDAL, work->selectedPanel);
        break;
    }
    case BEACON_VIEW_EVENT_PASS_POWER: {
        u32 args = 0;

        event = GameEvent_CreateOverlayDelegate(work->gsys, OVERLAY_ID(14), (GameEventProvider)EventPassPower_Create,
                                                &args);
        event = BeaconView_CreateWaitEvent(work, event, EVENT_WAIT_BUTTON, BEACON_VIEW_ACTOR_PASS_POWER);
        break;
    }
    case BEACON_VIEW_EVENT_PASS_POWER_USE: {
        u32 args[2];

        args[0] = work->usePassPower;
        args[1] = FALSE;
        event = GameEvent_CreateOverlayDelegate(work->gsys, OVERLAY_ID(156), EventPassPowerActivate_Create, args);
        event = BeaconView_CreateWaitEvent(work, event, EVENT_WAIT_MENU_BUTTON,
                                           (uptr)BeaconViewMenu_GetButton(work->menu, 1));
        work->usePassPower = PASS_POWER_NONE;
        break;
    }
    case BEACON_VIEW_EVENT_PASS_POWER_LIST:
        event = GameEvent_CreateOverlayDelegate(work->gsys, OVERLAY_ID(156),
                                                (GameEventProvider)EventPassPowerList_Create, work->field);
        event = BeaconView_CreateWaitEvent(work, event, EVENT_WAIT_MENU_BUTTON,
                                           (uptr)BeaconViewMenu_GetButton(work->menu, 0));
        break;
    case BEACON_VIEW_EVENT_PASS_POWER_LIST_2:
        event = GameEvent_CreateOverlayDelegate(work->gsys, OVERLAY_ID(156),
                                                (GameEventProvider)EventPassPowerList_Create, work->field);
        event = BeaconView_CreateWaitEvent(work, event, EVENT_WAIT_MENU_BUTTON,
                                           (uptr)BeaconViewMenu_GetButton(work->menu, 0));
        work->unk88 = 0;
        break;
    case BEACON_VIEW_EVENT_GREETING:
        event = EventPhraseInput_Create(work->gsys, work->field, NULL, NAME_ENTRY_PASSERBY_MESSAGE,
                                        (u16 *)BeaconStatus_GetGreetingChanged(work->status));
        event = BeaconView_CreateWaitEvent(work, event, EVENT_WAIT_BUTTON, BEACON_VIEW_ACTOR_GREETING);
        break;
    case BEACON_VIEW_EVENT_MISSION_START: {
        FestMissionEventArgs args;

        MI_CpuFill8(&args, 0, sizeof(args));
        args.config = work->mission;
        args.joined = TRUE;
        event =
            GameEvent_CreateOverlayDelegate(work->gsys, OVERLAY_FUNFEST_MISSION, EventFestMissionStart_Create, &args);
        event = BeaconView_CreateWaitEvent(work, event, EVENT_WAIT_MENU_BUTTON,
                                           (uptr)BeaconViewMenu_GetButton(work->menu, 1));
        ClearFesMissionCfg(&work->mission);
        break;
    }
    case BEACON_VIEW_EVENT_MISSION_MESSAGE: {
        FestMissionEventArgs args;

        MI_CpuFill8(&args, 0, sizeof(args));
        args.config = work->mission;
        args.joined = TRUE;
        args.messageType = work->missionRefusal;
        event =
            GameEvent_CreateOverlayDelegate(work->gsys, OVERLAY_FUNFEST_MISSION, EventFestMissionMessage_Create, &args);
        event = BeaconView_CreateWaitEvent(work, event, EVENT_WAIT_MENU_BUTTON,
                                           (uptr)BeaconViewMenu_GetButton(work->menu, work->missionButton));
        ClearFesMissionCfg(&work->mission);
        break;
    }
    case BEACON_VIEW_EVENT_MISSION_INFO:
        event = GameEvent_CreateOverlayDelegate(work->gsys, OVERLAY_FUNFEST_MISSION, EventFestMissionInfo_Create,
                                                &work->mission);
        event = BeaconView_CreateWaitEvent(work, event, EVENT_WAIT_MENU_BUTTON,
                                           (uptr)BeaconViewMenu_GetButton(work->menu, 0));
        break;
    case BEACON_VIEW_EVENT_NONE:
    default:
        return NULL;
    }
    BeaconView_ClearEventRequest(work);
    return event;
}

static GameEvent *BeaconView_CreateWaitEvent(BeaconViewWork *work, GameEvent *next, u32 wait, uptr param) {
    GameEvent *event = GameEvent_Create(work->gsys, NULL, BeaconView_WaitEvent, sizeof(BeaconViewEvent));
    BeaconViewEvent *data = GameEvent_GetData(event);

    data->work = work;
    data->gsys = work->gsys;
    data->field = work->field;
    data->next = next;
    data->wait = wait;
    data->param = param;
    work->eventRunning = TRUE;
    return event;
}

static GameEventReturnCode BeaconView_WaitEvent(GameEvent *event, u32 *state, void *data) {
    BeaconViewEvent *wait = data;
    BeaconViewWork *work = wait->work;
    BOOL done = FALSE;

    switch (wait->wait) {
    case EVENT_WAIT_BUTTON:
        done = BeaconView_WaitButton(work, wait);
        break;
    case EVENT_WAIT_TASKS:
        done = BeaconView_WaitTasks(work, wait);
        break;
    case EVENT_WAIT_MENU_BUTTON:
        done = BeaconView_WaitMenuButton(work, wait);
        break;
    case EVENT_WAIT_MEDAL:
        done = BeaconView_WaitMedal(work, wait);
        break;
    }
    if (done) {
        work->eventRunning = FALSE;
        GameEvent_Replace(event, wait->next);
    }
    return GAMEEVENT_CONTINUE;
}

static BOOL BeaconView_WaitButton(BeaconViewWork *work, BeaconViewEvent *event) {
    switch (event->seq) {
    case 0:
        BeaconView_SetButtonAnim(work, event->param, 1);
        event->seq++;
        return FALSE;
    case 1:
        if (BeaconView_IsButtonAnimating(work, event->param)) {
            return FALSE;
        }
        break;
    }
    return TRUE;
}

static BOOL BeaconView_WaitTasks(BeaconViewWork *work, BeaconViewEvent *event) {
    GFL_TCBExMgrUpdate(work->tcbManager);
    if (work->pending == 0) {
        return TRUE;
    }
    return FALSE;
}

static BOOL BeaconView_WaitMenuButton(BeaconViewWork *work, BeaconViewEvent *event) {
    AppTaskMenuWin *win = (AppTaskMenuWin *)event->param;

    switch (event->seq) {
    case 0:
        AppTaskMenuWin_SetFlashing(win, TRUE);
        event->seq++;
        break;
    case 1:
        AppTaskMenuWin_Update(win);
        if (AppTaskMenuWin_IsFlashFinished(win)) {
            AppTaskMenuWin_ResetFlash(win);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static BOOL BeaconView_WaitMedal(BeaconViewWork *work, BeaconViewEvent *event) {
    switch (event->seq) {
    case 0:
        BeaconView_SetActorAnim(work->panels[event->param].medal, 23);
        event->seq++;
        return FALSE;
    case 1:
        if (ClActor_IsAnimActive(work->panels[event->param].medal)) {
            return FALSE;
        }
        break;
    }
    return TRUE;
}

static void BeaconView_VBlankTask(TCB *tcb, void *data) {
    BeaconViewWork *work = data;

    if (work->paletteFade != NULL) {
        PaletteFade_Transfer(work->paletteFade);
    }
}

static void BeaconView_InitData(BeaconViewWork *work) {
    SaveControl *save;
    PlayerInfo *info;

    work->status = GameData_GetBeaconStatus(work->gameData);
    work->log = BeaconStatus_GetInfoTable(work->status);
    work->logCount = GameBeaconInfoTable_GetCount(work->log);
    work->top = BeaconStatus_GetLogTop(work->status);
    if (work->logCount < BEACON_VIEW_LINE_MAX) {
        work->viewCount = work->logCount;
    } else {
        work->viewCount = BEACON_VIEW_LINE_MAX;
    }
    work->usePassPower = PASS_POWER_NONE;
    save = GameData_GetSaveControl(work->gameData);
    work->passPower = HighLinkSave_GetPassPower(getHighLinkBlockAddress(save), 0);
    work->commSys = GSYS_GetGameCommSystem(work->gsys);
    work->wifiList = GameData_GetWifiList(work->gameData);
    work->bag = GameData_GetBag(work->gameData);
    work->trainerCard = getTrainerCardData_wrapper(save);
    work->passerbyCount = TrainerCard_GetPasserbyCount(work->trainerCard);
    info = SaveControl_GetPlayerInfo(save);
    work->trainerId = (u16)getIDAsUInt(info);
    work->gender = getTrainerGender(info);
    work->unk3D = PlayerInfo_GetUnionView(info);
    work->version = GAME_VERSION;
    work->newTable = GameBeaconInfoTable_Alloc(work->heapId);
    work->beacon = GameBeacon_Alloc(work->heapId);
    work->countBeacon = GameBeacon_Alloc(work->heapId);
    work->newBeacon = GameBeacon_Alloc(work->heapId);
    work->passPowerData = PassPowerData_Create(work->heapId);
    work->festText = getTextFileForFestMissions(work->heapId);
    BeaconView_CountOthers(work, &work->otherGenderCount, &work->otherVersionCount);
}

static void BeaconView_FreeData(BeaconViewWork *work) {
    GameBeaconInfoTable_MarkAllNew(work->newTable);
    FestivalText_Free(work->festText);
    PassPowerData_Free(work->passPowerData);
    GFL_HeapFree(work->newBeacon);
    GFL_HeapFree(work->countBeacon);
    GFL_HeapFree(work->beacon);
    GFL_HeapFree(work->newTable);
}

static void BeaconView_InitSystems(BeaconViewWork *work) {
    work->tcbManager = GFL_TCBExMgrCreate(work->heapId, work->heapId, 16, 0x80);
    work->vblankTask = GFL_VBlankTCBAdd(BeaconView_VBlankTask, work, 0);
    work->paletteFade = PaletteFade_Create(work->heapId);
    PaletteFade_AllocBuffer(work->paletteFade, 1, 0x1c0, work->heapId);
    PaletteFade_AllocBuffer(work->paletteFade, 3, 0x200, work->heapId);
    work->printQueue = PrintQueue_CreateDefault(work->heapId);
    PrintQueue_SetQueueAlways(work->printQueue, TRUE);
    PrintQueue_SetTimeLimit(work->printQueue, 500);
    work->font = func_ov036_0218799c(Field_GetMsgBGSys(work->field));
    GFL_TextRndResetColorIndexLUT();
    work->wordSet = GFL_WordSetSystemCreate(8, 64, work->heapId);
    work->msgData = GFL_MsgSysLoadData(TRUE, ARCID_SYSTEM_MESSAGE, TEXT_BANK_GAME_BEACON, work->heapId);
    work->strTemplate = GFL_StrBufCreate(217, work->heapId);
    work->strExpand = GFL_StrBufCreate(217, work->heapId);
    work->strMessage = GFL_StrBufCreate(217, work->heapId);
}

static void BeaconView_FreeSystems(BeaconViewWork *work) {
    GFL_StrBufFree(work->strMessage);
    GFL_StrBufFree(work->strExpand);
    GFL_StrBufFree(work->strTemplate);
    GFL_MsgDataFree(work->msgData);
    GFL_WordSetSystemFree(work->wordSet);
    PrintQueue_Clear(work->printQueue);
    PrintQueue_Free(work->printQueue);
    GFL_TextRndResetColorIndexLUT();
    PaletteFade_FreeBuffer(work->paletteFade, 3);
    PaletteFade_FreeBuffer(work->paletteFade, 1);
    PaletteFade_Free(work->paletteFade);
    work->paletteFade = NULL;
    GFL_TCBRemove(work->vblankTask);
    GFL_TCBExMgrFree(work->tcbManager);
}

static void BeaconView_InitBG(BeaconViewWork *work, ArcTool *arc) {
    static const BGSetup sBeaconViewBGSetups[] = {
        { 0, 0, 0x800, 0, 1, 0, 15, 0, 0x6000, 0, 0, 0, 0 },
        { 0, 0, 0x800, 0, 1, 0, 14, 0, 0x6000, 0, 1, 0, 0 },
        { 0, 0, 0x800, 0, 1, 0, 13, 0, 0x6000, 0, 2, 0, 0 },
        { 0, 0, 0x800, 0, 1, 0, 12, 0, 0x6000, 0, 3, 0, 0 },
    };

    GFL_BGSysCreateBG(4, &sBeaconViewBGSetups[0], 0);
    GFL_BGSysCreateBG(5, &sBeaconViewBGSetups[1], 0);
    GFL_BGSysCreateBG(6, &sBeaconViewBGSetups[2], 0);
    GFL_BGSysCreateBG(7, &sBeaconViewBGSetups[3], 0);
    MI_CpuClear32((void *)HW_DB_BG_VRAM, 0x20000);
    GXS_SetVisibleWnd(0);
    G2S_BlendNone();
    PaletteFade_LoadArcNCLR(work->paletteFade, arc, 19, work->tmpHeapId, 1, 0x1a0, 0);
    PaletteFade_LoadNCLR(work->paletteFade, ARCID_FONT, 5, work->tmpHeapId, 1, 0x20, 0xd0);
    GFL_BGSysLoadArcNCGRStatic(arc, 20, 7, 0, 0, TRUE, work->tmpHeapId);
    loadBGScrToVramByFileNoReserveNegAlign(arc, 21, 5, 0, 0x800, TRUE, work->tmpHeapId);
    loadBGScrToVramByFileNoReserveNegAlign(arc, 22, 6, 0, 0x800, TRUE, work->tmpHeapId);
    loadBGScrToVramByFileNoReserveNegAlign(arc, 23, 7, 0, 0x800, TRUE, work->tmpHeapId);
    G2x_SetBlendAlpha_(REG_DB_BLDCNT_ADDR, 4, 0x1e, 13, 3);
    GFL_BGSysSetBGEnabled(4, TRUE);
    GFL_BGSysSetBGEnabled(5, TRUE);
    GFL_BGSysSetBGEnabled(6, TRUE);
    GFL_BGSysSetBGEnabled(7, TRUE);
}

static void BeaconView_FreeBG(BeaconViewWork *work) {
    GFL_BGSysSetBGEnabled(4, FALSE);
    GFL_BGSysSetBGEnabled(5, FALSE);
    GFL_BGSysSetBGEnabled(6, FALSE);
    GFL_BGSysSetBGEnabled(7, FALSE);
    G2S_BlendNone();
    GFL_BGSysReleaseBG(4);
    GFL_BGSysReleaseBG(5);
    GFL_BGSysReleaseBG(6);
    GFL_BGSysReleaseBG(7);
}

static void BeaconView_InitMenu(BeaconViewWork *work) {
    work->keyCursor = KeyCursor_Create(9, FALSE, TRUE, work->heapId);
    work->menu = BeaconViewMenu_Create(4, 14, work->font, work->printQueue, work->heapId);
}

static void BeaconView_FreeMenu(BeaconViewWork *work) {
    BeaconViewMenu_Free(work->menu);
    KeyCursor_Free(work->keyCursor);
}

static void BeaconView_LoadResources(BeaconViewWork *work, ArcTool *arc) {
    int i;
    ArcTool *trainerArc = GFL_ArcSysCreateFileHandle(ARCID_WIFI_UNIONCHAR, work->tmpHeapId);

    {
        BeaconViewResSetup setup = { { { 1, 0 }, { 1, 1 }, { 1, 2 } } };

        BeaconView_LoadRes(work, &work->res[BEACON_VIEW_RES_COMMON], &setup, arc);
    }
    {
        BeaconViewResSetup setup = { { { 0, 0 }, { 5, 7 }, { 1, 5 } } };

        BeaconView_LoadRes(work, &work->res[BEACON_VIEW_RES_ICON], &setup, arc);
    }
    {
        BeaconViewResSetup setup = { { { 0, 0 }, { 5, 49 }, { 1, 65 } } };

        BeaconView_LoadRes(work, &work->res[BEACON_VIEW_RES_TRAINER], &setup, trainerArc);
    }
    PaletteFade_LoadArcNCLR(work->paletteFade, arc, 0, work->tmpHeapId, 3, 0x1c0, 0);
    PaletteFade_LoadNCLR(work->paletteFade, 0, 1, work->tmpHeapId, 3, 0x20, 0xe0);
    work->commonPalette.file = GFL_ArcToolReadHeapNew(arc, 4, work->heapId);
    NNS_G2dGetUnpackedPaletteData(work->commonPalette.file, &work->commonPalette.data);
    work->commonPalette.colors = work->commonPalette.data->pRawData;
    work->trainerPalette.file = GFL_ArcToolReadHeapNew(trainerArc, 0, work->heapId);
    NNS_G2dGetUnpackedPaletteData(work->trainerPalette.file, &work->trainerPalette.data);
    work->trainerPalette.colors = work->trainerPalette.data->pRawData;
    for (i = 0; i < BEACON_VIEW_TRAINER_CHARS; i++) {
        work->trainerChars[i].file = GFL_ArcToolReadHeapNew(trainerArc, i + 49, work->heapId);
        NNS_G2dGetUnpackedCharacterData(work->trainerChars[i].file, &work->trainerChars[i].data);
    }
    for (i = 0; i < BEACON_VIEW_ICON_CHARS; i++) {
        work->iconChars[i].file = GFL_ArcToolReadHeapNew(arc, i + 7, work->heapId);
        NNS_G2dGetUnpackedCharacterData(work->iconChars[i].file, &work->iconChars[i].data);
    }
    DC_FlushAll();
    GFL_ArcToolFree(trainerArc);
}

static void BeaconView_FreeResources(BeaconViewWork *work) {
    int i;

    for (i = 0; i < BEACON_VIEW_ICON_CHARS; i++) {
        GFL_HeapFree(work->iconChars[i].file);
    }
    for (i = 0; i < BEACON_VIEW_TRAINER_CHARS; i++) {
        GFL_HeapFree(work->trainerChars[i].file);
    }
    GFL_HeapFree(work->commonPalette.file);
    GFL_HeapFree(work->trainerPalette.file);
    BeaconView_FreeRes(work, &work->res[BEACON_VIEW_RES_TRAINER]);
    BeaconView_FreeRes(work, &work->res[BEACON_VIEW_RES_ICON]);
    BeaconView_FreeRes(work, &work->res[BEACON_VIEW_RES_COMMON]);
}

static void BeaconView_InitActors(BeaconViewWork *work, ArcTool *arc) {
    int i;

    work->clUnit = ClActUnit_Create(64, 0, work->resHeapId);
    {
        ClActSurfaceSetup surface = { 0, 0, 256, 192, 1, 2 };

        work->renderer = ClActRenderer_Create(&surface, 1, work->heapId);
    }
    ClActUnit_SetRenderer(work->clUnit, work->renderer);
    work->bmpOam = BmpOam_Init(work->heapId, work->clUnit);
    BeaconView_AddScreenActors(work);
    for (i = 0; i < BEACON_VIEW_PANEL_MAX; i++) {
        BeaconView_InitPanel(work, i);
    }
    GFL_BGSysSetBGEnabledB(GX_PLANEMASK_OBJ, TRUE);
}

static void BeaconView_FreeActors(BeaconViewWork *work) {
    int i;

    GFL_BGSysSetBGEnabledB(GX_PLANEMASK_OBJ, FALSE);
    for (i = 0; i < BEACON_VIEW_PANEL_MAX; i++) {
        BeaconView_FreePanel(work, i);
    }
    BeaconView_DeleteScreenActors(work);
    BmpOam_Exit(work->bmpOam);
    ClActRenderer_Delete(work->renderer);
    ClActUnit_Delete(work->clUnit);
}

static void BeaconView_InitWin(BeaconViewWork *work, BeaconViewWinSlot *slot, u8 bg, u8 palette, u8 x, u8 y, u8 width,
                               u8 height) {
    BmpWin *window;

    slot->win.font = work->font;
    slot->win.queue = work->printQueue;
    slot->win.tcbManager = work->tcbManager;
    slot->win.window = BmpWin_CreateDynamic(bg, x, y, width, height, palette, TRUE);
    slot->win.bitmap = BmpWin_GetBitmap(slot->win.window);
    PrintWindow_Init(&slot->win.printWindow, slot->win.window);
    window = slot->win.window;
    BmpWin_FlushChar(window);
    BmpWin_FlushMap(window);
    GFL_BGSysLoadScr(BmpWin_GetBGIndex(window));
}

static void BeaconView_InitWins(BeaconViewWork *work) {
    BmpOamActorSetup setup;

    BeaconView_InitWin(work, &work->wins[BEACON_VIEW_WIN_MESSAGE], 6, 1, 2, 22, 28, 6);
    BeaconView_InitWin(work, &work->wins[BEACON_VIEW_WIN_TOTAL], 4, 13, 1, 21, 15, 3);
    setup.bitmap = GFL_BitmapCreate(6, 2, 0x20, work->heapId);
    setup.x = 8;
    setup.y = 152;
    setup.palette = work->res[BEACON_VIEW_RES_COMMON].lists[BEACON_VIEW_RES_PALETTE].ids[0];
    setup.paletteOffset = 3;
    setup.priority = 0;
    setup.bgPriority = 3;
    setup.surface = 0;
    setup.vramType = CLACT_VRAM_SUB;
    BeaconViewOam_Init(&work->countOam, &setup, work->bmpOam, work->printQueue, work->font, work->tcbManager);
}

static void BeaconView_FreeWins(BeaconViewWork *work) {
    int i;

    BeaconViewOam_Free(&work->countOam);
    for (i = 0; i < BEACON_VIEW_WIN_COUNT; i++) {
        if (work->wins[i].win.window != NULL) {
            BmpWin_Free(work->wins[i].win.window);
        }
    }
}

static void BeaconView_LoadRes(BeaconViewWork *work, BeaconViewRes *res, const BeaconViewResSetup *setup,
                               ArcTool *arc) {
    int i;

    MI_CpuFill8(res, 0, sizeof(BeaconViewRes));
    for (i = 0; i < BEACON_VIEW_RES_KINDS; i++) {
        res->lists[i].count = setup->lists[i].count;
        if (res->lists[i].count != 0) {
            res->lists[i].ids =
                GFL_HeapAllocate(work->resHeapId, res->lists[i].count * sizeof(u32), TRUE, "beacon_view_setup.c", 938);
        }
    }
    for (i = 0; i < res->lists[BEACON_VIEW_RES_PALETTE].count; i++) {
        res->lists[BEACON_VIEW_RES_PALETTE].ids[i] = ClActSys_LoadPltt(
            arc, setup->lists[BEACON_VIEW_RES_PALETTE].fileId + i, CLACT_VRAM_SUB, 0, work->resHeapId);
    }
    for (i = 0; i < res->lists[BEACON_VIEW_RES_CHARS].count; i++) {
        res->lists[BEACON_VIEW_RES_CHARS].ids[i] = ClActSys_LoadChars(
            arc, setup->lists[BEACON_VIEW_RES_CHARS].fileId + i, FALSE, CLACT_VRAM_SUB, work->resHeapId);
    }
    for (i = 0; i < res->lists[BEACON_VIEW_RES_CELLS].count; i++) {
        res->lists[BEACON_VIEW_RES_CELLS].ids[i] =
            ClActSys_LoadCellAnims(arc, setup->lists[BEACON_VIEW_RES_CELLS].fileId + i * 2,
                                   setup->lists[BEACON_VIEW_RES_CELLS].fileId + 1 + i * 2, work->resHeapId);
    }
}

static void BeaconView_FreeRes(BeaconViewWork *work, BeaconViewRes *res) {
    int i;

    for (i = 0; i < res->lists[BEACON_VIEW_RES_CELLS].count; i++) {
        ClActSys_FreeCellAnims(res->lists[BEACON_VIEW_RES_CELLS].ids[i]);
    }
    for (i = 0; i < res->lists[BEACON_VIEW_RES_CHARS].count; i++) {
        ClActSys_FreeChars(res->lists[BEACON_VIEW_RES_CHARS].ids[i]);
    }
    for (i = 0; i < res->lists[BEACON_VIEW_RES_PALETTE].count; i++) {
        ClActSys_FreePltt(res->lists[BEACON_VIEW_RES_PALETTE].ids[i]);
    }
    for (i = 0; i < BEACON_VIEW_RES_KINDS; i++) {
        if (res->lists[i].ids != NULL) {
            GFL_HeapFree(res->lists[i].ids);
        }
    }
}

static ClActor *BeaconView_AddActor(BeaconViewWork *work, u32 chars, u32 palette, u32 cellAnims, s16 x, s16 y,
                                    u8 sequence, u8 bgPriority, u8 priority) {
    ClActorSetup setup;
    ClActor *actor;

    setup.x = x;
    setup.y = y;
    setup.sequence = sequence;
    setup.bgPriority = bgPriority;
    setup.priority = priority;
    actor = ClActor_Create(work->clUnit, chars, palette, cellAnims, &setup, 0, work->resHeapId);
    ClActor_SetAutoAnim(actor, FALSE);
    ClActor_SetDrawAnim(actor, FALSE);
    return actor;
}

static void BeaconView_AddScreenActors(BeaconViewWork *work) {
    static const BeaconViewActorSetup sBeaconViewActorSetups[BEACON_VIEW_ACTOR_COUNT] = {
        { 132, 172, 1, 1 },  { 164, 172, 4, 1 }, { 196, 172, 7, 1 }, { 228, 172, 10, 1 }, { 232, 24, 16, 3 },
        { 232, 144, 20, 3 }, { 232, 44, 21, 3 }, { 232, 44, 27, 3 }, { 0, 152, 26, 3 },
    };
    int i;

    for (i = 0; i < BEACON_VIEW_ACTOR_COUNT; i++) {
        work->actors[i] = BeaconView_AddActor(
            work, work->res[BEACON_VIEW_RES_COMMON].lists[BEACON_VIEW_RES_CHARS].ids[0],
            work->res[BEACON_VIEW_RES_COMMON].lists[BEACON_VIEW_RES_PALETTE].ids[0],
            work->res[BEACON_VIEW_RES_COMMON].lists[BEACON_VIEW_RES_CELLS].ids[0], sBeaconViewActorSetups[i].x,
            sBeaconViewActorSetups[i].y, sBeaconViewActorSetups[i].sequence, sBeaconViewActorSetups[i].bgPriority, 2);
        ClActor_SetVisible(work->actors[i], TRUE);
        ClActor_SetAutoAnim(work->actors[i], i < BEACON_VIEW_ACTOR_UP ? TRUE : FALSE);
    }
}

static void BeaconView_DeleteScreenActors(BeaconViewWork *work) {
    int i;

    for (i = 0; i < BEACON_VIEW_ACTOR_COUNT; i++) {
        ClActor_SetVisible(work->actors[i], FALSE);
        ClActor_Delete(work->actors[i]);
    }
}

static void BeaconView_InitPanel(BeaconViewWork *work, u8 id) {
    int i;
    BeaconViewPanel *panel = &work->panels[id];
    BmpOamActorSetup setup;

    MI_CpuFill8(panel, 0, sizeof(BeaconViewPanel));
    panel->id = id;
    panel->dataIndex = 0xff;
    panel->line = 0;
    panel->queue = work->printQueue;
    panel->x = 8;
    panel->y = -40 * -id;
    panel->base = BeaconView_AddActor(work, work->res[BEACON_VIEW_RES_COMMON].lists[BEACON_VIEW_RES_CHARS].ids[0],
                                      work->res[BEACON_VIEW_RES_COMMON].lists[BEACON_VIEW_RES_PALETTE].ids[0],
                                      work->res[BEACON_VIEW_RES_COMMON].lists[BEACON_VIEW_RES_CELLS].ids[0], panel->x,
                                      panel->y, 0, 3, id + 31);
    ClActor_SetPalette(panel->base, id, 0);
    panel->trainer = BeaconView_AddActor(work, work->res[BEACON_VIEW_RES_TRAINER].lists[BEACON_VIEW_RES_CHARS].ids[id],
                                         work->res[BEACON_VIEW_RES_COMMON].lists[BEACON_VIEW_RES_PALETTE].ids[0],
                                         work->res[BEACON_VIEW_RES_TRAINER].lists[BEACON_VIEW_RES_CELLS].ids[0],
                                         panel->x + 24, panel->y + 24, 0, 3, id + 21);
    ClActor_SetPalette(panel->trainer, (u8)(id + 9), 1);
    panel->icon = BeaconView_AddActor(work, work->res[BEACON_VIEW_RES_ICON].lists[BEACON_VIEW_RES_CHARS].ids[id],
                                      work->res[BEACON_VIEW_RES_COMMON].lists[BEACON_VIEW_RES_PALETTE].ids[0],
                                      work->res[BEACON_VIEW_RES_ICON].lists[BEACON_VIEW_RES_CELLS].ids[0],
                                      panel->x + 52, panel->y + 24, 0, 3, id + 16);
    panel->medal = BeaconView_AddActor(work, work->res[BEACON_VIEW_RES_COMMON].lists[BEACON_VIEW_RES_CHARS].ids[0],
                                       work->res[BEACON_VIEW_RES_COMMON].lists[BEACON_VIEW_RES_PALETTE].ids[0],
                                       work->res[BEACON_VIEW_RES_COMMON].lists[BEACON_VIEW_RES_CELLS].ids[0],
                                       panel->x + 188, panel->y + 12, 22, 3, id + 11);
    ClActor_SetAutoAnim(panel->medal, TRUE);
    for (i = 0; i < BEACON_VIEW_PANEL_DIGITS; i++) {
        panel->digits[i] =
            BeaconView_AddActor(work, work->res[BEACON_VIEW_RES_COMMON].lists[BEACON_VIEW_RES_CHARS].ids[0],
                                work->res[BEACON_VIEW_RES_COMMON].lists[BEACON_VIEW_RES_PALETTE].ids[0],
                                work->res[BEACON_VIEW_RES_COMMON].lists[BEACON_VIEW_RES_CELLS].ids[0],
                                panel->x + 200 - i * 8, panel->y + 28, 25, 3, id + 6);
        ClActor_SetPalette(panel->digits[i], id, 0);
        ClActor_SetVisible(panel->digits[i], FALSE);
    }
    setup.bitmap = GFL_BitmapCreate(12, 2, 0x20, work->heapId);
    setup.x = panel->x + 72;
    setup.y = panel->y + 16;
    setup.palette = work->res[BEACON_VIEW_RES_COMMON].lists[BEACON_VIEW_RES_PALETTE].ids[0];
    setup.paletteOffset = 3;
    setup.priority = 1;
    setup.bgPriority = 3;
    setup.surface = 0;
    setup.vramType = CLACT_VRAM_SUB;
    BeaconViewOam_Init(&panel->oam, &setup, work->bmpOam, work->printQueue, work->font, work->tcbManager);
    panel->greeting = GFL_StrBufCreate(11, work->heapId);
    panel->name = GFL_StrBufCreate(8, work->heapId);
    GFL_BitmapFill(panel->oam.bitmap, 0);
    BmpOam_ActorBmpTrans(panel->oam.actor);
    BmpOam_ActorSetDrawEnable(panel->oam.actor, FALSE);
    ClActor_SetVisible(panel->base, FALSE);
    ClActor_SetVisible(panel->trainer, FALSE);
    ClActor_SetVisible(panel->icon, FALSE);
    ClActor_SetVisible(panel->medal, FALSE);
}

static void BeaconView_FreePanel(BeaconViewWork *work, u8 id) {
    int i;
    BeaconViewPanel *panel = &work->panels[id];

    GFL_StrBufFree(panel->name);
    GFL_StrBufFree(panel->greeting);
    BeaconViewOam_Free(&panel->oam);
    for (i = 0; i < BEACON_VIEW_PANEL_DIGITS; i++) {
        ClActor_SetVisible(panel->digits[i], FALSE);
        ClActor_Delete(panel->digits[i]);
    }
    ClActor_SetVisible(panel->medal, FALSE);
    ClActor_Delete(panel->medal);
    ClActor_SetVisible(panel->icon, FALSE);
    ClActor_Delete(panel->icon);
    ClActor_SetVisible(panel->trainer, FALSE);
    ClActor_Delete(panel->trainer);
    ClActor_SetVisible(panel->base, FALSE);
    ClActor_Delete(panel->base);
    MI_CpuFill8(panel, 0, sizeof(BeaconViewPanel));
}
