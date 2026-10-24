// The Battle Recorder's connection to the Global Link: the requests that its online modes make to the Global Data
// Storage client, which the overlay 201 holds, and the result messages that they end with. The name is the ROM's
// string, which GFL_HeapAllocate is given.

#include "types.h"
#include "app/battle_recorder/br_net.h"
#include "app/gds_rap.h"
#include "gfl/dwc_rap.h"
#include "gfl/dwc_rapcommon.h"
#include "gfl/net.h"
#include "gfl/overlay.h"
#include "gfl/sound.h"
#include "gfl/std.h"
#include "save/gds_profile.h"
#include "save/player_info.h"
#include "system/game_data.h"

typedef struct BrNetSeq BrNetSeq;

typedef void (*BrNetSeqFunc)(BrNetSeq *p_seqwk, u32 *p_seq, BrNet *net);

// A step of a request, which runs every frame until it sets the next one
struct BrNetSeq {
    BrNetSeqFunc func;
    u32 seq;
    BrNet *net;
    u32 unkC;
};

struct BrNet {
    // The GDS client's work
    GdsRapWork gdsrap;
    BOOL isOpen;
    GdsProfile *profile;
    BrNetSeq *seq;
    BrNetRequestParam param;
    // Set by the GDS client's callbacks as the requests finish
    BOOL uploadMusicalShotEnd;
    BOOL downloadMusicalShotEnd;
    BOOL uploadVideoEnd;
    BOOL downloadVideoEnd;
    BOOL downloadVideoListEnd;
    u32 unk5cc;
    u32 unk5d0;
    u32 unk5d4;
    BOOL unk5d8End;
    BOOL isFailed;
    GameData *gameData;
};

static void BrNet_SeqIdle(BrNetSeq *p_seqwk, u32 *p_seq, BrNet *net);
static void BrNet_SeqWaitEnd(BrNetSeq *p_seqwk, u32 *p_seq, BrNet *net);
static void BrNet_SeqUploadMusicalShot(BrNetSeq *p_seqwk, u32 *p_seq, BrNet *net);
static void BrNet_SeqDownloadMusicalShot(BrNetSeq *p_seqwk, u32 *p_seq, BrNet *net);
static void BrNet_Seq2(BrNetSeq *p_seqwk, u32 *p_seq, BrNet *net);
static void BrNet_Seq3(BrNetSeq *p_seqwk, u32 *p_seq, BrNet *net);
static void BrNet_Seq4(BrNetSeq *p_seqwk, u32 *p_seq, BrNet *net);
static void BrNet_Seq5(BrNetSeq *p_seqwk, u32 *p_seq, BrNet *net);
static void BrNet_Seq6(BrNetSeq *p_seqwk, u32 *p_seq, BrNet *net);
static void BrNet_Seq7(BrNetSeq *p_seqwk, u32 *p_seq, BrNet *net);
static void BrNet_Seq8(BrNetSeq *p_seqwk, u32 *p_seq, BrNet *net);
static void BrNet_OnUploadMusicalShotEnd(void *work, const GdsRapResult *result);
static void BrNet_OnDownloadMusicalShotEnd(void *work, const GdsRapResult *result);
static void BrNet_OnUploadVideoEnd(void *work, const GdsRapResult *result);
static void BrNet_OnDownloadVideoListEnd(void *work, const GdsRapResult *result);
static void BrNet_OnDownloadVideoEnd(void *work, const GdsRapResult *result);
static void BrNet_OnUnk5d8End(void *work, const GdsRapResult *result);
static BrNetSeq *BrNetSeq_Init(BrNet *net, BrNetSeqFunc func, HeapID heapId);
static void BrNetSeq_Exit(BrNetSeq *p_wk);
static void BrNetSeq_Main(BrNetSeq *p_wk);
static void BrNetSeq_SetNext(BrNetSeq *p_wk, BrNetSeqFunc func);
static BOOL BrNetSeq_IsComp(const BrNetSeq *cp_wk, BrNetSeqFunc func);
static u32 BrNet_EventFunc(void *work, int a1, int event, int a3);

