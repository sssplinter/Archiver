#include "stdafx.h"
#include "Win32Project1.h"
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <shellapi.h>
#include <math.h>

#include "zip.h"
#include "unzip.h"

using namespace std;

struct information
{
	TCHAR name[MAX_PATH];
	TCHAR type[MAX_PATH];
	double size;
	FILETIME creation_date; //created
	FILETIME last_use_date; //last access
} info;

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, UINT, LONG);
INT_PTR CALLBACK InfoWindow(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK Archivation(HWND, UINT, WPARAM, LPARAM);

TCHAR szWindowClass[MAX_LOADSTRING], szTitle[MAX_LOADSTRING], dir[MAX_PATH], dir1[MAX_PATH], copy_buf1[MAX_PATH];
HINSTANCE hInst;
HWND hWndChild = nullptr;
HIMAGELIST g_hImageList = nullptr;

static HWND hListView_1, hComboBox_1, hLabel_1,
hLabel_2, hLabel_3, hLabel_4, hToolBar;

int sel, k = 0, y = 9, index = -1;
TCHAR c, * ls;

TCHAR buf1[MAX_PATH], cm_dir_from[MAX_PATH], cm_dir_to[MAX_PATH], cm_dir_to_[MAX_PATH], cm_dir_from_[MAX_PATH],
path[MAX_PATH], _dir[MAX_PATH], _dir1[MAX_PATH], buff[MAX_PATH], tempdir[MAX_PATH], copyBuffer[MAX_PATH];
LPCTSTR s;

//toolbar
const int ImageListID = 0;
const int numButtons = 6;
const DWORD buttonStyles = BTNS_AUTOSIZE;
const int bitmapSize = 16;

TBBUTTON tbButtons[numButtons] =
{
	{MAKELONG(STD_UNDO, ImageListID), IDM_UP, TBSTATE_ENABLED, buttonStyles, {0}, 0, (INT_PTR)(_T("Back"))},
	{MAKELONG(STD_COPY, ImageListID), IDM_COPY, TBSTATE_ENABLED, buttonStyles, {0}, 0, (INT_PTR)(_T("Copy"))},
	{MAKELONG(STD_DELETE, ImageListID), IDM_DEL, TBSTATE_ENABLED, buttonStyles, {0}, 0, (INT_PTR)(_T("Delete"))},
	{MAKELONG(STD_PASTE, ImageListID), IDM_PASTE,TBSTATE_ENABLED, buttonStyles, {0}, 0, (INT_PTR)(_T("Paste"))},
	{MAKELONG(STD_HELP, ImageListID), IDM_INFO, TBSTATE_ENABLED, buttonStyles, {0}, 0, (INT_PTR)(_T("Info"))},
	{MAKELONG(STD_FILENEW, ImageListID), IDM_ARCH, TBSTATE_ENABLED, buttonStyles, {0}, 0, (INT_PTR)(_T("Archive"))}
};

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPTSTR lpCmdLine,
	_In_ int nCmdShow)
{
	MSG Msg;

	HACCEL hAccelTable;

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	wcscpy(dir, _T("C:\\*"));
	wcscpy(dir1, _T("C:\\*"));
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_WIN32PROJECT1, szWindowClass, MAX_LOADSTRING);
	
	SendMessage(hToolBar, TB_SETSTATE, static_cast<WPARAM>(IDM_PASTE), MAKELONG(0, 0));
	MyRegisterClass(hInstance);

	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WIN32PROJECT1));
	InitCommonControls();

	while (GetMessage(&Msg, nullptr, 0, 0))
	{
		DispatchMessage(&Msg);
		TranslateMessage(&Msg);
	}
	return Msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WIN32PROJECT1));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCE(IDC_WIN32PROJECT1);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	HWND hWnd;
	// размеры окна
	int hh = 640, ww = 916;
	//окно посреди экрана
	int xx = (GetSystemMetrics(SM_CXSCREEN) - ww) / 2;
	int yy = (GetSystemMetrics(SM_CYSCREEN) - hh) / 2;
	hInst = hInstance;
	hWnd = CreateWindow(szWindowClass, szTitle, WS_BORDER | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
		xx, yy, ww, hh, NULL, NULL, hInstance, NULL);

	if (!hWnd)
	{
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

// создание меню
HWND CreateSimpleToolbar(HWND hWndParent)
{
	//создание тулбара
	HWND hWndToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, nullptr, WS_CHILD | TBSTYLE_WRAPABLE,
		0, 0, 0, 0, hWndParent, nullptr, hInst, nullptr);

	if (hWndToolbar == nullptr)
	{
		return nullptr;
	}

	//создание списка картинок, для кнопок
	g_hImageList = ImageList_Create(bitmapSize, bitmapSize, ILC_COLOR16 | ILC_MASK, numButtons, 0);

	//зачем сообщения отправляем то? но видимо чтобы перерисовать окно
	SendMessage(hWndToolbar, TB_SETIMAGELIST, static_cast<WPARAM>(ImageListID),
		(LPARAM)g_hImageList);

	SendMessage(hWndToolbar, TB_LOADIMAGES, static_cast<WPARAM>(IDB_STD_SMALL_COLOR),
		(LPARAM)HINST_COMMCTRL);

	SendMessage(hWndToolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
	SendMessage(hWndToolbar, TB_ADDBUTTONS, static_cast<WPARAM>(numButtons), (LPARAM)&tbButtons);
	SendMessage(hWndToolbar, TB_AUTOSIZE, 0, 0);

	ShowWindow(hWndToolbar, TRUE);

	return hWndToolbar;
}

//походу колонка файлов
void AddColToListView(TCHAR* st, int sub, int size)
{
	LVCOLUMN lvc;

	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvc.iSubItem = sub;
	lvc.pszText = st;
	lvc.cx = size;
	lvc.fmt = LVCFMT_LEFT;

	ListView_InsertColumn(hListView_1, sub, &lvc);
}

int i, i_dirs, i_files, i_all;

BOOL InitListViewImageLists(HWND hWndListView, int size, TCHAR c_dir[MAX_PATH])
{
	HIMAGELIST hSmall;
	SHFILEINFO lp;
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;
	TCHAR buf1[MAX_PATH], buffer[MAX_PATH];
	DWORD num;

	hSmall = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), ILC_MASK | ILC_COLOR32,
		size + 2, 1);

	int LVcounter = 0;
	for (int j = 0; j < size; j++)
	{
		hFind = FindFirstFile(c_dir, &FindFileData);

		if (hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				ListView_GetItemText(hWndListView, LVcounter, 0, buffer, MAX_PATH);
				if (wcscmp(FindFileData.cFileName, buffer) == 0)
				{
					if (wcscmp(FindFileData.cFileName, _T(".")) == 0) //если диск
					{
						wcscpy(buf1, c_dir);
						wcscat(buf1, FindFileData.cFileName);
						SHGetFileInfo(_T(""), FILE_ATTRIBUTE_DEVICE, &lp, sizeof(lp),
							SHGFI_ICONLOCATION | SHGFI_ICON | SHGFI_SMALLICON);
						ImageList_AddIcon(hSmall, lp.hIcon);
						DestroyIcon(lp.hIcon);
					}
					if (wcscmp(FindFileData.cFileName, _T("..")) == 0) //если фаилы,папки
					{
						wcscpy(buf1, c_dir);
						wcscat(buf1, FindFileData.cFileName);
						SHGetFileInfo(_T(""), FILE_ATTRIBUTE_DIRECTORY, &lp, sizeof(lp),
							SHGFI_ICONLOCATION | SHGFI_ICON | SHGFI_SMALLICON);
						ImageList_AddIcon(hSmall, lp.hIcon);
						DestroyIcon(lp.hIcon);
					}
					//присваеваем иконки
					wcscpy(buf1, c_dir);
					buf1[wcslen(buf1) - 1] = 0;
					wcscat(buf1, FindFileData.cFileName);
					num = GetFileAttributes(buf1);
					SHGetFileInfo(buf1, num, &lp, sizeof(lp), SHGFI_ICONLOCATION | SHGFI_ICON | SHGFI_SMALLICON);
					ImageList_AddIcon(hSmall, lp.hIcon);
					DestroyIcon(lp.hIcon);
					LVcounter++;
					break;
				}
			} while (FindNextFile(hFind, &FindFileData) != 0);

			FindClose(hFind);
		}
	}
	ListView_SetImageList(hWndListView, hSmall, LVSIL_SMALL);

	return TRUE;
}

