#include "OpenFunscripter.h"
#include "SDL_main.h"

#include "state/OpenFunscripterState.h"
#include "state/OFS_LibState.h"

int main(int argc, char* argv[])
{
	OFS_LibState::RegisterAll();
	OpenFunscripterState::RegisterAll();
	
	OpenFunscripter app;
	if(app.setup(argc, argv)) {
		int code = app.run();
		app.shutdown();
		return code;
	}
	return -1;
}