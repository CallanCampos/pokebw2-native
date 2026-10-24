#ifndef POKEBW2_APP_BATTLE_RECORDER_BR_NET_H
#define POKEBW2_APP_BATTLE_RECORDER_BR_NET_H

#include "types.h"
#include "gfl/heap.h"
#include "save/musical_savedata.h"
#include "save/save_control.h"
#include "struct_decls.h"

// The Battle Recorder's connection to the Global Link (br_net.c), used by its online modes

BrNet *BrNet_Init(GameData *gameData, uptr a1, HeapID heapId);
void BrNet_Exit(BrNet *net);
void BrNet_Main(BrNet *net);

// What a search for battle videos asks for
typedef struct {
    // The species the videos have, or BR_NET_VIDEO_SEARCH_MONSNO_NONE
    u16 monsno;
    // Where the videos' players live, or BR_NET_VIDEO_SEARCH_AREA_NONE in both
    u8 country;
    u8 area;
    // The videos' battle mode, a line of the search screen's list, or BR_NET_VIDEO_SEARCH_MODE_NONE
    u32 mode;
} BrNetVideoSearchData;

#define BR_NET_VIDEO_SEARCH_MONSNO_NONE 0xffff
#define BR_NET_VIDEO_SEARCH_AREA_NONE 0xff
#define BR_NET_VIDEO_SEARCH_MODE_NONE 19

// The data of a request to the Global Link
typedef union {
    struct {
        union {
            // The musical photo upload, request 0, sends the photo
            void *data;
            // The species whose musical photos request 1 downloads
            u16 species;
            // The arguments of request 4
            struct {
                u16 a1;
                u8 a3;
                u8 a4;
            };
        };
        u32 unk4;
    };
    // The arguments of request 4, as the search screen gives them
    BrNetVideoSearchData search;
    // The number of the battle video that requests 3 and 8 download and mark
    u64 dataNumber;
} BrNetRequestParam;

// A musical photo downloaded from the Global Link, after the profile of the player who sent it
typedef struct {
    // A GdsProfile, whose layout isn't recovered yet
    u8 profile[0x80];
    MusicalShot shot;
} BrMusicalShotRecv;

// The requests BrNet_StartRequest starts
enum {
    BR_NET_REQUEST_UPLOAD_MUSICAL_SHOT,
    BR_NET_REQUEST_DOWNLOAD_MUSICAL_SHOT,
    BR_NET_REQUEST_2,
    BR_NET_REQUEST_3,
    BR_NET_REQUEST_4,
    BR_NET_REQUEST_5,
    BR_NET_REQUEST_6,
    BR_NET_REQUEST_7,
    BR_NET_REQUEST_8,
    BR_NET_REQUEST_MAX,
};

// Starts a request, BR_NET_REQUEST_*
void BrNet_StartRequest(BrNet *net, int type, const BrNetRequestParam *param);
// TRUE once the request has finished
BOOL BrNet_IsRequestEnd(BrNet *net);
// Returns a status from 0 to 3 and consumes any valid GDS result, including those returning 0.
// msgID may be NULL and is written only for results that map to a message; otherwise it is unchanged.
BOOL BrNet_GetResultMsg(BrNet *net, u32 *msgID);
// 2 once the connection has had an error, which ends the Battle Recorder's screens
u32 BrNet_CheckError(BrNet *net);
// TRUE once the connection has failed
BOOL BrNet_IsFailed(BrNet *net);
// TRUE once the photos are downloaded, then giving up to tblMax of them in tbl and their number in recvNum
BOOL BrNet_GetDownloadMusicalShot(BrNet *net, BrMusicalShotRecv **tbl, int tblMax, int *recvNum);

// Once the video's upload is finished, TRUE and the video's number
BOOL BrNet_GetUploadVideoNumber(BrNet *net, u64 *number);
// Once a video is downloaded, TRUE, a pointer to it and the value that goes with it
BOOL BrNet_GetDownloadVideo(BrNet *net, void **video, u32 *value);
// Once a list of videos is downloaded, TRUE with up to listMax of its entries in list and their number in num. The
// three are for the three lists the Battle Recorder can ask for
BOOL BrNet_GetDownloadVideoListA(BrNet *net, void *list, int listMax, int *num);
BOOL BrNet_GetDownloadVideoListB(BrNet *net, void *list, int listMax, int *num);
BOOL BrNet_GetDownloadVideoListC(BrNet *net, void *list, int listMax, int *num);

#endif // POKEBW2_APP_BATTLE_RECORDER_BR_NET_H
