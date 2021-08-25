#include "OpenFunscripter.h"

int main(int argc, char* argv[])
{
#ifdef WIN32
	if(!SDL_HasAVX()) {
		Util::MessageBoxAlert("Missing AVX support", "CPU doesn't support AVX.");
		SDL_Delay(3000);
		return -1;
	}
#endif
	OpenFunscripter app;
	app.setup(argc, argv);
	int code = app.run();
	app.shutdown();
	return code;
}