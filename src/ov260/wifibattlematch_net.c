#include "app/wifibattlematch_net.h"
#include "types.h"
#include "app/livebattlematch_flow.h"
#include "constants/arc.h"
#include "constants/text_banks.h"
#include "dwc/dwc.h"
#include "dwc/gdb.h"
#include "dwc/nd.h"
#include "gfl/dwc_rap.h"
#include "gfl/dwc_rapcommon.h"
#include "gfl/heap.h"
#include "gfl/msg.h"
#include "gfl/net.h"
#include "gfl/net_command.h"
#include "gfl/net_handle.h"
#include "gfl/net_state.h"
#include "gfl/net_system.h"
#include "gfl/nhttp_rap.h"
#include "gfl/sound.h"
#include "gfl/std.h"
#include "gfl/str.h"
#include "nitro/math.h"
#include "nitro/os.h"
#include "nitro/rtc.h"
#include "pml/poke_party.h"
#include "save/join_avenue.h"
#include "save/player_info.h"
#include "save/save_control.h"
#include "save/tournament_block.h"
#include "save/wifi_list.h"
#include "system/game_data.h"
#include "text/system/ability_handlers_btl_main.h"

// wifibattlematch_net.c, named by its embedded string, in overlay 260: the net part of the Wi-Fi battle competitions.
// It matches two players through the Wi-Fi Connection, keeps their stats and teams in the Sake database through the
// Gdb client that follows it in the overlay (dwc/gdb.h), sends the teams from one to the other and downloads the
// competitions' files. The work is called p_wk, as its assertions' texts show

// The most fields that a request on a table can have
#define WBM_GDB_FIELD_TABLE_MAX 0x26

// The table that the players' stats are in
#define WBM_GDB_TABLE_NAME "PlayerStats_v1"

// The commands that the two players send each other, from the first of the table
enum {
    WBM_NET_CMD_INFO = 0x2400,
    WBM_NET_CMD_PLAYER_INFO,
    WBM_NET_CMD_2402,
    WBM_NET_CMD_PARTY,
    WBM_NET_CMD_VALUE4,
    WBM_NET_CMD_REPORT_VALUE,
    WBM_NET_CMD_PERSON,
};

// The matching keys, by the number the work gives them, with their names
enum {
    WBM_MATCH_KEY_MOD,
    WBM_MATCH_KEY_RUL,
    WBM_MATCH_KEY_DEB,
    WBM_MATCH_KEY_RAT,
    WBM_MATCH_KEY_CUP,
    WBM_MATCH_KEY_DIS,
    WBM_MATCH_KEY_CNT,
    WBM_MATCH_KEY_VER,
    WBM_MATCH_KEY_MAX,
};

// The steps of WifiBattleMatchNet_LoginMain: read the player's record ID, and create the record if there is none
enum {
    WBM_LOGIN_SEQ_READ_RECORD_ID,
    WBM_LOGIN_SEQ_WAIT_READ,
    WBM_LOGIN_SEQ_CREATE_RECORD,
    WBM_LOGIN_SEQ_WAIT_CREATE,
    WBM_LOGIN_SEQ_DONE,
};

// The steps of WifiBattleMatchNet_MatchMain: the query, the match, the negotiation of the two machines and their sync,
// or the end of the connection when it fails
enum {
    WBM_MATCH_SEQ_START_QUERY,
    WBM_MATCH_SEQ_QUERY_STARTED,
    WBM_MATCH_SEQ_WAIT_MATCH,
    WBM_MATCH_SEQ_NEGOTIATE,
    WBM_MATCH_SEQ_SYNC,
    WBM_MATCH_SEQ_WAIT_NEGOTIATION,
    WBM_MATCH_SEQ_WAIT_SYNC,
    WBM_MATCH_SEQ_END,
    WBM_MATCH_SEQ_DONE,
};

// The steps of WifiBattleMatchNet_EndMain: a last sync, the disconnect error turned off, a wait and the disconnect
enum {
    WBM_END_SEQ_SYNC,
    WBM_END_SEQ_WAIT_SYNC,
    WBM_END_SEQ_STOP_ERRORS,
    WBM_END_SEQ_WAIT,
    WBM_END_SEQ_DISCONNECT,
    WBM_END_SEQ_DONE,
};

// The steps of WifiBattleMatchNet_ReportMain: the exchange of the two sides' counts, the competition service's start,
// its session and the intention to report, which the two machines share, and the report built and submitted, each
// shared step followed by a sync
enum {
    WBM_REPORT_SEQ_SYNC,
    WBM_REPORT_SEQ_WAIT_SYNC,
    WBM_REPORT_SEQ_SEND_VALUE,
    WBM_REPORT_SEQ_TAKE_VALUE,
    WBM_REPORT_SEQ_STARTUP,
    WBM_REPORT_SEQ_GET_CERTIFICATE,
    WBM_REPORT_SEQ_CREATE_SESSION,
    WBM_REPORT_SEQ_WAIT_SESSION,
    WBM_REPORT_SEQ_SYNC_SESSION,
    WBM_REPORT_SEQ_WAIT_SYNC_SESSION,
    WBM_REPORT_SEQ_SHARE_SESSION,
    WBM_REPORT_SEQ_SET_INTENTION,
    WBM_REPORT_SEQ_WAIT_INTENTION,
    WBM_REPORT_SEQ_SYNC_INTENTION,
    WBM_REPORT_SEQ_WAIT_SYNC_INTENTION,
    WBM_REPORT_SEQ_SEND_INFO_PARENT,
    WBM_REPORT_SEQ_SEND_INFO_CHILD,
    WBM_REPORT_SEQ_SYNC_INFO,
    WBM_REPORT_SEQ_WAIT_SYNC_INFO,
    WBM_REPORT_SEQ_BUILD_REPORT,
    WBM_REPORT_SEQ_SUBMIT_REPORT,
    WBM_REPORT_SEQ_WAIT_SUBMIT,
    WBM_REPORT_SEQ_END,
    WBM_REPORT_SEQ_SYNC_END,
    WBM_REPORT_SEQ_WAIT_SYNC_END,
    WBM_REPORT_SEQ_DONE,
};

// The steps of WifiBattleMatchNet_ReadMain and WifiBattleMatchNet_WriteMain: the Gdb client's start, the request, its
// wait and its result, and the cancel of a request that failed
enum {
    WBM_GDB_SEQ_STARTUP,
    WBM_GDB_SEQ_REQUEST,
    WBM_GDB_SEQ_WAIT,
    WBM_GDB_SEQ_RESULT,
    WBM_GDB_SEQ_END,
    WBM_GDB_SEQ_DONE,
    WBM_GDB_SEQ_CANCEL,
    WBM_GDB_SEQ_WAIT_CANCEL,
    WBM_GDB_SEQ_FAILED,
};

// The steps of WifiBattleMatchNet_CreateRecordMain, as WBM_GDB_SEQ_* without the end
enum {
    WBM_CREATE_SEQ_STARTUP,
    WBM_CREATE_SEQ_REQUEST,
    WBM_CREATE_SEQ_WAIT,
    WBM_CREATE_SEQ_RESULT,
    WBM_CREATE_SEQ_CANCEL,
    WBM_CREATE_SEQ_WAIT_CANCEL,
    WBM_CREATE_SEQ_FAILED,
};

// The steps of WifiBattleMatchNet_DownloadMain: the download library's start, the attribute, the list of files, the
// file and its download, the check of its CRC, and how it ends. The waits for the library's callback, its cleanup and
// its cancel go on to the step kept in the work
enum {
    WBM_DOWNLOAD_SEQ_INIT,
    WBM_DOWNLOAD_SEQ_SET_ATTR,
    WBM_DOWNLOAD_SEQ_GET_FILE_LIST,
    WBM_DOWNLOAD_SEQ_GET_FILE,
    WBM_DOWNLOAD_SEQ_RECEIVE,
    WBM_DOWNLOAD_SEQ_RECEIVED,
    WBM_DOWNLOAD_SEQ_CHECK,
    WBM_DOWNLOAD_SEQ_NO_FILE,
    WBM_DOWNLOAD_SEQ_ERROR,
    WBM_DOWNLOAD_SEQ_WAIT = 100,
    WBM_DOWNLOAD_SEQ_WAIT_CLEANUP = 200,
    WBM_DOWNLOAD_SEQ_WAIT_CANCEL = 300,
};

// The steps of WifiBattleMatchNet_AccountRequestMain: the request's sending, its start, the poll for its answer, and
// how it ends
enum {
    WBM_ACCOUNT_SEQ_SEND,
    WBM_ACCOUNT_SEQ_START,
    WBM_ACCOUNT_SEQ_POLL,
    WBM_ACCOUNT_SEQ_SUCCESS,
    WBM_ACCOUNT_SEQ_SERVER_ERROR,
};

// The steps of WifiBattleMatchNet_PostRequestMain, as WBM_ACCOUNT_SEQ_* with the two ends in the other order
enum {
    WBM_POST_SEQ_SEND,
    WBM_POST_SEQ_START,
    WBM_POST_SEQ_POLL,
    WBM_POST_SEQ_SERVER_ERROR,
    WBM_POST_SEQ_SUCCESS,
};

// The steps of WifiBattleMatchNet_PokemonCheckMain: the request's sending, the poll for its answer and the answer
enum {
    WBM_CHECK_SEQ_SEND,
    WBM_CHECK_SEQ_POLL,
    WBM_CHECK_SEQ_ANSWER,
};

// The kinds of the error that the work keeps
enum {
    WBM_ERROR_NONE,
    WBM_ERROR_DWC,
    WBM_ERROR_GDB,
    WBM_ERROR_UNUSED_3,
    WBM_ERROR_HTTP,
    WBM_ERROR_TIMEOUT,
};

// The error that was found, by its kind
typedef struct {
    s32 dwcCode;
    s32 gdbCode;
    s32 gdbSubCode1;
    s32 gdbSubCode2;
    u32 unk10;
    s32 httpCode;
    s32 timeoutCode;
    u32 kind;
} WifiBattleMatchNetError;

// A key of the matching query: its value, the ID the DWC library gave it and its three letters
typedef struct {
    s32 value;
    u8 keyId;
    u8 unk5;
    char name[6];
} WifiBattleMatchNetKey;

// The rule of a match (WBM_RULE_*), how it ended (0 to 6) and the two sides' counts
typedef struct {
    u32 format;
    u32 outcome;
    u32 mine;
    u32 theirs;
} WifiBattleMatchNetParam;

// Sets a battle's fields in the exchange with the other player
typedef int (*WifiBattleMatchNetResultFunc)(WifiBattleMatchNetInfo *info, WifiBattleMatchNetParam *param, int isMine,
                                            int a3);

// Two values that the server answers with, and that are sent back
typedef struct {
    u32 value1;
    u32 value2;
} WifiBattleMatchNetValues;

// What an NHTTP answer starts with: its result, and the two values at its end
typedef struct {
    u32 result;
    u8 unk4[0x7c];
    WifiBattleMatchNetValues values;
} WifiBattleMatchNetAnswer;

// What the download's callback sets, which is in the .bss and not in the work
typedef struct {
    u8 done;
    u8 done2;
    u32 code;
} WifiBattleMatchNetDownload;

// The name that the bad word check reads and its result
typedef struct {
    u16 name[12];
    const u16 *words;
    u8 result;
    int badWordCount;
    u16 unk24;
} WifiBattleMatchNetNameCheck;

// The value of the matching key "ver", which only players of the same version match with
#ifdef BLACK2
#define WBM_MATCH_VERSION 0x20017
#else
#define WBM_MATCH_VERSION 0x20016
#endif

// The name that a player gets when the check refuses theirs, by the version
#ifdef BLACK2
#define WBM_DEFAULT_NAME AbilityHandlersBtlMain_Text_Kuro
#else
#define WBM_DEFAULT_NAME AbilityHandlersBtlMain_Text_Shiro
#endif

struct WifiBattleMatchNet {
    void *unk0;
    WifiList *wifiList;
    GameData *gameData;
    s32 profileId;
    void *loginBuffer;
    u32 syncId;
    WifiBattleMatchNetError error;
    u8 loginDate[6];
    u32 matchState;
    char matchQuery[0x80];
    WifiBattleMatchNetKey matchKeys[WBM_MATCH_KEY_MAX];
    HeapID msgHeapId;
    u32 phase;
    u32 state;
    u32 resumeState;
    u32 counter;
    u32 flagInfoReceived;
    u32 flagPlayerInfoReceived;
    u32 flag140;
    u32 flagPartyReceived;
    u32 flag148;
    u32 flagValueReceived;
    u32 flagPersonReceived;
    u32 receivedValue;
    u32 phaseB;
    WifiBattleMatchNetResultFunc resultFunc;
    s32 errorCode;
    u32 flag164;
    u32 requestSent;
    u32 flag16C;
    u32 flag170;
    u32 gdbOpen;
    u32 unk178;
    void *unk17c;
    GdbRecordsFunc recordsFunc;
    WifiBattleMatchNetParam param;
    GdbField *record;
    const char **tableNames;
    s32 table_name_num;
    u32 unk1a0;
    u32 timer;
    u32 matchTimer;
    u32 unk1ac;
    u32 mode;
    s32 recordIdArg;
    u8 unk1b8[0x10];
    WifiBattleMatchNetRecordData recordData;
    u8 recvBuffer[0x1000];
    DWCEvalPlayerCallback evalFunc;
    NHttpRap *p_nhttp;
    WifiBattleMatchNetValues httpValues;
    WifiBattleMatchNetValues postData;
    TournamentEntry downloadBuffer;
    DWCNdFileInfo downloadFile;
    u32 downloadPercent;
    u32 downloadReceived;
    u32 downloadSize;
    char downloadAttr[0xc];
    u32 downloading;
    u32 unk1460;
    u32 unk1464;
    WifiBattleMatchNetData *p_data;
    WifiBattleMatchNetNameCheck nameCheck;
    PlayerInfo *namePlayerInfo;
    u32 unk1498;
    u32 unk149c;
    JoinAvenuePerson person;
    u32 unk1564;
};

static void WifiBattleMatchNet_GetLoginDate(u8 *dest);
static void WifiBattleMatchNet_SetMatchKey(WifiBattleMatchNet *p_wk, u32 index, s32 value);
static int WifiBattleMatchNet_EvalAlias(int index, void *work);
static int WifiBattleMatchNet_EvalAny(int index, void *work);
static int WifiBattleMatchNet_Eval(int index, void *work);
static void WifiBattleMatchNet_OnRequestDone(int error, void *work);
static void WifiBattleMatchNet_OnRequestDoneWithData(int error, void *work);
static void WifiBattleMatchNet_OnRequestDone2(int error, void *work);
static void WifiBattleMatchNet_EndRequests(WifiBattleMatchNet *p_wk);
static int WifiBattleMatchNet_SendBattleResult(WifiBattleMatchNetInfo *info, WifiBattleMatchNetInfo *theirs,
                                               WifiBattleMatchNetParam *param, int a3,
                                               WifiBattleMatchNetResultFunc func, int a5, int mode, int a7);
static int WifiBattleMatchNet_SetBattleResult(WifiBattleMatchNetInfo *info, WifiBattleMatchNetParam *param, int isMine,
                                              int a3);
static int WifiBattleMatchNet_SetCupBattleResult(WifiBattleMatchNetInfo *info, WifiBattleMatchNetParam *param,
                                                 int isMine, int a3);
static int WifiBattleMatchNet_SetFlag1A(WifiBattleMatchNetInfo *info, WifiBattleMatchNetParam *param, int isMine,
                                        int a3);
static int WifiBattleMatchNet_SetFlag19(WifiBattleMatchNetInfo *info, WifiBattleMatchNetParam *param, int isMine,
                                        int a3);
static int WifiBattleMatchNet_SetNoRating1B(WifiBattleMatchNetInfo *info, WifiBattleMatchNetParam *param, int isMine,
                                            int a3);
static int WifiBattleMatchNet_SetNoRating1C(WifiBattleMatchNetInfo *info, WifiBattleMatchNetParam *param, int isMine,
                                            int a3);
static void WifiBattleMatchNet_OnInfoReceived(int netId, int size, const void *data, void *work, NetHandle *handle);
static void WifiBattleMatchNet_OnPlayerInfoReceived(int netId, int size, const void *data, void *work,
                                                    NetHandle *handle);
