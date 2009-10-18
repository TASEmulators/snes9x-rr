
#include "wsnes9x.h"
#include "wlanguage.h"
#include "../s9xlua.h"
#include "../language.h"

HWND LuaConsoleHWnd = NULL;

void PrintToWindowConsole(int hDlgAsInt, const char* str)
{
	HWND hDlg = (HWND)hDlgAsInt;
	HWND hConsole = GetDlgItem(hDlg, IDC_LUACONSOLE);

	int length = GetWindowTextLength(hConsole);
	if(length >= 250000)
	{
		// discard first half of text if it's getting too long
		SendMessage(hConsole, EM_SETSEL, 0, length/2);
		SendMessage(hConsole, EM_REPLACESEL, false, (LPARAM)"");
		length = GetWindowTextLength(hConsole);
	}
	SendMessage(hConsole, EM_SETSEL, length, length);

	//LuaPerWindowInfo& info = LuaWindowInfo[hDlg];

	{
		SendMessage(hConsole, EM_REPLACESEL, false, (LPARAM)str);
	}
}

void WinLuaOnStart(int hDlgAsInt)
{
	HWND hDlg = (HWND)hDlgAsInt;
	//LuaPerWindowInfo& info = LuaWindowInfo[hDlg];
	//info.started = true;
	EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_LUABROWSE), false); // disable browse while running because it misbehaves if clicked in a frameadvance loop
	EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_LUASTOP), true);
	SetWindowText(GetDlgItem(hDlg, IDC_BUTTON_LUARUN), "Restart");
	SetWindowText(GetDlgItem(hDlg, IDC_LUACONSOLE), ""); // clear the console
//	Show_Genesis_Screen(HWnd); // otherwise we might never show the first thing the script draws
}

void WinLuaOnStop(int hDlgAsInt)
{
	HWND hDlg = (HWND)hDlgAsInt;
	//LuaPerWindowInfo& info = LuaWindowInfo[hDlg];

	HWND prevWindow = GetActiveWindow();
	SetActiveWindow(hDlg); // bring to front among other script/secondary windows, since a stopped script will have some message for the user that would be easier to miss otherwise
	if(prevWindow == GUI.hWnd) SetActiveWindow(prevWindow);

	//info.started = false;
	EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_LUABROWSE), true);
	EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_LUASTOP), false);
	SetWindowText(GetDlgItem(hDlg, IDC_BUTTON_LUARUN), "Run");
//	if(statusOK)
//		Show_Genesis_Screen(MainWindow->getHWnd()); // otherwise we might never show the last thing the script draws
	//if(info.closeOnStop)
	//	PostMessage(hDlg, WM_CLOSE, 0, 0);
}