BrNet *BrNet_Init(GameData *gameData, uptr a1, HeapID heapId) {
    BrNet *net;
    GdsRapInit init;

    GFL_OvlLoad(OVERLAY_ID(201));
    net = GFL_HeapAllocate(heapId, sizeof(BrNet), FALSE, "br_net.c", 151);
    MI_CpuFill8(net, 0, sizeof(BrNet));
    net->gameData = gameData;

    MI_CpuFill8(&init, 0, sizeof(GdsRapInit));
    init.gameData = gameData;
    init.profileId = PlayerInfo_GetProfileId(GetGameDataPlayerInfo(gameData));
    init.loginBuffer = (void *)a1;
    init.callbacks.work = net;
    init.callbacks.onUploadMusicalShotEnd = BrNet_OnUploadMusicalShotEnd;
    init.callbacks.onDownloadMusicalShotEnd = BrNet_OnDownloadMusicalShotEnd;
    init.callbacks.onUploadVideoEnd = BrNet_OnUploadVideoEnd;
    init.callbacks.onDownloadVideoListEnd = BrNet_OnDownloadVideoListEnd;
    init.callbacks.onDownloadVideoEnd = BrNet_OnDownloadVideoEnd;
    init.callbacks.onFavoriteVideoEnd = BrNet_OnUnk5d8End;
    GdsRap_Init(&net->gdsrap, &init);
    net->isOpen = TRUE;

    GameData_GetSaveControl(gameData);
    net->profile = GdsProfile_Create(heapId);
    GdsProfile_SetFromGameData(net->profile, gameData);
    net->seq = BrNetSeq_Init(net, BrNet_SeqIdle, heapId);
    DWCRap_SetEventFunc(BrNet_EventFunc, net);
    return net;
}

void BrNet_Exit(BrNet *net) {
    DWCRap_SetEventFunc(NULL, NULL);
    BrNetSeq_Exit(net->seq);
    GdsProfile_Free(net->profile);
    if (net->isOpen) {
        GdsRap_Exit(&net->gdsrap);
        net->isOpen = FALSE;
    }
    GFL_HeapFree(net);
    GFL_OvlUnload(OVERLAY_ID(201));
}

void BrNet_Main(BrNet *net) {
    if (net->isOpen) {
        BrNetSeq_Main(net->seq);
        GdsRap_Main(&net->gdsrap);
    }
}

void BrNet_StartRequest(BrNet *net, int type, const BrNetRequestParam *param) {
    GFL_ASSERT(type < BR_NET_REQUEST_MAX);

    if (param != NULL) {
        net->param = *param;
    }
    MI_CpuFill8(&net->uploadMusicalShotEnd, 0, 0x24);

    switch (type) {
    case BR_NET_REQUEST_UPLOAD_MUSICAL_SHOT:
        BrNetSeq_SetNext(net->seq, BrNet_SeqUploadMusicalShot);
        break;
    case BR_NET_REQUEST_DOWNLOAD_MUSICAL_SHOT:
        BrNetSeq_SetNext(net->seq, BrNet_SeqDownloadMusicalShot);
        break;
    case BR_NET_REQUEST_2:
        BrNetSeq_SetNext(net->seq, BrNet_Seq2);
        break;
    case BR_NET_REQUEST_3:
        BrNetSeq_SetNext(net->seq, BrNet_Seq3);
        break;
    case BR_NET_REQUEST_4:
        BrNetSeq_SetNext(net->seq, BrNet_Seq4);
        break;
    case BR_NET_REQUEST_5:
        BrNetSeq_SetNext(net->seq, BrNet_Seq5);
        break;
    case BR_NET_REQUEST_6:
        BrNetSeq_SetNext(net->seq, BrNet_Seq6);
        break;
    case BR_NET_REQUEST_7:
        BrNetSeq_SetNext(net->seq, BrNet_Seq7);
        break;
    case BR_NET_REQUEST_8:
        BrNetSeq_SetNext(net->seq, BrNet_Seq8);
        break;
    default:
        GFL_ASSERT(0);
        break;
    }
}