static void *WifiBattleMatchNet_GetRecvBuffer(int netId, void *work, int size);
static void WifiBattleMatchNet_OnCmd2402(int netId, int size, const void *data, void *work, NetHandle *handle);
static void WifiBattleMatchNet_OnPartyReceived(int netId, int size, const void *data, void *work, NetHandle *handle);
static void WifiBattleMatchNet_OnCmd2404(int netId, int size, const void *data, void *work, NetHandle *handle);
static void WifiBattleMatchNet_OnValueReceived(int netId, int size, const void *data, void *work, NetHandle *handle);
static void WifiBattleMatchNet_OnPersonReceived(int netId, int size, const void *data, void *work, NetHandle *handle);
static void WifiBattleMatchNet_EndGdb(WifiBattleMatchNet *p_wk);
static void WifiBattleMatchNet_BuildProfileRecord(WifiBattleMatchNet *p_wk);
static void WifiBattleMatchNet_OnRecords(int recordNum, GdbField **records, void *work, int fieldNum);
static void WifiBattleMatchNet_ReadArena(int recordNum, GdbField **records, void *work, int fieldNum);
static void WifiBattleMatchNet_ReadWifiCup(int recordNum, GdbField **records, void *work, int fieldNum);
static void WifiBattleMatchNet_ReadRecordId(int recordNum, GdbField **records, void *work, int fieldNum);
static void WifiBattleMatchNet_ReadWifiCupParty(int recordNum, GdbField **records, void *work, int fieldNum);
static void WifiBattleMatchNet_ReadLastLogin(int recordNum, GdbField **records, void *work, int fieldNum);
static BOOL WifiBattleMatchNet_IsAllZero(const u32 *data, u32 size);
static void WifiBattleMatchNet_OnDwcError(void *work, int code, int type, int error);
static u32 WifiBattleMatchNet_OnDwcEvent(void *work, int a1, int event, int a3);
static u32 WifiBattleMatchNet_GetHttpResultMessage(u32 result);
static void WifiBattleMatchNet_StartDownloadWait(WifiBattleMatchNet *p_wk, u32 nextState);
static void WifiBattleMatchNet_StartDownloadEnd(WifiBattleMatchNet *p_wk, u32 nextState);
static void WifiBattleMatchNet_StartDownloadCancel(WifiBattleMatchNet *p_wk, u32 nextState);
static void WifiBattleMatchNet_OnDownload(DWCNdCallbackReason reason, DWCNdError error, int serverError);
static u32 WifiBattleMatchNet_GetDwcResult(u32 code);
static u32 WifiBattleMatchNet_GetGdbResult(u32 code, u32 subCode1, u32 subCode2);
static u32 WifiBattleMatchNet_GetTimeoutResult(u32 code);
static void WifiBattleMatchNet_ClearError(WifiBattleMatchNetError *error);
static void WifiBattleMatchNet_SetTimeoutError(WifiBattleMatchNetError *error, int code, int line);
static void WifiBattleMatchNet_SetDwcError(WifiBattleMatchNetError *error, int code, int line);
static void WifiBattleMatchNet_SetGdbError(WifiBattleMatchNetError *error, int code, int line);
static void WifiBattleMatchNet_SetGdbSubError1(WifiBattleMatchNetError *error, int code, int line);
static void WifiBattleMatchNet_SetGdbSubError2(WifiBattleMatchNetError *error, int code, int line);
static void WifiBattleMatchNet_SetHttpError(WifiBattleMatchNetError *error, int code, int line);

// What WBM_NET_CMD_2402 sends, and the receivers of the commands
static const u32 sCmd2402Data = 0;

static const NetCommand sNetCommands[] = {
    [WBM_NET_CMD_INFO - WBM_NET_CMD_INFO] = { WifiBattleMatchNet_OnInfoReceived, NULL },
    [WBM_NET_CMD_PLAYER_INFO - WBM_NET_CMD_INFO] = { WifiBattleMatchNet_OnPlayerInfoReceived,
                                                     WifiBattleMatchNet_GetRecvBuffer },
    [WBM_NET_CMD_2402 - WBM_NET_CMD_INFO] = { WifiBattleMatchNet_OnCmd2402, NULL },
    [WBM_NET_CMD_PARTY - WBM_NET_CMD_INFO] = { WifiBattleMatchNet_OnPartyReceived, WifiBattleMatchNet_GetRecvBuffer },
    [WBM_NET_CMD_VALUE4 - WBM_NET_CMD_INFO] = { WifiBattleMatchNet_OnCmd2404, WifiBattleMatchNet_GetRecvBuffer },
    [WBM_NET_CMD_REPORT_VALUE - WBM_NET_CMD_INFO] = { WifiBattleMatchNet_OnValueReceived, NULL },
    [WBM_NET_CMD_PERSON - WBM_NET_CMD_INFO] = { WifiBattleMatchNet_OnPersonReceived, WifiBattleMatchNet_GetRecvBuffer },
};

static char sVer[] = "ver";
static char sDis[] = "dis";
static const char *sWifiCupPartyTable = "WIFICUP_POKEMON_PARTY";
static const char *sLastLoginTable = "LAST_LOGIN_DATETIME";
static const char *sRecordIdTable = "recordid";
static const char *sRecordDataTables[6] = {
    "RECORD_DATA_01", "RECORD_DATA_02", "RECORD_DATA_03", "RECORD_DATA_04", "RECORD_DATA_05", "RECORD_DATA_06",
};
// The names of the matching keys, by the number the work gives them
static const char *sMatchKeyNames[WBM_MATCH_KEY_MAX] = { "mod", "rul", "deb", "rat", "cup", sDis, "cnt", sVer };

static WifiBattleMatchNetDownload sDownload;

WifiBattleMatchNet *WifiBattleMatchNet_Create(WifiBattleMatchNetData *data, GameData *gameData, void *loginBuffer,
                                              HeapID heapId) {
    WifiBattleMatchNet *p_wk;

    p_wk = GFL_HeapAllocate(heapId, sizeof(WifiBattleMatchNet), FALSE, "wifibattlematch_net.c", 700);
    MI_CpuFill8(p_wk, 0, sizeof(WifiBattleMatchNet));
    p_wk->wifiList = GameData_GetWifiList(gameData);
    p_wk->unk0 = WifiList_GetMyUserData(p_wk->wifiList);
    p_wk->profileId = PlayerInfo_GetProfileId(GetGameDataPlayerInfo(gameData));
    p_wk->gameData = gameData;
    p_wk->loginBuffer = loginBuffer;
    p_wk->p_data = data;
    p_wk->unk1a0 = 0x573;
    p_wk->record =
        GFL_HeapAllocate(heapId, sizeof(GdbField) * WBM_GDB_FIELD_TABLE_MAX, FALSE, "wifibattlematch_net.c", 716);
    MI_CpuFill8(p_wk->record, 0, sizeof(GdbField) * WBM_GDB_FIELD_TABLE_MAX);
    if (!GFL_NetIsInitialized()) {
        GFL_ASSERT(0);
    }
    NetCommand_AddTable(WBM_NET_CMD_INFO, sNetCommands, NELEMS(sNetCommands), p_wk);
    GFL_NetSetMPSendEnabled(0);
    DWCRap_SetErrorFunc(WifiBattleMatchNet_OnDwcError, p_wk);
    DWCRap_SetEventFunc(WifiBattleMatchNet_OnDwcEvent, p_wk);
    return p_wk;
}

void WifiBattleMatchNet_Delete(WifiBattleMatchNet *p_wk) {
    DWCRap_SetEventFunc(NULL, NULL);
    DWCRap_SetErrorFunc(NULL, NULL);
    NetCommand_RemoveTable(WBM_NET_CMD_INFO);
    GFL_HeapFree(p_wk->record);
    GFL_HeapFree(p_wk);
}

void WifiBattleMatchNet_Nop(WifiBattleMatchNet *p_wk) {
}

void WifiBattleMatchNet_StartSync(WifiBattleMatchNet *p_wk, u32 syncId) {
    p_wk->syncId = syncId;
    NetHandle_SendTimingPair(NetHandle_GetOwn(), (u8)p_wk->syncId, 0x24);
}

BOOL WifiBattleMatchNet_CheckSync(WifiBattleMatchNet *p_wk) {
    return NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), (u8)p_wk->syncId, 0x24);
}

u32 WifiBattleMatchNet_CheckError(WifiBattleMatchNet *p_wk, u32 a1, u32 a2) {
    WifiBattleMatchNetError *error = &p_wk->error;
    WifiBattleMatchNetResult result = WBM_RESULT_OK;

    if (GFL_NetIsInitialized()) {
        if (GFL_NetErrCheck()) {
            func_02042e94(0);
            GFL_NetSetDisconnectError(0);
            GFL_SndStop();
            if (GameData_CheckEventsPaused(p_wk->gameData)) {
                GameData_CancelSave(p_wk->gameData);
                GFL_NetErrShowWifi(0x29);
            }
        }
        switch (DWCRapCommon_CheckError(a1, a2)) {
        case DWCRAP_RESULT_OK:
            result = WBM_RESULT_OK;
            break;
        case DWCRAP_RESULT_DISCONNECTED:
            result = WBM_RESULT_DISCONNECTED;
            break;
        case DWCRAP_RESULT_FAILED:
            result = WBM_RESULT_FAILED;
            break;
        case DWCRAP_RESULT_NET_ERROR:
            result = WBM_RESULT_NET_ERROR;
            break;
        case DWCRAP_RESULT_TIMEOUT:
            result = WBM_RESULT_TIMEOUT;
            break;
        }
        return result;
    }
    if (result == WBM_RESULT_OK) {
        switch (error->kind) {
        case WBM_ERROR_NONE:
            result = WBM_RESULT_OK;
            break;
        case WBM_ERROR_DWC:
            result = WifiBattleMatchNet_GetDwcResult(error->dwcCode);
            break;
        case WBM_ERROR_GDB:
            result = WifiBattleMatchNet_GetGdbResult(error->gdbCode, error->gdbSubCode1, error->gdbSubCode2);
            break;
        case WBM_ERROR_UNUSED_3:
        case WBM_ERROR_HTTP:
            result = WBM_RESULT_OK;
            break;
        case WBM_ERROR_TIMEOUT:
            result = WifiBattleMatchNet_GetTimeoutResult(error->timeoutCode);
            break;
        default:
            result = WBM_RESULT_OK;
            break;
        }
        if (result != WBM_RESULT_OK) {
            func_02042e94(0);
            GFL_NetSetDisconnectError(0);
            GFL_SndStop();
            if (GameData_CheckEventsPaused(p_wk->gameData)) {
                GameData_CancelSave(p_wk->gameData);
                GFL_NetErrShowWifi(0x29);
            }
        }
        switch (result) {
        case WBM_RESULT_DISCONNECTED:
            NetState_ClearError();
            NetState_ClearWifiError();
            GFL_NetErrShowScreen();
            GFL_NetErrClear();
            break;
        case WBM_RESULT_FAILED:
            if (!GFL_NetErrCheck()) {
                NetState_RecordError(0, 0, 0, 0x3f7);
                GFL_NetErrRecord();
            }
            GFL_NetErrMarkShown();
            break;
        case WBM_RESULT_NET_ERROR:
            GFL_NetErrAbort();
            break;
        }
        if (result != WBM_RESULT_OK) {
            WifiBattleMatchNet_ClearError(error);
        }
    }
    return result;
}

void WifiBattleMatchNet_ResetLogin(WifiBattleMatchNet *p_wk) {
    p_wk->state = 0;
    p_wk->phase = WBM_LOGIN_SEQ_READ_RECORD_ID;
}

u32 WifiBattleMatchNet_LoginMain(WifiBattleMatchNet *p_wk) {
    switch (p_wk->phase) {
    case WBM_LOGIN_SEQ_READ_RECORD_ID:
        WifiBattleMatchNet_StartRead(p_wk, -1, WBM_READ_RECORD_ID, p_wk->p_data);
        p_wk->phase = WBM_LOGIN_SEQ_WAIT_READ;
        break;
    case WBM_LOGIN_SEQ_WAIT_READ: {
        u32 result = WifiBattleMatchNet_ReadMain(p_wk);

        if (result == WBM_NET_SUCCESS) {
            if (p_wk->p_data->sake_recordID != 0) {
                p_wk->phase = WBM_LOGIN_SEQ_DONE;
            } else {
                p_wk->phase = WBM_LOGIN_SEQ_CREATE_RECORD;
            }
        } else if (result == WBM_NET_FAILED) {
            return WBM_NET_FAILED;
        }
        break;
    }
    case WBM_LOGIN_SEQ_CREATE_RECORD:
        WifiBattleMatchNet_StartCreateRecord(p_wk);
        p_wk->phase = WBM_LOGIN_SEQ_WAIT_CREATE;
        break;
    case WBM_LOGIN_SEQ_WAIT_CREATE: {
        u32 result = WifiBattleMatchNet_CreateRecordMain(p_wk);

        if (result == WBM_NET_SUCCESS) {
            p_wk->phase = WBM_LOGIN_SEQ_DONE;
        } else if (result == WBM_NET_FAILED) {
            return WBM_NET_FAILED;
        }
        break;
    }
    case WBM_LOGIN_SEQ_DONE:
        return WBM_NET_SUCCESS;
    }
    return WBM_NET_RUNNING;
}

void WifiBattleMatchNet_StartMatch(WifiBattleMatchNet *p_wk, u32 mode, u32 a2, u32 rule,
                                   const WifiBattleMatchNetKeys *keys) {
    u32 modValue = mode;

    if (mode == WBM_MATCH_MODE_UNUSED) {
        modValue += a2;
    }
    if (!GFL_NetIsInitialized()) {
        GFL_NetErrAbort();
    }
    DWC_CloseAllConnectionsHard();
    WifiBattleMatchNet_SetMatchKey(p_wk, WBM_MATCH_KEY_MOD, modValue);
    WifiBattleMatchNet_SetMatchKey(p_wk, WBM_MATCH_KEY_RUL, rule);
    WifiBattleMatchNet_SetMatchKey(p_wk, WBM_MATCH_KEY_DEB, 1);
    WifiBattleMatchNet_SetMatchKey(p_wk, WBM_MATCH_KEY_RAT, keys->rating);
    WifiBattleMatchNet_SetMatchKey(p_wk, WBM_MATCH_KEY_DIS, keys->disconnects);
    WifiBattleMatchNet_SetMatchKey(p_wk, WBM_MATCH_KEY_CUP, keys->cup);
    WifiBattleMatchNet_SetMatchKey(p_wk, WBM_MATCH_KEY_CNT, keys->matches);
    WifiBattleMatchNet_SetMatchKey(p_wk, WBM_MATCH_KEY_VER, WBM_MATCH_VERSION);
    STD_TSPrintf(p_wk->matchQuery, "mod=%d And rul=%d And deb=%d", modValue, rule, 1);
    p_wk->matchState = WBM_MATCH_SEQ_START_QUERY;
    p_wk->timer = 0;
    p_wk->matchTimer = 0;
    MI_CpuFill8(&p_wk->flagInfoReceived, 0, 7 * sizeof(u32));
    switch (mode) {
    case WBM_MATCH_MODE_COMPETITION:
        p_wk->evalFunc = WifiBattleMatchNet_Eval;
        break;
    case WBM_MATCH_MODE_UNUSED:
        GFL_ASSERT(0);
        break;
    case WBM_MATCH_MODE_RATING:
        p_wk->evalFunc = WifiBattleMatchNet_EvalAlias;
        break;
    case WBM_MATCH_MODE_FREE:
        p_wk->evalFunc = WifiBattleMatchNet_EvalAny;
        break;
    default:
        GFL_ASSERT(0);
        break;
    }
}

