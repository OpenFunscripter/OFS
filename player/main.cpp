#include "OFP.h"

int main(int argc, char* argv[])
{
	OFP app;
	app.setup();
	int code = app.run();
	app.shutdown();
	return code;
}