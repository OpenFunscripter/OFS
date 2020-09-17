#include "OpenFunscripter.h"

int main(int argc, char* argv[])
{
	OpenFunscripter app;
	app.setup();
	int code = app.run();
	app.shutdown();
	return code;
}