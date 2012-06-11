#include <max.h>
#include "../common/common.h"

static HINSTANCE s_hInstance = NULL;
ClassDesc *GetMaxtorNewClassDesc();


BOOL WINAPI 
DllMain(
	HINSTANCE hinstDLL,
	ULONG fdwReason,
	LPVOID /*lpvReserved*/)
{
    if( fdwReason == DLL_PROCESS_ATTACH )
    {
        // Hang on to this DLL's instance handle.
        //
        s_hInstance = hinstDLL;

		// Don't call us with THREAD_ATTACH et al.
		//
        DisableThreadLibraryCalls(s_hInstance);
    }
    return TRUE;
}

DLLEXPORT const WCHAR*
LibDescription()
{
    return L"MaxToR New";
}

DLLEXPORT int 
LibNumberClasses()
{
    return 1;
}

DLLEXPORT ClassDesc*
LibClassDesc(int i)
{
    assert(i == 0);
    return GetMaxtorNewClassDesc();
}

DLLEXPORT ULONG 
LibVersion()
{
    return Get3DSMAXVersion();
}

DLLEXPORT BOOL 
LibInitialize()
{
    return TRUE;
}

DLLEXPORT BOOL 
LibShutdown()
{
    return TRUE;
}

HINSTANCE GetMaxtorNewHinstance()
{
    return s_hInstance;
}