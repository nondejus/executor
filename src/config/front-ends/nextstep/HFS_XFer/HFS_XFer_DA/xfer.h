#include "rsys/common.h"
#include "QuickDraw.h"
#include <WindowMgr.h>
#include <EventMgr.h>
#include <ControlMgr.h>
#include <DialogMgr.h>
#include <pascal.h>
#include <ToolboxUtil.h>
#include <TextEdit.h>
#include <FontMgr.h>
#include <ListMgr.h>
#include <MenuMgr.h>
#include <ResourceMgr.h>
#include <FileMgr.h>
#include <MemoryMgr.h>
#include <DeskMgr.h>
#include <StdFilePkg.h>
#include <OSUtil.h>
#include <SegmentLdr.h>
#ifndef UNIX
#include <HFS.h>
#else /* UNIX */
#include <ToolboxEvent.h>
#include <BinaryDecimal.h>
#endif /* UNIX */

#include "mytype.h"
#include "xbar2.h"

#define PLAN8

#define NELEM(x) (sizeof(x) / sizeof(x[0]))

#define DRVR(id, offset) ((short)(0xC000 | (id << 5) | offset))

extern short ourid;

#define ONEPARAMALERT DRVR(ourid, 0)
#define DOERRORALERT DRVR(ourid, 1)
#define ASKALERT DRVR(ourid, 2)
#define NOROOMALERTID DRVR(ourid, 3)
#define ABOUTALERT DRVR(ourid, 4)
#define PIECHARTID DRVR(ourid, 5)
#define XFERDLGID DRVR(ourid, 6)
#define DIRSONLYDLG DRVR(ourid, 7)
#define DESTDIRWINID DRVR(ourid, 8)
#define ONEPARAMABORTALERT DRVR(ourid, 9)
#define DOERRORABORTALERT DRVR(ourid, 10)

#define STARTALERT DRVR(ourid, 11)

#define COPY_DISK_ITEM 1
#define MOVE_FILE_ITEM 2
#define COPY_FILE_ITEM 3
#define RENAME_FILE_ITEM 4
#define DELETE_FILE_ITEM 5
#define NEW_FOLDER_ITEM 6
#define NEW_VOLUME_ITEM 11

#define FIRST_VERIFY_ITEM 7 /* THESE MUST BE CONTIGUOUS */

#define VERIFY_OVERWRITE_FILE_ITEM 7
#define VERIFY_OVERWRITE_FOLDER_ITEM 8
#define VERIFY_DELETE_FILE_ITEM 9
#define VERIFY_DELETE_FOLDER_ITEM 10

#define LAST_VERIFY_ITEM 10

#define ITEM_TO_BIT(item) (1 << (item - VERIFY_OVERWRITE_FILE_ITEM))

#define VERIFY_OVERWRITE_FILE ITEM_TO_BIT(VERIFY_OVERWRITE_FILE_ITEM)
#define VERIFY_OVERWRITE_FOLDER ITEM_TO_BIT(VERIFY_OVERWRITE_FOLDER_ITEM)
#define VERIFY_DELETE_FILE ITEM_TO_BIT(VERIFY_DELETE_FILE_ITEM)
#define VERIFY_DELETE_FOLDER ITEM_TO_BIT(VERIFY_DELETE_FOLDER_ITEM)

#define ABORTITEM 3
extern int abortflag;

#define ACTIONBUTTON 11
#define NEWDESTBUTTON 12
#define DESTNAME 13
#define TEXTITEM 14
#define FILENAMEITEM 2
#define PIECHARTITEM 3
#define SELECTBUTTON 11

#define VOLLOCKEDMASK ((1 << 7) | (1 << 15))
#define ISDIRMASK 16

#define ON 0
#define OFF 255

extern void about_HFS_XFer(), movefiles(), copyfiles(),
    renamefile(), deletefiles(), copydisk(), newdir(),
    getnewdest();
extern INTEGER quitfunc(), docopydisk(), donewdir();
extern void dotransfer();
#ifndef __STDC__
extern pascal INTEGER movefileshook(),
    copyfileshook(), renamefileshook(), deletefileshook(),
    copydiskhook(), newdirhook();
extern void delete1file(), doerror();
extern INTEGER copy1file(), move1file();
extern INTEGER ask();
extern BOOLEAN caneject();
extern void getnameandfromdirid();
#else /* __STDC__ */
extern pascal INTEGER movefileshook(INTEGER item, DialogPtr dp);
extern pascal INTEGER copyfileshook(INTEGER item, DialogPtr dp);
extern pascal INTEGER renamefileshook(INTEGER item, DialogPtr dp);
extern pascal INTEGER deletefileshook(INTEGER item, DialogPtr dp);
extern pascal INTEGER copydiskhook(INTEGER item, DialogPtr dp);
extern pascal INTEGER newdirhook(INTEGER item, DialogPtr dp);
extern void getnameandfromdirid(Str255 *sp, LONGINT *fromdirid);
extern INTEGER copy1file(INTEGER srcvrn, INTEGER dstvrn, LONGINT srcdirid,
                         LONGINT dstdirid, Str255 s, BOOLEAN doit);
extern INTEGER move1file(INTEGER srcvrn, INTEGER dstvrn, LONGINT srcdirid,
                         LONGINT dstdirid, Str255 s, BOOLEAN doit);
extern void doerror(OSErr errno, char *s);
extern void delete1file(INTEGER vrn, LONGINT dirid, Str255 s);
extern INTEGER ask(char *s1, Str255 s2);
extern BOOLEAN caneject(DialogPtr dp);
#endif /* __STDC__ */

extern LONGINT destdir;
extern INTEGER destdisk, verify_flags;
extern SFReply globalreply;

typedef enum { up,
               down,
               nowhere } direction;

typedef struct
{
    char *name;
    void (*ptr)();
} func;

typedef struct
{
    char *name;
    INTEGER (*dlgHook)
    (INTEGER item, DialogPtr dp);
    INTEGER h, v;
    char *prompt;
} funcinfo;

typedef struct
{
    char *name;
    INTEGER *var;
} option;

typedef struct
{
    INTEGER number;
    char message[50];
} errortable;

typedef enum { NOTE,
               CAUTION,
               STOP } alerttype;

typedef enum { datafork,
               resourcefork = 0xFF } forktype;