INT_PTR CALLBACK DlgLuaScriptDialog(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	RECT r;
	RECT r2;
	int dx1, dy1, dx2, dy2;

	switch (msg) {

	case WM_INITDIALOG:
	{
		// disable resizing
		LONG wndStyle = GetWindowLong(hDlg, GWL_STYLE);
		wndStyle &= ~WS_THICKFRAME;
		SetWindowLong(hDlg, GWL_STYLE, wndStyle);

		// remove the 30000 character limit from the console control
		SendMessage(GetDlgItem(hDlg, IDC_LUACONSOLE),EM_LIMITTEXT,0,0);

		GetWindowRect(GUI.hWnd, &r);
		dx1 = (r.right - r.left) / 2;
		dy1 = (r.bottom - r.top) / 2;

		GetWindowRect(hDlg, &r2);
		dx2 = (r2.right - r2.left) / 2;
		dy2 = (r2.bottom - r2.top) / 2;

		int windowIndex = 0;//std::find(LuaScriptHWnds.begin(), LuaScriptHWnds.end(), hDlg) - LuaScriptHWnds.begin();
		int staggerOffset = windowIndex * 24;
		r.left += staggerOffset;
		r.right += staggerOffset;
		r.top += staggerOffset;
		r.bottom += staggerOffset;

		// push it away from the main window if we can
		const int width = (r.right-r.left); 
		const int width2 = (r2.right-r2.left); 
		if(r.left+width2 + width < GetSystemMetrics(SM_CXSCREEN))
		{
			r.right += width;
			r.left += width;
		}
		else if((int)r.left - (int)width2 > 0)
		{
			r.right -= width2;
			r.left -= width2;
		}

		SetWindowPos(hDlg, NULL, r.left, r.top, NULL, NULL, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);

		DragAcceptFiles(hDlg, true);
		SetDlgItemText(hDlg, IDC_EDIT_LUAPATH, S9xGetLuaScriptName());
		return true;
	}	break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case IDOK:
			case IDCANCEL: {
				EndDialog(hDlg, true); // goto case WM_CLOSE;
			}	break;

			case IDC_BUTTON_LUARUN:
			{
				char filename[MAX_PATH];
				GetDlgItemText(hDlg, IDC_EDIT_LUAPATH, filename, MAX_PATH);
				S9xLoadLuaCode(filename);
			}	break;

			case IDC_BUTTON_LUASTOP:
			{
				S9xLuaStop();
			}	break;

			case IDC_BUTTON_LUAEDIT:
			{
				char Str_Tmp [1024]; // shadow added because the global one is unreliable
				SendDlgItemMessage(hDlg,IDC_EDIT_LUAPATH,WM_GETTEXT,(WPARAM)512,(LPARAM)Str_Tmp);
				// tell the OS to open the file with its associated editor,
				// without blocking on it or leaving a command window open.
				ShellExecute(NULL, "open", Str_Tmp, NULL, NULL, SW_SHOWNORMAL);
			}	break;

			case IDC_BUTTON_LUABROWSE:
			{
				OPENFILENAME  ofn;
				char  szFileName[MAX_PATH];

				strcpy(szFileName, S9xGetFilenameRel("lua"));

				ZeroMemory( (LPVOID)&ofn, sizeof(OPENFILENAME) );
				ofn.lStructSize = sizeof(OPENFILENAME);
				ofn.hwndOwner = hDlg;
				ofn.lpstrFilter = "Lua Script (*.lua)" "\0*.lua\0" FILE_INFO_ANY_FILE_TYPE "\0*.*\0\0";
				ofn.lpstrFile = szFileName;
				ofn.lpstrDefExt = "lua";
				ofn.nMaxFile = MAX_PATH;
				ofn.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST; // hide previously-ignored read-only checkbox (the real read-only box is in the open-movie dialog itself)
				if(GetOpenFileName( &ofn ))
				{
					SetWindowText(GetDlgItem(hDlg, IDC_EDIT_LUAPATH), szFileName);
				}
				//SetCurrentDirectory(movieDirectory);
				return true;
			}	break;

			case IDC_EDIT_LUAPATH:
			{
				char filename[MAX_PATH];
				GetDlgItemText(hDlg, IDC_EDIT_LUAPATH, filename, MAX_PATH);
				FILE* file = fopen(filename, "rb");
				EnableWindow(GetDlgItem(hDlg, IDOK), file != NULL);
				if(file)
					fclose(file);
			}	break;
		}
		break;

	case WM_CLOSE: {
		S9xLuaStop();
		DragAcceptFiles(hDlg, FALSE);
		LuaConsoleHWnd = NULL;
	}	break;

	case WM_DROPFILES: {
		HDROP hDrop;
		//UINT fileNo;
		UINT fileCount;
		char filename[PATH_MAX];

		hDrop = (HDROP)wParam;
		fileCount = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
		if (fileCount > 0) {
			DragQueryFile(hDrop, 0, filename, COUNT(filename));
			SetWindowText(GetDlgItem(hDlg, IDC_EDIT_LUAPATH), filename);
		}
		DragFinish(hDrop);
		return true;
	}	break;

	}

	return false;

}
