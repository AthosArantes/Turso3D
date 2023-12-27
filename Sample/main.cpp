#include "Application.h"
#include <Turso3D/IO/Log.h>

#ifdef _DEBUG
#include <crtdbg.h>
#endif

int WinMain(int argc, char** argv)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	//_CrtSetBreakAlloc();
#endif

	Turso3D::Log::Initialize("turso3d.log", true);

	try {
		Application app {};
		if (app.Initialize()) {
			app.Run();
			return 0;
		}
	} catch (std::exception& e) {
		Turso3D::LOG_ERROR("[Exception] {:s}", e.what());
	}

	return 1;
}
