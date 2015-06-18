// Gateway BWL4 Plugin (called icctv so it will work with all gateways)
// BOOL is a 32Bit Value
// All BWL4 functions are C-CallingConvention(cdecl) not stdcall

#define BWLAPI 4
#define STARCRAFTBUILD -1

/*  STARCRAFTBUILD
   -1   All
    0   1.04
    1   1.08b
    2   1.09b
    3   1.10
    4   1.11b
    5   1.12b
    6   1.13f
    7   1.14
    8   1.15
    9    1.15.1
    10    1.15.2
    11    1.15.3
    12    1.16.0
    13    1.16.1
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

// Global vars
char dll_path[255];
DWORD* iatAddr = (DWORD*)0x1504501C;
DWORD oldProc = 0;
DWORD oldProtect = 0;

HKEY hKey = 0;
DWORD bufferSize = 0;
char* serverListBuffer = 0;

// function pointer (I think rather useless now)
typedef LONG WINAPI (*PRegSetValueExA)(HKEY, LPCTSTR, DWORD, DWORD, const BYTE*, DWORD); // function pointer to the original storm.dll function
PRegSetValueExA orig_RegSetValueExA;

// prototpe
__declspec(dllexport) int my_RegSetValueExA(HKEY hKey, LPCTSTR lpValueName, DWORD Reserved, DWORD dwType, const BYTE* lpData, DWORD cbData);

struct ExchangeData {
	int iPluginAPI;
	int iStarCraftBuild;
	BOOL bNotSCBWmodule;                //Inform user that closing BWL will shut down your plugin
	BOOL bConfigDialog;                 //Is Configurable
};

extern "C" BOOL APIENTRY DllMain( HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
	//Is this DLL also StarCraft module?

	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
            GetModuleFileName(hModule, dll_path, 255);

            // Get the current server list
            RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Battle.net\\Configuration", 0, KEY_ALL_ACCESS, &hKey);
            RegQueryValueEx(hKey, "Battle.net gateways", NULL, NULL, NULL, &bufferSize);
            serverListBuffer = (char*)malloc(bufferSize);
            RegQueryValueExA(hKey, "Battle.net gateways", NULL, NULL, (BYTE*)serverListBuffer, &bufferSize);

            // attached to CL or SCBW?
            if (!FindWindow("SWarClass", 0)) return TRUE;

            // Patch the StarCraft.exe IAT
            VirtualProtect(iatAddr, 4, PAGE_EXECUTE_READWRITE, &oldProtect);
            orig_RegSetValueExA = (PRegSetValueExA)*iatAddr; // save the old pointer
            oldProc = *iatAddr;
            *iatAddr = (int)&my_RegSetValueExA;           // set the new address

			return TRUE;
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
			break;
	}

	return TRUE;
}

//
// GetPluginAPI and GetData get called during the startup of the Launcher and gather information about the plugin
//
extern "C" __declspec(dllexport) void GetPluginAPI(ExchangeData &Data)
{
	// BWL Gets version from Resource - VersionInfo
	Data.iPluginAPI = BWLAPI;
	Data.iStarCraftBuild = STARCRAFTBUILD;
	Data.bConfigDialog = false;
	Data.bNotSCBWmodule = false;
}

extern "C" __declspec(dllexport) void GetData(char *name, char *description, char *updateurl)
{
	// if necessary you can add Initialize function here
	// possibly check CurrentCulture (CultureInfo) to localize your DLL due to system settings

	strcpy(name,      "Server List Protector");
	strcpy(description, "Author WhuazGoodNjaggah\r\n\r\nThis plugin prevents the servers from overwriting the list.");
	strcpy(updateurl,   "http://www.bwprogrammers.com/files/update/bwl4/plugin/");
}

//
// Called when the user clicks on Config
//
extern "C" __declspec(dllexport) BOOL OpenConfig()
{
	// If you set "Data.bConfigDialog = true;" at function GetPluginAPI then
	// BWLauncher will call this function if user clicks Config button
	system("start regedit.exe");
	return true; //everything OK
}

//
// ApplyPatchSuspended and ApplyPatch get
// called during the startup of Starcraft in the Launcher process
// the hProcess passed to them is shared between all plugins, so don't close it.
// Best practice is duplicating(DuplicateHandle from Win32 API) it if you want to use if after these function returns
//
extern "C" __declspec(dllexport) bool ApplyPatchSuspended(HANDLE hProcess, DWORD dwProcessID)
{
	// This function is called in the Launcher process while Starcraft is still suspended
	// Durning the suspended process some modules of starcraft.exe may not yet exist.
	return true; //everything OK
}

extern "C" __declspec(dllexport) bool ApplyPatch(HANDLE hProcess, DWORD dwProcessID)
{
	// This function is called in the Launcher process after the Starcraft window has been created
    void* pDllPath = VirtualAllocEx(hProcess, 0, 255, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (pDllPath == NULL) return FALSE;

    DWORD ret = WriteProcessMemory( hProcess, pDllPath, (void*)dll_path, 254, NULL );
    if (ret == 0) return FALSE;

    LPTHREAD_START_ROUTINE loadLibAddr = (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandle("Kernel32"), "LoadLibraryA");
    HANDLE hThread = CreateRemoteThread( hProcess, NULL, 0, loadLibAddr, pDllPath, 0, NULL );

    WaitForSingleObject( hThread, INFINITE );

	return true; //everything OK
}

__declspec(dllexport) int my_RegSetValueExA(HKEY hKey, LPCTSTR lpValueName, DWORD Reserved, DWORD dwType, const BYTE* lpData, DWORD cbData) {
    // Only accept changes if the buffersize does not change
    // this allows to choose different servers, but may fuck up
    if (strcmp(lpValueName, "Battle.net gateways") == 0 && (cbData != (DWORD)bufferSize)) {
        return 1;
    }
    // reset the hook to prevent crash on shutdown
    if (strcmp(lpValueName, "CPUThrottle") == 0) {
        *iatAddr = oldProc;
    }

    return orig_RegSetValueExA(hKey, lpValueName, Reserved, dwType, lpData, cbData);
}