BOOL BrNet_IsRequestEnd(BrNet *net) {
    return BrNetSeq_IsComp(net->seq, BrNet_SeqIdle);
}

BOOL BrNet_GetDownloadMusicalShot(BrNet *net, BrMusicalShotRecv **tbl, int tblMax, int *recvNum) {
    BOOL isEnd = net->downloadMusicalShotEnd;

    if (isEnd) {
        *recvNum = GdsRapResponse_GetMusicalShots(&net->gdsrap, (POKE_NET_GDS_MUSICALSHOT_SEND **)tbl, tblMax);
    }
    return isEnd;
}

BOOL BrNet_GetUploadVideoNumber(BrNet *net, u64 *number) {
    BOOL isEnd = net->uploadVideoEnd;

    if (isEnd) {
        *number = GdsRapResponse_GetBattleVideoNumber(&net->gdsrap);
    }
    return isEnd;
}

BOOL BrNet_GetDownloadVideo(BrNet *net, void **video, u32 *value) {
    BOOL isEnd = net->downloadVideoEnd;

    if (isEnd) {
        *value = GdsRapResponse_GetBattleVideo(&net->gdsrap, (BattleRecVideo **)video);
    }
    return isEnd;
}

BOOL BrNet_GetDownloadVideoListA(BrNet *net, void *list, int listMax, int *num) {
    BOOL isEnd = net->downloadVideoListEnd;

    if (isEnd) {
        *num = GdsRapResponse_GetBattleVideoList(&net->gdsrap, list, listMax);
    }
    return *num != 0 && isEnd;
}

BOOL BrNet_GetDownloadVideoListB(BrNet *net, void *list, int listMax, int *num) {
    BOOL isEnd = net->downloadVideoListEnd;

    if (isEnd) {
        *num = GdsRapResponse_GetBattleVideoList(&net->gdsrap, list, listMax);
    }
    return *num != 0 && isEnd;
}

BOOL BrNet_GetDownloadVideoListC(BrNet *net, void *list, int listMax, int *num) {
    BOOL isEnd = net->downloadVideoListEnd;

    if (isEnd) {
        *num = GdsRapResponse_GetBattleVideoList(&net->gdsrap, list, listMax);
    }
    return *num != 0 && isEnd;
}

BOOL BrNet_GetResultMsg(BrNet *net, u32 *msgID) {
    GdsRapResult *result;
    BOOL ret;

    if (GdsRap_GetResult(&net->gdsrap, &result)) {
        ret = TRUE;
        if (result->type == 0) {
            if (result->detail != 0) {
                if (msgID != NULL) {
                    *msgID = result->detail + 0x11b;
                }
                ret = TRUE;
            }
        } else if (result->type == 1) {
            if (result->detail == 7) {
                if (msgID != NULL) {
                    *msgID = result->detail + 0x12a;
                }
                ret = TRUE;
            } else {
                ret = TRUE;
            }
        } else if (result->type == 2) {
            switch (result->code) {
            case 0x5208:
                if (result->detail != 0) {
                    if (msgID != NULL) {
                        *msgID = result->detail + 0xe6;
                    }
                    ret = 2;
                }
                break;
            case 0x5209:
                if (result->detail != 0) {
                    if (msgID != NULL) {
                        *msgID = result->detail + 0xed;
                    }
                    ret = 2;
                }
                break;
            case 0x55f0:
                if (result->detail != 0) {
                    switch (result->detail) {
                    case 0x96:
                        GFL_SndStop();
                        GFL_NetErrRecordWifi(0x41);
                        ret = 3;
                        break;
                    case 0x97:
                        GFL_SndStop();
                        GFL_NetErrRecordWifi(0x42);
                        ret = 3;
                        break;
                    case 0x98:
                        GFL_SndStop();
                        GFL_NetErrRecordWifi(0x44);
                        ret = 3;
                        break;
                    case 0x99:
                        GFL_SndStop();
                        GFL_NetErrRecordWifi(0x43);
                        ret = 3;
                        break;
                    default:
                        if (msgID != NULL) {
                            *msgID = result->detail + 0x105;
                        }
                        ret = 2;
                        break;
                    }
                }
                break;
            case 0x55f1:
                if (result->detail != 0) {
                    if (msgID != NULL) {
                        *msgID = result->detail + 0x10d;
                    }
                    ret = 2;
                }
                break;
            case 0x55f2:
                if (result->detail != 0) {
                    if (msgID != NULL) {
                        *msgID = result->detail + 0x112;
                    }
                    ret = 1;
                }
                break;
            case 0x55f3:
                ret = 0;
                break;
            }
        }

        GdsRap_ClearResult(&net->gdsrap);
        if (ret == 3) {
            net->isFailed = TRUE;
        }
        return ret;
    }
    return FALSE;
}

