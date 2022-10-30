#include "OpenFunscripter.h"
#include "SDL_main.h"

int main(int argc, char* argv[])
{
	OpenFunscripter app;
	if(app.Init(argc, argv)) {
		int code = app.Run();
		app.Shutdown();
		return code;
	}
	return -1;
}