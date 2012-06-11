#pragma once
#include <max.h>

class MaxtorNewRendParamDlg : public RendParamDlg
{
    void AcceptParams() {}
    void DeleteThis() { delete this; }

};

INT_PTR CALLBACK RendParamsDlgProc(
    HWND hwndDlg,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam);
