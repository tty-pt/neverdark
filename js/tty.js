function
params_push(tty, x)
{
        let     fg = tty.c_attr.fg,
                bg = tty.c_attr.bg;

	switch (x) {
	case 0: fg = 7; bg = 0; break;
	case 1: fg += 8; break;
	default: if (x >= 40)
			 bg = x - 40;
		 else if (x >= 30)
			 fg = (fg >= 8 ? 8 : 0) + x - 30;
	}

	tty.csi.fg = fg;
	tty.csi.bg = bg;
	tty.csi.x = x;
}

function csi_change(tty)
{
	const   a = tty.csi.fg != 7,
                b = tty.csi.bg != 0;
        tty.output += tty.end_tag;

	if (a || b) {
                tty.output += "<span class=\"";
		if (a)
                        tty.output += "cf" + tty.csi.fg;
		if (b)
                        tty.output += " c" + tty.csi.bg;

                tty.output += "\">";;
		tty.end_tag = "</span>";
	} else
		tty.end_tag = "";
}

function tty_init() {
        return {
                csi: {
                        fg: 7,
                        bg: 0,
                        x: 0,
                },
                c_attr: {
                        fg: 7,
                        bg: 0,
                        x: 0,
                },
                end_tag: "",
                esc_state: 0,
                output: "",
        };
}

function
esc_state_0(tty, ch) {
	// char *fout = out;

	if (tty.csi_changed) {
		csi_change(tty);
		tty.csi_changed = 0;
	}

	switch (ch) {
        // case '\n':
        //         tty.output += "\\n";
        //         return 2;
        // case '\t':
        //         tty.output += "\\t";
        //         return 2;
        case '<':
                tty.output += "&lt;";
                return 4;
        case '>':
                tty.output += "&gt;";
                return 4;
	case '"':
                tty.output += "\"";
                return 2;
	// case '\\':
	// case '/':
                // tty.output += "\\";
	}

        tty.output += ch;
	// *out++ = ch;
	// return out - fout;
        return 1;
}

function tty_proc_ch(tty, ch) {
	switch (ch) {
	case '\x18':
	case '\x1a':
		tty.esc_state = 0;
		return 0;
	case '\x1b':
		tty.esc_state = 1;
		return 0;
	case '\x9b':
		tty.esc_state = 2;
		return 0;
	case '\x07': 
	case '\x00':
	case '\x7f':
	case '\v':
	case '\r':
	case '\f':
		return 0;
	}

	switch (tty.esc_state) {
	case 0:
		return esc_state_0(tty, ch);
	case 1:
		switch (ch) {
		case '[':
			tty.esc_state = 2;
			break;
		case '=':
		case '>':
		case 'H':
			tty.esc_state = 0; /* IGNORED */
		}
		break;
	case 2: // just saw CSI
		switch (ch) {
		case 'K':
		case 'H':
		case 'J':
			tty.esc_state = 0;
			return 0;
		case '?':
			tty.esc_state = 5;
			return 0;
		}
		params_push(tty, 0);
		tty.esc_state = 3;
	case 3: // saw CSI and parameters
		switch (ch) {
		case 'm':
			if (tty.c_attr.bg != tty.csi.bg
			    || tty.c_attr.fg != tty.csi.fg)
			{
				tty.c_attr.fg = tty.csi.fg;
				tty.c_attr.bg = tty.csi.bg;
				tty.c_attr.x = 0;
				tty.csi.x = 0;
				tty.csi_changed = 1;
			}
			tty.esc_state = 0;
			break;
		case '[':
			tty.esc_state = 4;
			break;
		case ';':
			params_push(tty, 0);
			break;
		default:
			if (ch >= '0' && ch <= '9')
				params_push(tty, tty.csi.x * 10 + (ch - '0'));
			else
				tty.esc_state = 0;
		}
		break;

	case 5: params_push(tty, ch);
		tty.esc_state = 6;
		break;
	case 4:
	case 6: tty.esc_state = 0;
		break;
	}

	return 0;
}

function tty_proc(input) {
        let tty = tty_init();
        let in_i;

        for (in_i = 0; in_i < input.length; in_i++)
		tty_proc_ch(tty, input.charAt(in_i));

        return tty.output;
}

export default tty_proc;