u32 WifiBattleMatchNet_MatchMain(WifiBattleMatchNet *p_wk) {
    switch (p_wk->matchState) {
    case WBM_MATCH_SEQ_START_QUERY:
        if (DWCRap_StartMatchQuery(p_wk->matchQuery, 2, p_wk->evalFunc, p_wk)) {
            NetState_WaitWifiMatch();
            DWCRap_SetVoiceChatEnabled(0);
            p_wk->matchState = WBM_MATCH_SEQ_QUERY_STARTED;
        } else {
            NetState_WifiDisconnect(TRUE);
            return WBM_NET_FAILED;
        }
        break;
    case WBM_MATCH_SEQ_QUERY_STARTED:
        p_wk->matchState = WBM_MATCH_SEQ_WAIT_MATCH;
        break;
    case WBM_MATCH_SEQ_WAIT_MATCH:
        switch (NetState_GetWifiStatus()) {
        case 0: {
            GFLNetErrorInfo *info = NetState_GetErrorInfo();

            if ((u32)(info->type - 0x3f0) <= 1) {
                WifiBattleMatchNet_ResetEnd(p_wk);
                p_wk->matchState = WBM_MATCH_SEQ_END;
            }
            break;
        }
        case 1:
            p_wk->matchTimer = 0;
            p_wk->matchState = WBM_MATCH_SEQ_NEGOTIATE;
            func_02042e94(1);
            GFL_NetSetDisconnectError(1);
            break;
        default:
            WifiBattleMatchNet_ResetEnd(p_wk);
            p_wk->matchState = WBM_MATCH_SEQ_END;
            break;
        }
        break;
    case WBM_MATCH_SEQ_NEGOTIATE:
        if (NetSystem_GetNetId()) {
            if (NetHandle_SendNegotiation()) {
                p_wk->matchState = WBM_MATCH_SEQ_SYNC;
            }
            if (p_wk->matchTimer++ > 3600) {
                WifiBattleMatchNet_ResetEnd(p_wk);
                p_wk->matchState = WBM_MATCH_SEQ_END;
            }
        } else {
            p_wk->matchTimer = 0;
            p_wk->matchState = WBM_MATCH_SEQ_WAIT_NEGOTIATION;
        }
        break;
    case WBM_MATCH_SEQ_SYNC:
        NetHandle_SendTimingPair(NetHandle_GetOwn(), 0xf, 0x24);
        p_wk->matchTimer = 0;
        p_wk->matchState = WBM_MATCH_SEQ_WAIT_SYNC;
        break;
    case WBM_MATCH_SEQ_WAIT_NEGOTIATION:
        if (p_wk->matchTimer++ > 3600) {
            WifiBattleMatchNet_ResetEnd(p_wk);
            p_wk->matchState = WBM_MATCH_SEQ_END;
        } else if (NetHandle_CountNegotiated() && NetHandle_SendNegotiation()) {
            NetHandle_SendTimingPair(NetHandle_GetOwn(), 0xf, 0x24);
            p_wk->matchTimer = 0;
            p_wk->matchState = WBM_MATCH_SEQ_WAIT_SYNC;
        }
        break;
    case WBM_MATCH_SEQ_WAIT_SYNC:
        if (p_wk->matchTimer++ > 3600) {
            WifiBattleMatchNet_ResetEnd(p_wk);
            p_wk->matchState = WBM_MATCH_SEQ_END;
        } else if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 0xf, 0x24)) {
            p_wk->matchState = WBM_MATCH_SEQ_DONE;
        }
        break;
    case WBM_MATCH_SEQ_END:
        if (WifiBattleMatchNet_EndMain(p_wk)) {
            GFLNetErrorInfo *info = NetState_GetErrorInfo();

            if ((u32)(info->type - 0x3f0) <= 1) {
                NetState_ClearWifiError();
                NetState_ClearError();
                GFL_NetErrClear();
                NetState_WifiDisconnect(TRUE);
            }
            return WBM_NET_FAILED;
        }
        break;
    case WBM_MATCH_SEQ_DONE:
        GFL_NetIsParent();
        return WBM_NET_SUCCESS;
    }
    return WBM_NET_RUNNING;
}

void WifiBattleMatchNet_CancelMatch(WifiBattleMatchNet *p_wk) {
    func_02042e94(0);
    GFL_NetSetDisconnectError(0);
    NetState_WifiDisconnect(TRUE);
}

void WifiBattleMatchNet_ResetEnd(WifiBattleMatchNet *p_wk) {
    p_wk->phaseB = WBM_END_SEQ_SYNC;
}

BOOL WifiBattleMatchNet_EndMain(WifiBattleMatchNet *p_wk) {
    switch (p_wk->phaseB) {
    case WBM_END_SEQ_SYNC:
        WifiBattleMatchNet_StartSync(p_wk, 0x18);
        p_wk->timer = 0;
        p_wk->phaseB++;
        break;
    case WBM_END_SEQ_WAIT_SYNC:
        if (WifiBattleMatchNet_CheckSync(p_wk)) {
            p_wk->phaseB++;
        } else if (p_wk->timer++ > 600) {
            p_wk->phaseB++;
        }
        break;
    case WBM_END_SEQ_STOP_ERRORS:
        func_02042e94(0);
        GFL_NetSetDisconnectError(0);
        p_wk->counter = 0;
        p_wk->phaseB++;
        break;
    case WBM_END_SEQ_WAIT:
        if (p_wk->counter++ > 30) {
            p_wk->phaseB++;
        }
        break;
    case WBM_END_SEQ_DISCONNECT:
        NetState_WifiDisconnect(TRUE);
        p_wk->phaseB++;
        break;
    case WBM_END_SEQ_DONE:
        return TRUE;
    }
    return FALSE;
}

BOOL WifiBattleMatchNet_SendCmd2402(WifiBattleMatchNet *p_wk) {
    if (DWCRap_IsConnected()) {
        return GFL_NetSendCommand(NetHandle_GetOwn(), WBM_NET_CMD_2402, 4, &sCmd2402Data);
    }
    return TRUE;
}

BOOL WifiBattleMatchNet_TakeCmd2402(WifiBattleMatchNet *p_wk) {
    u32 flag = p_wk->flag140;

    if (flag != 0) {
        p_wk->flag140 = 0;
    }
    return flag;
}

BOOL WifiBattleMatchNet_SendPerson(WifiBattleMatchNet *p_wk) {
    BOOL toServer = GFL_NetGetHandleNetId(NetHandle_GetOwn()) == 0;

    JoinAvenuePerson_InitPlayer(&p_wk->person, p_wk->gameData);
    return GFL_NetSendCommandToMachine(NetHandle_GetOwn(), toServer, WBM_NET_CMD_PERSON, 0xc4, &p_wk->person, TRUE,
                                       FALSE, TRUE);
}

BOOL WifiBattleMatchNet_TakePerson(WifiBattleMatchNet *p_wk) {
    u32 flag = p_wk->flagPersonReceived;

    if (flag != 0) {
        p_wk->flagPersonReceived = 0;
    }
    return flag;
}

void WifiBattleMatchNet_SetGuest(WifiBattleMatchNet *p_wk, u32 value) {
    p_wk->unk1564 = value;
}

static void WifiBattleMatchNet_GetLoginDate(u8 *dest) {
    RTCDate date;
    RTCTime time;

    if (!GFL_NetIsInitialized()) {
        GFL_NetErrAbort();
    }
    if (!DWC_GetDateTime(&date, &time)) {
        GFL_NetErrAbort();
    }
    MI_CpuFill8(dest, 0, 6);
    dest[0] = date.year;
    dest[1] = date.month;
    dest[2] = date.day;
    dest[3] = time.hour;
    dest[4] = time.minute;
    dest[5] = time.second;
}

static void WifiBattleMatchNet_SetMatchKey(WifiBattleMatchNet *p_wk, u32 index, s32 value) {
    WifiBattleMatchNetKey *key;

    p_wk->matchKeys[index].value = value;
    key = &p_wk->matchKeys[index];
    MI_CpuCopy8(sMatchKeyNames[index], key->name, 3);
    key->name[3] = '\0';
    key->keyId = DWC_AddMatchKeyInt(key->keyId, key->name, &key->value);
}

static int WifiBattleMatchNet_EvalAlias(int index, void *work) {
    return WifiBattleMatchNet_Eval(index, work);
}

static int WifiBattleMatchNet_EvalAny(int index, void *work) {
    return 1000;
}

static int WifiBattleMatchNet_Eval(int index, void *work) {
    WifiBattleMatchNet *p_wk = work;
    int disconnects;
    int rating;
    int matches;
    int cup;
    int ratio;
    int myRatio;
    int ratingGap;
    int ratioGap;
    int score;

    rating = DWC_GetMatchIntValue(index, p_wk->matchKeys[WBM_MATCH_KEY_RAT].name, -1);
    disconnects = DWC_GetMatchIntValue(index, p_wk->matchKeys[WBM_MATCH_KEY_DIS].name, -1);
    matches = DWC_GetMatchIntValue(index, p_wk->matchKeys[WBM_MATCH_KEY_CNT].name, -1);
    cup = DWC_GetMatchIntValue(index, p_wk->matchKeys[WBM_MATCH_KEY_CUP].name, -1);

    if (rating == -1 && disconnects == -1 && cup == -1 && matches == -1) {
        return 0;
    }
    if (cup == p_wk->matchKeys[WBM_MATCH_KEY_CUP].value) {
        ratio = disconnects * 50 / (matches + 5);
        myRatio = p_wk->matchKeys[WBM_MATCH_KEY_DIS].value * 50 / (p_wk->matchKeys[WBM_MATCH_KEY_CNT].value + 5);
        ratingGap = MATH_ABS(rating - p_wk->matchKeys[WBM_MATCH_KEY_RAT].value);
        ratioGap = MATH_ABS(ratio - myRatio);
        score = 1000 - ratingGap - ratioGap * 32;
        if (score < 0) {
            return 0;
        }
        return score;
    }
    return 0;
}

void WifiBattleMatchNet_StartReport(WifiBattleMatchNet *p_wk, u32 mode, u32 kind, u32 rule,
                                    const WifiBattleMatchBattleResult *battleResult, u32 a5) {
    p_wk->unk1498 = 0;
    p_wk->state = WBM_REPORT_SEQ_SYNC;
    p_wk->unk1ac = 1;
    p_wk->flagValueReceived = 0;
    p_wk->flagInfoReceived = 0;
    p_wk->mode = mode;
    p_wk->flag164 = a5;
    p_wk->requestSent = 0;
    p_wk->flag16C = 0;
    p_wk->flag170 = 0;
    if (mode == WBM_REPORT_MODE_START) {
        switch (kind) {
        case WBM_MATCH_MODE_COMPETITION:
            p_wk->resultFunc = WifiBattleMatchNet_SetFlag19;
            break;
        case WBM_MATCH_MODE_RATING:
            p_wk->resultFunc = WifiBattleMatchNet_SetFlag1A;
            break;
        default:
            GFL_ASSERT(0);
            break;
        }
    } else if (mode == WBM_REPORT_MODE_RESULT) {
        if (a5 != 0) {
            switch (kind) {
            case WBM_MATCH_MODE_COMPETITION:
                p_wk->resultFunc = WifiBattleMatchNet_SetNoRating1C;
                break;
            case WBM_MATCH_MODE_RATING:
                p_wk->resultFunc = WifiBattleMatchNet_SetNoRating1B;
                break;
            default:
                GFL_ASSERT(0);
                break;
            }
        } else {
            GFL_NetIsParent();
            p_wk->param.format = rule;
            p_wk->param.outcome = battleResult->result;
            p_wk->param.mine = battleResult->unk10;
            switch (kind) {
            case WBM_MATCH_MODE_COMPETITION:
                p_wk->resultFunc = WifiBattleMatchNet_SetCupBattleResult;
                break;
            case WBM_MATCH_MODE_RATING:
                p_wk->resultFunc = WifiBattleMatchNet_SetBattleResult;
                break;
            default:
                GFL_ASSERT(0);
                break;
            }
        }
    }
}

static inline void WifiBattleMatchNet_CopySessionId(u8 *dest) {
    const char *src = Sc_GetSessionId();

    MI_CpuCopy8(src, dest, 0x28);
}

static inline void WifiBattleMatchNet_CopyConnectionId(u8 *dest) {
    const char *src = Sc_GetConnectionId();

    MI_CpuCopy8(src, dest, 0x28);
}

