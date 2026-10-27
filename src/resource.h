#pragma once
#include "dialog.h"

#define FILESPACER_VERSION        1,1,9,0
#define FILESPACER_VERSION_STRING "1.1.9\0"

#define IDR_RT_MANIFEST1 1 // must be 1 for an exe

#define IDR_ITEM_ACCEL      102
#define IDR_ITEM_MENU       111
#define IDM_PREV_WINDOW     1002
#define IDM_CLOSE_WINDOW    1003
#define IDM_REFRESH         1004
// #define IDM_HELP            1005 // Deferred public services.
#define IDM_PROXY_MENU      1006
#define IDM_NEW_FOLDER      1007
#define IDM_RENAME_PROXY    1008
#define IDM_SETTINGS        1009
#define IDM_PARENT_MENU     1010
#define IDM_VIEW_MENU       1012
#define IDM_NEW_TEXT_FILE   1015
#define IDM_DELETE_PROXY    1016
#define IDM_DEBUG_NAMES     1017
#define IDM_CONTEXT_MENU    1018
#define IDM_PROXY_BUTTON    1019
#define IDM_QUICK_ACCESS    1020
#define IDM_REOPEN_WINDOW   1021

#define IDM_SHELL_FIRST     0x4000
#define IDM_SHELL_LAST      0x7FFF

// https://docs.microsoft.com/en-us/windows/apps/design/style/segoe-ui-symbol-font
#define MDL2_CHEVRON_LEFT_MED       L"\uE973"
#define MDL2_REFRESH                L"\uE72C"
#define MDL2_MORE                   L"\uE712"
#define MDL2_SETTINGS               L"\uE713"
#define ICON_QUICK_ACCESS           L"\uE734"
#define ICON_UP_DIR      			L"\uE74A"
#define ICON_VIEW_MODE   			L"\uE71D"

#define IDC_NAME_SEARCH 151

#define IDS_SETTINGS_CAPTION       200
#define IDS_MENU_COMMAND           201
#define IDS_REFRESH_COMMAND        202
#define IDS_VIEW_COMMAND           204
#define IDS_SUCCESS_CAPTION        211
#define IDS_BROWSER_SET_FAILED     212
#define IDS_BROWSER_SET            213
#define IDS_BROWSER_RESET          214
#define IDS_REQUIRE_CONTEXT        215
#define IDS_BROWSER_SET_CONFIRM    216
#define IDS_APP_NAME               217
#define IDS_WELCOME_HEADER         218
#define IDS_WELCOME_BODY           219
// #define IDS_WELCOME_TUTORIAL       220 // Deferred public services.
#define IDS_WELCOME_BROWSER        222
#define IDS_CONFIRM_CAPTION        223
#define IDS_NO_UPDATE_CAPTION      224
#define IDS_NO_UPDATE              225
#define IDS_UPDATE_ERROR           226
#define IDS_WELCOME_UPDATE         227
#define IDS_OPEN_PARENT_COMMAND    228
#define IDS_ERROR_CAPTION          230
#define IDS_UPDATE_NOTIF_TITLE     233
#define IDS_UPDATE_NOTIF_INFO      234
#define IDS_CANT_FIND_ITEM         235
#define IDS_FOLDER_STATUS          236
#define IDS_FOLDER_STATUS_SEL      237
#define IDS_UNKNOWN_ERROR          246
#define IDS_LEGAL_INFO             247
#define IDS_FOLDER_ERROR           248
#define IDS_INVALID_CHARS          250
#define IDS_ADMIN_WARNING          251
#define IDS_DONT_ASK               252
#define IDS_FOLDER_STATUS_SEL_SIZE 253
#define IDS_RESET_FOLDER_STATE_CONFIRM       254
#define IDS_RESET_FOLDER_STATE_CLOSE_ERROR   255
#define IDS_RESET_FOLDER_STATE_STORAGE_ERROR 256
#define IDS_RESET_FOLDER_STATE_DELETE_ERROR  257
#define IDS_RESET_FOLDER_STATE_SUCCESS       259
#define IDS_FOLDER_STATE_ERROR               278
#define IDS_PATH_BAR                         260
#define IDS_ADDRESS_NOT_FOLDER               261
#define IDS_QUICK_ACCESS                      262
#define IDS_QUICK_ACCESS_UNAVAILABLE          263
#define IDS_QUICK_ACCESS_EMPTY                264
#define IDS_QUICK_ACCESS_OPEN                 265
#define IDS_QUICK_ACCESS_KEY                  266
#define IDS_QUICK_ACCESS_ENUM                 267
#define IDS_QUICK_ACCESS_READ                 268
#define IDS_QUICK_ACCESS_TARGET               269
#define IDS_QUICK_ACCESS_MENU                 270
#define IDS_SEARCH_NAME                       271
#define IDS_SEARCH_CUE                        272
#define IDS_SEARCH_APPLY                      273
#define IDS_SEARCH_ERROR                      274


