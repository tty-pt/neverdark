#include "view.h"
#include "metal.h"

static char view_buf[128];
/* static view_t view; */

MEXPORT char *
view_input() {
	return view_buf;
}

MEXPORT int
view_build() {
	return 0;
}
