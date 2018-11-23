#include <application.h>
#include <game.h>
#include <demo2.h>
#include <helpers.h>
#include <defines.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Shlwapi.h>d

#include <DXGIDebug.h>

void ReportLiveObjects()
{
	IDXGIDebug1* dxgiDebug;
	DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug));

	dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_IGNORE_INTERNAL);
	dxgiDebug->Release();
}

int CALLBACK wWinMain(HINSTANCE arg_h_instance, HINSTANCE h_prev_instance, PWSTR lp_cmd_line, int n_cmd_show)
{
	
	int ret_code = 0;

	// Set the working directory to the path of the executable.
	WCHAR path[MAX_PATH];
	HMODULE hModule = GetModuleHandleW(NULL);
	if (GetModuleFileNameW(hModule, path, MAX_PATH) > 0)
	{
		PathRemoveFileSpecW(path);
		SetCurrentDirectoryW(path);
	}

	// Application constructor
	Application app(arg_h_instance);
	{
		std::shared_ptr<Demo2> demo = std::make_shared<Demo2>(app, L"DirectX 12 Demo", 1000, 500);

		ret_code = app.Run(demo);
	}

	atexit(&ReportLiveObjects);

	return ret_code;
}