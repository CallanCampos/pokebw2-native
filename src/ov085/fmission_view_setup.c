#include "types.h"
#include "constants/arc.h"
#include "constants/text_banks.h"
#include "constants/version.h"
#include "field/beacon_view_common.h"
#include "field/event_fest_mission.h"
#include "field/event_festival.h"
#include "field/event_pass_power.h"
#include "field/event_phrase_input.h"
#include "field/fest_mission_data.h"
#include "field/festival.h"
#include "field/field.h"
#include "field/field_menu.h"
#include "field/field_msgbg.h"
#include "field/field_pass_power.h"
#include "field/fmission_log_accessor_fv.h"
#include "field/fmission_view.h"
#include "field/fmission_view_local.h"
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
#include "system/game_data.h"
#include "system/game_event.h"
#include "system/game_system.h"
#include "system/gf_font.h"
#include "system/palanm.h"
#include "system/printsys.h"
#include "system/wordset.h"

// Sets the Funfest mission's view up and frees it, and makes the events of the screens its buttons open, named after
// the ROM's "fmission_view_setup.c". The names are ours

#define OVERLAY_EVENT_FESTIVAL OVERLAY_ID(21)
#define OVERLAY_EVENT_PASS_POWER OVERLAY_ID(14)

// What the view's event does before it hands over to the screen's event
enum {
    // Shows a button pressed, and waits for its animation
    CALL_BUTTON,
    // Waits for the view's tasks
    CALL_TASKS,
    // Flashes a button of the menu
    CALL_MENU_BUTTON,
};

typedef struct {
    int seq;
    GameSystem *gsys;
    Field *field;
    FMissionView *view;
    GameEvent *next;
    int type;
    // A FMISSION_VIEW_BUTTON_ for CALL_BUTTON, an AppTaskMenuWin for CALL_MENU_BUTTON
    uptr param;
} FMissionViewCallEvent;

// How many resources of a kind there are, and the first one's file
typedef struct {
    u16 count;
    u16 fileId;
} FMissionViewResSetup;

typedef struct {
    u8 x;
    u8 y;
} FMissionViewButtonPos;

static GameEvent *FMissionViewSetup_CreateCallEvent(FMissionView *view, GameEvent *next, int type, uptr param);
static GameEventReturnCode FMissionViewSetup_CallEvent(GameEvent *event, u32 *state, void *data);
static BOOL FMissionViewSetup_CallButton(FMissionView *view, FMissionViewCallEvent *call);
static BOOL FMissionViewSetup_CallTasks(FMissionView *view, FMissionViewCallEvent *call);
static BOOL FMissionViewSetup_CallMenuButton(FMissionView *view, FMissionViewCallEvent *call);
static void FMissionViewSetup_VBlank(TCB *tcb, void *data);
static void FMissionViewSetup_InitData(FMissionView *view);
static void FMissionViewSetup_FreeData(FMissionView *view);
static void FMissionViewSetup_InitSystems(FMissionView *view);
static void FMissionViewSetup_FreeSystems(FMissionView *view);
static void FMissionViewSetup_InitBG(FMissionView *view, ArcTool *arc);
static void FMissionViewSetup_FreeBG(FMissionView *view);
static void FMissionViewSetup_InitMenu(FMissionView *view);
static void FMissionViewSetup_FreeMenu(FMissionView *view);
static void FMissionViewSetup_LoadResources(FMissionView *view, ArcTool *arc);
static void FMissionViewSetup_FreeResources(FMissionView *view);
static void FMissionViewSetup_InitActors(FMissionView *view, ArcTool *arc);
static void FMissionViewSetup_FreeActors(FMissionView *view);
static void FMissionViewSetup_InitWin(FMissionView *view, BeaconViewWin *win, u8 bg, u8 palette, u8 x, u8 y, u8 width,
                                      u8 height);
static void FMissionViewSetup_InitWins(FMissionView *view);
static void FMissionViewSetup_FreeWins(FMissionView *view);
static void FMissionViewSetup_LoadRes(FMissionView *view, FMissionViewRes *res, const FMissionViewResSetup *setup,
                                      ArcTool *arc, BOOL compressed);
static void FMissionViewSetup_FreeRes(FMissionView *view, FMissionViewRes *res);
static ClActor *FMissionViewSetup_AddActor(FMissionView *view, u32 chars, u32 palette, u32 cellAnims, s16 x, s16 y,
                                           u8 sequence, u8 bgPriority, u8 priority);
static void FMissionViewSetup_InitButtons(FMissionView *view);
static void FMissionViewSetup_FreeButtons(FMissionView *view);
static void FMissionViewSetup_InitLine(FMissionView *view, BeaconViewOam *line, u8 width, u8 height, s16 x, s16 y);
static void FMissionViewSetup_InitPanel(FMissionView *view, u8 index);
static void FMissionViewSetup_FreePanel(FMissionView *view, u8 index);