void View_List(TCHAR* buf, HWND hList, int i, int j)
{
	LVITEM lvItem;

	lvItem.mask = LVIF_IMAGE | LVIF_TEXT;
	lvItem.state = 0;
	lvItem.stateMask = 0;
	lvItem.iItem = i;
	lvItem.iImage = i;
	lvItem.iSubItem = j;
	lvItem.pszText = buf;
	lvItem.cchTextMax = sizeof(buf);

	ListView_InsertItem(hList, &lvItem);
}

void reverseString(wchar_t str1[255], wchar_t str2[255])
{
	int a1, a2, a3;
	a1 = wcslen(str1);
	a3 = a1;
	for (a2 = 0; a2 < a1; a2++, a3--)
		str2[a2] = str1[a3 - 1];
}

//получаем тип файла для отображения
void getTypeOfFile(TCHAR nameStr[MAX_PATH], TCHAR res[MAX_PATH])
{
	TCHAR typestr[255], temp[255];
	int ending = 0;
	BOOL flag = false;
	wcscpy(typestr, nameStr);
	reverseString(typestr, temp);
	while ((ending < wcslen(temp)))
	{
		if (!flag)
		{
			if (temp[ending] == '.')
			{
				temp[ending] = NULL;
				flag = true;
			}
		}
		else
			temp[ending] = NULL;
		ending++;
	}
	wcscpy(typestr, temp);
	reverseString(typestr, temp);
	wcscpy(res, temp);
}