u32 WifiBattleMatchNet_ReportMain(WifiBattleMatchNet *p_wk, u32 *out) {
    GFLNetErrorInfo *netError;
    int err;

    if (out != NULL) {
        *out = p_wk->flag170;
    }
    netError = NetState_GetErrorInfo();
    if ((u32)(netError->type - 0x3f0) <= 1) {
        p_wk->flag164 = TRUE;
    }
    if (GFL_NetErrCheck() == 1) {
        if (netError->errorType == DWC_ETYPE_DISCONNECT || netError->errorType == DWC_ETYPE_SHUTDOWN_FM) {
            WifiBattleMatchNet_EndRequests(p_wk);
            return WBM_NET_FAILED;
        }
    } else if (p_wk->flag164 == 0 && GFL_NetErrCheck() == 2) {
        WifiBattleMatchNet_EndRequests(p_wk);
        return WBM_NET_FAILED;
    }
    if (p_wk->flag164 != 0 && p_wk->mode != WBM_REPORT_MODE_START && p_wk->requestSent == 0 &&
        p_wk->state > WBM_REPORT_SEQ_WAIT_SESSION && p_wk->state < WBM_REPORT_SEQ_SHARE_SESSION) {
        p_wk->flag16C = 0;
        p_wk->state = WBM_REPORT_SEQ_CREATE_SESSION;
    }
    switch (p_wk->state) {
    case WBM_REPORT_SEQ_SYNC:
        if (p_wk->flag164 != 0) {
            p_wk->state = WBM_REPORT_SEQ_STARTUP;
        } else {
            NetHandle_SendTimingPair(NetHandle_GetOwn(), 4, 0x24);
            p_wk->state = WBM_REPORT_SEQ_WAIT_SYNC;
        }
        break;
    case WBM_REPORT_SEQ_WAIT_SYNC:
        if (p_wk->flag164 != 0) {
            p_wk->state = WBM_REPORT_SEQ_STARTUP;
        } else if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 4, 0x24) || p_wk->flag164 != 0) {
            p_wk->state = WBM_REPORT_SEQ_SEND_VALUE;
        }
        break;
    case WBM_REPORT_SEQ_SEND_VALUE:
        if (p_wk->flag164 != 0) {
            p_wk->state = WBM_REPORT_SEQ_STARTUP;
        } else if (WifiBattleMatchNet_SendReportValue(p_wk, &p_wk->param.mine) || p_wk->flag164 != 0) {
            p_wk->state = WBM_REPORT_SEQ_TAKE_VALUE;
        }
        break;
    case WBM_REPORT_SEQ_TAKE_VALUE:
        if (p_wk->flag164 != 0) {
            p_wk->state = WBM_REPORT_SEQ_STARTUP;
        } else if (WifiBattleMatchNet_TakeReportValue(p_wk, &p_wk->param.theirs) || p_wk->flag164 != 0) {
            p_wk->state = WBM_REPORT_SEQ_STARTUP;
        }
        break;
    case WBM_REPORT_SEQ_STARTUP:
        p_wk->counter = 0;
        p_wk->gdbOpen = TRUE;
        err = Sc_Startup(0xb5f, 0);
        if (err != 0) {
            WifiBattleMatchNet_SetDwcError(&p_wk->error, err, 1937);
            WifiBattleMatchNet_EndRequests(p_wk);
            return WBM_NET_FAILED;
        }
        p_wk->state = WBM_REPORT_SEQ_GET_CERTIFICATE;
        break;
    case WBM_REPORT_SEQ_GET_CERTIFICATE:
        err = Sc_GetCertificate(&p_wk->p_data->mine.certificate);
        if (err != 0) {
            WifiBattleMatchNet_SetDwcError(&p_wk->error, err, 1953);
            WifiBattleMatchNet_EndRequests(p_wk);
            return WBM_NET_FAILED;
        }
        p_wk->state = WBM_REPORT_SEQ_CREATE_SESSION;
        break;
    case WBM_REPORT_SEQ_CREATE_SESSION:
        if (GFL_NetIsParent() || p_wk->flag164 != 0) {
            p_wk->counter++;
            err = Sc_CreateSession(WifiBattleMatchNet_OnRequestDone, 100, p_wk);
            p_wk->requestSent = TRUE;
            if (err != 0) {
                WifiBattleMatchNet_SetDwcError(&p_wk->error, err, 1971);
                WifiBattleMatchNet_EndRequests(p_wk);
                return WBM_NET_FAILED;
            }
            p_wk->timer = 0;
            p_wk->state = WBM_REPORT_SEQ_WAIT_SESSION;
        } else {
            p_wk->state = WBM_REPORT_SEQ_SYNC_SESSION;
        }
        break;
    case WBM_REPORT_SEQ_WAIT_SESSION:
        if (p_wk->counter == 0) {
            WifiBattleMatchNet_CopySessionId(p_wk->p_data->mine.unk8);
            p_wk->state = WBM_REPORT_SEQ_SYNC_SESSION;
        } else {
            if (p_wk->timer++ > 3600) {
                WifiBattleMatchNet_SetTimeoutError(&p_wk->error, 1, 1999);
                WifiBattleMatchNet_EndRequests(p_wk);
                return WBM_NET_FAILED;
            }
            Sc_Update();
            if (p_wk->errorCode != 0) {
                WifiBattleMatchNet_SetDwcError(&p_wk->error, p_wk->errorCode, 2007);
                WifiBattleMatchNet_EndRequests(p_wk);
                return WBM_NET_FAILED;
            }
        }
        break;
    case WBM_REPORT_SEQ_SYNC_SESSION:
        if (p_wk->flag164 != 0) {
            p_wk->state = WBM_REPORT_SEQ_SET_INTENTION;
        } else {
            WifiBattleMatchNet_CopyConnectionId(p_wk->p_data->mine.unk30);
            NetHandle_SendTimingPair(NetHandle_GetOwn(), 1, 0x24);
            p_wk->state = WBM_REPORT_SEQ_WAIT_SYNC_SESSION;
        }
        break;
    case WBM_REPORT_SEQ_WAIT_SYNC_SESSION:
        if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 1, 0x24) || p_wk->flag164 != 0) {
            p_wk->state = WBM_REPORT_SEQ_SHARE_SESSION;
        }
        break;
    case WBM_REPORT_SEQ_SHARE_SESSION:
        if (GFL_NetIsParent() || p_wk->flag164 != 0) {
            if (GFL_NetSendCommand(NetHandle_GetOwn(), WBM_NET_CMD_INFO, 0x68, &p_wk->p_data->mine) ||
                p_wk->flag164 != 0) {
                p_wk->state = WBM_REPORT_SEQ_SET_INTENTION;
            }
        } else if (p_wk->flagInfoReceived != 0 || p_wk->flag164 != 0) {
            p_wk->flagInfoReceived = 0;
            err = Sc_SetSessionId(p_wk->p_data->theirs.unk8);
            WifiBattleMatchNet_CopySessionId(p_wk->p_data->mine.unk8);
            if (err != 0) {
                WifiBattleMatchNet_SetDwcError(&p_wk->error, err, 2061);
                WifiBattleMatchNet_EndRequests(p_wk);
                return WBM_NET_FAILED;
            }
            p_wk->state = WBM_REPORT_SEQ_SET_INTENTION;
        }
        break;
    case WBM_REPORT_SEQ_SET_INTENTION:
        p_wk->counter++;
        err = Sc_SetReportIntention(p_wk->unk1ac, WifiBattleMatchNet_OnRequestDoneWithData, 100, p_wk);
        if (err != 0) {
            WifiBattleMatchNet_SetDwcError(&p_wk->error, err, 2077);
            WifiBattleMatchNet_EndRequests(p_wk);
            return WBM_NET_FAILED;
        }
        p_wk->timer = 0;
        p_wk->state = WBM_REPORT_SEQ_WAIT_INTENTION;
        break;
    case WBM_REPORT_SEQ_WAIT_INTENTION:
        if (p_wk->counter == 0) {
            p_wk->state = WBM_REPORT_SEQ_SYNC_INTENTION;
        } else {
            if (p_wk->timer++ > 3600) {
                WifiBattleMatchNet_SetTimeoutError(&p_wk->error, 1, 2098);
                WifiBattleMatchNet_EndRequests(p_wk);
                return WBM_NET_FAILED;
            }
            Sc_Update();
            if (p_wk->errorCode != 0) {
                WifiBattleMatchNet_SetDwcError(&p_wk->error, p_wk->errorCode, 2106);
                WifiBattleMatchNet_EndRequests(p_wk);
                return WBM_NET_FAILED;
            }
        }
        break;
    case WBM_REPORT_SEQ_SYNC_INTENTION:
        if (p_wk->flag164 != 0) {
            p_wk->state = WBM_REPORT_SEQ_BUILD_REPORT;
        } else {
            NetHandle_SendTimingPair(NetHandle_GetOwn(), 2, 0x24);
            p_wk->state = WBM_REPORT_SEQ_WAIT_SYNC_INTENTION;
        }
        break;
    case WBM_REPORT_SEQ_WAIT_SYNC_INTENTION:
        if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 2, 0x24) || p_wk->flag164 != 0) {
            p_wk->state = WBM_REPORT_SEQ_SEND_INFO_PARENT;
        }
        break;
    case WBM_REPORT_SEQ_SEND_INFO_PARENT:
        if (GFL_NetIsParent()) {
            if (GFL_NetSendCommand(NetHandle_GetOwn(), WBM_NET_CMD_INFO, 0x68, &p_wk->p_data->mine) ||
                p_wk->flag164 != 0) {
                p_wk->state = WBM_REPORT_SEQ_SEND_INFO_CHILD;
            }
        } else if (p_wk->flagInfoReceived != 0 || p_wk->flag164 != 0) {
            if (p_wk->flag164 == 0) {
                p_wk->flag16C = TRUE;
            }
            p_wk->flagInfoReceived = 0;
            p_wk->state = WBM_REPORT_SEQ_SEND_INFO_CHILD;
        }
        break;
    case WBM_REPORT_SEQ_SEND_INFO_CHILD:
        if (!GFL_NetIsParent()) {
            if (GFL_NetSendCommand(NetHandle_GetOwn(), WBM_NET_CMD_INFO, 0x68, &p_wk->p_data->mine) ||
                p_wk->flag164 != 0) {
                p_wk->state = WBM_REPORT_SEQ_SYNC_INFO;
            }
        } else if (p_wk->flagInfoReceived != 0 || p_wk->flag164 != 0) {
            if (p_wk->flag164 == 0) {
                p_wk->flag16C = TRUE;
            }
            p_wk->flagInfoReceived = 0;
            p_wk->state = WBM_REPORT_SEQ_SYNC_INFO;
        }
        break;
    case WBM_REPORT_SEQ_SYNC_INFO:
        NetHandle_SendTimingPair(NetHandle_GetOwn(), 3, 0x24);
        p_wk->state = WBM_REPORT_SEQ_WAIT_SYNC_INFO;
        break;
    case WBM_REPORT_SEQ_WAIT_SYNC_INFO:
        if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 3, 0x24) || p_wk->flag164 != 0) {
            p_wk->state = WBM_REPORT_SEQ_BUILD_REPORT;
        }
        break;
    case WBM_REPORT_SEQ_BUILD_REPORT:
        err =
            WifiBattleMatchNet_SendBattleResult(&p_wk->p_data->mine, &p_wk->p_data->theirs, &p_wk->param, p_wk->unk1ac,
                                                p_wk->resultFunc, p_wk->flag164, p_wk->mode, p_wk->flag16C);
        if (err != 0) {
            WifiBattleMatchNet_SetDwcError(&p_wk->error, err, 2221);
            WifiBattleMatchNet_EndRequests(p_wk);
            return WBM_NET_FAILED;
        }
        p_wk->state = WBM_REPORT_SEQ_SUBMIT_REPORT;
        break;
    case WBM_REPORT_SEQ_SUBMIT_REPORT:
        p_wk->counter++;
        err =
            Sc_SubmitReport(p_wk->p_data->mine.connection, p_wk->unk1ac, WifiBattleMatchNet_OnRequestDone2, 100, p_wk);
        if (err != 0) {
            WifiBattleMatchNet_SetDwcError(&p_wk->error, err, 2236);
            WifiBattleMatchNet_EndRequests(p_wk);
            return WBM_NET_FAILED;
        }
        p_wk->timer = 0;
        p_wk->state = WBM_REPORT_SEQ_WAIT_SUBMIT;
        break;
    case WBM_REPORT_SEQ_WAIT_SUBMIT:
        if (p_wk->counter == 0) {
            p_wk->state = WBM_REPORT_SEQ_END;
        } else {
            if (p_wk->timer++ > 3600) {
                WifiBattleMatchNet_SetTimeoutError(&p_wk->error, 1, 2256);
                WifiBattleMatchNet_EndRequests(p_wk);
                return WBM_NET_FAILED;
            }
            Sc_Update();
            if (p_wk->errorCode != 0) {
                WifiBattleMatchNet_SetDwcError(&p_wk->error, p_wk->errorCode, 2264);
                WifiBattleMatchNet_EndRequests(p_wk);
                return WBM_NET_FAILED;
            }
        }
        break;
    case WBM_REPORT_SEQ_END:
        p_wk->flag170 = TRUE;
        WifiBattleMatchNet_EndRequests(p_wk);
        p_wk->state = WBM_REPORT_SEQ_SYNC_END;
        break;
    case WBM_REPORT_SEQ_SYNC_END:
        NetHandle_SendTimingPair(NetHandle_GetOwn(), 5, 0x24);
        p_wk->state = WBM_REPORT_SEQ_WAIT_SYNC_END;
        break;
    case WBM_REPORT_SEQ_WAIT_SYNC_END:
        if (NetHandle_IsTimingPairRecv(NetHandle_GetOwn(), 5, 0x24) || p_wk->flag164 != 0) {
            p_wk->state = WBM_REPORT_SEQ_DONE;
        }
        break;
    case WBM_REPORT_SEQ_DONE:
        return WBM_NET_SUCCESS;
    }
    return WBM_NET_RUNNING;
}

u32 WifiBattleMatchNet_GetReceivedValue(WifiBattleMatchNet *p_wk) {
    return p_wk->receivedValue;
}

static void WifiBattleMatchNet_OnRequestDone(int error, void *work) {
    WifiBattleMatchNet *p_wk = work;

    if (error != 0) {
        p_wk->errorCode = error;
    }
    p_wk->counter--;
}

static void WifiBattleMatchNet_OnRequestDoneWithData(int error, void *work) {
    WifiBattleMatchNet *p_wk = work;
    WifiBattleMatchNetInfo *mine = &p_wk->p_data->mine;

    if (error == 0) {
        MI_CpuCopy8(Sc_GetConnectionId(), mine->unk30, 0x28);
    } else {
        p_wk->errorCode = error;
    }
    p_wk->counter--;
}

static void WifiBattleMatchNet_OnRequestDone2(int error, void *work) {
    WifiBattleMatchNet *p_wk = work;

    if (error != 0) {
        p_wk->errorCode = error;
    }
    p_wk->counter--;
}

static void WifiBattleMatchNet_EndRequests(WifiBattleMatchNet *p_wk) {
    if (p_wk->gdbOpen == 1) {
        if (p_wk->state > WBM_REPORT_SEQ_BUILD_REPORT) {
            Sc_FreeReport(p_wk->p_data->mine.connection);
        }
        if (p_wk->state >= WBM_REPORT_SEQ_STARTUP) {
            Sc_Shutdown();
        }
        p_wk->gdbOpen = 0;
    }
}

static int WifiBattleMatchNet_SendBattleResult(WifiBattleMatchNetInfo *info, WifiBattleMatchNetInfo *theirs,
                                               WifiBattleMatchNetParam *param, int a3,
                                               WifiBattleMatchNetResultFunc func, int a5, int mode, int a7) {
    int err;
    int resultMode;

    err = Sc_CreateReport(1, 2, 0, &info->connection);
    if (err != 0) {
        return err;
    }
    err = Sc_BeginGame(info->connection);
    if (err != 0) {
        return err;
    }
    err = Sc_BeginPlayers(info->connection);
    if (err != 0) {
        return err;
    }
    err = Sc_BeginGroup(info->connection);
    if (err != 0) {
        return err;
    }
    if (a5 != 0 || mode == WBM_REPORT_MODE_START) {
        resultMode = SCGameResult_NONE;
    } else {
        switch (param->outcome) {
        case 1:
        case 4:
            resultMode = SCGameResult_WIN;
            break;
        case 0:
        case 3:
            resultMode = SCGameResult_LOSS;
            break;
        case 2:
            resultMode = SCGameResult_DRAW;
            break;
        default:
            GFL_ASSERT_MSG(0, "\x83\x6f\x83\x67\x83\x8b\x8c\x8b\x89\xca\x95\x73\x90\xb3\x92\x6c %d\n", param->outcome);
            break;
        }
    }
    err = Sc_SetPlayer(info->connection, 0, info->unk30, 0, resultMode, info->certificate, info->unk58);
    if (err != 0) {
        return err;
    }
    err = func(info, param, 1, a5);
    if (err != 0) {
        return err;
    }
    err = Sc_BeginGroup(info->connection);
    if (err != 0) {
        return err;
    }
    if (a7 == 1) {
        int theirsMode;

        if (mode == WBM_REPORT_MODE_START) {
            theirsMode = SCGameResult_NONE;
        } else if (a5 != 0) {
            theirsMode = SCGameResult_DISCONNECT;
        } else {
            switch (param->outcome) {
            case 1:
            case 4:
                theirsMode = SCGameResult_LOSS;
                break;
            case 0:
            case 3:
                theirsMode = SCGameResult_WIN;
                break;
            case 2:
                theirsMode = SCGameResult_DRAW;
                break;
            default:
                // clang-format off
                GFL_ASSERT_MSG(0, "\x83\x6f\x83\x67\x83\x8b\x8c\x8b\x89\xca\x95\x73\x90\xb3\x92\x6c %d\n", param->outcome);
                // clang-format on
                break;
            }
        }
        err = Sc_SetPlayer(info->connection, 1, theirs->unk30, 0, theirsMode, theirs->certificate, theirs->unk58);
        if (err != 0) {
            return err;
        }
        err = func(info, param, 0, a5);
        if (err != 0) {
            return err;
        }
    }
    err = Sc_BeginTeams(info->connection);
    if (err != 0) {
        return err;
    }
    if (a5 != 0) {
        return Sc_FinishReport(info->connection, a3, SCGameStatus_PARTIAL);
    }
    return Sc_FinishReport(info->connection, a3, SCGameStatus_COMPLETE);
}

static int WifiBattleMatchNet_SetBattleResult(WifiBattleMatchNetInfo *info, WifiBattleMatchNetParam *param, int isMine,
                                              int a3) {
    int win;
    int lose;
    int skip;
    int draw;
    int decided;
    int err;

    decided = 0;
    skip = 0;
    draw = 1;
    if (param->mine == 0 && param->theirs == 0) {
        draw = 0;
    }
    switch (param->outcome) {
    case 0:
    case 3:
        win = 0;
        lose = 1;
        decided = 1;
        break;
    case 1:
    case 4:
        win = 1;
        lose = 0;
        decided = 1;
        break;
    case 2:
        win = 0;
        lose = 0;
        skip = 1;
        break;
    case 5:
    case 6:
    default:
        win = 0;
        lose = 0;
        break;
    }
    if (isMine == 0 && decided) {
        win ^= 1;
        lose ^= 1;
    }
    switch (param->format) {
    case WBM_RULE_SINGLE:
        if (skip == 0) {
            err = Sc_AddByte(info->connection, 0xf, 1);
            if (err != 0) {
                return err;
            }
        }
        err = Sc_AddInt(info->connection, 5, win);
        if (err != 0) {
            return err;
        }
        err = Sc_AddInt(info->connection, 6, lose);
        if (err != 0) {
            return err;
        }
        break;
    case WBM_RULE_DOUBLE:
        if (skip == 0) {
            err = Sc_AddByte(info->connection, 0x10, 1);
            if (err != 0) {
                return err;
            }
        }
        err = Sc_AddInt(info->connection, 8, win);
        if (err != 0) {
            return err;
        }
        err = Sc_AddInt(info->connection, 7, lose);
        if (err != 0) {
            return err;
        }
        break;
    case WBM_RULE_TRIPLE:
        if (skip == 0) {
            err = Sc_AddByte(info->connection, 0x12, 1);
            if (err != 0) {
                return err;
            }
        }
        err = Sc_AddInt(info->connection, 0xb, win);
        if (err != 0) {
            return err;
        }
        err = Sc_AddInt(info->connection, 0xc, lose);
        if (err != 0) {
            return err;
        }
        break;
    case WBM_RULE_ROTATION:
        if (skip == 0) {
            err = Sc_AddByte(info->connection, 0x11, 1);
            if (err != 0) {
                return err;
            }
        }
        err = Sc_AddInt(info->connection, 9, win);
        if (err != 0) {
            return err;
        }
        err = Sc_AddInt(info->connection, 0xa, lose);
        if (err != 0) {
            return err;
        }
        break;
    case WBM_RULE_SHOOTER:
        if (skip == 0) {
            err = Sc_AddByte(info->connection, 0x13, 1);
            if (err != 0) {
                return err;
            }
        }
        err = Sc_AddInt(info->connection, 0xd, win);
        if (err != 0) {
            return err;
        }
        err = Sc_AddInt(info->connection, 0xe, lose);
        if (err != 0) {
            return err;
        }
        break;
    default:
        GFL_ASSERT(0);
        break;
    }
    err = Sc_AddInt(info->connection, 0x14, draw != 0);
    if (err != 0) {
        return err;
    }
    return Sc_AddInt(info->connection, 0x1b, -1);
}