#define IDS_CAPTION_COPY_PATH                275
#define IDS_CAPTION_RENAME                   276
#define IDS_CAPTION_PROPERTIES               277
#define IDS_CAPTION_CLOSE_PARENTS            309
#define IDS_CAPTION_REOPEN_NAMED             310
#define IDS_REOPEN_LAST                      311
#define IDS_REOPEN_EMPTY                     312
#define IDS_REOPEN_FAILED                    313
#define IDS_REOPEN_MENU                      314
#define IDS_REOPEN_NAMED                     315
#define MDL2_COPY_PATH                       L"\uE8C8"
#define MDL2_RENAME                          L"\uE8AC"
#define MDL2_PROPERTIES                      L"\uE946"
#define MDL2_FOLDER                          L"\uE8B7"
#define MDL2_CLOSE_PARENTS                   L"\uE711"

#define IDS_FOLDER_WIDTH_LABEL               279
#define IDS_FOLDER_HEIGHT_LABEL              280
#define IDS_FOLDER_WIDTH_INVALID             281
#define IDS_FOLDER_HEIGHT_INVALID            282
#define IDS_BROWSER_DIRECTORY                283
#define IDS_BROWSER_COMPRESSED_FOLDER        284
#define IDS_BROWSER_DRIVE                    285
#define IDS_BROWSER_ASSOCIATION_ERROR        286
#define IDS_BROWSER_ASSOCIATIONS_FAILED      287

#define IDS_RECOVERY_UNAVAILABLE                 298
#define IDS_RESET_CLI_ARGUMENTS                  300
#define IDS_RESET_CLI_BUSY                       301
#define IDS_RESET_CLI_FAILED                     302
#define IDS_RESET_CLI_ALL_DONE                   303
#define IDS_RESET_CLI_OPTIONS_DONE               304

#define IDS_RECOVERY_CRASHED                     306
#define IDS_SETTINGS_ACCESS_FAILED               308

#define IDS_UPDATE_SEARCH_BUTTON               316
#define IDS_UPDATE_INSTALL_BUTTON              317
#define IDS_UPDATE_RELEASE_BUTTON              318
#define IDS_UPDATE_SEARCHING                   319
#define IDS_UPDATE_AVAILABLE                   320
#define IDS_UPDATE_DOWNLOADING                 321
#define IDS_UPDATE_PROGRESS                    322
#define IDS_UPDATE_CLOSING                     323
#define IDS_UPDATE_FAILED                      324
#define IDS_UPDATE_FAILED_DETAIL               325
#define IDS_UPDATE_LEGACY                      326
#define IDS_UPDATE_BUSY                        327
#define IDS_UPDATE_CONFIRM_TITLE               328
#define IDS_UPDATE_CONFIRM                     329
#define IDS_UPDATE_RELAY_FAILED                330
#define IDS_UPDATE_CLOSE_BUTTON                331
#define IDS_UPDATE_RESTORE_FAILED              332


#define IDM_TOGGLE_STATE_LOCK 1100
#define IDS_LOCK_WINDOW_STATE 333
#define IDS_UNLOCK_WINDOW_STATE 334
#define IDS_WINDOW_STATE_LOCKED 335
#define IDS_DISABLE_STATE_LOCK_CONFIRM 336

#define IDR_LOCK_BADGE_ICON 113

#define IDS_SHORTCUT_MOVE 337
#define IDS_SHORTCUT_RESIZE 338
#define IDS_SHORTCUT_DISABLED 339
#define IDS_SHORTCUT_LEFT 340
#define IDS_SHORTCUT_MIDDLE 341
#define IDS_SHORTCUT_RIGHT 342
#define IDS_SHORTCUT_CTRL 343
#define IDS_SHORTCUT_ALT 344
#define IDS_SHORTCUT_SHIFT 345
#define IDS_SHORTCUT_DESCRIPTION 346
#define IDS_SHORTCUT_HEADER 347
#define IDS_SHORTCUT_ACCESSIBLE 348
#define IDS_SHORTCUT_GENERAL 349
#define IDS_SHORTCUT_RESERVED 350
#define IDS_SHORTCUT_CONFLICT 351
#define IDS_SHORTCUT_RECORD 352
#define IDS_SHORTCUT_STOP_RECORD 353
#define IDS_SHORTCUT_LEFT_HINT 354
#define IDS_SHORTCUT_RIGHT_HINT 355
#define IDS_SHORTCUT_SAVE_FAILED 356
#define IDS_SEARCH_EVERYTHING 357
#define IDS_SEARCH_MENU 358
#define IDS_EVERYTHING_ERROR 359
#define IDS_EVERYTHING_AUTO 360
#define IDS_EVERYTHING_FOLDER 361
#define IDS_EVERYTHING_EVERYWHERE 362
#define IDS_EVERYTHING_NOT_FOUND 363
#define IDS_SEARCH_HISTORY_INVALID 364
#define IDS_EVERYTHING_EXECUTABLE 365
#define IDS_SMART_WINDOW_PLACEMENT_HELP 367
#define IDS_GROUP_FOLDER_WINDOWS_HELP 369
#define IDS_OPEN_IN_EXPLORER 370
#define IDS_CONTEXT_MENU_SYNC_FAILED 371
#define IDC_SEARCH_SUBMIT 1601