//ищем файл
void FindFile(HWND hList, TCHAR c_dir[MAX_PATH])
{
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;
	BOOL firstSearch = true;

	//удаляем элементы из списка?
	SendMessage(hList, LVM_DELETEALLITEMS, static_cast<WPARAM>(0), static_cast<LPARAM>(0));

	i = i_dirs = i_files = i_all = 0;
	for (int j = 0; j < 2; j++)
	{
		hFind = FindFirstFile(c_dir, &FindFileData);

		if (hFind == INVALID_HANDLE_VALUE)
		{
			if (firstSearch)
				MessageBox(nullptr, _T("Not found"), _T("Error"), MB_OK | MB_ICONERROR);
		}
		else
		{
			do
			{
				if (wcscmp(FindFileData.cFileName, _T(".")) != 0 && wcscmp(FindFileData.cFileName, _T("..")) != 0)
				{
					if (firstSearch)
					{
						if ((FindFileData.dwFileAttributes == 16) || (FindFileData.dwFileAttributes == 17))
						{
							View_List(FindFileData.cFileName, hList, i, 0);

							ListView_SetItemText(hList, i, 1, _T("<directory>"));

							i++;
						}
					}
					else
					{
						if (FindFileData.dwFileAttributes == 32)
						{
							View_List(FindFileData.cFileName, hList, i, 0);

							TCHAR temp[255], finaltype[255];
							getTypeOfFile(FindFileData.cFileName, temp);
							wcscpy(finaltype, _T("'"));
							wcscat(finaltype, temp);
							wcscat(finaltype, _T("' file"));
							ListView_SetItemText(hList, i, 1, finaltype);

							double size = (FindFileData.nFileSizeHigh * (MAXDWORD64 + 1)) + FindFileData.nFileSizeLow;
							wchar_t sizestr[255];
							swprintf(sizestr, L"%f", size);
							sizestr[wcslen(sizestr) - 7] = 0;
							wcscat(sizestr, _T(" bytes"));
							ListView_SetItemText(hList, i, 2, sizestr);
							i++;
						}
					}
				}
			} while (FindNextFile(hFind, &FindFileData) != 0);

			FindClose(hFind);
			InitListViewImageLists(hList, i, c_dir);
		}
		firstSearch = false;
	}
}

void Copy_File(TCHAR from[MAX_PATH], TCHAR directory[MAX_PATH], TCHAR buf[MAX_PATH])
{
	wcscpy(from, directory);
	from[wcslen(from) - 1] = 0;
	wcscat(from, buf);
}

void Delete_File(TCHAR from[MAX_PATH], TCHAR directory[MAX_PATH], TCHAR buf[MAX_PATH])
{
	wcscpy(from, directory);
	from[wcslen(from) - 1] = 0;
	wcscat(from, buf);
	DeleteFile(from);
}