// The four BGs of the sub screen, from the front: the menu's buttons, the windows, the message and the background
static const BGSetup sFMissionViewBGSetups[4] = {
    { 0, 0, 0x800, 0, BGRES_256x256, GX_BG_COLORMODE_16, GX_BG_SCRBASE(0x7800), GX_BG_CHARBASE(0x0000), 0x6000,
      GX_BG_EXTPLTT_01, 0, 0, 0 },
    { 0, 0, 0x800, 0, BGRES_256x256, GX_BG_COLORMODE_16, GX_BG_SCRBASE(0x7000), GX_BG_CHARBASE(0x0000), 0x6000,
      GX_BG_EXTPLTT_01, 1, 0, 0 },
    { 0, 0, 0x800, 0, BGRES_256x256, GX_BG_COLORMODE_16, GX_BG_SCRBASE(0x6800), GX_BG_CHARBASE(0x0000), 0x6000,
      GX_BG_EXTPLTT_01, 2, 0, 0 },
    { 0, 0, 0x800, 0, BGRES_256x256, GX_BG_COLORMODE_16, GX_BG_SCRBASE(0x6000), GX_BG_CHARBASE(0x0000), 0x6000,
      GX_BG_EXTPLTT_01, 3, 0, 0 },
};

FMissionView *FMissionViewSetup_Create(GameSystem *gsys, FieldSubscreen *subscreen) {
    FMissionView *view = GFL_HeapAllocate(HEAPID_FIELD_MENU, sizeof(FMissionView), TRUE, "fmission_view_setup.c", 93);

    view->gsys = gsys;
    view->gameData = GSYS_GetGameData(gsys);
    view->field = GSYS_GetField(gsys);
    view->dispControl = FieldG2D_GetDispControl(view->field);
    view->festival = GSYS_GetLinkFestival(gsys);
    view->config = GetFestMissionCfg(view->festival);
    view->subscreen = subscreen;
    view->wifiList = GameData_GetWifiList(view->gameData);
    view->heapId = HEAPID_FIELD_MENU;
    view->fieldHeapId = HEAPID_FIELDMAP;
    view->tmpHeapId = HEAPID_TAIL(HEAPID_FIELD_MENU);
    view->archive = GFL_ArcSysCreateFileHandle(ARCID_FMISSION_VIEW, view->heapId);
    FMissionViewSetup_InitData(view);
    FMissionViewSetup_InitSystems(view);
    FMissionViewSetup_InitBG(view, view->archive);
    FMissionViewSetup_InitMenu(view);
    FMissionViewSetup_LoadResources(view, view->archive);
    FMissionViewSetup_InitActors(view, view->archive);
    FMissionViewSetup_InitWins(view);
    return view;
}

void FMissionViewSetup_Free(FMissionView *view) {
    GFL_TCBExMgrFreeTasks(view->tcbManager);
    FMissionViewSetup_FreeWins(view);
    FMissionViewSetup_FreeActors(view);
    FMissionViewSetup_FreeResources(view);
    FMissionViewSetup_FreeMenu(view);
    FMissionViewSetup_FreeBG(view);
    FMissionViewSetup_FreeSystems(view);
    FMissionViewSetup_FreeData(view);
    GFL_ArcToolFree(view->archive);
    GFL_HeapFree(view);
}

void FMissionViewSetup_RequestEvent(FMissionView *view, u16 request) {
    view->eventRequest = request;
    view->eventDone = FALSE;
}

void FMissionViewSetup_ClearEventRequest(FMissionView *view) {
    view->eventRequest = FMISSION_VIEW_EVENT_NONE;
}

