/* main file for PWM */
#include <stdio.h>
#include <stdlib.h>
#include "wm.h"
#include <X11/Xlib.h>

int main() {

	/* create wm */
	wm w;
	wmInit(&w);
	
	/* run wm */
	wmRun(&w);
	
	/* close wm */
	wmClose(&w);
	
	return 0;
}
