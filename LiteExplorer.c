// LiteExplorer.c
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <shellapi.h>
#include <tchar.h>
#include <stdio.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

#define ID_LISTVIEW 1001
#define ID_EDITPATH 1002
#define ID_BUTTON_PARENT 1003
#define ID_BUTTON_GO 1004
#define ID_EDITPATH       1005
#define ID_BUTTON_GO      1006
#define ID_BUTTON_PARENT  1007
#define ID_LISTVIEW       1008
#define ID_EDITSEARCH     1009
#define ID_BUTTON_SEARCH  1010
#define ID_MENU_DELETE 2001
#define ID_MENU_COPY 2002
#define ID_MENU_PASTE 2003
#define ID_MENU_RENAME 2004
#define ID_MENU_PROPERTIES 2005

HWND hListView, hEditPath, hWndMain;
HWND hSearchBox;
TCHAR currentPath[MAX_PATH];
TCHAR clipboardPath[MAX_PATH] = TEXT("");

HIMAGELIST hImageList = NULL;

void ShowError(LPCTSTR msg) {
    MessageBox(NULL, msg, TEXT("LiteExplorer"), MB_ICONERROR);
}

BOOL IsDirectory(const TCHAR* path) {
    DWORD attr = GetFileAttributes(path);
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
}

void FormatFileSize(LARGE_INTEGER size, TCHAR* out) {
    if (size.QuadPart >= (1LL << 20))
        wsprintf(out, TEXT("%.1f MB"), size.QuadPart / 1048576.0);
    else if (size.QuadPart >= (1LL << 10))
        wsprintf(out, TEXT("%.1f KB"), size.QuadPart / 1024.0);
    else
        wsprintf(out, TEXT("%llu B"), size.QuadPart);
}

void FormatFileTime(const FILETIME* ft, TCHAR* out) {
    SYSTEMTIME st;
    FileTimeToLocalFileTime(ft, (LPFILETIME)&st);
    FileTimeToSystemTime((LPFILETIME)&st, &st);
    wsprintf(out, TEXT("%04d/%02d/%02d %02d:%02d"),
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
}

void UpdateWindowTitle() {
    TCHAR title[MAX_PATH + 50];
    wsprintf(title, TEXT("LiteExplorer - [%s]"), currentPath);
    SetWindowText(hWndMain, title);
}

void ListDirectory(const TCHAR* path) {
    ListView_DeleteAllItems(hListView);
    SetWindowText(hEditPath, path);
    lstrcpy(currentPath, path);
    UpdateWindowTitle();

    WIN32_FIND_DATA fd;
    TCHAR searchPath[MAX_PATH];
    wsprintf(searchPath, TEXT("%s\\*"), path);

    HANDLE hFind = FindFirstFile(searchPath, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    int index = 0;
    do {
        if (lstrcmp(fd.cFileName, TEXT(".")) == 0 || lstrcmp(fd.cFileName, TEXT("..")) == 0)
            continue;

        TCHAR fullPath[MAX_PATH];
        wsprintf(fullPath, TEXT("%s\\%s"), path, fd.cFileName);

        SHFILEINFO shfi;
        int imageIndex = 0;
        if (SHGetFileInfo(fullPath, 0, &shfi, sizeof(shfi), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_SYSICONINDEX)) {
            imageIndex = shfi.iIcon;
        }

        LVITEM item = { 0 };
        item.mask = LVIF_TEXT | LVIF_IMAGE;
        item.iItem = index;
        item.pszText = fd.cFileName;
        item.iImage = imageIndex;
        int iItem = ListView_InsertItem(hListView, &item);

        // サイズ
        TCHAR sizeStr[64] = TEXT("");
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            LARGE_INTEGER li;
            li.LowPart = fd.nFileSizeLow;
            li.HighPart = fd.nFileSizeHigh;
            FormatFileSize(li, sizeStr);
        }
        ListView_SetItemText(hListView, iItem, 1, sizeStr);

        // 更新日時
        TCHAR timeStr[64];
        FormatFileTime(&fd.ftLastWriteTime, timeStr);
        ListView_SetItemText(hListView, iItem, 2, timeStr);

        index++;
    } while (FindNextFile(hFind, &fd));

    FindClose(hFind);
}

void ShowContextMenu(HWND hwnd, int x, int y) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, ID_MENU_COPY, TEXT("コピー"));
    AppendMenu(hMenu, MF_STRING, ID_MENU_PASTE, TEXT("貼り付け"));
    AppendMenu(hMenu, MF_STRING, ID_MENU_RENAME, TEXT("名前変更"));
    AppendMenu(hMenu, MF_STRING, ID_MENU_DELETE, TEXT("削除"));
    AppendMenu(hMenu, MF_STRING, ID_MENU_PROPERTIES, TEXT("プロパティ"));

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, x, y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

