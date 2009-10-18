
#include "wsnes9x.h"
#include "wlanguage.h"
#include "../s9xlua.h"
#include "../language.h"

INT_PTR CALLBACK DlgLuaScriptDialog(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {

	static int *success;

	switch (msg) {
	case WM_INITDIALOG:
		{
		DragAcceptFiles(hDlg, true);

		SetDlgItemText(hDlg, IDC_LUA_FILENAME, S9xGetLuaScriptName());

		// Nothing very useful to do
		success = (int*)lParam;
		return true;
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case IDOK:
			{
				char filename[MAX_PATH];
				GetDlgItemText(hDlg, IDC_LUA_FILENAME, filename, MAX_PATH);
				if (S9xLoadLuaCode(filename)) {
					*success = 1;
					// For user's convenience, don't close dialog unless we're done.
					// Users who make syntax errors and fix/reload will thank us.
					EndDialog(hDlg, 1);
				} else {
					//MessageBox(hDlg, "Oops", "Script not loaded", MB_OK); // Errors are displayed by the Lua code.
					*success = 0;
				}
				return true;
			}
			case IDCANCEL:
			{
				EndDialog(hDlg, 0);
				return true;
			}
			case IDC_LUA_BROWSE:
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
					SetWindowText(GetDlgItem(hDlg, IDC_LUA_FILENAME), szFileName);
				}
				//SetCurrentDirectory(movieDirectory);
				return true;
			}
			case IDC_LUA_FILENAME:
			{
				char filename[MAX_PATH];
				GetDlgItemText(hDlg, IDC_LUA_FILENAME, filename, MAX_PATH);
				FILE* file = fopen(filename, "rb");
				EnableWindow(GetDlgItem(hDlg, IDOK), file != NULL);
				if(file)
					fclose(file);
				break;
			}
		}
		break;

	case WM_DROPFILES: {
		HDROP hDrop;
		//UINT fileNo;
		UINT fileCount;
		char filename[PATH_MAX];

		hDrop = (HDROP)wParam;
		fileCount = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
		if (fileCount > 0) {
			DragQueryFile(hDrop, 0, filename, COUNT(filename));
			SetWindowText(GetDlgItem(hDlg, IDC_LUA_FILENAME), filename);
		}
		DragFinish(hDrop);
		return true;
	 }
	}
//	char message[1024];
//	sprintf(message, "Unkonwn command %d,%d",msg,wParam);
	//MessageBox(hDlg, message, TEXT("Range Error"), MB_OK);

//	printf("Unknown entry %d,%d,%d\n",msg,wParam,lParam);
	// All else, fall off
	return false;

}