static int WifiBattleMatchNet_SetCupBattleResult(WifiBattleMatchNetInfo *info, WifiBattleMatchNetParam *param,
                                                 int isMine, int a3) {
    int win;
    int lose;
    int draw;
    int decided;
    int skip;
    int err;

    decided = 0;
    skip = 0;
    draw = 1;
    if (param->mine == 0 && param->theirs == 0) {
        draw = 0;
    }
    switch (param->outcome) {
    case 0:
    case 3:
        win = 0;
        lose = 1;
        decided = 1;
        break;
    case 1:
    case 4:
        win = 1;
        lose = 0;
        decided = 1;
        break;
    case 2:
        win = 0;
        lose = 0;
        skip = 1;
        break;
    case 5:
    case 6:
    default:
        win = 0;
        lose = 0;
        break;
    }
    if (isMine == 0 && decided) {
        win ^= 1;
        lose ^= 1;
    }
    if (skip == 0) {
        err = Sc_AddByte(info->connection, 0x15, 1);
        if (err != 0) {
            return err;
        }
    }
    err = Sc_AddInt(info->connection, 0x16, win);
    if (err != 0) {
        return err;
    }
    err = Sc_AddInt(info->connection, 0x17, lose);
    if (err != 0) {
        return err;
    }
    err = Sc_AddInt(info->connection, 0x18, draw != 0);
    if (err != 0) {
        return err;
    }
    return Sc_AddInt(info->connection, 0x1c, -1);
}

static int WifiBattleMatchNet_SetFlag1A(WifiBattleMatchNetInfo *info, WifiBattleMatchNetParam *param, int isMine,
                                        int a3) {
    return Sc_AddInt(info->connection, 0x1a, 1);
}

static int WifiBattleMatchNet_SetFlag19(WifiBattleMatchNetInfo *info, WifiBattleMatchNetParam *param, int isMine,
                                        int a3) {
    return Sc_AddInt(info->connection, 0x19, 1);
}

static int WifiBattleMatchNet_SetNoRating1B(WifiBattleMatchNetInfo *info, WifiBattleMatchNetParam *param, int isMine,
                                            int a3) {
    return Sc_AddInt(info->connection, 0x1b, -1);
}

static int WifiBattleMatchNet_SetNoRating1C(WifiBattleMatchNetInfo *info, WifiBattleMatchNetParam *param, int isMine,
                                            int a3) {
    return Sc_AddInt(info->connection, 0x1c, -1);
}

static void WifiBattleMatchNet_OnInfoReceived(int netId, int size, const void *data, void *work, NetHandle *handle) {
    WifiBattleMatchNet *p_wk = work;

    if (handle == NetHandle_GetOwn() && netId != GFL_NetGetHandleNetId(NetHandle_GetOwn())) {
        MI_CpuCopy8(data, &p_wk->p_data->theirs, 0x68);
        p_wk->flagInfoReceived = 1;
    }
}

static void WifiBattleMatchNet_OnPlayerInfoReceived(int netId, int size, const void *data, void *work,
                                                    NetHandle *handle) {
    WifiBattleMatchNet *p_wk = work;
    PlayerInfo *info = GameData_GetPlayerInfoByNetID(p_wk->gameData, netId);

    MI_CpuCopy8(data, info, PlayerInfo_GetSize());
    if (handle == NetHandle_GetOwn() && netId != GFL_NetGetHandleNetId(NetHandle_GetOwn())) {
        p_wk->flagPlayerInfoReceived = 1;
    }
}

static void *WifiBattleMatchNet_GetRecvBuffer(int netId, void *work, int size) {
    WifiBattleMatchNet *p_wk = work;

    if (netId == GFL_NetGetHandleNetId(NetHandle_GetOwn())) {
        GFL_ASSERT(0);
        return NULL;
    }
    return p_wk->recvBuffer;
}

static void WifiBattleMatchNet_OnCmd2402(int netId, int size, const void *data, void *work, NetHandle *handle) {
    WifiBattleMatchNet *p_wk = work;

    if (handle == NetHandle_GetOwn() && netId != GFL_NetGetHandleNetId(NetHandle_GetOwn())) {
        p_wk->flag140 = 1;
    }
}

static void WifiBattleMatchNet_OnPartyReceived(int netId, int size, const void *data, void *work, NetHandle *handle) {
    WifiBattleMatchNet *p_wk = work;

    if (handle == NetHandle_GetOwn() && netId != GFL_NetGetHandleNetId(NetHandle_GetOwn())) {
        p_wk->flagPartyReceived = 1;
    }
}

static void WifiBattleMatchNet_OnCmd2404(int netId, int size, const void *data, void *work, NetHandle *handle) {
    WifiBattleMatchNet *p_wk = work;

    if (handle == NetHandle_GetOwn() && netId != GFL_NetGetHandleNetId(NetHandle_GetOwn())) {
        p_wk->flag148 = 1;
    }
}

static void WifiBattleMatchNet_OnValueReceived(int netId, int size, const void *data, void *work, NetHandle *handle) {
    WifiBattleMatchNet *p_wk = work;
    u32 value = *(const u32 *)data;

    if (handle == NetHandle_GetOwn() && netId != GFL_NetGetHandleNetId(NetHandle_GetOwn())) {
        p_wk->receivedValue = value;
        p_wk->flagValueReceived = 1;
    }
}

static void WifiBattleMatchNet_OnPersonReceived(int netId, int size, const void *data, void *work, NetHandle *handle) {
    WifiBattleMatchNet *p_wk = work;
    JoinAvenueSave *joinAvenue = SaveControl_GetJoinAvenue(GameData_GetSaveControl(p_wk->gameData));

    if (handle == NetHandle_GetOwn() && netId != GFL_NetGetHandleNetId(NetHandle_GetOwn())) {
        JoinAvenuePerson *person = (JoinAvenuePerson *)data;

        JoinAvenuePerson_SetParam(person, JOIN_AVE_PARAM_UNK_26, 1);
        if (p_wk->unk1564 != 0) {
            MsgData *msgData =
                GFL_MsgSysLoadData(FALSE, ARCID_SYSTEM_MESSAGE, TEXT_BANK_ABILITY_HANDLERS_BTL_MAIN, p_wk->msgHeapId);
            StrBuf *name = GFL_MsgDataLoadStrbufNew(msgData, WBM_DEFAULT_NAME);

            GFL_MsgDataFree(msgData);
            JoinAvenuePerson_SetParam(person, JOIN_AVE_PARAM_NAME, (uptr)GFL_StrBufGetStringPtr(name));
            GFL_StrBufFree(name);
        }
        JoinAvenue_Add(joinAvenue, p_wk->gameData, person, 1);
        p_wk->flagPersonReceived = 1;
    }
}

void WifiBattleMatchNet_StartRead(WifiBattleMatchNet *p_wk, int recordId, int kind, void *out) {
    p_wk->state = WBM_GDB_SEQ_STARTUP;
    p_wk->unk17c = out;
    p_wk->recordIdArg = recordId;
    switch (kind) {
    case WBM_READ_ARENA:
        p_wk->tableNames = WifiBattleMatchNet_GetArenaFieldNames();
        p_wk->table_name_num = WifiBattleMatchNet_GetArenaFieldCount();
        p_wk->recordsFunc = WifiBattleMatchNet_ReadArena;
        break;
    case WBM_READ_WIFICUP:
        p_wk->tableNames = WifiBattleMatchNet_GetWifiCupFieldNames();
        p_wk->table_name_num = WifiBattleMatchNet_GetWifiCupFieldCount();
        p_wk->recordsFunc = WifiBattleMatchNet_ReadWifiCup;
        break;
    case WBM_READ_RECORD_ID:
        p_wk->tableNames = &sRecordIdTable;
        p_wk->table_name_num = 1;
        p_wk->recordsFunc = WifiBattleMatchNet_ReadRecordId;
        break;
    case WBM_READ_WIFICUP_PARTY:
        p_wk->tableNames = &sWifiCupPartyTable;
        p_wk->table_name_num = 1;
        p_wk->recordsFunc = WifiBattleMatchNet_ReadWifiCupParty;
        break;
    case WBM_READ_LAST_LOGIN:
        p_wk->tableNames = &sLastLoginTable;
        p_wk->table_name_num = 1;
        p_wk->recordsFunc = WifiBattleMatchNet_ReadLastLogin;
        break;
    }
}

u32 WifiBattleMatchNet_ReadMain(WifiBattleMatchNet *p_wk) {
    int err;

    switch (p_wk->state) {
    case WBM_GDB_SEQ_STARTUP:
        err = Gdb_Startup(0xb5f, p_wk->unk0, 0);
        if (err != 0) {
            WifiBattleMatchNet_SetGdbError(&p_wk->error, err, 3674);
            WifiBattleMatchNet_EndGdb(p_wk);
            return WBM_NET_FAILED;
        }
        p_wk->state = WBM_GDB_SEQ_REQUEST;
        break;
    case WBM_GDB_SEQ_REQUEST:
        GFL_ASSERT(p_wk->table_name_num < WBM_GDB_FIELD_TABLE_MAX);
        if (p_wk->recordIdArg == -1) {
            err = Gdb_GetMyRecords(WBM_GDB_TABLE_NAME, p_wk->tableNames, p_wk->table_name_num,
                                   WifiBattleMatchNet_OnRecords, p_wk);
        } else {
            err = Gdb_GetSpecificRecords(WBM_GDB_TABLE_NAME, &p_wk->recordIdArg, 1, p_wk->tableNames,
                                         p_wk->table_name_num, WifiBattleMatchNet_OnRecords, p_wk);
        }
        if (err != 0) {
            WifiBattleMatchNet_SetGdbError(&p_wk->error, err, 3701);
            WifiBattleMatchNet_EndGdb(p_wk);
            return WBM_NET_FAILED;
        }
        p_wk->state = WBM_GDB_SEQ_WAIT;
        p_wk->timer = 0;
        break;
    case WBM_GDB_SEQ_WAIT:
        if (p_wk->timer++ > 3600) {
            WifiBattleMatchNet_SetTimeoutError(&p_wk->error, 1, 3714);
            p_wk->state = WBM_GDB_SEQ_CANCEL;
        } else {
            Gdb_Update();
            err = Gdb_GetState();
            if (err == GDB_STATE_OFF || err == 4) {
                WifiBattleMatchNet_SetGdbSubError1(&p_wk->error, err, 3729);
                p_wk->state = WBM_GDB_SEQ_CANCEL;
            } else if (err == GDB_STATE_IDLE) {
                p_wk->state = WBM_GDB_SEQ_RESULT;
            }
        }
        break;
    case WBM_GDB_SEQ_RESULT:
        err = Gdb_GetResult();
        if (err == 0) {
            WifiBattleMatchNet_SetGdbSubError2(&p_wk->error, err, 3748);
            p_wk->state = WBM_GDB_SEQ_CANCEL;
        } else if (err != 1) {
            WifiBattleMatchNet_SetGdbSubError2(&p_wk->error, err, 3754);
            WifiBattleMatchNet_EndGdb(p_wk);
            return WBM_NET_FAILED;
        } else {
            p_wk->state = WBM_GDB_SEQ_END;
        }
        break;
    case WBM_GDB_SEQ_END:
        WifiBattleMatchNet_EndGdb(p_wk);
        p_wk->state = WBM_GDB_SEQ_DONE;
        break;
    case WBM_GDB_SEQ_DONE:
        return WBM_NET_SUCCESS;
    case WBM_GDB_SEQ_CANCEL:
        err = Gdb_Cancel();
        if (err == 0) {
            p_wk->timer = 0;
            p_wk->state = WBM_GDB_SEQ_WAIT_CANCEL;
        } else {
            WifiBattleMatchNet_SetGdbError(&p_wk->error, err, 3785);
            WifiBattleMatchNet_EndGdb(p_wk);
            return WBM_NET_FAILED;
        }
        break;
    case WBM_GDB_SEQ_WAIT_CANCEL:
        if (p_wk->timer++ > 3600) {
            WifiBattleMatchNet_SetTimeoutError(&p_wk->error, 1, 3797);
            WifiBattleMatchNet_EndGdb(p_wk);
            return WBM_NET_FAILED;
        }
        Gdb_Update();
        err = Gdb_GetState();
        if (err == GDB_STATE_OFF || err == 4) {
            WifiBattleMatchNet_EndGdb(p_wk);
            return WBM_NET_FAILED;
        } else if (err == GDB_STATE_IDLE) {
            p_wk->state = WBM_GDB_SEQ_FAILED;
        }
        break;
    case WBM_GDB_SEQ_FAILED:
        WifiBattleMatchNet_EndGdb(p_wk);
        return WBM_NET_FAILED;
    }
    return WBM_NET_RUNNING;
}

static void WifiBattleMatchNet_EndGdb(WifiBattleMatchNet *p_wk) {
    Gdb_Shutdown();
}

static void WifiBattleMatchNet_BuildProfileRecord(WifiBattleMatchNet *p_wk) {
    PlayerInfo *info = GetGameDataPlayerInfo(p_wk->gameData);

    p_wk->record[0].name = "MYSTATUS";
    p_wk->record[0].type = GDB_FIELD_BINARY;
    p_wk->record[0].value.binaryData.data = info;
    p_wk->record[0].value.binaryData.size = 0x20;
    WifiBattleMatchNet_GetLoginDate(p_wk->loginDate);
    p_wk->record[1].name = "LAST_LOGIN_DATETIME";
    p_wk->record[1].type = GDB_FIELD_BINARY;
    p_wk->record[1].value.binaryData.data = p_wk->loginDate;
    p_wk->record[1].value.binaryData.size = 6;
    p_wk->record[2].name = "INITIAL_PROFILE_ID";
    p_wk->record[2].type = GDB_FIELD_INT;
    p_wk->record[2].value.intValue = p_wk->profileId;
    p_wk->record[3].name = "NOW_PROFILE_ID";
    p_wk->record[3].type = GDB_FIELD_INT;
    p_wk->record[3].value.intValue = WifiBattleMatchNet_GetGSID(p_wk);
    p_wk->table_name_num = 4;
}

static void WifiBattleMatchNet_OnRecords(int recordNum, GdbField **records, void *work, int fieldNum) {
    WifiBattleMatchNet *p_wk = work;
    int i;
    int j;

    p_wk->recordsFunc(recordNum, records, p_wk->unk17c, p_wk->table_name_num);
    if (p_wk->recordIdArg == -1) {
        for (i = 0; i < recordNum; i++) {
            for (j = 0; j < p_wk->table_name_num; j++) {
                GdbField *field = &records[i][j];

                if (_STD_CompareNString(field->name, "recordid") == 0) {
                    p_wk->p_data->sake_recordID = field->value.intValue;
                }
            }
        }
    }
}

static void WifiBattleMatchNet_ReadArena(int recordNum, GdbField **records, void *work, int fieldNum) {
    WifiBattleMatchNetStats *stats = work;
    int i;
    int j;

    for (i = 0; i < recordNum; i++) {
        for (j = 0; j < fieldNum; j++) {
            GdbField *field = &records[i][j];

            if (!_STD_CompareNString(field->name, WifiBattleMatchNet_GetArenaFieldName(ARENA_NUM_SINGLE_WIN))) {
                stats->numSingleWin = field->value.intValue;
            } else if (!_STD_CompareNString(field->name, WifiBattleMatchNet_GetArenaFieldName(ARENA_NUM_SINGLE_LOSE))) {
                stats->numSingleLose = field->value.intValue;
            } else if (!_STD_CompareNString(field->name, WifiBattleMatchNet_GetArenaFieldName(ARENA_NUM_DOUBLE_WIN))) {
                stats->numDoubleWin = field->value.intValue;
            } else if (!_STD_CompareNString(field->name, WifiBattleMatchNet_GetArenaFieldName(ARENA_NUM_DOUBLE_LOSE))) {
                stats->numDoubleLose = field->value.intValue;
            } else if (!_STD_CompareNString(field->name, WifiBattleMatchNet_GetArenaFieldName(ARENA_NUM_ROTATE_WIN))) {
                stats->numRotateWin = field->value.intValue;
            } else if (!_STD_CompareNString(field->name, WifiBattleMatchNet_GetArenaFieldName(ARENA_NUM_ROTATE_LOSE))) {
                stats->numRotateLose = field->value.intValue;
            } else if (!_STD_CompareNString(field->name, WifiBattleMatchNet_GetArenaFieldName(ARENA_NUM_TRIPLE_WIN))) {
                stats->numTripleWin = field->value.intValue;
            } else if (!_STD_CompareNString(field->name, WifiBattleMatchNet_GetArenaFieldName(ARENA_NUM_TRIPLE_LOSE))) {
                stats->numTripleLose = field->value.intValue;
            } else if (!_STD_CompareNString(field->name, WifiBattleMatchNet_GetArenaFieldName(ARENA_NUM_SHOOTER_WIN))) {
                stats->numShooterWin = field->value.intValue;
            } else if (!_STD_CompareNString(field->name,
                                            WifiBattleMatchNet_GetArenaFieldName(ARENA_NUM_SHOOTER_LOSE))) {
                stats->numShooterLose = field->value.intValue;
            } else if (!_STD_CompareNString(field->name,
                                            WifiBattleMatchNet_GetArenaFieldName(ARENA_ELO_RATING_1V1_SINGLE))) {
                stats->eloRatingSingle = field->value.intValue;
            } else if (!_STD_CompareNString(field->name,
                                            WifiBattleMatchNet_GetArenaFieldName(ARENA_ELO_RATING_1V1_DOUBLE))) {
                stats->eloRatingDouble = field->value.intValue;
            } else if (!_STD_CompareNString(field->name,
                                            WifiBattleMatchNet_GetArenaFieldName(ARENA_ELO_RATING_1V1_ROTATE))) {
                stats->eloRatingRotate = field->value.intValue;
            } else if (!_STD_CompareNString(field->name,
                                            WifiBattleMatchNet_GetArenaFieldName(ARENA_ELO_RATING_1V1_TRIPLE))) {
                stats->eloRatingTriple = field->value.intValue;
            } else if (!_STD_CompareNString(field->name,
                                            WifiBattleMatchNet_GetArenaFieldName(ARENA_ELO_RATING_1V1_SHOOTER))) {
                stats->eloRatingShooter = field->value.intValue;
            } else if (!_STD_CompareNString(field->name,
                                            WifiBattleMatchNet_GetArenaFieldName(ARENA_DISCONNECTS_COUNTER))) {
                stats->disconnectsCounter = field->value.intValue;
            } else if (!_STD_CompareNString(field->name, WifiBattleMatchNet_GetArenaFieldName(ARENA_CHEATS_COUNTER))) {
                stats->cheatsCounter = field->value.intValue;
            } else if (!_STD_CompareNString(field->name, "INITIAL_PROFILE_ID")) {
                stats->initialProfileId = field->value.intValue;
            } else if (!_STD_CompareNString(field->name, "NOW_PROFILE_ID")) {
                stats->nowProfileId = field->value.intValue;
            } else if (!_STD_CompareNString(field->name, "RECORD_SAVE_IDX")) {
                stats->recordSaveIdx = field->value.intValue;
            } else if (!_STD_CompareNString(field->name, "recordid")) {
                stats->recordId = field->value.intValue;
            }
        }
    }
}

