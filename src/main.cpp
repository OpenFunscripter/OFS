#include "OpenFunscripter.h"
#include "SDL_main.h"

int main(int argc, char* argv[])
{
	OpenFunscripter app;
	if(app.setup(argc, argv)) {
		int code = app.run();
		app.shutdown();
		return code;
	}
	return -1;
}