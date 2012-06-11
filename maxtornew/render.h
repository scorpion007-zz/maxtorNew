#pragma once
#include <max.h>
#include "common.h"

class MaxtorNewRenderer : public Renderer
{
public:
    RefResult NotifyRefChanged(Interval,RefTargetHandle,PartID &,RefMessage)
    {
        return REF_SUCCEED;
    }

    int Open(INode *,INode *,ViewParams *,RendParams &,HWND,DefaultLight *,int,RendProgressCallback *);
    int Render(TimeValue,Bitmap *,FrameRendParams &,HWND,RendProgressCallback *,ViewParams *);
    void Close(HWND,RendProgressCallback *)
    {}
    RendParamDlg *CreateParamDialog(IRendParams *,BOOL);
    void ResetParams(void){}

    // This is called to populate the string in the currently-selected renderer
    // text box, and the main window caption.
    //
    void GetClassName (MSTR& s)
    {
        s = PRODUCT_NAME;
    }

};