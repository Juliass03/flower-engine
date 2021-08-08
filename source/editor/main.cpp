#include "editor.h"

int main()
{
	Editor editor;
	editor.init();
	editor.loop();
	editor.release();
	return 0;
}