// Makes the event of the screen that was asked for, wrapped in an event that lets the view finish first
GameEvent *FMissionViewSetup_CheckEvent(FMissionView *view, BOOL enabled) {
    GameEvent *event;

    if (!enabled) {
        return NULL;
    }
    switch (view->eventRequest) {
    case FMISSION_VIEW_EVENT_MENU_END_COMM:
        func_ov012_02153654(view->commSys);
        // fallthrough
    case FMISSION_VIEW_EVENT_MENU:
    case FMISSION_VIEW_EVENT_MENU_3:
        FMissionLogFV_SetTop(view->log, 0);
        event = FMissionViewSetup_CreateCallEvent(view, EventFieldMenuReturn_Create(view->gsys, view->field, 0),
                                                  CALL_BUTTON, FMISSION_VIEW_BUTTON_QUIT);
        break;
    case FMISSION_VIEW_EVENT_INFO: {
        u32 args = 0;

        event = FMissionViewSetup_CreateCallEvent(
            view, GameEvent_CreateOverlayDelegate(view->gsys, OVERLAY_EVENT_FESTIVAL, EventFestival_Create, &args),
            CALL_BUTTON, FMISSION_VIEW_BUTTON_INFO);
        break;
    }
    case FMISSION_VIEW_EVENT_QUIT:
        event = FMissionViewSetup_CreateCallEvent(view,
                                                  GameEvent_CreateOverlayDelegate(view->gsys, OVERLAY_FUNFEST_MISSION,
                                                                                  EventFestMissionRetire_Create,
                                                                                  view->field),
                                                  CALL_MENU_BUTTON, (uptr)BeaconViewMenu_GetButton(view->menu, 0));
        break;
    case FMISSION_VIEW_EVENT_PASS_POWER: {
        u32 args = 1;

        event = FMissionViewSetup_CreateCallEvent(
            view,
            GameEvent_CreateOverlayDelegate(view->gsys, OVERLAY_EVENT_PASS_POWER,
                                            (GameEventProvider)EventPassPower_Create, &args),
            CALL_BUTTON, FMISSION_VIEW_BUTTON_PASS_POWER);
        break;
    }
    case FMISSION_VIEW_EVENT_USE_PASS_POWER: {
        u32 args[2];

        args[0] = view->usePassPower;
        args[1] = view->unk89;
        event = FMissionViewSetup_CreateCallEvent(
            view,
            GameEvent_CreateOverlayDelegate(view->gsys, OVERLAY_FIELD_PASS_POWER, EventPassPowerActivate_Create, args),
            CALL_MENU_BUTTON, (uptr)BeaconViewMenu_GetButton(view->menu, 1));
        view->usePassPower = PASS_POWER_NONE;
        break;
    }
    case FMISSION_VIEW_EVENT_PASS_POWER_LIST:
        event = FMissionViewSetup_CreateCallEvent(
            view,
            GameEvent_CreateOverlayDelegate(view->gsys, OVERLAY_FIELD_PASS_POWER,
                                            (GameEventProvider)EventPassPowerList_Create, view->field),
            CALL_MENU_BUTTON, (uptr)BeaconViewMenu_GetButton(view->menu, 0));
        break;
    case FMISSION_VIEW_EVENT_PASS_POWER_LIST_9:
        event = FMissionViewSetup_CreateCallEvent(
            view,
            GameEvent_CreateOverlayDelegate(view->gsys, OVERLAY_FIELD_PASS_POWER,
                                            (GameEventProvider)EventPassPowerList_Create, view->field),
            CALL_MENU_BUTTON, (uptr)BeaconViewMenu_GetButton(view->menu, 0));
        view->unk80 = 0;
        break;
    case FMISSION_VIEW_EVENT_GREETING:
        event = FMissionViewSetup_CreateCallEvent(
            view,
            EventPhraseInput_Create(view->gsys, view->field, NULL, NAME_ENTRY_TRAINER_MESSAGE,
                                    (u16 *)BeaconStatus_GetGreetingChanged(view->beaconStatus)),
            CALL_BUTTON, FMISSION_VIEW_BUTTON_GREETING);
        break;
    default:
        return NULL;
    }
    FMissionViewSetup_ClearEventRequest(view);
    return event;
}

static GameEvent *FMissionViewSetup_CreateCallEvent(FMissionView *view, GameEvent *next, int type, uptr param) {
    GameEvent *event = GameEvent_Create(view->gsys, NULL, FMissionViewSetup_CallEvent, sizeof(FMissionViewCallEvent));
    FMissionViewCallEvent *call = GameEvent_GetData(event);

    call->view = view;
    call->gsys = view->gsys;
    call->field = view->field;
    call->next = next;
    call->type = type;
    call->param = param;
    view->eventRunning = TRUE;
    return event;
}

static GameEventReturnCode FMissionViewSetup_CallEvent(GameEvent *event, u32 *state, void *data) {
    FMissionViewCallEvent *call = data;
    BOOL done = FALSE;
    FMissionView *view = call->view;

    switch (call->type) {
    case CALL_BUTTON:
        done = FMissionViewSetup_CallButton(view, call);
        break;
    case CALL_TASKS:
        done = FMissionViewSetup_CallTasks(view, call);
        break;
    case CALL_MENU_BUTTON:
        done = FMissionViewSetup_CallMenuButton(view, call);
        break;
    }
    if (done) {
        view->eventRunning = FALSE;
        view->eventDone = TRUE;
        GameEvent_Replace(event, call->next);
    }
    return GAMEEVENT_CONTINUE;
}

static BOOL FMissionViewSetup_CallButton(FMissionView *view, FMissionViewCallEvent *call) {
    switch (call->seq) {
    case 0:
        FMissionView_SetButtonState(view, call->param, FMISSION_VIEW_BUTTON_STATE_PRESSED);
        call->seq++;
        return FALSE;
    case 1:
        if (FMissionView_IsButtonAnimating(view, call->param)) {
            return FALSE;
        }
        break;
    }
    return TRUE;
}