static void WifiBattleMatchNet_ReadWifiCup(int recordNum, GdbField **records, void *work, int fieldNum) {
    WifiBattleMatchNetCupStats *stats = work;
    int i;
    int j;

    for (i = 0; i < recordNum; i++) {
        for (j = 0; j < fieldNum; j++) {
            GdbField *field = &records[i][j];

            if (!_STD_CompareNString(field->name, WifiBattleMatchNet_GetArenaFieldName(ARENA_NUM_WIFICUP_WIN))) {
                stats->numWin = field->value.intValue;
            } else if (!_STD_CompareNString(field->name,
                                            WifiBattleMatchNet_GetArenaFieldName(ARENA_NUM_WIFICUP_LOSE))) {
                stats->numLose = field->value.intValue;
            } else if (!_STD_CompareNString(field->name,
                                            WifiBattleMatchNet_GetArenaFieldName(ARENA_ELO_RATING_1V1_WIFICUP))) {
                stats->eloRating = field->value.intValue;
            } else if (!_STD_CompareNString(field->name,
                                            WifiBattleMatchNet_GetArenaFieldName(ARENA_DISCONNECTS_WIFICUP_COUNTER))) {
                stats->disconnectsCounter = field->value.intValue;
            } else if (!_STD_CompareNString(field->name,
                                            WifiBattleMatchNet_GetArenaFieldName(ARENA_CHEATS_WIFICUP_COUNTER))) {
                stats->cheatsCounter = field->value.intValue;
            } else if (!_STD_CompareNString(field->name, "RECORD_SAVE_IDX")) {
                stats->recordSaveIdx = field->value.intValue;
            } else if (!_STD_CompareNString(field->name, "WIFICUP_POKEMON_PARTY")) {
                GdbBinaryData *bin = &field->value.binaryData;

                MI_CpuCopy8(bin->data, stats->party, bin->size);
            }
        }
    }
}

static void WifiBattleMatchNet_ReadRecordId(int recordNum, GdbField **records, void *work, int fieldNum) {
    int i;
    int j;

    for (i = 0; i < recordNum; i++) {
        for (j = 0; j < fieldNum; j++) {
            GdbField *field = &records[i][j];

            if (_STD_CompareNString(field->name, "recordid") == 0) {
                *(s32 *)work = field->value.intValue;
            }
        }
    }
}

static void WifiBattleMatchNet_ReadWifiCupParty(int recordNum, GdbField **records, void *work, int fieldNum) {
    int i;
    int j;

    for (i = 0; i < recordNum; i++) {
        for (j = 0; j < fieldNum; j++) {
            GdbField *field = &records[i][j];

            if (_STD_CompareNString(field->name, "WIFICUP_POKEMON_PARTY") == 0) {
                MI_CpuCopy8(field->value.binaryData.data, work, field->value.binaryData.size);
            }
        }
    }
}

static void WifiBattleMatchNet_ReadLastLogin(int recordNum, GdbField **records, void *work, int fieldNum) {
    int i;
    int j;

    for (i = 0; i < recordNum; i++) {
        for (j = 0; j < fieldNum; j++) {
            GdbField *field = &records[i][j];

            if (_STD_CompareNString(field->name, "LAST_LOGIN_DATETIME") == 0) {
                MI_CpuCopy8(field->value.binaryData.data, work, field->value.binaryData.size);
            }
        }
    }
}

void WifiBattleMatchNet_StartWrite(WifiBattleMatchNet *p_wk, int kind, void *data) {
    p_wk->state = WBM_GDB_SEQ_STARTUP;
    GFL_ASSERT(p_wk->p_data->sake_recordID != 0);
    switch (kind) {
    case WBM_WRITE_WIFICUP:
        p_wk->record[0].name = WifiBattleMatchNet_GetArenaFieldName(ARENA_ELO_RATING_1V1_WIFICUP);
        p_wk->record[0].type = GDB_FIELD_INT;
        p_wk->record[0].value.intValue = 1500;
        p_wk->record[1].name = WifiBattleMatchNet_GetArenaFieldName(ARENA_CHEATS_WIFICUP_COUNTER);
        p_wk->record[1].type = GDB_FIELD_INT;
        p_wk->record[1].value.intValue = 0;
        p_wk->record[2].name = WifiBattleMatchNet_GetArenaFieldName(ARENA_DISCONNECTS_WIFICUP_COUNTER);
        p_wk->record[2].type = GDB_FIELD_INT;
        p_wk->record[2].value.intValue = 0;
        p_wk->record[3].name = WifiBattleMatchNet_GetArenaFieldName(ARENA_NUM_WIFICUP_LOSE);
        p_wk->record[3].type = GDB_FIELD_INT;
        p_wk->record[3].value.intValue = 0;
        p_wk->record[4].name = WifiBattleMatchNet_GetArenaFieldName(ARENA_NUM_WIFICUP_WIN);
        p_wk->record[4].type = GDB_FIELD_INT;
        p_wk->record[4].value.intValue = 0;
        p_wk->record[5].name = "WIFICUP_POKEMON_PARTY";
        p_wk->record[5].type = GDB_FIELD_BINARY;
        p_wk->record[5].value.binaryData.data = data;
        p_wk->record[5].value.binaryData.size = 1332;
        p_wk->table_name_num = 6;
        break;
    case WBM_WRITE_LAST_LOGIN:
        WifiBattleMatchNet_GetLoginDate(p_wk->loginDate);
        p_wk->table_name_num = 1;
        p_wk->record[0].name = "LAST_LOGIN_DATETIME";
        p_wk->record[0].type = GDB_FIELD_BINARY;
        p_wk->record[0].value.binaryData.data = p_wk->loginDate;
        p_wk->record[0].value.binaryData.size = 6;
        break;
    case WBM_WRITE_PROFILE:
        WifiBattleMatchNet_BuildProfileRecord(p_wk);
        break;
    case WBM_WRITE_RECORD_DATA:
        p_wk->recordData = *(WifiBattleMatchNetRecordData *)data;
        p_wk->table_name_num = 2;
        p_wk->record[0].name = sRecordDataTables[p_wk->recordData.slot];
        p_wk->record[0].type = GDB_FIELD_BINARY;
        p_wk->record[0].value.binaryData.data = p_wk->recordData.data;
        p_wk->record[0].value.binaryData.size = 0x50;
        p_wk->record[1].name = "RECORD_SAVE_IDX";
        p_wk->record[1].type = GDB_FIELD_BYTE;
        p_wk->record[1].value.byteValue = (p_wk->recordData.slot + 1) % 6;
        break;
    }
}

u32 WifiBattleMatchNet_WriteMain(WifiBattleMatchNet *p_wk) {
    int err;

    switch (p_wk->state) {
    case WBM_GDB_SEQ_STARTUP:
        err = Gdb_Startup(0xb5f, p_wk->unk0, 0);
        if (err != 0) {
            WifiBattleMatchNet_SetGdbError(&p_wk->error, err, 4556);
            WifiBattleMatchNet_EndGdb(p_wk);
            return WBM_NET_FAILED;
        }
        p_wk->state = WBM_GDB_SEQ_REQUEST;
        break;
    case WBM_GDB_SEQ_REQUEST:
        GFL_ASSERT(p_wk->table_name_num < WBM_GDB_FIELD_TABLE_MAX);
        err = Gdb_UpdateRecord(WBM_GDB_TABLE_NAME, p_wk->p_data->sake_recordID, p_wk->record, p_wk->table_name_num);
        if (err != 0) {
            WifiBattleMatchNet_SetGdbError(&p_wk->error, err, 4569);
            WifiBattleMatchNet_EndGdb(p_wk);
            return WBM_NET_FAILED;
        }
        p_wk->timer = 0;
        p_wk->state = WBM_GDB_SEQ_WAIT;
        break;
    case WBM_GDB_SEQ_WAIT:
        if (p_wk->timer++ > 3600) {
            WifiBattleMatchNet_SetTimeoutError(&p_wk->error, 1, 4582);
            p_wk->state = WBM_GDB_SEQ_CANCEL;
        } else {
            Gdb_Update();
            err = Gdb_GetState();
            if (err == GDB_STATE_OFF || err == 4) {
                WifiBattleMatchNet_SetGdbSubError1(&p_wk->error, err, 4593);
                p_wk->state = WBM_GDB_SEQ_CANCEL;
            } else if (err == GDB_STATE_IDLE) {
                p_wk->state = WBM_GDB_SEQ_RESULT;
            }
        }
        break;
    case WBM_GDB_SEQ_RESULT:
        err = Gdb_GetResult();
        if (err == 0) {
            WifiBattleMatchNet_SetGdbSubError2(&p_wk->error, err, 4612);
            p_wk->state = WBM_GDB_SEQ_CANCEL;
        } else if (err != 1) {
            WifiBattleMatchNet_EndGdb(p_wk);
            WifiBattleMatchNet_SetGdbSubError2(&p_wk->error, err, 4619);
            return WBM_NET_FAILED;
        } else {
            p_wk->state = WBM_GDB_SEQ_END;
        }
        break;
    case WBM_GDB_SEQ_END:
        WifiBattleMatchNet_EndGdb(p_wk);
        p_wk->state = WBM_GDB_SEQ_DONE;
        // fall through
    case WBM_GDB_SEQ_DONE:
        return WBM_NET_SUCCESS;
    case WBM_GDB_SEQ_CANCEL:
        err = Gdb_Cancel();
        if (err == 0) {
            p_wk->timer = 0;
            p_wk->state = WBM_GDB_SEQ_WAIT_CANCEL;
        } else {
            WifiBattleMatchNet_SetGdbError(&p_wk->error, err, 4649);
            WifiBattleMatchNet_EndGdb(p_wk);
            return WBM_NET_FAILED;
        }
        break;
    case WBM_GDB_SEQ_WAIT_CANCEL:
        if (p_wk->timer++ > 3600) {
            WifiBattleMatchNet_SetTimeoutError(&p_wk->error, 1, 4661);
            WifiBattleMatchNet_EndGdb(p_wk);
            return WBM_NET_FAILED;
        }
        Gdb_Update();
        err = Gdb_GetState();
        if (err == GDB_STATE_OFF || err == 4) {
            WifiBattleMatchNet_EndGdb(p_wk);
            return WBM_NET_FAILED;
        } else if (err == GDB_STATE_IDLE) {
            p_wk->state = WBM_GDB_SEQ_FAILED;
        }
        break;
    case WBM_GDB_SEQ_FAILED:
        WifiBattleMatchNet_EndGdb(p_wk);
        return WBM_NET_FAILED;
    }
    return WBM_NET_RUNNING;
}

void WifiBattleMatchNet_StartCreateRecord(WifiBattleMatchNet *p_wk) {
    p_wk->state = WBM_CREATE_SEQ_STARTUP;
    WifiBattleMatchNet_BuildProfileRecord(p_wk);
}

u32 WifiBattleMatchNet_CreateRecordMain(WifiBattleMatchNet *p_wk) {
    int err;

    switch (p_wk->state) {
    case WBM_CREATE_SEQ_STARTUP:
        err = Gdb_Startup(0xb5f, p_wk->unk0, 0);
        if (err != 0) {
            WifiBattleMatchNet_SetGdbError(&p_wk->error, err, 4736);
            return WBM_NET_FAILED;
        }
        p_wk->state = WBM_CREATE_SEQ_REQUEST;
        break;
    case WBM_CREATE_SEQ_REQUEST:
        err = Gdb_CreateRecord(WBM_GDB_TABLE_NAME, p_wk->record, p_wk->table_name_num, &p_wk->p_data->sake_recordID);
        if (err != 0) {
            WifiBattleMatchNet_SetGdbError(&p_wk->error, err, 4750);
            return WBM_NET_FAILED;
        }
        p_wk->timer = 0;
        p_wk->state = WBM_CREATE_SEQ_WAIT;
        break;
    case WBM_CREATE_SEQ_WAIT:
        if (p_wk->timer++ > 3600) {
            WifiBattleMatchNet_SetTimeoutError(&p_wk->error, 1, 4762);
            p_wk->state = WBM_CREATE_SEQ_CANCEL;
        } else {
            Gdb_Update();
            err = Gdb_GetState();
            if (err == GDB_STATE_OFF || err == 4) {
                WifiBattleMatchNet_SetGdbSubError1(&p_wk->error, err, 4774);
                p_wk->state = WBM_CREATE_SEQ_CANCEL;
            } else if (err == GDB_STATE_IDLE) {
                p_wk->state = WBM_CREATE_SEQ_RESULT;
            }
        }
        break;
    case WBM_CREATE_SEQ_RESULT:
        err = Gdb_GetResult();
        if (err == 0) {
            WifiBattleMatchNet_SetGdbSubError2(&p_wk->error, err, 4794);
            p_wk->state = WBM_CREATE_SEQ_CANCEL;
        } else if (err != 1) {
            WifiBattleMatchNet_EndGdb(p_wk);
            WifiBattleMatchNet_SetGdbSubError2(&p_wk->error, err, 4801);
            return WBM_NET_FAILED;
        } else {
            if (p_wk->p_data->sake_recordID == 0) {
                WifiBattleMatchNet_SetTimeoutError(&p_wk->error, 2, 4807);
            }
            WifiBattleMatchNet_EndGdb(p_wk);
            return WBM_NET_SUCCESS;
        }
        break;
    case WBM_CREATE_SEQ_CANCEL:
        err = Gdb_Cancel();
        if (err == 0) {
            p_wk->timer = 0;
            p_wk->state = WBM_CREATE_SEQ_WAIT_CANCEL;
        } else {
            WifiBattleMatchNet_SetGdbError(&p_wk->error, err, 4826);
            WifiBattleMatchNet_EndGdb(p_wk);
            return WBM_NET_FAILED;
        }
        break;
    case WBM_CREATE_SEQ_WAIT_CANCEL:
        if (p_wk->timer++ > 3600) {
            WifiBattleMatchNet_SetTimeoutError(&p_wk->error, 1, 4838);
            WifiBattleMatchNet_EndGdb(p_wk);
            return WBM_NET_FAILED;
        }
        Gdb_Update();
        err = Gdb_GetState();
        if (err == GDB_STATE_OFF || err == 4) {
            WifiBattleMatchNet_EndGdb(p_wk);
            return WBM_NET_FAILED;
        } else if (err == GDB_STATE_IDLE) {
            p_wk->state = WBM_CREATE_SEQ_FAILED;
        }
        break;
    case WBM_CREATE_SEQ_FAILED:
        WifiBattleMatchNet_EndGdb(p_wk);
        return WBM_NET_FAILED;
    }
    return WBM_NET_RUNNING;
}

u32 WifiBattleMatchNet_GetRecordId(WifiBattleMatchNet *p_wk) {
    return p_wk->p_data->sake_recordID;
}