information GetFileInform(TCHAR file[MAX_PATH])
{
	WIN32_FIND_DATA fd;
	information fileInfo;

	wcscpy(fileInfo.type, _T(""));

	FindFirstFile(file, &fd);

	fileInfo.creation_date = fd.ftCreationTime;
	fileInfo.last_use_date = fd.ftLastAccessTime;

	wcscpy(fileInfo.name, fd.cFileName);

	fileInfo.size = (fd.nFileSizeHigh * (MAXDWORD64 + 1)) + fd.nFileSizeLow;

	if (fd.dwFileAttributes == FILE_ATTRIBUTE_DEVICE)
		wcscpy(fileInfo.type, _T("Device"));
	else if (fd.dwFileAttributes == 32)
		wcscpy(fileInfo.type, _T("File"));
	else if (fd.dwFileAttributes == FILE_ATTRIBUTE_SYSTEM)
		wcscpy(fileInfo.type, _T("System file"));
	else if (fd.dwFileAttributes == FILE_ATTRIBUTE_VIRTUAL)
		wcscpy(fileInfo.type, _T("Virtual file"));
	else if (fd.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY)
		wcscpy(fileInfo.type, _T("Directory"));

	if (wcscmp(fileInfo.type, _T("")) == 0)
		wcscpy(fileInfo.type, _T("File"));

	return fileInfo;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	auto pnmLV = (LPNMLISTVIEW)lParam;

	auto lpnmHdr = (LPNMHDR)lParam;

	GetLogicalDrives();
	GetLogicalDriveStrings(128, buff);

	switch (msg)
	{
	case WM_COMMAND:
	{
		switch (LOWORD(wParam))
		{
		case IDM_COPY:
		{
			Copy_File(cm_dir_from, dir, copy_buf1);
			wcscpy(copyBuffer, copy_buf1);
			return 0;
		}
		case IDM_DEL:
		{
			Delete_File(cm_dir_from, dir, copy_buf1);
			FindFile(hListView_1, dir);
			return 0;
		}
		case IDM_PASTE:
		{
			wcscpy(cm_dir_to, dir);
			cm_dir_to[wcslen(cm_dir_to) - 1] = 0;
			wcscat(cm_dir_to, copyBuffer);
			CopyFile(cm_dir_from, cm_dir_to, FALSE);
			FindFile(hListView_1, dir);
			return 0;
		}
		case IDM_UP:
		{
			bool isFind = false;
			ls = buff;
			dir[wcslen(dir) - 1] = 0;
			while (*ls)
			{
				if (wcscmp(dir, ls) == 0)
				{
					isFind = true;
					break;
				}
				ls += wcslen(ls) + 1;
			}
			wcscat(dir, _T("*"));
			if (!isFind)
			{
				wcscpy(tempdir, _T(".."));
				if (wcscmp(tempdir, _T("..")) == 0)
				{
					dir[wcslen(dir) - 2] = 0;

					for (i = 0; i < wcslen(dir); i++)
					{
						string s;
						s = dir[i];

						if (s == "\\")
							k = i;
					}

					dir[k + 1] = 0;
					wcscat(dir, _T("*"));
				}

				SetWindowText(hLabel_3, dir);
				FindFile(hListView_1, dir);
			}
			return 0;
		}
		case IDM_INFO:
		{
			DialogBox(hInst, MAKEINTRESOURCE(IDM_INFO), hwnd, InfoWindow);
			return 0;
		}
		case IDM_ARCH:
		{
			DialogBox(hInst, MAKEINTRESOURCE(IDM_ARCH), hwnd, Archivation);
			return 0;
		}
		case IDM_EXIT:
			DestroyWindow(hwnd);
			return 0;
		case ID_DRIVE_SELECTOR:
		{
			switch (HIWORD(wParam))
			{
			case CBN_SELENDOK:
			{
				wcscpy(path, _T(""));
				sel = SendMessage(hComboBox_1, CB_GETCURSEL, 0, 0);
				SendMessage(hComboBox_1, CB_GETLBTEXT, sel, (LPARAM)path);
				wcscat(path, _T("\*"));
				wcscpy(dir, path);
				SetWindowText(hLabel_3, dir);
				FindFile(hListView_1, dir);
				return 0;
			}
			default:
				return 0;
			}
		}
		return 0;
		}
	}
	case WM_NOTIFY:
	{
		if (((lpnmHdr->idFrom == ID_LISTVIEW_1)) && (lpnmHdr->code == NM_CLICK))
		{
			ListView_GetItemText(lpnmHdr->hwndFrom, pnmLV->iItem, pnmLV->iSubItem, buf1, MAX_PATH);

			wcscpy(copy_buf1, buf1);
			wcscpy(_dir, dir);
			_dir[wcslen(_dir) - 1] = 0;
			wcscat(_dir, buf1);

			if ((FILE_ATTRIBUTE_DIRECTORY & GetFileAttributes(_dir)) == FILE_ATTRIBUTE_DIRECTORY)
			{
				SendMessage(hToolBar, TB_SETSTATE, static_cast<WPARAM>(IDM_COPY), MAKELONG(0, 0));
				SendMessage(hToolBar, TB_SETSTATE, static_cast<WPARAM>(IDM_DEL), MAKELONG(0, 0));
				SendMessage(hToolBar, TB_SETSTATE, static_cast<WPARAM>(IDM_CUT), MAKELONG(0, 0));
			}
			else
			{
				SendMessage(hToolBar, TB_SETSTATE, static_cast<WPARAM>(IDM_COPY), MAKELONG(TBSTATE_ENABLED, 0));
				SendMessage(hToolBar, TB_SETSTATE, static_cast<WPARAM>(IDM_DEL), MAKELONG(TBSTATE_ENABLED, 0));
				SendMessage(hToolBar, TB_SETSTATE, static_cast<WPARAM>(IDM_CUT), MAKELONG(TBSTATE_ENABLED, 0));
			}
		}

		if ((lpnmHdr->idFrom == ID_LISTVIEW_1) && (lpnmHdr->code == NM_DBLCLK))
		{
			wcscpy(buf1, _T(""));
			ListView_GetItemText(lpnmHdr->hwndFrom, pnmLV->iItem, pnmLV->iSubItem, buf1, MAX_PATH);
			if (lpnmHdr->idFrom == ID_LISTVIEW_1)
			{
				k = 0;
				wcscpy(_dir, dir);
				_dir[wcslen(_dir) - 1] = 0;
				wcscat(_dir, buf1);

				for (int i = 0; i < wcslen(buf1); i++)
				{
					string s;
					s = buf1[i];

					if (s == ".")
						k = i;
				}

				if ((k != 0) && (k != 1))
				{
					if ((FILE_ATTRIBUTE_DIRECTORY & GetFileAttributes(_dir)) != FILE_ATTRIBUTE_DIRECTORY)
						ShellExecute(nullptr, _T("open"), _dir, nullptr, nullptr, SW_SHOWNORMAL);
					else
						FindFile(hListView_1, _dir);
				}
				else if (wcscmp(buf1, _T("..")) == 0)
				{
					k = 0;
					dir[wcslen(dir) - 2] = 0;

					for (i = 0; i < wcslen(dir); i++)
					{
						string s;
						s = dir[i];

						if (s == "\\")
							k = i;
					}

					dir[k + 1] = 0;
					wcscat(dir, _T("*"));
				}
				else if (wcscmp(buf1, _T(".")) == 0)
				{
					dir[3] = 0;
					wcscat(dir, _T("*"));
				}
				else
				{
					if (wcscmp(buf1, _T("")) == 0)
						break;
					wcscat(buf1, _T("\\*"));
					dir[wcslen(dir) - 1] = 0;
					wcscat(dir, buf1);
				}

				SetWindowText(hLabel_3, dir);
				FindFile(hListView_1, dir);
			}
		}
		break;
	}

	case WM_CREATE:
	{
		hToolBar = CreateSimpleToolbar(hwnd);
		hLabel_1 = CreateWindow(_T("static"), _T(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP,
			0, 35 + y, 900, 16, hwnd, (HMENU)ID_LABEL_1, hInst, NULL);
		hLabel_2 = CreateWindow(_T("static"), _T(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP,
			0, 45 + y, 900, 16, hwnd, (HMENU)ID_LABEL_2, hInst, NULL);
		hLabel_3 = CreateWindow(_T("static"), _T("way1"), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 3, 57 + y, 900, 16,
			hwnd, (HMENU)ID_LABEL_3, hInst, NULL);
		hComboBox_1 = CreateWindow(_T("ComboBox"), NULL,
			WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST | CBS_SORT,
			3, 33 + y, 50, 110, hwnd, (HMENU)ID_DRIVE_SELECTOR, hInst, NULL);
		hListView_1 = CreateWindow(WC_LISTVIEW, NULL,
			LVS_REPORT | WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | LVS_AUTOARRANGE,
			0, 60 + y + 12, 900, 500, hwnd, (HMENU)ID_LISTVIEW_1, hInst, NULL);
		ListView_SetExtendedListViewStyle(hListView_1, LVS_EX_FULLROWSELECT);

		ls = buff;

		while (*ls)
		{
			SendMessage(hComboBox_1, CB_ADDSTRING, 0, (LPARAM)ls);
			ls += wcslen(ls) + 1;
		}

		SendMessage(hComboBox_1, CB_SETCURSEL, 0, 0);

		AddColToListView(_T("Name"), 1, 550);
		AddColToListView(_T("Type"), 2, 103);
		AddColToListView(_T("Size"), 3, 130);
		//AddColToListView(_T("CRC64"), 4, 100);

		FindFile(hListView_1, dir);
		SetWindowText(hLabel_3, dir);

		return 0;
	}

	case WM_DESTROY:
	{
		DestroyWindow(hListView_1);
		DestroyWindow(hComboBox_1);
		DestroyWindow(hLabel_1);
		DestroyWindow(hLabel_2);
		DestroyWindow(hLabel_3);
		DestroyWindow(hLabel_4);
		DestroyWindow(hToolBar);
		PostQuitMessage(0);
		return 0;
	}
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void FileTimeToString(FILETIME ft, TCHAR str[255])
{
	SYSTEMTIME st;
	TCHAR szLocalDate[255], szLocalTime[255];

	FileTimeToLocalFileTime(&ft, &ft);
	FileTimeToSystemTime(&ft, &st);
	GetDateFormat(LOCALE_USER_DEFAULT, DATE_LONGDATE, &st, nullptr, szLocalDate, 255);
	GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, nullptr, szLocalTime, 255);
	wcscpy(str, szLocalDate);
	wcscat(str, _T(" "));
	wcscat(str, szLocalTime);
}

void ShortSize(double size, wchar_t str[255])
{
	int power = 0;
	double shortsize = size;

	while (floor(shortsize / 1024) != 0)
	{
		shortsize /= 1024;
		power++;
	}

	wchar_t buffer[MAX_PATH];
	swprintf(buffer, L"%f", shortsize);
	buffer[wcslen(buffer) - 5] = 0;
	switch (power)
	{
	case 0:
		break;
	case 1:
		wcscat(str, _T(" ("));
		wcscat(str, buffer);
		wcscat(str, _T(" Kb)"));
		break;
	case 2:
		wcscat(str, _T(" ("));
		wcscat(str, buffer);
		wcscat(str, _T(" Mb)"));
		break;
	case 3:
		wcscat(str, _T(" ("));
		wcscat(str, buffer);
		wcscat(str, _T(" Gb)"));
		break;
	case 4:
		wcscat(str, _T(" ("));
		wcscat(str, buffer);
		wcscat(str, _T(" Tb)"));
		break;
	}
}

INT_PTR CALLBACK InfoWindow(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		static HWND LabelName, LabelSize, LabelType, LabelState, LabelCrtd, LabelLast, LabelAtr;
		TCHAR stringCrtd[255], stringLast[255];
		wchar_t istr[255];
		if (wcscmp(_dir, _T("")) == 0)
			info = GetFileInform(dir);
		else
			info = GetFileInform(_dir);
		LabelName = CreateWindow(_T("static"), info.name, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
			50, 10, 215, 16, hDlg, (HMENU)ID_LABELNAME, hInst, NULL);
		swprintf(istr, L"%f", info.size);
		istr[wcslen(istr) - 7] = 0;
		wcscat(istr, _T(" bytes"));
		ShortSize(info.size, istr);
		LabelSize = CreateWindow(_T("static"), istr, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
			50, 50, 500, 16, hDlg, (HMENU)ID_LABELSIZE, hInst, NULL);
		LabelType = CreateWindow(_T("static"), info.type, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
			50, 30, 215, 16, hDlg, (HMENU)ID_LABELTYPE, hInst, NULL);
		FileTimeToString(info.creation_date, stringCrtd);
		LabelCrtd = CreateWindow(_T("static"), stringCrtd, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
			80, 72, 215, 16, hDlg, (HMENU)ID_LABELCRTD, hInst, NULL);
		FileTimeToString(info.last_use_date, stringLast);
		LabelLast = CreateWindow(_T("static"), stringLast, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
			80, 95, 215, 16, hDlg, (HMENU)ID_LABELLAST, hInst, NULL);
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		break;
	}
	return FALSE;
}

static HWND rb_arch, rb_disarch, edit_path, edit_name, edit_password, cb_alg;

void getNameOfDir(TCHAR nameStr[MAX_PATH], TCHAR res[MAX_PATH])
{
	TCHAR typestr[255], temp[255];
	int ending = 0;
	BOOL flag = false;
	wcscpy(typestr, nameStr);
	reverseString(typestr, temp);
	while ((ending < wcslen(temp)))
	{
		if (!flag)
		{
			if (temp[ending] == '\\')
			{
				temp[ending] = NULL;
				flag = true;
			}
		}
		else
			temp[ending] = NULL;
		ending++;
	}
	wcscpy(typestr, temp);
	reverseString(typestr, temp);
	wcscpy(res, temp);
}

INT_PTR CALLBACK Archivation(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		TCHAR name[MAX_PATH], archive_path[MAX_PATH];

		// выбор архивации или дезархивации
		rb_arch = CreateWindow(_T("button"), _T("Archive"), WS_CHILD | WS_VISIBLE | BS_RADIOBUTTON,
			50, 8, 80, 20, hDlg, (HMENU)ID_RBARCH, hInst, NULL);

		rb_disarch = CreateWindow(_T("button"), _T("Unarchive"), WS_CHILD | WS_VISIBLE | BS_RADIOBUTTON,
			135, 8, 100, 20, hDlg, (HMENU)ID_RBDISARCH, hInst, NULL);
		// путь куда сохранять
		wcscpy(archive_path, dir);
		archive_path[wcslen(archive_path) - 1] = 0;
		edit_path = CreateWindow(_T("edit"), archive_path, WS_CHILD | WS_VISIBLE | ES_LEFT | WS_BORDER,
			70, 39, 380, 20, hDlg, (HMENU)ID_EDITPATH, hInst, NULL);

		edit_password = CreateWindow(_T("edit"), _T(""), WS_CHILD | WS_VISIBLE | ES_CENTER | WS_BORDER,
			70, 105, 380, 20, hDlg, (HMENU)ID_PASSWORD, hInst, NULL);

		// по умолчанию используем изначальное название ресурса или архива
		wcscpy(name, L"");
		// получаем имя файла из списка файлов
		index = ListView_GetNextItem(hListView_1, -1, LVIS_SELECTED);
		ListView_GetItemText(hListView_1, index, 0, name, MAX_PATH);
		if (name != L"")
			edit_name = CreateWindow(_T("edit"), name, WS_CHILD | WS_VISIBLE | ES_CENTER | WS_BORDER,
				70, 72, 380, 20, hDlg, (HMENU)ID_EDITNAME, hInst, NULL);
		else
			edit_name = CreateWindow(_T("edit"), _T(""), WS_CHILD | WS_VISIBLE | ES_CENTER | WS_BORDER,
				70, 72, 380, 20, hDlg, (HMENU)ID_EDITNAME, hInst, NULL);

		//выбор способа архивации
		// TODO сделать обычным текстом
		cb_alg = CreateWindow(_T("ComboBox"), NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST,
			320, 8, 130, 110, hDlg, (HMENU)ID_COMBOBOXALG, hInst, NULL);

		SendMessage(cb_alg, CB_ADDSTRING, 0, (LPARAM)_T("ZIP"));

		SendMessage(cb_alg, CB_SETCURSEL, 0, 0);

		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			//Forces a list-view control to redraw a range of items. You can send this message explicitly or by using the ListView_RedrawItems macro.
			// перерисовываем список файлов
			SendMessage(hListView_1, LVM_REDRAWITEMS, 0, 0);
			return TRUE;
		}

		if (ID_RBARCH <= LOWORD(wParam) && LOWORD(wParam) <= ID_RBDISARCH)
		{
			CheckRadioButton(hDlg, ID_RBARCH, ID_RBDISARCH, LOWORD(wParam));
		}

		//начать шифрование
		if (LOWORD(wParam) == ID_START)
		{
			TCHAR option[MAX_PATH];
			GetDlgItemText(hDlg, ID_COMBOBOXALG, option, MAX_PATH);

			TCHAR* args[2], tempFilePath[MAX_PATH], nameOfDir[MAX_PATH];
			TCHAR nameStr[MAX_PATH], pathStr[MAX_PATH], temp[MAX_PATH], passwordStr[MAX_PATH];
			// выбрно заархивировать
			if (IsDlgButtonChecked(hDlg, ID_RBARCH))
			{
				index = ListView_GetNextItem(hListView_1, -1, LVIS_SELECTED);
				if (index != -1)
				{
					wcscpy(nameStr, L"");
					wcscpy(pathStr, L"");
					wcscpy(passwordStr, L"");
					GetWindowText(edit_name, nameStr, MAX_PATH);
					GetWindowText(edit_path, pathStr, MAX_PATH);
					GetWindowText(edit_password, passwordStr, MAX_PATH);
					if (nameStr != L"" && pathStr != L"")
					{
						wcscat(pathStr, nameStr);
						wcscat(pathStr, L".zip");
						wcscpy(temp, pathStr);
						args[1] = temp;

						HZIP hz;
						if (passwordStr == L"")
						{
							hz = CreateZip(args[1], "");
						}
						else
						{
							char password[MAX_PATH];
							wcstombs(password, passwordStr, MAX_PATH + 1);
							hz = CreateZip(args[1], password);
						}
						while (index != -1)
						{
							ListView_GetItemText(hListView_1, index, 0, temp, MAX_PATH);
							wcscpy(tempFilePath, dir);
							tempFilePath[wcslen(tempFilePath) - 1] = 0;
							wcscat(tempFilePath, temp);
							args[0] = tempFilePath;
							info = GetFileInform(args[0]);
							if (wcscmp(info.type, _T("Directory")) == 0)
							{
								getNameOfDir(args[0], nameOfDir);
								ZipAddFolder(hz, nameOfDir);
								index = ListView_GetNextItem(hListView_1, -1, LVNI_ALL);
							}
							else
								ZipAdd(hz, temp, args[0]);
							index = ListView_GetNextItem(hListView_1, index, LVIS_SELECTED | LVNI_BELOW);
							if (index == 0)
								index = -1;
						}
						CloseZip(hz);
						MessageBox(nullptr, _T("Done!"), _T(""), MB_OK | MB_ICONASTERISK);
						FindFile(hListView_1, dir);
						SendMessage(hDlg, WM_CLOSE, 0, 0);
					}
					else
						MessageBox(nullptr, _T("Input name & path to file"), _T("Info"), MB_OK | MB_ICONINFORMATION);
				}
				else
					MessageBox(nullptr, _T("Choose any file"), _T("Info"), MB_OK | MB_ICONINFORMATION);
			}
			//разархивировать
			else if (IsDlgButtonChecked(hDlg, ID_RBDISARCH))
			{
				index = ListView_GetNextItem(hListView_1, -1, LVIS_SELECTED);
				if (index != -1)
				{
					ListView_GetItemText(hListView_1, index, 0, temp, MAX_PATH);
					wcscpy(tempFilePath, dir);
					tempFilePath[wcslen(tempFilePath) - 1] = 0;
					wcscat(tempFilePath, temp);
					args[0] = tempFilePath;

					wcscpy(nameStr, L"");
					wcscpy(pathStr, L"");
					wcscpy(passwordStr, L"");
					GetWindowText(edit_name, nameStr, MAX_PATH);
					GetWindowText(edit_path, pathStr, MAX_PATH);
					GetWindowText(edit_password, passwordStr, MAX_PATH);
					if (nameStr != L"" && pathStr != L"")
					{
						getTypeOfFile(nameStr, temp);
						if (wcscmp(temp, _T("zip")) == 0)
						{
							nameStr[wcslen(nameStr) - 4] = 0;
							wcscpy(temp, pathStr);
							args[1] = temp;

							HZIP hz;
							if (passwordStr == L"")
							{
								hz = OpenZip(args[0], "");
							}
							else
							{
								char password[MAX_PATH];
								wcstombs(password, passwordStr, MAX_PATH + 1);
								hz = OpenZip(args[0], password);
							}

							SetUnzipBaseDir(hz, args[1]);
							ZIPENTRY ze;
							GetZipItem(hz, -1, &ze);
							int numitems = ze.index;
							for (int i = 0; i < numitems; i++)
							{
								GetZipItem(hz, i, &ze);
								UnzipItem(hz, i, ze.name);
							}
							CloseZip(hz);
							MessageBox(nullptr, _T("Done!"), _T(""), MB_OK | MB_ICONASTERISK);
							FindFile(hListView_1, dir);
							SendMessage(hDlg, WM_CLOSE, 0, 0);
						}
						else
							MessageBox(nullptr, _T("Input file must be .zip"), _T("Info"), MB_OK | MB_ICONINFORMATION);
					}
					else
						MessageBox(nullptr, _T("Input name & path to file"), _T("Info"), MB_OK | MB_ICONINFORMATION);
				}
				else
					MessageBox(nullptr, _T("Choose any file"), _T("Info"), MB_OK | MB_ICONINFORMATION);
			}
			else
				MessageBox(nullptr, _T("Choose method"), _T("Info"), MB_OK | MB_ICONINFORMATION);
		}
		break;
	}
	return FALSE;
}