static BOOL FMissionViewSetup_CallTasks(FMissionView *view, FMissionViewCallEvent *call) {
    GFL_TCBExMgrUpdate(view->tcbManager);
    if (view->pending == 0) {
        return TRUE;
    }
    return FALSE;
}

static BOOL FMissionViewSetup_CallMenuButton(FMissionView *view, FMissionViewCallEvent *call) {
    AppTaskMenuWin *button = (AppTaskMenuWin *)call->param;

    switch (call->seq) {
    case 0:
        AppTaskMenuWin_SetFlashing(button, TRUE);
        call->seq++;
        break;
    case 1:
        AppTaskMenuWin_Update(button);
        if (AppTaskMenuWin_IsFlashFinished(button)) {
            AppTaskMenuWin_ResetFlash(button);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static void FMissionViewSetup_VBlank(TCB *tcb, void *data) {
    FMissionView *view = data;

    if (view->fade != NULL) {
        PaletteFade_Transfer(view->fade);
    }
    FMissionView_TransferPanels(view);
}

static void FMissionViewSetup_InitData(FMissionView *view) {
    SaveControl *save;
    PlayerInfo *info;

    view->beaconStatus = GameData_GetBeaconStatus(view->gameData);
    view->newLog = LinkFestival_GetLog(view->festival);
    view->log = LinkFestival_GetViewLog(view->festival);
    view->passPowers = LinkFestival_GetPassPowers(view->festival);
    view->logCount = FMissionLogFV_GetCount(view->log);
    view->logTop = FMissionLogFV_GetTop(view->log);
    if (view->logCount < FMISSION_VIEW_PANEL_COUNT) {
        view->logVisible = view->logCount;
    } else {
        view->logVisible = FMISSION_VIEW_PANEL_COUNT;
    }
    view->usePassPower = PASS_POWER_NONE;
    save = GameData_GetSaveControl(view->gameData);
    view->passPower = HighLinkSave_GetPassPower(getHighLinkBlockAddress(save), 0);
    view->commSys = GSYS_GetGameCommSystem(view->gsys);
    view->wifiList = GameData_GetWifiList(view->gameData);
    view->bag = GameData_GetBag(view->gameData);
    view->trainerCard = getTrainerCardData_wrapper(save);
    view->unk33C = TrainerCard_GetPasserbyCount(view->trainerCard);
    info = SaveControl_GetPlayerInfo(save);
    view->trainerId = getTrainerID(info);
    view->gender = getTrainerGender(info);
    view->unk3D = PlayerInfo_GetUnionView(info);
    view->version = GAME_VERSION;
    view->entry = FMissionLogFV_AllocEntry(view->heapId);
    view->unk54 = FMissionLogFV_AllocEntry(view->heapId);
    view->newEntry = FMissionLogFV_AllocEntry(view->heapId);
    view->passPowerData = PassPowerData_Create(view->heapId);
    view->festText = getTextFileForFestMissions(view->heapId);
}

static void FMissionViewSetup_FreeData(FMissionView *view) {
    FestivalText_Free(view->festText);
    PassPowerData_Free(view->passPowerData);
    GFL_HeapFree(view->newEntry);
    GFL_HeapFree(view->unk54);
    GFL_HeapFree(view->entry);
}

static void FMissionViewSetup_InitSystems(FMissionView *view) {
    view->tcbManager = GFL_TCBExMgrCreate(view->heapId, view->heapId, 16, 128);
    view->vblankTask = GFL_VBlankTCBAdd(FMissionViewSetup_VBlank, view, 0);
    view->fade = PaletteFade_Create(view->heapId);
    PaletteFade_AllocBuffer(view->fade, PALFADE_BUFFER_SUB_BG, 0x1c0, view->heapId);
    PaletteFade_AllocBuffer(view->fade, PALFADE_BUFFER_SUB_OBJ, 0x200, view->heapId);
    view->printQueue = PrintQueue_CreateDefault(view->heapId);
    PrintQueue_SetQueueAlways(view->printQueue, TRUE);
    PrintQueue_SetTimeLimit(view->printQueue, 500);
    view->font = func_ov036_0218799c(Field_GetMsgBGSys(view->field));
    GFL_TextRndResetColorIndexLUT();
    view->wordSet = GFL_WordSetSystemCreateDefault(view->heapId);
    view->msgData = GFL_MsgSysLoadData(TRUE, ARCID_SYSTEM_MESSAGE, TEXT_BANK_FMISSION_VIEW_SETUP, view->heapId);
    view->tmpStr = GFL_StrBufCreate(217, view->heapId);
    view->message = GFL_StrBufCreate(217, view->heapId);
    view->messageCopy = GFL_StrBufCreate(217, view->heapId);
}

static void FMissionViewSetup_FreeSystems(FMissionView *view) {
    GFL_StrBufFree(view->messageCopy);
    GFL_StrBufFree(view->message);
    GFL_StrBufFree(view->tmpStr);
    GFL_MsgDataFree(view->msgData);
    GFL_WordSetSystemFree(view->wordSet);
    PrintQueue_Clear(view->printQueue);
    PrintQueue_Free(view->printQueue);
    GFL_TextRndResetColorIndexLUT();
    PaletteFade_FreeBuffer(view->fade, PALFADE_BUFFER_SUB_OBJ);
    PaletteFade_FreeBuffer(view->fade, PALFADE_BUFFER_SUB_BG);
    PaletteFade_Free(view->fade);
    view->fade = NULL;
    GFL_TCBRemove(view->vblankTask);
    GFL_TCBExMgrFreeTasks(view->tcbManager);
    GFL_TCBExMgrFree(view->tcbManager);
}

static void FMissionViewSetup_InitBG(FMissionView *view, ArcTool *arc) {
    GFL_BGSysCreateBG(4, &sFMissionViewBGSetups[0], BGMODE_TEXT);
    GFL_BGSysCreateBG(5, &sFMissionViewBGSetups[1], BGMODE_TEXT);
    GFL_BGSysCreateBG(6, &sFMissionViewBGSetups[2], BGMODE_TEXT);
    GFL_BGSysCreateBG(7, &sFMissionViewBGSetups[3], BGMODE_TEXT);
    MIi_CpuClear32(0, (void *)HW_DB_BG_VRAM, HW_DB_BG_VRAM_SIZE);
    GXS_SetVisibleWnd(GX_WNDMASK_NONE);
    G2S_BlendNone();
    PaletteFade_LoadArcNCLR(view->fade, arc, 4, view->tmpHeapId, PALFADE_BUFFER_SUB_BG, 0x1a0, 0);
    PaletteFade_LoadNCLR(view->fade, ARCID_FONT, 5, view->tmpHeapId, PALFADE_BUFFER_SUB_BG, 0x20, 0xd0);
    GFL_BGSysLoadArcNCGRStatic(arc, 5, 7, 0, 0, TRUE, view->tmpHeapId);
    loadBGScrToVramByFileNoReserveNegAlign(arc, 6, 5, 0, 0x800, TRUE, view->tmpHeapId);
    loadBGScrToVramByFileNoReserveNegAlign(arc, 7, 6, 0, 0x800, TRUE, view->tmpHeapId);
    loadBGScrToVramByFileNoReserveNegAlign(arc, 8, 7, 0, 0x800, TRUE, view->tmpHeapId);
    G2x_SetBlendAlpha_(
        REG_DB_BLDCNT_ADDR, GX_BLEND_PLANEMASK_BG2,
        GX_BLEND_PLANEMASK_BG1 | GX_BLEND_PLANEMASK_BG2 | GX_BLEND_PLANEMASK_BG3 | GX_BLEND_PLANEMASK_OBJ, 13, 3);
    GFL_BGSysSetBGEnabled(4, TRUE);
    GFL_BGSysSetBGEnabled(5, TRUE);
    GFL_BGSysSetBGEnabled(6, TRUE);
    GFL_BGSysSetBGEnabled(7, TRUE);
}

static void FMissionViewSetup_FreeBG(FMissionView *view) {
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

static void FMissionViewSetup_InitMenu(FMissionView *view) {
    view->keyCursor = KeyCursor_Create(9, FALSE, TRUE, view->heapId);
    view->menu = BeaconViewMenu_Create(4, 14, view->font, view->printQueue, view->heapId);
}

static void FMissionViewSetup_FreeMenu(FMissionView *view) {
    BeaconViewMenu_Free(view->menu);
    KeyCursor_Free(view->keyCursor);
}

static void FMissionViewSetup_LoadResources(FMissionView *view, ArcTool *arc) {
    ArcTool *iconArc = GFL_ArcSysCreateFileHandle(ARCID_WIFI_UNIONCHAR, view->tmpHeapId);
    int i;

    {
        FMissionViewResSetup setup[FMISSION_VIEW_RES_TYPE_COUNT] = { { 1, 0 }, { 1, 1 }, { 1, 2 } };

        FMissionViewSetup_LoadRes(view, &view->res[FMISSION_VIEW_RES_MAIN], setup, arc, TRUE);
    }
    {
        FMissionViewResSetup setup[FMISSION_VIEW_RES_TYPE_COUNT] = { { 0, 0 },
                                                                     { FMISSION_VIEW_PANEL_COUNT, 49 },
                                                                     { 1, 65 } };

        FMissionViewSetup_LoadRes(view, &view->res[FMISSION_VIEW_RES_ICON], setup, iconArc, FALSE);
    }
    PaletteFade_LoadArcNCLR(view->fade, arc, 0, view->tmpHeapId, PALFADE_BUFFER_SUB_OBJ, 0x1c0, 0);
    PaletteFade_LoadNCLR(view->fade, 0, 1, view->tmpHeapId, PALFADE_BUFFER_SUB_OBJ, 0x20, 0xe0);
    view->iconPaletteFile = GFL_ArcToolReadHeapNew(iconArc, 0, view->heapId);
    NNS_G2dGetUnpackedPaletteData(view->iconPaletteFile, &view->iconPaletteData);
    view->iconPalettes = view->iconPaletteData->pRawData;
    for (i = 0; i < FMISSION_VIEW_LOOK_COUNT; i++) {
        view->iconChars[i].file = GFL_ArcToolReadHeapNew(iconArc, i + 49, view->heapId);
        NNS_G2dGetUnpackedCharacterData(view->iconChars[i].file, &view->iconChars[i].data);
    }
    DC_FlushAll();
    GFL_ArcToolFree(iconArc);
}

static void FMissionViewSetup_FreeResources(FMissionView *view) {
    int i;

    for (i = 0; i < FMISSION_VIEW_LOOK_COUNT; i++) {
        GFL_HeapFree(view->iconChars[i].file);
    }
    GFL_HeapFree(view->iconPaletteFile);
    FMissionViewSetup_FreeRes(view, &view->res[FMISSION_VIEW_RES_ICON]);
    FMissionViewSetup_FreeRes(view, &view->res[FMISSION_VIEW_RES_MAIN]);
}

static void FMissionViewSetup_InitActors(FMissionView *view, ArcTool *arc) {
    int i;

    view->clUnit = ClActUnit_Create(64, 0, view->fieldHeapId);
    {
        ClActSurfaceSetup surface = { 0, 0, 256, 192, 1, 2 };

        view->renderer = ClActRenderer_Create(&surface, 1, view->heapId);
    }
    ClActUnit_SetRenderer(view->clUnit, view->renderer);
    view->bmpOam = BmpOam_Init(view->heapId, view->clUnit);
    FMissionViewSetup_InitButtons(view);
    for (i = 0; i < FMISSION_VIEW_PANEL_COUNT; i++) {
        FMissionViewSetup_InitPanel(view, i);
    }
    GFL_BGSysSetBGEnabledB(GX_PLANEMASK_OBJ, TRUE);
}

static void FMissionViewSetup_FreeActors(FMissionView *view) {
    int i;

    GFL_BGSysSetBGEnabledB(GX_PLANEMASK_OBJ, FALSE);
    for (i = 0; i < FMISSION_VIEW_PANEL_COUNT; i++) {
        FMissionViewSetup_FreePanel(view, i);
    }
    FMissionViewSetup_FreeButtons(view);
    BmpOam_Exit(view->bmpOam);
    ClActRenderer_Delete(view->renderer);
    ClActUnit_Delete(view->clUnit);
}

static void FMissionViewSetup_InitWin(FMissionView *view, BeaconViewWin *win, u8 bg, u8 palette, u8 x, u8 y, u8 width,
                                      u8 height) {
    win->font = view->font;
    win->queue = view->printQueue;
    win->tcbManager = view->tcbManager;
    win->window = BmpWin_CreateDynamic(bg, x, y, width, height, palette, TRUE);
    win->bitmap = BmpWin_GetBitmap(win->window);
    PrintWindow_Init(&win->printWindow, win->window);
    BmpWin_TransferNow(win->window);
}

static void FMissionViewSetup_InitWins(FMissionView *view) {
    FMissionViewSetup_InitWin(view, &view->wins[FMISSION_VIEW_WIN_MESSAGE].win, 6, 1, 2, 22, 28, 6);
    FMissionViewSetup_InitWin(view, &view->wins[FMISSION_VIEW_WIN_MISSION].win, 5, 1, 5, 0, 23, 3);
    FMissionViewSetup_InitWin(view, &view->wins[FMISSION_VIEW_WIN_GOAL].win, 5, 1, 2, 3, 15, 2);
    FMissionViewSetup_InitWin(view, &view->wins[FMISSION_VIEW_WIN_MEMBERS].win, 5, 1, 18, 3, 12, 2);
}

static void FMissionViewSetup_FreeWins(FMissionView *view) {
    int i;

    for (i = 0; i < FMISSION_VIEW_WIN_COUNT; i++) {
        if (view->wins[i].win.window != NULL) {
            BmpWin_Free(view->wins[i].win.window);
        }
    }
}

// Loads the palettes, characters and cells with animations that the setup lists, from consecutive files
static void FMissionViewSetup_LoadRes(FMissionView *view, FMissionViewRes *res, const FMissionViewResSetup *setup,
                                      ArcTool *arc, BOOL compressed) {
    int i;

    MI_CpuFill8(res, 0, sizeof(FMissionViewRes));
    for (i = 0; i < FMISSION_VIEW_RES_TYPE_COUNT; i++) {
        res->lists[i].count = setup[i].count;
        if (res->lists[i].count != 0) {
            res->lists[i].ids = GFL_HeapAllocate(view->fieldHeapId, res->lists[i].count * sizeof(u32), TRUE,
                                                 "fmission_view_setup.c", 848);
        }
    }
    for (i = 0; i < res->lists[FMISSION_VIEW_RES_PALETTE].count; i++) {
        res->lists[FMISSION_VIEW_RES_PALETTE].ids[i] =
            ClActSys_LoadPltt(arc, setup[FMISSION_VIEW_RES_PALETTE].fileId + i, CLACT_VRAM_SUB, 0, view->fieldHeapId);
    }
    for (i = 0; i < res->lists[FMISSION_VIEW_RES_CHARS].count; i++) {
        res->lists[FMISSION_VIEW_RES_CHARS].ids[i] = ClActSys_LoadChars(arc, setup[FMISSION_VIEW_RES_CHARS].fileId + i,
                                                                        compressed, CLACT_VRAM_SUB, view->fieldHeapId);
    }
    for (i = 0; i < res->lists[FMISSION_VIEW_RES_CELL_ANIMS].count; i++) {
        res->lists[FMISSION_VIEW_RES_CELL_ANIMS].ids[i] =
            ClActSys_LoadCellAnims(arc, setup[FMISSION_VIEW_RES_CELL_ANIMS].fileId + i * 2,
                                   setup[FMISSION_VIEW_RES_CELL_ANIMS].fileId + 1 + i * 2, view->fieldHeapId);
    }
}

static void FMissionViewSetup_FreeRes(FMissionView *view, FMissionViewRes *res) {
    int i;

    for (i = 0; i < res->lists[FMISSION_VIEW_RES_CELL_ANIMS].count; i++) {
        ClActSys_FreeCellAnims(res->lists[FMISSION_VIEW_RES_CELL_ANIMS].ids[i]);
    }
    for (i = 0; i < res->lists[FMISSION_VIEW_RES_CHARS].count; i++) {
        ClActSys_FreeChars(res->lists[FMISSION_VIEW_RES_CHARS].ids[i]);
    }
    for (i = 0; i < res->lists[FMISSION_VIEW_RES_PALETTE].count; i++) {
        ClActSys_FreePltt(res->lists[FMISSION_VIEW_RES_PALETTE].ids[i]);
    }
    for (i = 0; i < FMISSION_VIEW_RES_TYPE_COUNT; i++) {
        if (res->lists[i].ids != NULL) {
            GFL_HeapFree(res->lists[i].ids);
        }
    }
}

static ClActor *FMissionViewSetup_AddActor(FMissionView *view, u32 chars, u32 palette, u32 cellAnims, s16 x, s16 y,
                                           u8 sequence, u8 bgPriority, u8 priority) {
    ClActorSetup setup;
    ClActor *actor;

    setup.x = x;
    setup.y = y;
    setup.sequence = sequence;
    setup.bgPriority = bgPriority;
    setup.priority = priority;
    actor = ClActor_Create(view->clUnit, chars, palette, cellAnims, &setup, CLACT_SURFACE_MAIN, view->fieldHeapId);
    ClActor_SetAutoAnim(actor, FALSE);
    ClActor_SetDrawAnim(actor, FALSE);
    return actor;
}

static void FMissionViewSetup_InitButtons(FMissionView *view) {
    static const FMissionViewButtonPos sFMissionViewButtonPos[FMISSION_VIEW_BUTTON_COUNT] = {
        { 164, 172 }, { 196, 172 }, { 228, 172 }, { 16, 172 }, { 88, 172 }, { 112, 172 },
    };
    int i;

    for (i = 0; i < FMISSION_VIEW_BUTTON_COUNT; i++) {
        view->buttons[i] = FMissionViewSetup_AddActor(
            view, view->res[FMISSION_VIEW_RES_MAIN].lists[FMISSION_VIEW_RES_CHARS].ids[0],
            view->res[FMISSION_VIEW_RES_MAIN].lists[FMISSION_VIEW_RES_PALETTE].ids[0],
            view->res[FMISSION_VIEW_RES_MAIN].lists[FMISSION_VIEW_RES_CELL_ANIMS].ids[0], sFMissionViewButtonPos[i].x,
            sFMissionViewButtonPos[i].y, (i + 1) * FMISSION_VIEW_BUTTON_STATE_COUNT, 1, 1);
        ClActor_SetVisible(view->buttons[i], TRUE);
        ClActor_SetAutoAnim(view->buttons[i], TRUE);
    }
}

static void FMissionViewSetup_FreeButtons(FMissionView *view) {
    int i;

    for (i = 0; i < FMISSION_VIEW_BUTTON_COUNT; i++) {
        ClActor_SetVisible(view->buttons[i], FALSE);
        ClActor_Delete(view->buttons[i]);
    }
}

// Makes the bitmap of a panel's line, width by height tiles, and its actors
static void FMissionViewSetup_InitLine(FMissionView *view, BeaconViewOam *line, u8 width, u8 height, s16 x, s16 y) {
    BmpOamActorSetup setup;

    setup.bitmap = GFL_BitmapCreate(width, height, 0x20, view->heapId);
    setup.x = x;
    setup.y = y;
    setup.palette = view->res[FMISSION_VIEW_RES_MAIN].lists[FMISSION_VIEW_RES_PALETTE].ids[0];
    setup.paletteOffset = 3;
    setup.priority = 0;
    setup.bgPriority = 3;
    setup.surface = CLACT_SURFACE_MAIN;
    setup.vramType = CLACT_VRAM_SUB;
    BeaconViewOam_Init(line, &setup, view->bmpOam, view->printQueue, view->font, view->tcbManager);
    GFL_BitmapFill(line->bitmap, 0);
    BmpOam_ActorBmpTrans(line->actor);
    BmpOam_ActorSetDrawEnable(line->actor, FALSE);
}

static void FMissionViewSetup_InitPanel(FMissionView *view, u8 index) {
    FMissionViewPanel *panel = &view->panels[index];

    MI_CpuFill8(panel, 0, sizeof(FMissionViewPanel));
    panel->index = index;
    panel->logIndex = FMISSION_VIEW_PANEL_NONE;
    panel->slot = 0;
    panel->queue = view->printQueue;
    panel->x = FMISSION_VIEW_PANEL_X;
    panel->y = index * FMISSION_VIEW_PANEL_HEIGHT + FMISSION_VIEW_PANEL_Y;
    panel->frame =
        FMissionViewSetup_AddActor(view, view->res[FMISSION_VIEW_RES_MAIN].lists[FMISSION_VIEW_RES_CHARS].ids[0],
                                   view->res[FMISSION_VIEW_RES_MAIN].lists[FMISSION_VIEW_RES_PALETTE].ids[0],
                                   view->res[FMISSION_VIEW_RES_MAIN].lists[FMISSION_VIEW_RES_CELL_ANIMS].ids[0],
                                   panel->x, panel->y, index % 3 + 1, 3, index + 12);
    panel->markActor =
        FMissionViewSetup_AddActor(view, view->res[FMISSION_VIEW_RES_MAIN].lists[FMISSION_VIEW_RES_CHARS].ids[0],
                                   view->res[FMISSION_VIEW_RES_MAIN].lists[FMISSION_VIEW_RES_PALETTE].ids[0],
                                   view->res[FMISSION_VIEW_RES_MAIN].lists[FMISSION_VIEW_RES_CELL_ANIMS].ids[0],
                                   panel->x + 16, panel->y, 21, 3, index + 3);
    panel->icon =
        FMissionViewSetup_AddActor(view, view->res[FMISSION_VIEW_RES_ICON].lists[FMISSION_VIEW_RES_CHARS].ids[index],
                                   view->res[FMISSION_VIEW_RES_MAIN].lists[FMISSION_VIEW_RES_PALETTE].ids[0],
                                   view->res[FMISSION_VIEW_RES_ICON].lists[FMISSION_VIEW_RES_CELL_ANIMS].ids[0],
                                   panel->x + 16, panel->y + 16, 0, 3, index + 6);
    ClActor_SetPalette(panel->icon, (u8)(index + 9), 1);
    FMissionViewSetup_InitLine(view, &panel->lines[0], 8, 2, panel->x + 48, panel->y + 4);
    panel->strs[0] = GFL_StrBufCreate(8, view->heapId);
    FMissionViewSetup_InitLine(view, &panel->lines[1], 24, 2, panel->x + 48, panel->y + 20);
    panel->strs[1] = GFL_StrBufCreate(41, view->heapId);
    FMissionViewSetup_InitLine(view, &panel->lines[2], 12, 2, panel->x + 128, panel->y + 4);
    panel->strs[2] = GFL_StrBufCreate(24, view->heapId);
    ClActor_SetVisible(panel->frame, FALSE);
    ClActor_SetVisible(panel->markActor, FALSE);
    ClActor_SetVisible(panel->icon, FALSE);
}

static void FMissionViewSetup_FreePanel(FMissionView *view, u8 index) {
    int i;
    FMissionViewPanel *panel = &view->panels[index];

    for (i = 0; i < FMISSION_VIEW_PANEL_LINE_COUNT; i++) {
        GFL_StrBufFree(panel->strs[i]);
        BeaconViewOam_Free(&panel->lines[i]);
    }
    ClActor_SetVisible(panel->icon, FALSE);
    ClActor_Delete(panel->icon);
    ClActor_SetVisible(panel->markActor, FALSE);
    ClActor_Delete(panel->markActor);
    ClActor_SetVisible(panel->frame, FALSE);
    ClActor_Delete(panel->frame);
    MI_CpuFill8(panel, 0, sizeof(FMissionViewPanel));
}