BOOL WifiBattleMatchNet_SendPlayerInfo(WifiBattleMatchNet *p_wk, const void *data) {
    BOOL toServer = GFL_NetGetHandleNetId(NetHandle_GetOwn()) == 0;
    NetHandle *handle = NetHandle_GetOwn();

    return GFL_NetSendCommandToMachine(handle, toServer, WBM_NET_CMD_PLAYER_INFO, PokeParty_GetSaveDataSize() + 0xd4,
                                       data, TRUE, TRUE, TRUE);
}

BOOL WifiBattleMatchNet_TakePlayerInfo(WifiBattleMatchNet *p_wk, void **out) {
    u32 flag = p_wk->flagPlayerInfoReceived;

    if (flag != 0) {
        p_wk->flagPlayerInfoReceived = 0;
        *out = p_wk->recvBuffer;
    }
    return flag;
}

BOOL WifiBattleMatchNet_SendParty(WifiBattleMatchNet *p_wk, PokeParty *party) {
    BOOL toServer = GFL_NetGetHandleNetId(NetHandle_GetOwn()) == 0;
    NetHandle *handle = NetHandle_GetOwn();

    return GFL_NetSendCommandToMachine(handle, toServer, WBM_NET_CMD_PARTY, PokeParty_GetSaveDataSize(), party, TRUE,
                                       TRUE, TRUE);
}

BOOL WifiBattleMatchNet_TakeParty(WifiBattleMatchNet *p_wk, PokeParty *party) {
    u32 flag = p_wk->flagPartyReceived;

    if (flag != 0) {
        p_wk->flagPartyReceived = 0;
        MI_CpuCopy8(p_wk->recvBuffer, party, PokeParty_GetSaveDataSize());
    }
    return flag;
}

BOOL WifiBattleMatchNet_SendValue4(WifiBattleMatchNet *p_wk, const void *data) {
    BOOL toServer = GFL_NetGetHandleNetId(NetHandle_GetOwn()) == 0;

    return GFL_NetSendCommandToMachine(NetHandle_GetOwn(), toServer, WBM_NET_CMD_VALUE4, 4, data, TRUE, TRUE, TRUE);
}

BOOL WifiBattleMatchNet_TakeValue4(WifiBattleMatchNet *p_wk, void *out) {
    u32 flag = p_wk->flag148;

    if (flag != 0) {
        p_wk->flag148 = 0;
        MI_CpuCopy8(p_wk->recvBuffer, out, 4);
    }
    return flag;
}

BOOL WifiBattleMatchNet_SendReportValue(WifiBattleMatchNet *p_wk, const void *data) {
    return GFL_NetSendCommand(NetHandle_GetOwn(), WBM_NET_CMD_REPORT_VALUE, 4, data);
}

BOOL WifiBattleMatchNet_TakeReportValue(WifiBattleMatchNet *p_wk, u32 *out) {
    u32 flag = p_wk->flagValueReceived;

    if (flag != 0) {
        p_wk->flagValueReceived = 0;
        *out = p_wk->receivedValue;
    }
    return flag;
}

void WifiBattleMatchNet_InitDownload(WifiBattleMatchNet *p_wk, s32 profileId) {
    p_wk->state = WBM_DOWNLOAD_SEQ_INIT;
    p_wk->downloading = 0;
    MI_CpuFill8(p_wk->downloadAttr, 0, 10);
    MI_CpuFill8(&p_wk->downloadFile, 0, sizeof(DWCNdFileInfo));
    STD_TSPrintf(p_wk->downloadAttr, "%d", profileId);
}

u32 WifiBattleMatchNet_DownloadMain(WifiBattleMatchNet *p_wk) {
    switch (p_wk->state) {
    case WBM_DOWNLOAD_SEQ_INIT:
        if (!DWC_NdInitAsync(WifiBattleMatchNet_OnDownload, "IRAO", "WX9x7Zh6J3aBC4zQ")) {
            return WBM_DOWNLOAD_ERROR;
        }
        p_wk->downloading = TRUE;
        WifiBattleMatchNet_StartDownloadWait(p_wk, WBM_DOWNLOAD_SEQ_SET_ATTR);
        break;
    case WBM_DOWNLOAD_SEQ_SET_ATTR:
        if (!DWC_NdSetAttr("REGCARD_E", p_wk->downloadAttr, "")) {
            WifiBattleMatchNet_StartDownloadEnd(p_wk, WBM_DOWNLOAD_SEQ_ERROR);
        } else {
            p_wk->state = WBM_DOWNLOAD_SEQ_GET_FILE_LIST;
        }
        break;
    case WBM_DOWNLOAD_SEQ_GET_FILE_LIST:
        if (!DWC_NdGetFileListAsync(&p_wk->downloadFile, 0, 1)) {
            WifiBattleMatchNet_StartDownloadEnd(p_wk, WBM_DOWNLOAD_SEQ_ERROR);
        } else {
            WifiBattleMatchNet_StartDownloadWait(p_wk, WBM_DOWNLOAD_SEQ_GET_FILE);
        }
        break;
    case WBM_DOWNLOAD_SEQ_GET_FILE:
        if (!WifiBattleMatchNet_IsAllZero((const u32 *)&p_wk->downloadFile, sizeof(DWCNdFileInfo))) {
            if (!DWC_NdGetFileAsync(&p_wk->downloadFile, (char *)&p_wk->downloadBuffer, sizeof(p_wk->downloadBuffer))) {
                WifiBattleMatchNet_StartDownloadEnd(p_wk, WBM_DOWNLOAD_SEQ_ERROR);
            } else {
                sDownload.done = 0;
                sDownload.code = 0;
                p_wk->state = WBM_DOWNLOAD_SEQ_RECEIVE;
            }
        } else if (!DWC_NdCleanupAsync()) {
            p_wk->downloading = FALSE;
            return WBM_DOWNLOAD_NO_FILE;
        } else {
            WifiBattleMatchNet_StartDownloadWait(p_wk, WBM_DOWNLOAD_SEQ_NO_FILE);
        }
        break;
    case WBM_DOWNLOAD_SEQ_RECEIVE:
        NdReportResults();
        if (p_wk->timer++ > 7200) {
            NetState_RecordError(0, 0, 0, 0x3f7);
            WifiBattleMatchNet_StartDownloadCancel(p_wk, WBM_DOWNLOAD_SEQ_ERROR);
        }
        if (sDownload.done == 0) {
            if (DWC_NdGetProgress(&p_wk->downloadReceived, &p_wk->downloadSize) == 1) {
                u32 percent = p_wk->downloadReceived * 100 / p_wk->downloadSize;

                if (p_wk->downloadPercent != percent) {
                    p_wk->downloadPercent = percent;
                }
            }
        } else if (sDownload.code != 0) {
            if (sDownload.code == DWC_ND_ERROR_FATAL) {
                GFL_NetErrAbort();
            } else {
                WifiBattleMatchNet_StartDownloadEnd(p_wk, WBM_DOWNLOAD_SEQ_ERROR);
            }
        } else {
            if (!DWC_NdCleanupAsync()) {
                p_wk->downloading = FALSE;
                return WBM_DOWNLOAD_ERROR;
            }
            WifiBattleMatchNet_StartDownloadWait(p_wk, WBM_DOWNLOAD_SEQ_RECEIVED);
        }
        break;
    case WBM_DOWNLOAD_SEQ_RECEIVED:
        p_wk->state = WBM_DOWNLOAD_SEQ_CHECK;
        break;
    case WBM_DOWNLOAD_SEQ_CHECK:
        if (TournamentEntry_CheckCRC(&p_wk->downloadBuffer)) {
            p_wk->downloading = FALSE;
            return WBM_DOWNLOAD_SUCCESS;
        }
        p_wk->downloading = FALSE;
        return WBM_DOWNLOAD_NO_FILE;
    case WBM_DOWNLOAD_SEQ_NO_FILE:
        p_wk->downloading = FALSE;
        return WBM_DOWNLOAD_NO_FILE;
    case WBM_DOWNLOAD_SEQ_ERROR:
        p_wk->downloading = FALSE;
        return WBM_DOWNLOAD_ERROR;
    case WBM_DOWNLOAD_SEQ_WAIT:
        NdReportResults();
        if (p_wk->timer++ > 7200) {
            NetState_RecordError(0, 0, 0, 0x3f7);
            WifiBattleMatchNet_StartDownloadCancel(p_wk, WBM_DOWNLOAD_SEQ_ERROR);
        }
        if (sDownload.code != 0) {
            if (sDownload.code == DWC_ND_ERROR_FATAL) {
                GFL_NetErrAbort();
            } else {
                WifiBattleMatchNet_StartDownloadEnd(p_wk, WBM_DOWNLOAD_SEQ_ERROR);
            }
        }
        if (sDownload.done != 0) {
            sDownload.done = 0;
            if (sDownload.code == 0) {
                p_wk->state = p_wk->resumeState;
            }
        }
        break;
    case WBM_DOWNLOAD_SEQ_WAIT_CLEANUP:
        NdReportResults();
        if (sDownload.done2 != 0) {
            sDownload.done2 = 0;
            p_wk->state = p_wk->resumeState;
        }
        break;
    case WBM_DOWNLOAD_SEQ_WAIT_CANCEL:
        NdReportResults();
        if (sDownload.done != 0) {
            sDownload.done = 0;
            WifiBattleMatchNet_StartDownloadEnd(p_wk, p_wk->resumeState);
        }
        break;
    }
    return WBM_DOWNLOAD_RUNNING;
}

static BOOL WifiBattleMatchNet_IsAllZero(const u32 *data, u32 size) {
    u32 i;

    for (i = 0; i < size / 4; i++) {
        if (data[i] != 0) {
            return FALSE;
        }
    }
    return TRUE;
}

static void WifiBattleMatchNet_OnDwcError(void *work, int code, int type, int error) {
    WifiBattleMatchNet *p_wk = work;

    if (type == DWC_ETYPE_SHUTDOWN_FM || type == DWC_ETYPE_DISCONNECT) {
        if (p_wk->p_nhttp != NULL) {
            NHttpRap_EndRequest(p_wk->p_nhttp);
            NHttpRap_Destroy(p_wk->p_nhttp);
            p_wk->p_nhttp = NULL;
        }
        WifiBattleMatchNet_EndRequests(p_wk);
    }
}

static u32 WifiBattleMatchNet_OnDwcEvent(void *work, int a1, int event, int a3) {
    WifiBattleMatchNet *p_wk = work;

    if (event == DWC_ETYPE_SHUTDOWN_FM || event == DWC_ETYPE_DISCONNECT) {
        return p_wk->downloading;
    }
    return 0;
}

static u32 WifiBattleMatchNet_GetHttpResultMessage(u32 result) {
    switch (result) {
    case 0:
        GFL_ASSERT(0);
        return 0;
    case 1:
        return 0x34;
    case 2:
        return 0x35;
    case 3:
        return 0x36;
    case 4:
        return 0x37;
    case 5:
        return 0x38;
    case 6:
        return 0x39;
    case 7:
        return 0x3a;
    case 8:
        return 0x3b;
    case 9:
    default:
        return 0x3c;
    }
}

void WifiBattleMatchNet_GetDownload(WifiBattleMatchNet *p_wk, void *dest) {
    MI_CpuCopy8(&p_wk->downloadBuffer, dest, sizeof(p_wk->downloadBuffer));
}

static void WifiBattleMatchNet_StartDownloadWait(WifiBattleMatchNet *p_wk, u32 nextState) {
    sDownload.done = 0;
    sDownload.code = 0;
    p_wk->resumeState = nextState;
    p_wk->state = WBM_DOWNLOAD_SEQ_WAIT;
    p_wk->timer = 0;
}

static void WifiBattleMatchNet_StartDownloadEnd(WifiBattleMatchNet *p_wk, u32 nextState) {
    sDownload.done = 0;
    sDownload.code = 0;
    p_wk->resumeState = nextState;
    p_wk->state = WBM_DOWNLOAD_SEQ_WAIT_CLEANUP;
    if (!DWC_NdCleanupAsync()) {
        p_wk->resumeState = nextState;
    }
}

static void WifiBattleMatchNet_StartDownloadCancel(WifiBattleMatchNet *p_wk, u32 nextState) {
    sDownload.done = 0;
    sDownload.code = 0;
    p_wk->resumeState = nextState;
    p_wk->state = WBM_DOWNLOAD_SEQ_WAIT_CANCEL;
    if (!DWC_NdCancelAsync()) {
        p_wk->resumeState = nextState;
    }
}

static void WifiBattleMatchNet_OnDownload(DWCNdCallbackReason reason, DWCNdError error, int serverError) {
    switch (error) {
    case DWC_ND_ERROR_NONE:
    case DWC_ND_ERROR_ALLOC:
    case DWC_ND_ERROR_BUSY:
    case DWC_ND_ERROR_HTTP:
    case DWC_ND_ERROR_BUFFULL:
    case DWC_ND_ERROR_PARAM:
    case DWC_ND_ERROR_DLSERVER:
        break;
    case DWC_ND_ERROR_CANCELED:
    case DWC_ND_ERROR_FATAL:
        GFL_NetErrAbort();
        break;
    }
    sDownload.done = 1;
    sDownload.code = error;
    sDownload.done2 = 1;
}

void WifiBattleMatchNet_StartAccountRequest(WifiBattleMatchNet *p_wk, HeapID heapId) {
    GFL_ASSERT(p_wk->p_nhttp == NULL);
    p_wk->state = WBM_ACCOUNT_SEQ_SEND;
    p_wk->p_nhttp =
        NHttpRap_Create(heapId, PlayerInfo_GetProfileId(GetGameDataPlayerInfo(p_wk->gameData)), p_wk->loginBuffer);
}

u32 WifiBattleMatchNet_AccountRequestMain(WifiBattleMatchNet *p_wk) {
    NHttpRap *p_nhttp = p_wk->p_nhttp;

    if (p_nhttp == NULL) {
        return WBM_NET_FAILED;
    }
    switch (p_wk->state) {
    case WBM_ACCOUNT_SEQ_SEND:
        if (NHttpRap_SendRequest(5, p_nhttp)) {
            p_wk->state = WBM_ACCOUNT_SEQ_START;
        }
        break;
    case WBM_ACCOUNT_SEQ_START:
        if (NHttpRap_StartRequest(p_nhttp) == 0) {
            p_wk->state = WBM_ACCOUNT_SEQ_POLL;
        } else {
            if (p_wk->p_nhttp != NULL) {
                NHttpRap_Destroy(p_wk->p_nhttp);
                p_wk->p_nhttp = NULL;
            }
            return WBM_NET_FAILED;
        }
        break;
    case WBM_ACCOUNT_SEQ_POLL: {
        int status = NHttpRap_GetStatus(p_nhttp);
        int result = NHttpRap_Poll(p_wk->p_nhttp);

        if (result == 0) {
            WifiBattleMatchNetAnswer *answer = NHttpRap_GetAnswer(p_wk->p_nhttp);

            switch (status) {
            case 503:
                p_wk->state = WBM_ACCOUNT_SEQ_SERVER_ERROR;
                GFL_NetErrRecordWifi(0x3e);
                return WBM_NET_RUNNING;
            case 502:
                p_wk->state = WBM_ACCOUNT_SEQ_SERVER_ERROR;
                GFL_NetErrRecordWifi(0x3f);
                return WBM_NET_RUNNING;
            }
            p_wk->httpValues = answer->values;
            if (answer->result == 0) {
                p_wk->state = WBM_ACCOUNT_SEQ_SUCCESS;
            } else {
                GFL_NetErrRecordWifi(WifiBattleMatchNet_GetHttpResultMessage(answer->result));
                p_wk->state = WBM_ACCOUNT_SEQ_SERVER_ERROR;
            }
        } else if (result != 15) {
            if (p_wk->p_nhttp != NULL) {
                NHttpRap_Destroy(p_wk->p_nhttp);
                p_wk->p_nhttp = NULL;
            }
            return WBM_NET_FAILED;
        }
        break;
    }
    case WBM_ACCOUNT_SEQ_SERVER_ERROR:
        MI_CpuFill8(&p_wk->httpValues, 0, sizeof(p_wk->httpValues));
        if (p_wk->p_nhttp != NULL) {
            NHttpRap_EndRequest(p_wk->p_nhttp);
            NHttpRap_Destroy(p_wk->p_nhttp);
            p_wk->p_nhttp = NULL;
        }
        return WBM_NET_SERVER_ERROR;
    case WBM_ACCOUNT_SEQ_SUCCESS:
        if (p_nhttp != NULL) {
            NHttpRap_Destroy(p_nhttp);
            p_wk->p_nhttp = NULL;
        }
        return WBM_NET_SUCCESS;
    }
    return WBM_NET_RUNNING;
}