u32 BrNet_CheckError(BrNet *net) {
    BOOL ret;

    if (net == NULL) {
        return FALSE;
    }

    ret = TRUE;
    switch (DWCRapCommon_CheckError(TRUE, FALSE)) {
    case DWCRAP_RESULT_DISCONNECTED:
        GFL_SndStop();
        return ret;
    case DWCRAP_RESULT_FAILED:
        GFL_SndStop();
        net->isFailed = ret;
        return 2;
    case DWCRAP_RESULT_NET_ERROR:
        GFL_SndStop();
        GFL_NetErrShowFatal();
        break;
    }
    return FALSE;
}

BOOL BrNet_IsFailed(BrNet *net) {
    return net->isFailed;
}

static void BrNet_SeqIdle(BrNetSeq *p_seqwk, u32 *p_seq, BrNet *net) {
}

static void BrNet_SeqWaitEnd(BrNetSeq *p_seqwk, u32 *p_seq, BrNet *net) {
    if (GdsRap_IsIdle(&net->gdsrap)) {
        BrNetSeq_SetNext(p_seqwk, BrNet_SeqIdle);
    }
}

static void BrNet_SeqUploadMusicalShot(BrNetSeq *p_seqwk, u32 *p_seq, BrNet *net) {
    BOOL ret = GdsRap_SendMusicalShotUpload(&net->gdsrap, net->profile, net->param.data);

    GFL_ASSERT(ret);
    BrNetSeq_SetNext(p_seqwk, BrNet_SeqWaitEnd);
}

static void BrNet_SeqDownloadMusicalShot(BrNetSeq *p_seqwk, u32 *p_seq, BrNet *net) {
    BOOL ret = GdsRap_SendMusicalShotDownload(&net->gdsrap, net->param.species);

    GFL_ASSERT(ret);
    BrNetSeq_SetNext(p_seqwk, BrNet_SeqWaitEnd);
}

static void BrNet_Seq2(BrNetSeq *p_seqwk, u32 *p_seq, BrNet *net) {
    BOOL ret = GdsRap_SendBattleVideoUpload(&net->gdsrap, net->profile);

    GFL_ASSERT(ret);
    BrNetSeq_SetNext(p_seqwk, BrNet_SeqWaitEnd);
}

static void BrNet_Seq3(BrNetSeq *p_seqwk, u32 *p_seq, BrNet *net) {
    BOOL ret = GdsRap_SendBattleVideoDownload(&net->gdsrap, net->param.dataNumber);

    GFL_ASSERT(ret);
    BrNetSeq_SetNext(p_seqwk, BrNet_SeqWaitEnd);
}

static void BrNet_Seq4(BrNetSeq *p_seqwk, u32 *p_seq, BrNet *net) {
    BOOL ret = GdsRap_SendBattleVideoSearch(&net->gdsrap, net->param.a1, net->param.unk4, net->param.a3, net->param.a4);

    GFL_ASSERT(ret);
    BrNetSeq_SetNext(p_seqwk, BrNet_SeqWaitEnd);
}