void DoCommand(int cmd) {
    int sel = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
    if (sel == -1) return;

    TCHAR name[MAX_PATH];
    ListView_GetItemText(hListView, sel, 0, name, MAX_PATH);

    TCHAR fullPath[MAX_PATH];
    wsprintf(fullPath, TEXT("%s\\%s"), currentPath, name);

    switch (cmd) {
    case ID_MENU_COPY:
        lstrcpy(clipboardPath, fullPath);
        break;
    case ID_MENU_PASTE: {
        if (lstrlen(clipboardPath) == 0) break;
        TCHAR fname[MAX_PATH];
        PathStripPath(clipboardPath);
        wsprintf(fname, TEXT("%s\\%s"), currentPath, PathFindFileName(clipboardPath));
        CopyFile(clipboardPath, fname, FALSE);
        ListDirectory(currentPath);
        break;
    }
    case ID_MENU_RENAME: {
        TCHAR newName[MAX_PATH];
        if (DialogBoxParam(NULL, MAKEINTRESOURCE(101), hWndMain, NULL, (LPARAM)newName)) {
            TCHAR newPath[MAX_PATH];
            wsprintf(newPath, TEXT("%s\\%s"), currentPath, newName);
            MoveFile(fullPath, newPath);
            ListDirectory(currentPath);
        }
        break;
    }
    case ID_MENU_DELETE:
        if (MessageBox(hWndMain, TEXT("本当に削除しますか？"), TEXT("確認"), MB_YESNO | MB_ICONQUESTION) == IDYES) {
            if (IsDirectory(fullPath))
                RemoveDirectory(fullPath);
            else
                DeleteFile(fullPath);
            ListDirectory(currentPath);
        }
        break;
    case ID_MENU_PROPERTIES: {
        SHELLEXECUTEINFO sei;
        ZeroMemory(&sei, sizeof(sei));
        sei.cbSize = sizeof(sei);
        sei.lpVerb = TEXT("properties");
        sei.lpFile = fullPath;
        sei.nShow = SW_SHOW;
        sei.fMask = SEE_MASK_INVOKEIDLIST;
        ShellExecuteEx(&sei);
        break;
    }
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        InitCommonControls();

        hEditPath = CreateWindowEx(0, TEXT("EDIT"), TEXT(""),
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL,
            10, 10, 500, 24, hwnd, (HMENU)ID_EDITPATH, NULL, NULL);

        HWND hGo = CreateWindow(TEXT("BUTTON"), TEXT("移動"),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            520, 10, 60, 24, hwnd, (HMENU)ID_BUTTON_GO, NULL, NULL);

        HWND hParent = CreateWindow(TEXT("BUTTON"), TEXT("↑ 上へ"),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            590, 10, 60, 24, hwnd, (HMENU)ID_BUTTON_PARENT, NULL, NULL);

        hListView = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, TEXT(""),
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
            10, 45, 640, 400, hwnd, (HMENU)ID_LISTVIEW, NULL, NULL);
        hSearchBox = CreateWindowEx(0, TEXT("EDIT"), TEXT(""),
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            10, 450, 300, 24, hwnd, (HMENU)ID_EDITSEARCH, NULL, NULL);

        HWND hSearchBtn = CreateWindow(TEXT("BUTTON"), TEXT("検索"),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            320, 450, 80, 24, hwnd, (HMENU)ID_BUTTON_SEARCH, NULL, NULL);

        ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        LVCOLUMN col = { 0 };
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.cx = 250; col.pszText = TEXT("名前");
        ListView_InsertColumn(hListView, 0, &col);
        col.cx = 100; col.pszText = TEXT("サイズ");
        ListView_InsertColumn(hListView, 1, &col);
        col.cx = 150; col.pszText = TEXT("更新日時");
        ListView_InsertColumn(hListView, 2, &col);

        SHFILEINFO shfi;
        hImageList = (HIMAGELIST)SHGetFileInfo(TEXT("C:\\"), 0, &shfi, sizeof(shfi),
            SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
        ListView_SetImageList(hListView, hImageList, LVSIL_SMALL);

        GetCurrentDirectory(MAX_PATH, currentPath);
        ListDirectory(currentPath);
        break;
    }

    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case ID_BUTTON_GO: {
            TCHAR path[MAX_PATH];
            GetWindowText(hEditPath, path, MAX_PATH);
            if (PathFileExists(path) && IsDirectory(path)) {
                ListDirectory(path);
            }
            else {
                ShowError(TEXT("そのパスは存在しません"));
            }
            break;
        }
        case ID_BUTTON_PARENT: {
            if (PathRemoveFileSpec(currentPath)) {
                ListDirectory(currentPath);
            }
            else {
                ShowError(TEXT("親フォルダがありません"));
            }
            break;
        }
        default:
            DoCommand(LOWORD(wParam));
            break;
        }
    case ID_BUTTON_SEARCH: {
        TCHAR keyword[MAX_PATH];
        GetWindowText(hSearchBox, keyword, MAX_PATH);

        ListView_DeleteAllItems(hListView);
        WIN32_FIND_DATA fd;
        TCHAR searchPath[MAX_PATH];
        wsprintf(searchPath, TEXT("%s\\*"), currentPath);
        HANDLE hFind = FindFirstFile(searchPath, &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            int index = 0;
            do {
                if (lstrcmp(fd.cFileName, TEXT(".")) == 0 || lstrcmp(fd.cFileName, TEXT("..")) == 0)
                    continue;

                if (StrStrI(fd.cFileName, keyword)) {
                    TCHAR fullPath[MAX_PATH];
                    wsprintf(fullPath, TEXT("%s\\%s"), currentPath, fd.cFileName);

                    SHFILEINFO shfi;
                    SHGetFileInfo(fullPath, 0, &shfi, sizeof(shfi),
                        SHGFI_ICON | SHGFI_SMALLICON | SHGFI_TYPENAME);

                    LVITEM item = { 0 };
                    item.mask = LVIF_TEXT | LVIF_IMAGE;
                    item.pszText = fd.cFileName;
                    item.iItem = index++;
                    item.iImage = shfi.iIcon;
                    ListView_InsertItem(hListView, &item);

                    TCHAR sizeStr[32] = TEXT("-");
                    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                        wsprintf(sizeStr, TEXT("%u KB"), fd.nFileSizeLow / 1024);
                    }
                    ListView_SetItemText(hListView, item.iItem, 1, sizeStr);

                    FILETIME ft = fd.ftLastWriteTime;
                    SYSTEMTIME st;
                    FileTimeToSystemTime(&ft, &st);
                    TCHAR timeStr[64];
                    wsprintf(timeStr, TEXT("%04d/%02d/%02d %02d:%02d"),
                        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
                    ListView_SetItemText(hListView, item.iItem, 2, timeStr);
                }
            } while (FindNextFile(hFind, &fd));
            FindClose(hFind);
        }
        break;
    }
        break;
    }

    case WM_DROPFILES: {
        HDROP hDrop = (HDROP)wParam;
        TCHAR src[MAX_PATH];
        DragQueryFile(hDrop, 0, src, MAX_PATH);

        TCHAR dst[MAX_PATH];
        PathCombine(dst, currentPath, PathFindFileName(src));
        CopyFile(src, dst, FALSE);

        DragFinish(hDrop);
        ListDirectory(currentPath);
        break;
    }

    case WM_NOTIFY: {
        LPNMHDR hdr = (LPNMHDR)lParam;
        if (hdr->idFrom == ID_LISTVIEW && hdr->code == NM_DBLCLK) {
            int sel = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
            if (sel != -1) {
                TCHAR name[MAX_PATH];
                ListView_GetItemText(hListView, sel, 0, name, MAX_PATH);

                TCHAR fullPath[MAX_PATH];
                wsprintf(fullPath, TEXT("%s\\%s"), currentPath, name);

                if (IsDirectory(fullPath)) {
                    ListDirectory(fullPath);
                }
                else {
                    ShellExecute(NULL, TEXT("open"), fullPath, NULL, NULL, SW_SHOW);
                }
            }
        }
        break;
    }

    case WM_CONTEXTMENU: {
        HWND hwndFrom = (HWND)wParam;
        if (hwndFrom == hListView) {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            ShowContextMenu(hwnd, x, y);
        }
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = TEXT("LiteExplorer");
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wc);
    DragAcceptFiles(hWndMain, TRUE);

    hWndMain = CreateWindow(TEXT("LiteExplorer"), TEXT("LiteExplorer"),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 680, 500,
        NULL, NULL, hInstance, NULL);

    ShowWindow(hWndMain, nCmdShow);
    UpdateWindow(hWndMain);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
