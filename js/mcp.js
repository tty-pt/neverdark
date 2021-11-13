#define GET_FLAG(x)	( mcp.flags & x )
#define SET_FLAGS(x)	{ mcp.flags |= x ; }
#define UNSET_FLAGS(x)	( mcp.flags &= ~(x) )

let out_buf = "";

String.prototype.replaceAt = function(index, replacement) {
        return this.substr(0, index) + replacement
                + this.substr(index + replacement.length);
}

const MCP_ON = 1;
const MCP_KEY = 2;
const MCP_MULTI = 4;
const MCP_ARG = 8;
const MCP_NOECHO = 16;
const MCP_SKIP = 32;
const MCP_INBAND = 64;
const MCP_NAME = 128;
const MCP_HASH = 256;

const MCP_CONFIRMED = 4;

let mcp = {
        state: 1,
        args: [],
        name: "",
        cache: "",
        args_l: 0,
        cache_i: 0,
        flags: 0,

}, mcp_arr;

function mcp_clear() {
        mcp.args = [];
        mcp.name = mcp.cache = "";
        mcp.args_l = 0;
        mcp_cache_i = 0;
        mcp.flags = 0;
        mcp.state = 1;
}

function mcp_emit() {
        mcp_arr.push(mcp.args.reduce((a, i) => Object.assign(a, {
                [i.key]: i.value
        }), { key: mcp.name }));
        mcp_clear();
}

function inband_emit() {
        mcp_arr.push({
                key: "inband",
                data: mcp.cache,
        });
	mcp_clear();
}

function mcp_proc_ch(p) {
	if (GET_FLAG(MCP_SKIP)) {
		if (p != '\n')
			return;
		mcp.flags ^= MCP_SKIP;
		mcp.state = 1;
		return;
	}

	switch (p) {
	case '#':
		switch (mcp.state) {
		case 3:
		case 1:
			mcp.state++;
			return;
		}
		break;
	case '$':
		if (mcp.state == 2) {
			mcp.state++;
			return;
		}
		break;
	case '*':
		if (mcp.state == MCP_CONFIRMED) {
			// only assert MULTI
			mcp.flags = MCP_MULTI | MCP_NOECHO;
			mcp.state = 0;
			return;
		}

		if (!GET_FLAG(MCP_ON) || !GET_FLAG(MCP_KEY))
			break;

		SET_FLAGS(MCP_MULTI | MCP_SKIP);
		// mcp_set(mcp_arg()->key);
                const arg = {
                        key: mcp.cache,
                        value: null,
                }
                mcp.args.push(arg);
                mcp.args_l ++;
                mcp.cache = "";
                mcp.cache_i = 0;
		return;

	case ':':
		if (GET_FLAG(MCP_MULTI)) {
			if (mcp.state == MCP_CONFIRMED) {
				mcp.state = 0;
				// mcp_set(mcp_arg()->value);
                                mcp.args[mcp.args_l - 1].value = mcp.cache;
                                mcp.cache = "";
                                mcp.cache_i = 0;
				mcp.args_l ++;
				mcp_emit();
				mcp.flags = MCP_SKIP;
			} else
				mcp.flags &= ~MCP_NOECHO;
			return;
		} else if (GET_FLAG(MCP_KEY)) {
			// mcp_set(mcp_arg()->key);
                        const arg = {
                                key: mcp.cache,
                                value: null,
                        }
                        mcp.args.push(arg);
                        mcp.args_l ++;
                        mcp.cache = "";
                        mcp.cache_i = 0;
                        // console.log("key??", mcp);
			return;
		}
		break;

	case '"':
		if (!GET_FLAG(MCP_ON))
			break;

		mcp.flags ^= MCP_KEY;
		if (GET_FLAG(MCP_KEY)) {
                        // console.log("value??", mcp);
			// mcp_set(mcp_arg()->value);
                        mcp.args[mcp.args_l - 1].value = mcp.cache;
                        mcp.cache = "";
                        mcp.cache_i = 0;
		}
		return;

	case ' ':
		if (!GET_FLAG(MCP_ON))
			break;

		else if (GET_FLAG(MCP_NAME)) {
			mcp.flags ^= MCP_NAME | MCP_KEY;
			// mcp_set(mcp.name);
                        mcp.name = mcp.cache;
                        mcp.cache = "";
                        mcp.cache_i = 0;
                        // console.log("got name", mcp);
			if (mcp.name)
				mcp.args_l = 0;
			else
				mcp.flags = MCP_SKIP;
			return;

		} else if (GET_FLAG(MCP_HASH)) {
			mcp.flags ^= MCP_HASH | MCP_KEY | MCP_NOECHO;
                        // console.log("hash");
			return;
		} else if (GET_FLAG(MCP_KEY))
			return;
		break;
	case '\n':
		mcp.state = 1;
		if (GET_FLAG(MCP_MULTI)) {
                        mcp.cache += '\n';
                        mcp.cache_i++;
                }
		return;
        }

	if (mcp.state == MCP_CONFIRMED) {
		// new mcp
		if (mcp.name)
			mcp_emit();
		else if (GET_FLAG(MCP_INBAND))
			inband_emit();
		mcp.flags = MCP_ON | MCP_NAME;
		mcp.state = 0;
	} else if (mcp.state) {
		// mcp turned out impossible
		if (GET_FLAG(MCP_ON))
			mcp_emit();

		if (!GET_FLAG(MCP_MULTI))
			mcp.flags = MCP_INBAND;

		// strncpy(mcp.cache_p, "\n#$#", mcp.state);
                mcp.cache += "\n#$#".substr(0, mcp.state);
		mcp.cache_i += mcp.state;
		mcp.state = 0;
	}

	if (!GET_FLAG(MCP_NOECHO)) {
                mcp.cache += p;
                mcp.cache_i++;
	}
        // alert(mcp.cache);
}

function mcp_proc(data) {
        let in_i;
        mcp_arr = [];

        for (in_i = 0; in_i < data.length; in_i++) {
                mcp_proc_ch(data.charAt(in_i));
        }

	if (GET_FLAG(MCP_MULTI))
		;
	else if (GET_FLAG(MCP_ON))
		mcp_emit();
	else if (GET_FLAG(MCP_INBAND))
		inband_emit();

	return mcp_arr;
}

function mcp_reset() {
        last_i = out_i = 0;
        // out_buf += "[";
        out_i ++;
}

function mcp_init() {
        mcp.state = 1;
        mcp_reset();
        mcp_clear();
}