static void BrNet_Seq5(BrNetSeq *p_seqwk, u32 *p_seq, BrNet *net) {
    BOOL ret = GdsRap_SendBattleVideoList1(&net->gdsrap);

    GFL_ASSERT(ret);
    BrNetSeq_SetNext(p_seqwk, BrNet_SeqWaitEnd);
}

static void BrNet_Seq6(BrNetSeq *p_seqwk, u32 *p_seq, BrNet *net) {
    BOOL ret = GdsRap_SendBattleVideoList3(&net->gdsrap);

    GFL_ASSERT(ret);
    BrNetSeq_SetNext(p_seqwk, BrNet_SeqWaitEnd);
}

static void BrNet_Seq7(BrNetSeq *p_seqwk, u32 *p_seq, BrNet *net) {
    BOOL ret = GdsRap_SendBattleVideoList2(&net->gdsrap);

    GFL_ASSERT(ret);
    BrNetSeq_SetNext(p_seqwk, BrNet_SeqWaitEnd);
}

static void BrNet_Seq8(BrNetSeq *p_seqwk, u32 *p_seq, BrNet *net) {
    BOOL ret = GdsRap_SendBattleVideoFavorite(&net->gdsrap, net->param.dataNumber);

    GFL_ASSERT(ret);
    BrNetSeq_SetNext(p_seqwk, BrNet_SeqWaitEnd);
}

static void BrNet_OnUploadMusicalShotEnd(void *work, const GdsRapResult *result) {
    BrNet *net = work;

    net->uploadMusicalShotEnd = TRUE;
}

static void BrNet_OnDownloadMusicalShotEnd(void *work, const GdsRapResult *result) {
    BrNet *net = work;

    net->downloadMusicalShotEnd = TRUE;
}

static void BrNet_OnUploadVideoEnd(void *work, const GdsRapResult *result) {
    BrNet *net = work;

    net->uploadVideoEnd = TRUE;
}

static void BrNet_OnDownloadVideoListEnd(void *work, const GdsRapResult *result) {
    BrNet *net = work;

    net->downloadVideoListEnd = TRUE;
}

static void BrNet_OnDownloadVideoEnd(void *work, const GdsRapResult *result) {
    BrNet *net = work;

    net->downloadVideoEnd = TRUE;
}

static void BrNet_OnUnk5d8End(void *work, const GdsRapResult *result) {
    BrNet *net = work;

    net->unk5d8End = TRUE;
}

static BrNetSeq *BrNetSeq_Init(BrNet *net, BrNetSeqFunc func, HeapID heapId) {
    BrNetSeq *p_wk = GFL_HeapAllocate(heapId, sizeof(BrNetSeq), FALSE, "br_net.c", 1103);

    MI_CpuFill8(p_wk, 0, sizeof(BrNetSeq));
    p_wk->net = net;
    BrNetSeq_SetNext(p_wk, func);
    return p_wk;
}

static void BrNetSeq_Exit(BrNetSeq *p_wk) {
    GFL_HeapFree(p_wk);
}

static void BrNetSeq_Main(BrNetSeq *p_wk) {
    if (p_wk->func != NULL) {
        p_wk->func(p_wk, &p_wk->seq, p_wk->net);
    }
}

static void BrNetSeq_SetNext(BrNetSeq *p_wk, BrNetSeqFunc func) {
    p_wk->func = func;
    p_wk->seq = 0;
}

static BOOL BrNetSeq_IsComp(const BrNetSeq *cp_wk, BrNetSeqFunc func) {
    return cp_wk->func == func;
}

static u32 BrNet_EventFunc(void *work, int a1, int event, int a3) {
    BrNet *net = work;

    if (GameData_CheckEventsPaused(net->gameData)) {
        return TRUE;
    }

    GdsRap_OnNetEvent(&net->gdsrap, a1, event, a3);
    if (event == 3 || event == 6) {
        if (net->isOpen) {
            GdsRap_Exit(&net->gdsrap);
            net->isOpen = FALSE;
        }
        DWCRap_SetEventFunc(NULL, NULL);
    }
    return FALSE;
}
