#include "Application.h"
#include <Turso3D/IO/Log.h>

int WinMain(int argc, char** argv)
{
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
