#include "App.h"

int main(int argc, char* argv[])
{
	App::CreateInstance();
	INIT_OPT();

	LOG_FATAL_IF(!App::GetInstance()->Init(argv[0]), "app init error!!!");
	App::GetInstance()->Start();
	App::DestroyInstance();

	return 0;
}
