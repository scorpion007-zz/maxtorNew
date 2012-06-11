#include <max.h>
#include <iparamb2.h>

#include "render.h"

static const Class_ID maxtorNewClsId(0x248f4de7, 0x61f26248);
class MaxtorNewRenderer;

// Class descriptor for our plugin.
//
class MaxtorNewClassDesc : public ClassDesc2 {
public:
    BOOL IsPublic() {
        return TRUE;
    }

    // Factory method. Create an instance of our plugin.
    //
    void *Create(BOOL /* loading */) {
        return new MaxtorNewRenderer;
    }

    const TCHAR *ClassName() {
        // This string shows up in the renderer selection dialog. Make it user-
        // friendly.
        //
        return _T("MaxtorNew");
    }
    SClass_ID SuperClassID() {
        return RENDERER_CLASS_ID;
    }
    Class_ID ClassID() {
        return maxtorNewClsId;
    }
    const TCHAR *Category() {
        return _T("");
    }
};

static MaxtorNewClassDesc maxtorNewClsDesc;

// Returns a singleton instance of our class descriptor.
ClassDesc *GetMaxtorNewClassDesc() {
    return &maxtorNewClsDesc;
}