void WifiBattleMatchNet_GetAccountValues(WifiBattleMatchNet *p_wk, void *out) {
    WifiBattleMatchNetValues *values = out;

    *values = p_wk->httpValues;
}

void WifiBattleMatchNet_StartPostRequest(WifiBattleMatchNet *p_wk, const void *in, HeapID heapId) {
    const WifiBattleMatchNetValues *values = in;

    GFL_ASSERT(p_wk->p_nhttp == NULL);
    p_wk->p_nhttp =
        NHttpRap_Create(heapId, PlayerInfo_GetProfileId(GetGameDataPlayerInfo(p_wk->gameData)), p_wk->loginBuffer);
    p_wk->postData = *values;
    p_wk->state = WBM_POST_SEQ_SEND;
}

u32 WifiBattleMatchNet_PostRequestMain(WifiBattleMatchNet *p_wk) {
    NHttpRap *p_nhttp = p_wk->p_nhttp;

    if (p_nhttp == NULL) {
        return WBM_NET_FAILED;
    }
    switch (p_wk->state) {
    case WBM_POST_SEQ_SEND:
        if (NHttpRap_SendRequest(6, p_nhttp)) {
            NHTTPSetPostDataRaw(NHttpRap_GetConnection(p_wk->p_nhttp), &p_wk->postData, sizeof(p_wk->postData));
            p_wk->state = WBM_POST_SEQ_START;
        }
        break;
    case WBM_POST_SEQ_START:
        if (NHttpRap_StartRequest(p_nhttp) == 0) {
            p_wk->state = WBM_POST_SEQ_POLL;
        } else {
            if (p_wk->p_nhttp != NULL) {
                NHttpRap_Destroy(p_wk->p_nhttp);
                p_wk->p_nhttp = NULL;
            }
            return WBM_NET_FAILED;
        }
        break;
    case WBM_POST_SEQ_POLL: {
        int status = NHttpRap_GetStatus(p_nhttp);
        int result = NHttpRap_Poll(p_wk->p_nhttp);

        if (result == 0) {
            WifiBattleMatchNetAnswer *answer = NHttpRap_GetAnswer(p_wk->p_nhttp);

            switch (status) {
            case 503:
                p_wk->state = WBM_POST_SEQ_SERVER_ERROR;
                GFL_NetErrRecordWifi(0x3e);
                return WBM_NET_RUNNING;
            case 502:
                p_wk->state = WBM_POST_SEQ_SERVER_ERROR;
                GFL_NetErrRecordWifi(0x3f);
                return WBM_NET_RUNNING;
            }
            if (answer->result == 0) {
                p_wk->state = WBM_POST_SEQ_SUCCESS;
            } else {
                GFL_NetErrRecordWifi(WifiBattleMatchNet_GetHttpResultMessage(answer->result));
                p_wk->state = WBM_POST_SEQ_SERVER_ERROR;
            }
        } else if (result != 15) {
            WifiBattleMatchNet_SetHttpError(&p_wk->error, result, 5899);
            if (p_wk->p_nhttp != NULL) {
                NHttpRap_Destroy(p_wk->p_nhttp);
                p_wk->p_nhttp = NULL;
            }
            return WBM_NET_FAILED;
        }
        break;
    }
    case WBM_POST_SEQ_SERVER_ERROR:
        if (p_nhttp != NULL) {
            NHttpRap_EndRequest(p_nhttp);
            NHttpRap_Destroy(p_wk->p_nhttp);
            p_wk->p_nhttp = NULL;
        }
        return WBM_NET_SERVER_ERROR;
    case WBM_POST_SEQ_SUCCESS:
        if (p_nhttp != NULL) {
            NHttpRap_Destroy(p_nhttp);
            p_wk->p_nhttp = NULL;
        }
        return WBM_NET_SUCCESS;
    }
    return WBM_NET_RUNNING;
}

void WifiBattleMatchNet_StartNameCheck(WifiBattleMatchNet *p_wk, PlayerInfo *info, u16 unk24) {
    StrBuf *name;
    const u16 *chars;
    u32 i;

    p_wk->namePlayerInfo = info;
    name = copyTrainerNameToNewStrbuf(info->name, p_wk->msgHeapId);
    MI_CpuFill8(&p_wk->nameCheck, 0, sizeof(p_wk->nameCheck));
    p_wk->nameCheck.unk24 = unk24;
    chars = GFL_StrBufGetStringPtr(name);
    for (i = 0; i < GFL_StrBufGetCharCount(name) + 1; i++) {
        if (chars[i] == GFL_StrBufGetTerminator()) {
            p_wk->nameCheck.name[i] = 0;
        } else {
            p_wk->nameCheck.name[i] = chars[i];
        }
    }
    p_wk->nameCheck.words = p_wk->nameCheck.name;
    p_wk->nameCheck.badWordCount = 0;
    DWC_CheckProfanityExAsync(&p_wk->nameCheck.words, 1, NULL, 0, &p_wk->nameCheck.result,
                              &p_wk->nameCheck.badWordCount, 0x80);
    GFL_StrBufFree(name);
}

BOOL WifiBattleMatchNet_NameCheckMain(WifiBattleMatchNet *p_wk, u32 *out) {
    BOOL done = DWC_CheckProfanityProcess() == DWC_PROF_STATE_SUCCESS;

    if (done) {
        *out = p_wk->nameCheck.badWordCount > 0;
    }
    if (done == 1 && *out == 1) {
        MsgData *msgData =
            GFL_MsgSysLoadData(FALSE, ARCID_SYSTEM_MESSAGE, TEXT_BANK_ABILITY_HANDLERS_BTL_MAIN, p_wk->msgHeapId);
        StrBuf *name = GFL_MsgDataLoadStrbufNew(msgData, WBM_DEFAULT_NAME);

        GFL_MsgDataFree(msgData);
        copyTrainerNameFromStrbuf(p_wk->namePlayerInfo, name);
        GFL_StrBufFree(name);
    }
    return done;
}

void WifiBattleMatchNet_StartPokemonCheck(WifiBattleMatchNet *p_wk, void *data, int kind) {
    p_wk->state = WBM_CHECK_SEQ_SEND;
    p_wk->p_nhttp = NHttpRap_Create(p_wk->msgHeapId, PlayerInfo_GetProfileId(GetGameDataPlayerInfo(p_wk->gameData)),
                                    p_wk->loginBuffer);
    switch (kind) {
    case WBM_POKEMON_CHECK_ONE:
        NHttpRap_BeginPost(p_wk->p_nhttp, p_wk->msgHeapId, PokeParty_GetPkmRawSize(), 0);
        NHttpRap_AddPostData(p_wk->p_nhttp, data, PokeParty_GetPkmRawSize());
        p_wk->unk178 = 1;
        break;
    case WBM_POKEMON_CHECK_PARTY: {
        PokeParty *party = data;
        u32 size = PokeParty_GetPkmRawSize();
        int i;

        NHttpRap_BeginPost(p_wk->p_nhttp, p_wk->msgHeapId, PokeParty_GetPkmCount(party) * size, 0);
        for (i = 0; i < PokeParty_GetPkmCount(party); i++) {
            PartyPkm *pkm = PokeParty_GetPkm(party, i);

            NHttpRap_AddPostData(p_wk->p_nhttp, pkm, PokeParty_GetPkmRawSize());
        }
        p_wk->unk178 = PokeParty_GetPkmCount(party);
        break;
    }
    }
}

u32 WifiBattleMatchNet_PokemonCheckMain(WifiBattleMatchNet *p_wk, WifiBattleMatchNetCheck *out) {
    switch (p_wk->state) {
    case WBM_CHECK_SEQ_SEND:
        if (NHttpRap_SendValidate(p_wk->p_nhttp)) {
            if (NHttpRap_StartRequest(p_wk->p_nhttp) == 0) {
                p_wk->state++;
            } else {
                if (p_wk->p_nhttp != NULL) {
                    NHttpRap_Destroy(p_wk->p_nhttp);
                    p_wk->p_nhttp = NULL;
                    p_wk->state = WBM_CHECK_SEQ_SEND;
                }
                return WBM_NET_FAILED;
            }
        } else {
            if (p_wk->p_nhttp != NULL) {
                NHttpRap_Destroy(p_wk->p_nhttp);
                p_wk->p_nhttp = NULL;
                p_wk->state = WBM_CHECK_SEQ_SEND;
            }
            return WBM_NET_FAILED;
        }
        break;
    case WBM_CHECK_SEQ_POLL: {
        int status = NHttpRap_GetStatus(p_wk->p_nhttp);
        int result = NHttpRap_Poll(p_wk->p_nhttp);

        if (result == 0) {
            switch (status) {
            case 400:
                GFL_NetErrRecordWifi(0x41);
                if (p_wk->p_nhttp != NULL) {
                    NHttpRap_FreePostData(p_wk->p_nhttp);
                    NHttpRap_Destroy(p_wk->p_nhttp);
                    NHttpRap_EndRequest(p_wk->p_nhttp);
                    p_wk->p_nhttp = NULL;
                    p_wk->state = WBM_CHECK_SEQ_SEND;
                }
                return WBM_NET_SERVER_ERROR;
            case 401:
                GFL_NetErrRecordWifi(0x42);
                if (p_wk->p_nhttp != NULL) {
                    NHttpRap_FreePostData(p_wk->p_nhttp);
                    NHttpRap_Destroy(p_wk->p_nhttp);
                    NHttpRap_EndRequest(p_wk->p_nhttp);
                    p_wk->p_nhttp = NULL;
                    p_wk->state = WBM_CHECK_SEQ_SEND;
                }
                return WBM_NET_SERVER_ERROR;
            case 408:
                GFL_NetErrRecordWifi(0x44);
                if (p_wk->p_nhttp != NULL) {
                    NHttpRap_FreePostData(p_wk->p_nhttp);
                    NHttpRap_Destroy(p_wk->p_nhttp);
                    NHttpRap_EndRequest(p_wk->p_nhttp);
                    p_wk->p_nhttp = NULL;
                    p_wk->state = WBM_CHECK_SEQ_SEND;
                }
                return WBM_NET_SERVER_ERROR;
            default:
                GFL_NetErrRecordWifi(0x43);
                if (p_wk->p_nhttp != NULL) {
                    NHttpRap_FreePostData(p_wk->p_nhttp);
                    NHttpRap_Destroy(p_wk->p_nhttp);
                    NHttpRap_EndRequest(p_wk->p_nhttp);
                    p_wk->p_nhttp = NULL;
                    p_wk->state = WBM_CHECK_SEQ_SEND;
                }
                return WBM_NET_SERVER_ERROR;
            case 200:
                p_wk->state++;
                break;
            }
        } else if (result != 15) {
            WifiBattleMatchNet_SetHttpError(&p_wk->error, result, 6159);
            NHttpRap_FreePostData(p_wk->p_nhttp);
            if (p_wk->p_nhttp != NULL) {
                NHttpRap_Destroy(p_wk->p_nhttp);
                p_wk->p_nhttp = NULL;
                p_wk->state = WBM_CHECK_SEQ_SEND;
            }
            return WBM_NET_FAILED;
        }
        break;
    }
    case WBM_CHECK_SEQ_ANSWER: {
        void *answer = NHttpRap_GetAnswer(p_wk->p_nhttp);
        u32 i;

        out->status = NHttpRap_GetCheckStatus(answer);
        for (i = 0; i < p_wk->unk178; i++) {
            out->result[i] = NHttpRap_GetCheckResult(answer, i);
        }
        if (out->status == 0) {
            MI_CpuCopy8(NHttpRap_GetCheckSignature(answer, p_wk->unk178), out->signature, 0x80);
        }
        NHttpRap_FreePostData(p_wk->p_nhttp);
        if (p_wk->p_nhttp != NULL) {
            NHttpRap_Destroy(p_wk->p_nhttp);
            p_wk->p_nhttp = NULL;
        }
        return WBM_NET_SUCCESS;
    }
    }
    return WBM_NET_RUNNING;
}

static void WifiBattleMatchNet_SetTimeoutError(WifiBattleMatchNetError *error, int code, int line) {
    error->timeoutCode = code;
    error->kind = WBM_ERROR_TIMEOUT;
    NetState_RecordError(0, 0, 0, 0x3f5);
}

static void WifiBattleMatchNet_SetDwcError(WifiBattleMatchNetError *error, int code, int line) {
    error->dwcCode = code;
    error->kind = WBM_ERROR_DWC;
    NetState_RecordError(0, 0, 0, 0x3f7);
}

static void WifiBattleMatchNet_SetGdbError(WifiBattleMatchNetError *error, int code, int line) {
    error->gdbCode = code;
    error->kind = WBM_ERROR_GDB;
}

static void WifiBattleMatchNet_SetGdbSubError1(WifiBattleMatchNetError *error, int code, int line) {
    error->gdbSubCode1 = code;
    error->kind = WBM_ERROR_GDB;
}

static void WifiBattleMatchNet_SetGdbSubError2(WifiBattleMatchNetError *error, int code, int line) {
    error->gdbSubCode2 = code;
    error->kind = WBM_ERROR_GDB;
}

static void WifiBattleMatchNet_SetHttpError(WifiBattleMatchNetError *error, int code, int line) {
    error->httpCode = code;
    error->kind = WBM_ERROR_HTTP;
}

static u32 WifiBattleMatchNet_GetDwcResult(u32 code) {
    switch (code) {
    case 0:
        return WBM_RESULT_OK;
    case 1:
        return WBM_RESULT_FAILED;
    case 2:
        return WBM_RESULT_FAILED;
    case 3:
        return WBM_RESULT_FAILED;
    case 4:
        return WBM_RESULT_FAILED;
    case 5:
        return WBM_RESULT_FAILED;
    case 6:
        return WBM_RESULT_FAILED;
    case 7:
        return WBM_RESULT_FAILED;
    case 8:
        return WBM_RESULT_FAILED;
    case 9:
        return WBM_RESULT_FAILED;
    case 10:
        return WBM_RESULT_FAILED;
    case 11:
        return WBM_RESULT_FAILED;
    case 12:
        return WBM_RESULT_FAILED;
    case 13:
        return WBM_RESULT_FAILED;
    case 14:
        return WBM_RESULT_FAILED;
    default:
        GFL_ASSERT(0);
        return WBM_RESULT_OK;
    }
}

static u32 WifiBattleMatchNet_GetGdbResult(u32 code, u32 subCode1, u32 subCode2) {
    switch (code) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        return WBM_RESULT_FAILED;
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
        return WBM_RESULT_DISCONNECTED;
    case 17:
    case 18:
        return WBM_RESULT_FAILED;
    case 19:
        return WBM_RESULT_DISCONNECTED;
    case 21:
    case 22:
        return WBM_RESULT_FAILED;
    }
    switch (subCode1) {
    case 0:
        return WBM_RESULT_DISCONNECTED;
    case 1:
    case 2:
    case 3:
        break;
    case 4:
        return WBM_RESULT_FAILED;
    }
    switch (subCode2) {
    case 2:
        return WBM_RESULT_FAILED;
    case 3:
        return WBM_RESULT_FAILED;
    case 4:
        return WBM_RESULT_DISCONNECTED;
    case 5:
        return WBM_RESULT_FAILED;
    case 6:
        return WBM_RESULT_FAILED;
    case 7:
        return WBM_RESULT_FAILED;
    case 8:
    case 9:
    case 10:
        return WBM_RESULT_FAILED;
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
        return WBM_RESULT_DISCONNECTED;
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
        return WBM_RESULT_DISCONNECTED;
    case 22:
        return WBM_RESULT_FAILED;
    case 23:
    case 24:
    case 25:
    case 26:
    case 27:
        return WBM_RESULT_DISCONNECTED;
    case 28:
    case 29:
        return WBM_RESULT_FAILED;
    case 30:
    case 31:
        return WBM_RESULT_FAILED;
    }
    return WBM_RESULT_OK;
}

static u32 WifiBattleMatchNet_GetTimeoutResult(u32 code) {
    switch (code) {
    case 0:
        return WBM_RESULT_OK;
    case 1:
        return WBM_RESULT_DISCONNECTED;
    case 2:
        return WBM_RESULT_FAILED;
    }
    return WBM_RESULT_OK;
}

static void WifiBattleMatchNet_ClearError(WifiBattleMatchNetError *error) {
    MI_CpuFill8(error, 0, sizeof(WifiBattleMatchNetError));
}

void WifiBattleMatchNet_ShowIcon(WifiBattleMatchNet *p_wk, int a1) {
    if (a1 != 0) {
        GFL_NetSetIconScreen(TRUE, p_wk->msgHeapId);
    }
}

s32 WifiBattleMatchNet_GetGSID(WifiBattleMatchNet *p_wk) {
    return WifiList_GetMyGSID(p_wk->wifiList);
}
