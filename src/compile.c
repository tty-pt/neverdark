/*
 *  Compile.c   (This is really a tokenizer, not a compiler)
 */

#include "copyright.h"
#include "config.h"

#include "db.h"
#include "props.h"
#include "interface.h"
#include "inst.h"
#include "externs.h"
#include "params.h"
#include "tune.h"
#include "match.h"
#include "interp.h"
#include <ctype.h>
#include <time.h>

/* This file contains code for doing "byte-compilation" of
   mud-forth programs.  As such, it contains many internal
   data structures and other such which are not found in other
   parts of TinyMUCK.                                       */

/* The CONTROL_STACK is a stack for holding previous control statements.
   This is used to resolve forward references for IF/THEN and loops, as well
   as a placeholder for back references for loops. */

#define CTYPE_IF    1
#define CTYPE_ELSE  2
#define CTYPE_BEGIN 3
#define CTYPE_FOR   4			/* Get it?  CTYPE_FOUR!!  HAHAHAHAHA  -Fre'ta */
								/* C-4?  *BOOM!*  -Revar */
#define CTYPE_WHILE 5
#define CTYPE_TRY   6			/* reserved for exception handling */
#define CTYPE_CATCH 7			/* reserved for exception handling */


/* These would be constants, but their value isn't known until runtime. */
static int IN_FORITER;
static int IN_FOREACH;
static int IN_FORPOP;
static int IN_FOR;
static int IN_TRYPOP;


static hash_tab primitive_list[COMP_HASH_SIZE];

struct CONTROL_STACK {
	short type;
	struct INTERMEDIATE *place;
	struct CONTROL_STACK *next;
	struct CONTROL_STACK *extra;
};

/* This structure is an association list that contains both a procedure
   name and the place in the code that it belongs.  A lookup to the procedure
   will see both it's name and it's number and so we can generate a
   reference to it.  Since I want to disallow co-recursion,  I will not allow
   forward referencing.
   */

struct PROC_LIST {
	const char *name;
	int returntype;
	struct INTERMEDIATE *code;
	struct PROC_LIST *next;
};

/* The intermediate code is code generated as a linked list
   when there is no hint or notion of how much code there
   will be, and to help resolve all references.
   There is always a pointer to the current word that is
   being compiled kept.
   */

struct INTERMEDIATE {
	int no;						/* which number instruction this is */
	struct inst in;				/* instruction itself */
	short line;					/* line number of instruction */
	struct INTERMEDIATE *next;	/* next instruction */
};


/* The state structure for a compile. */
typedef struct COMPILE_STATE_T {
	struct CONTROL_STACK *control_stack;
	struct PROC_LIST *procs;

	int nowords;				/* number of words compiled */
	struct INTERMEDIATE *curr_word;	/* word being compiled */
	struct INTERMEDIATE *first_word;	/* first word of the list */
	struct INTERMEDIATE *curr_proc;	/* first word of curr. proc. */
	struct publics *currpubs;
	int nested_fors;
	int nested_trys;

	/* Address resolution data.  Used to relink addresses after compile. */
	struct INTERMEDIATE **addrlist; /* list of addresses to resolve */
	int *addroffsets;               /* list of offsets from instrs */
	int addrmax;                    /* size of current addrlist array */
	int addrcount;                  /* number of allocated addresses */

	/* variable names.  The index into cstat->variables give you what position
	 * the variable holds.
	 */
	const char *variables[MAX_VAR];
	int variabletypes[MAX_VAR];
	const char *localvars[MAX_VAR];
	int localvartypes[MAX_VAR];
	const char *scopedvars[MAX_VAR];
	int scopedvartypes[MAX_VAR];

	struct line *curr_line;		/* current line */
	int lineno;					/* current line number */
	const char *next_char;		/* next char * */
	dbref player, program;		/* player and program for this compile */

	int compile_err;			/* 1 if error occured */

	char *line_copy;
	int macrosubs;				/* Safeguard for macro-subst. infinite loops */
	int descr;					/* the descriptor that initiated compiling */
	int force_err_display;		/* If true, always show compiler errors. */
	struct INTERMEDIATE *nextinst;
	hash_tab defhash[DEFHASHSIZE];
} COMPSTATE;


int primitive(const char *s);	/* returns primitive_number if

								 * primitive */
void free_prog(dbref);
const char *next_token(COMPSTATE *);
const char *next_token_raw(COMPSTATE *);
struct INTERMEDIATE *next_word(COMPSTATE *, const char *);
struct INTERMEDIATE *process_special(COMPSTATE *, const char *);
struct INTERMEDIATE *primitive_word(COMPSTATE *, const char *);
struct INTERMEDIATE *string_word(COMPSTATE *, const char *);
struct INTERMEDIATE *number_word(COMPSTATE *, const char *);
struct INTERMEDIATE *float_word(COMPSTATE *, const char *);
struct INTERMEDIATE *object_word(COMPSTATE *, const char *);
struct INTERMEDIATE *quoted_word(COMPSTATE *, const char *);
struct INTERMEDIATE *call_word(COMPSTATE *, const char *);
struct INTERMEDIATE *var_word(COMPSTATE *, const char *);
struct INTERMEDIATE *lvar_word(COMPSTATE *, const char *);
struct INTERMEDIATE *svar_word(COMPSTATE *, const char *);
const char *do_string(COMPSTATE *);
void do_comment(COMPSTATE *);
void do_directive(COMPSTATE *, char *direct);
struct prog_addr *alloc_addr(COMPSTATE *, int, struct inst *);
struct INTERMEDIATE *prealloc_inst(COMPSTATE * cstat);
struct INTERMEDIATE *new_inst(COMPSTATE *);
void cleanpubs(struct publics *mypub);
void clean_mcpbinds(struct mcp_binding *mcpbinds);
void cleanup(COMPSTATE *);
void add_proc(COMPSTATE *, const char *, struct INTERMEDIATE *, int rettype);
void add_control_structure(COMPSTATE *, int typ, struct INTERMEDIATE *);
void add_loop_exit(COMPSTATE *, struct INTERMEDIATE *);
int in_loop(COMPSTATE * cstat);
int innermost_control_type(COMPSTATE * cstat);
int count_trys_inside_loop(COMPSTATE* cstat);
struct INTERMEDIATE *locate_control_structure(COMPSTATE* cstat, int type1, int type2);
struct INTERMEDIATE *innermost_control_place(COMPSTATE * cstat, int type1);
struct INTERMEDIATE *pop_control_structure(COMPSTATE * cstat, int type1, int type2);
struct INTERMEDIATE *pop_loop_exit(COMPSTATE *);
void resolve_loop_addrs(COMPSTATE *, int where);
int add_variable(COMPSTATE *, const char *, int valtype);
int add_localvar(COMPSTATE *, const char *, int valtype);
int add_scopedvar(COMPSTATE *, const char *, int valtype);
int special(const char *);
int call(COMPSTATE *, const char *);
int quoted(COMPSTATE *, const char *);
int object(const char *);
int string(const char *);
int variable(COMPSTATE *, const char *);
int localvar(COMPSTATE *, const char *);
int scopedvar(COMPSTATE *, const char *);
void copy_program(COMPSTATE *);
void set_start(COMPSTATE *);

/* Character defines */
#define BEGINCOMMENT '('
#define ENDCOMMENT ')'
#define BEGINSTRING '"'
#define ENDSTRING '"'
#define BEGINMACRO '.'
#define BEGINDIRECTIVE '$'
#define BEGINESCAPE '\\'

#define SUBSTITUTIONS 20		/* How many nested macros will we allow? */

void
do_abort_compile(COMPSTATE * cstat, const char *c)
{
	static char _buf[BUFFER_LEN];

	sprintf(_buf, "Error in line %d: %s", cstat->lineno, c);
	if (cstat->line_copy) {
		free((void *) cstat->line_copy);
		cstat->line_copy = NULL;
	}
	if (((FLAGS(cstat->player) & INTERACTIVE) && !(FLAGS(cstat->player) & READMODE)) ||
		cstat->force_err_display) {
		notify_nolisten(cstat->player, _buf, 1);
	} else {
		log_muf("MUF compiler warning in program %d:\n%s\n", cstat->program, _buf);
	}
	cstat->compile_err++;
	if (cstat->compile_err > 1) {
		return;
	}
	if (cstat->nextinst) {
		struct INTERMEDIATE* ptr;
		while (cstat->nextinst)
		{
			ptr = cstat->nextinst;
			cstat->nextinst = ptr->next;
			free(ptr);
		}
		cstat->nextinst = NULL;
	}
	cleanup(cstat);
	cleanpubs(cstat->currpubs);
	cstat->currpubs = NULL;
	free_prog(cstat->program);
	cleanpubs(PROGRAM_PUBS(cstat->program));
	PROGRAM_SET_PUBS(cstat->program, NULL);
	clean_mcpbinds(PROGRAM_MCPBINDS(cstat->program));
	PROGRAM_SET_MCPBINDS(cstat->program, NULL);
	PROGRAM_SET_PROFTIME(cstat->program, 0, 0);
}

/* abort compile macro */
#define abort_compile(ST,C) { do_abort_compile(ST,C); return 0; }

/* abort compile for void functions */
#define v_abort_compile(ST,C) { do_abort_compile(ST,C); return; }


/*****************************************************************/


#define ADDRLIST_ALLOC_CHUNK_SIZE 256

int
get_address(COMPSTATE* cstat, struct INTERMEDIATE* dest, int offset)
{
	int i;

	if (!cstat->addrlist)
	{
		cstat->addrcount = 0;
		cstat->addrmax = ADDRLIST_ALLOC_CHUNK_SIZE;
		cstat->addrlist = (struct INTERMEDIATE**)
			malloc(cstat->addrmax * sizeof(struct INTERMEDIATE*));
		cstat->addroffsets = (int*)
			malloc(cstat->addrmax * sizeof(int));
	}

	for (i = 0; i < cstat->addrcount; i++)
		if (cstat->addrlist[i] == dest && cstat->addroffsets[i] == offset)
			return i;

    if (cstat->addrcount >= cstat->addrmax)
	{
		cstat->addrmax += ADDRLIST_ALLOC_CHUNK_SIZE;
		cstat->addrlist = (struct INTERMEDIATE**)
			realloc(cstat->addrlist, cstat->addrmax * sizeof(struct INTERMEDIATE*));
		cstat->addroffsets = (int*)
			realloc(cstat->addroffsets, cstat->addrmax * sizeof(int));
	}

	cstat->addrlist[cstat->addrcount] = dest;
	cstat->addroffsets[cstat->addrcount] = offset;
	return cstat->addrcount++;
}


void
fix_addresses(COMPSTATE* cstat)
{
	struct INTERMEDIATE* ptr;
	struct publics* pub;
	int count = 0;

	/* renumber the instruction chain */
	for (ptr = cstat->first_word; ptr; ptr = ptr->next)
		ptr->no = count++;

	/* repoint publics to targets */
	for (pub = cstat->currpubs; pub; pub = pub->next)
		pub->addr.no = cstat->addrlist[pub->addr.no]->no +
			cstat->addroffsets[pub->addr.no];

	/* repoint addresses to targets */
	for (ptr = cstat->first_word; ptr; ptr = ptr->next)
	{
		switch (ptr->in.type) {
		case PROG_ADD:
		case PROG_IF:
		case PROG_TRY:
		case PROG_JMP:
		case PROG_EXEC:
			ptr->in.data.number = cstat->addrlist[ptr->in.data.number]->no +
				cstat->addroffsets[ptr->in.data.number];
			break;
		default:
			break;
		}
	}
}


void
free_addresses(COMPSTATE* cstat)
{
	cstat->addrcount = 0;
	cstat->addrmax = 0;
	if (cstat->addrlist)
		free(cstat->addrlist);
	if (cstat->addroffsets)
		free(cstat->addroffsets);
	cstat->addrlist = NULL;
}


/*****************************************************************/


void
fixpubs(struct publics *mypubs, struct inst *offset)
{
	while (mypubs) {
		mypubs->addr.ptr = offset + mypubs->addr.no;
		mypubs = mypubs->next;
	}
}


int
size_pubs(struct publics *mypubs)
{
	int bytes = 0;

	while (mypubs) {
		bytes += sizeof(*mypubs);
		mypubs = mypubs->next;
	}
	return bytes;
}



char *
expand_def(COMPSTATE * cstat, const char *defname)
{
	hash_data *exp = find_hash(defname, cstat->defhash, DEFHASHSIZE);

	if (!exp) {
		if (*defname == BEGINMACRO) {
			return (macro_expansion(macrotop, &defname[1]));
		} else {
			return (NULL);
		}
	}
	return (string_dup((char *) exp->pval));
}


void
kill_def(COMPSTATE * cstat, const char *defname)
{
	hash_data *exp = find_hash(defname, cstat->defhash, DEFHASHSIZE);

	if (exp) {
		free(exp->pval);
		(void) free_hash(defname, cstat->defhash, DEFHASHSIZE);
	}
}


void
insert_def(COMPSTATE * cstat, const char *defname, const char *deff)
{
	hash_data hd;

	(void) kill_def(cstat, defname);
	hd.pval = (void *) string_dup(deff);
	(void) add_hash(defname, hd, cstat->defhash, DEFHASHSIZE);
}


void
insert_intdef(COMPSTATE * cstat, const char *defname, int deff)
{
	char buf[sizeof(int) * 3];

	sprintf(buf, "%d", deff);
	insert_def(cstat, defname, buf);
}


void
purge_defs(COMPSTATE * cstat)
{
	kill_hash(cstat->defhash, DEFHASHSIZE, 1);
}


void
include_defs(COMPSTATE * cstat, dbref i)
{
	char dirname[BUFFER_LEN];
	char temp[BUFFER_LEN];
	const char *tmpptr;
	PropPtr j, pptr;

	strcpy(dirname, "/_defs/");
	j = first_prop(i, dirname, &pptr, temp);
	while (j) {
		strcpy(dirname, "/_defs/");
		strcat(dirname, temp);
		tmpptr = uncompress(get_property_class(i, dirname));
		if (tmpptr && *tmpptr)
			insert_def(cstat, temp, (char *) tmpptr);
		j = next_prop(pptr, j, temp);
	}
}


void
init_defs(COMPSTATE * cstat)
{
	/* initialize hash table */
	int i;

	for (i = 0; i < DEFHASHSIZE; i++) {
		cstat->defhash[i] = NULL;
	}

	/* Create standard server defines */
	insert_def(cstat, "__version", VERSION);
	insert_def(cstat, "__muckname", tp_muckname);
	insert_intdef(cstat, "__fuzzball__", 1);
	insert_def(cstat, "strip", "striplead striptail");
	insert_def(cstat, "instring", "tolower swap tolower swap instr");
	insert_def(cstat, "rinstring", "tolower swap tolower swap rinstr");
	insert_intdef(cstat, "bg_mode", BACKGROUND);
	insert_intdef(cstat, "fg_mode", FOREGROUND);
	insert_intdef(cstat, "pr_mode", PREEMPT);
	insert_intdef(cstat, "max_variable_count", MAX_VAR);
	insert_intdef(cstat, "sorttype_case_ascend", SORTTYPE_CASE_ASCEND);
	insert_intdef(cstat, "sorttype_nocase_ascend", SORTTYPE_NOCASE_ASCEND);
	insert_intdef(cstat, "sorttype_case_descend", SORTTYPE_CASE_DESCEND);
	insert_intdef(cstat, "sorttype_nocase_descend", SORTTYPE_NOCASE_DESCEND);
	insert_intdef(cstat, "sorttype_shuffle", SORTTYPE_SHUFFLE);

	/* Make defines for compatability to removed primitives */
	insert_def(cstat, "desc", "\"_/de\" getpropstr");
	insert_def(cstat, "succ", "\"_/sc\" getpropstr");
	insert_def(cstat, "fail", "\"_/fl\" getpropstr");
	insert_def(cstat, "drop", "\"_/dr\" getpropstr");
	insert_def(cstat, "osucc", "\"_/osc\" getpropstr");
	insert_def(cstat, "ofail", "\"_/ofl\" getpropstr");
	insert_def(cstat, "odrop", "\"_/odr\" getpropstr");
	insert_def(cstat, "setdesc", "\"_/de\" swap 0 addprop");
	insert_def(cstat, "setsucc", "\"_/sc\" swap 0 addprop");
	insert_def(cstat, "setfail", "\"_/fl\" swap 0 addprop");
	insert_def(cstat, "setdrop", "\"_/dr\" swap 0 addprop");
	insert_def(cstat, "setosucc", "\"_/osc\" swap 0 addprop");
	insert_def(cstat, "setofail", "\"_/ofl\" swap 0 addprop");
	insert_def(cstat, "setodrop", "\"_/odr\" swap 0 addprop");
	insert_def(cstat, "preempt", "pr_mode setmode");
	insert_def(cstat, "background", "bg_mode setmode");
	insert_def(cstat, "foreground", "fg_mode setmode");
	insert_def(cstat, "notify_except", "1 swap notify_exclude");
	insert_def(cstat, "event_wait", "0 array_make event_waitfor");
	insert_def(cstat, "tread", "\"__tread\" timer_start { \"TIMER.__tread\" \"READ\" }list event_waitfor swap pop \"READ\" strcmp if \"\" 0 else read 1 \"__tread\" timer_stop then");

	/* MUF Error defines */
	insert_def(cstat, "err_divzero?", "0 is_set?");
	insert_def(cstat, "err_nan?", "1 is_set?");
	insert_def(cstat, "err_imaginary?", "2 is_set?");
	insert_def(cstat, "err_fbounds?", "3 is_set?");
	insert_def(cstat, "err_ibounds?", "4 is_set?");

	/* Array convenience defines */
	insert_def(cstat, "}list", "} array_make");
	insert_def(cstat, "}dict", "}  2 / array_make_dict");
	insert_def(cstat, "[]", "array_getitem");
	insert_def(cstat, "->[]", "array_setitem");
	insert_def(cstat, "[..]", "array_getrange");
	insert_def(cstat, "array_diff", "2 array_ndiff");
	insert_def(cstat, "array_union", "2 array_nunion");
	insert_def(cstat, "array_intersect", "2 array_nintersect");

	/* GUI dialog types */
	insert_def(cstat, "d_simple", "\"simple\"");
	insert_def(cstat, "d_tabbed", "\"tabbed\"");
	insert_def(cstat, "d_helper", "\"helper\"");

	/* GUI control types */
	insert_def(cstat, "c_menu",      "\"menu\"");
	insert_def(cstat, "c_datum",     "\"datum\"");
	insert_def(cstat, "c_label",     "\"text\"");
	insert_def(cstat, "c_image",     "\"image\"");
	insert_def(cstat, "c_hrule",     "\"hrule\"");
	insert_def(cstat, "c_vrule",     "\"vrule\"");
	insert_def(cstat, "c_button",    "\"button\"");
	insert_def(cstat, "c_checkbox",  "\"checkbox\"");
	insert_def(cstat, "c_radiobtn",  "\"radio\"");
	insert_def(cstat, "c_edit",      "\"edit\"");
	insert_def(cstat, "c_multiedit", "\"multiedit\"");
	insert_def(cstat, "c_combobox",  "\"combobox\"");
	insert_def(cstat, "c_spinner",   "\"spinner\"");
	insert_def(cstat, "c_scale",     "\"scale\"");
	insert_def(cstat, "c_listbox",   "\"listbox\"");
	insert_def(cstat, "c_frame",     "\"frame\"");
	insert_def(cstat, "c_notebook",  "\"notebook\"");

	/* Backwards compatibility for old GUI dialog creation prims */
	insert_def(cstat, "gui_dlog_simple", "d_simple 0 array_make_dict");
    insert_def(cstat, "gui_dlog_tabbed", "d_tabbed swap \"panes\" over array_keys array_make \"names\" 4 rotate array_vals array_make 2 array_make_dict gui_dlog_create");
    insert_def(cstat, "gui_dlog_helper", "d_helper swap \"panes\" over array_keys array_make \"names\" 4 rotate array_vals array_make 2 array_make_dict gui_dlog_create");

	/* Include any defines set in #0's _defs/ propdir. */
	include_defs(cstat, (dbref) 0);

	/* Include any defines set in program owner's _defs/ propdir. */
	include_defs(cstat, OWNER(cstat->program));
}


void
uncompile_program(dbref i)
{
	/* free program */
	(void) dequeue_prog(i, 1);
	free_prog(i);
	cleanpubs(PROGRAM_PUBS(i));
	PROGRAM_SET_PUBS(i, NULL);
	clean_mcpbinds(PROGRAM_MCPBINDS(i));
	PROGRAM_SET_MCPBINDS(i, NULL);
	PROGRAM_SET_PROFTIME(i, 0, 0);
	PROGRAM_SET_CODE(i, NULL);
	PROGRAM_SET_SIZ(i, 0);
	PROGRAM_SET_START(i, NULL);
}


void
do_uncompile(dbref player)
{
	dbref i;

	if (!Wizard(OWNER(player))) {
		notify_nolisten(player, "Permission denied.", 1);
		return;
	}
	for (i = 0; i < db_top; i++) {
		if (Typeof(i) == TYPE_PROGRAM) {
			uncompile_program(i);
		}
	}
	notify_nolisten(player, "All programs decompiled.", 1);
}


void
free_unused_programs()
{
	dbref i;
	time_t now = time(NULL);

	for (i = 0; i < db_top; i++) {
		if ((Typeof(i) == TYPE_PROGRAM) && !(FLAGS(i) & (ABODE | INTERNAL)) &&
			(now - DBFETCH(i)->ts.lastused > tp_clean_interval) && (PROGRAM_INSTANCES(i) == 0)) {
			uncompile_program(i);
		}
	}
}


/* overall control code.  Does piece-meal tokenization parsing and
   backward checking.                                            */
void
do_compile(int descr, dbref player_in, dbref program_in, int force_err_display)
{
	const char *token;
	struct INTERMEDIATE *new_word;
	int i;
	COMPSTATE cstat;

	/* set all compile state variables */
	cstat.force_err_display = force_err_display;
	cstat.descr = descr;
	cstat.control_stack = 0;
	cstat.procs = 0;
	cstat.nowords = 0;
	cstat.curr_word = cstat.first_word = NULL;
	cstat.curr_proc = NULL;
	cstat.currpubs = NULL;
	cstat.nested_fors = 0;
	cstat.nested_trys = 0;
	for (i = 0; i < MAX_VAR; i++) {
		cstat.variables[i] = NULL;
		cstat.variabletypes[i] = 0;
		cstat.localvars[i] = NULL;
		cstat.localvartypes[i] = 0;
		cstat.scopedvars[i] = NULL;
		cstat.scopedvartypes[i] = 0;
	}
	cstat.curr_line = PROGRAM_FIRST(program_in);
	cstat.lineno = 1;
	cstat.next_char = NULL;
	if (cstat.curr_line)
		cstat.next_char = cstat.curr_line->this_line;
	cstat.player = player_in;
	cstat.program = program_in;
	cstat.compile_err = 0;
	cstat.line_copy = NULL;
	cstat.macrosubs = 0;
	cstat.nextinst = NULL;
	cstat.addrlist = NULL;
	cstat.addroffsets = NULL;
	init_defs(&cstat);

	cstat.variables[0] = "ME";
	cstat.variabletypes[0] = PROG_OBJECT;
	cstat.variables[1] = "LOC";
	cstat.variabletypes[1] = PROG_OBJECT;
	cstat.variables[2] = "TRIGGER";
	cstat.variabletypes[2] = PROG_OBJECT;
	cstat.variables[3] = "COMMAND";
	cstat.variabletypes[3] = PROG_STRING;

	/* free old stuff */
	(void) dequeue_prog(cstat.program, 1);
	free_prog(cstat.program);
	cleanpubs(PROGRAM_PUBS(cstat.program));
	PROGRAM_SET_PUBS(cstat.program, NULL);
	clean_mcpbinds(PROGRAM_MCPBINDS(cstat.program));
	PROGRAM_SET_MCPBINDS(cstat.program, NULL);
	PROGRAM_SET_PROFTIME(cstat.program, 0, 0);
	PROGRAM_SET_PROFSTART(cstat.program, time(NULL));
	PROGRAM_SET_PROF_USES(cstat.program, 0);

	if (!cstat.curr_line)
		v_abort_compile(&cstat, "Missing program text.");

	/* do compilation */
	while ((token = next_token(&cstat))) {
		new_word = next_word(&cstat, token);

		/* test for errors */
		if (cstat.compile_err)
			return;

		if (new_word) {
			if (!cstat.first_word)
				cstat.first_word = cstat.curr_word = new_word;
			else {
				cstat.curr_word->next = new_word;
				cstat.curr_word = cstat.curr_word->next;
			}
		}
		while (cstat.curr_word && cstat.curr_word->next)
			cstat.curr_word = cstat.curr_word->next;

		free((void *) token);
	}

	if (cstat.curr_proc)
		v_abort_compile(&cstat, "Unexpected end of file.");

	if (!cstat.procs)
		v_abort_compile(&cstat, "Missing procedure definition.");

	/* do copying over */
	fix_addresses(&cstat);
	copy_program(&cstat);
	fixpubs(cstat.currpubs, PROGRAM_CODE(cstat.program));
	PROGRAM_SET_PUBS(cstat.program, cstat.currpubs);

	if (cstat.nextinst) {
		struct INTERMEDIATE* ptr;
		while (cstat.nextinst)
		{
			ptr = cstat.nextinst;
			cstat.nextinst = ptr->next;
			free(ptr);
		}
		cstat.nextinst = NULL;
	}
	if (cstat.compile_err)
		return;

	set_start(&cstat);
	cleanup(&cstat);

	/* Set PROGRAM_INSTANCES to zero (cuz they don't get set elsewhere) */
	PROGRAM_INSTANCES(cstat.program) = 0;

	/* restart AUTOSTART program. */
	if ((FLAGS(cstat.program) & ABODE) && TrueWizard(OWNER(cstat.program)))
		add_muf_queue_event(-1, OWNER(cstat.program), NOTHING, NOTHING,
							cstat.program, "Startup", "Queued Event.", 0);

	if (force_err_display)
		notify_nolisten(cstat.player, "Program compiled successfully.", 1);
}

struct INTERMEDIATE *
next_word(COMPSTATE * cstat, const char *token)
{
	struct INTERMEDIATE *new_word;
	static char buf[BUFFER_LEN];

	if (!token)
		return 0;

	if (call(cstat, token))
		new_word = call_word(cstat, token);
	else if (scopedvar(cstat, token))
		new_word = svar_word(cstat, token);
	else if (localvar(cstat, token))
		new_word = lvar_word(cstat, token);
	else if (variable(cstat, token))
		new_word = var_word(cstat, token);
	else if (special(token))
		new_word = process_special(cstat, token);
	else if (primitive(token))
		new_word = primitive_word(cstat, token);
	else if (string(token))
		new_word = string_word(cstat, token + 1);
	else if (number(token))
		new_word = number_word(cstat, token);
	else if (ifloat(token))
		new_word = float_word(cstat, token);
	else if (object(token))
		new_word = object_word(cstat, token);
	else if (quoted(cstat, token))
		new_word = quoted_word(cstat, token + 1);
	else {
		sprintf(buf, "Unrecognized word %s.", token);
		abort_compile(cstat, buf);
	}
	return new_word;
}



/* Little routine to do the line_copy handling right */
void
advance_line(COMPSTATE * cstat)
{
	cstat->curr_line = cstat->curr_line->next;
	cstat->lineno++;
	cstat->macrosubs = 0;
	if (cstat->line_copy) {
		free((void *) cstat->line_copy);
		cstat->line_copy = NULL;
	}
	if (cstat->curr_line)
		cstat->next_char = (cstat->line_copy = alloc_string(cstat->curr_line->this_line));
	else
		cstat->next_char = (cstat->line_copy = NULL);
}

/* Skips comments, grabs strings, returns NULL when no more tokens to grab. */
const char *
next_token_raw(COMPSTATE * cstat)
{
	static char buf[BUFFER_LEN];
	int i;

	if (!cstat->curr_line)
		return (char *) 0;

	if (!cstat->next_char)
		return (char *) 0;

	/* skip white space */
	while (*cstat->next_char && isspace(*cstat->next_char))
		cstat->next_char++;

	if (!(*cstat->next_char)) {
		advance_line(cstat);
		return next_token_raw(cstat);
	}
	/* take care of comments */
	if (*cstat->next_char == BEGINCOMMENT) {
		do_comment(cstat);
		return next_token_raw(cstat);
	}
	if (*cstat->next_char == BEGINSTRING)
		return do_string(cstat);

	for (i = 0; *cstat->next_char && !isspace(*cstat->next_char); i++) {
		buf[i] = *cstat->next_char;
		cstat->next_char++;
	}
	buf[i] = '\0';
	return alloc_string(buf);
}


const char *
next_token(COMPSTATE * cstat)
{
	char *expansion, *temp;

	temp = (char *) next_token_raw(cstat);
	if (!temp)
		return NULL;

	if (temp[0] == BEGINDIRECTIVE) {
		do_directive(cstat, temp);
		free(temp);
		return next_token(cstat);
	}
	if (temp[0] == BEGINESCAPE) {
		if (temp[1]) {
			expansion = temp;
			temp = (char *) malloc(strlen(expansion));
			strcpy(temp, (expansion + 1));
			free(expansion);
		}
		return (temp);
	}
	if ((expansion = expand_def(cstat, temp))) {
		free(temp);
		if (++cstat->macrosubs > SUBSTITUTIONS) {
			abort_compile(cstat, "Too many macro substitutions.");
		} else {
			temp = (char *) malloc(strlen(cstat->next_char) + strlen(expansion) + 21);
			strcpy(temp, expansion);
			strcat(temp, cstat->next_char);
			free((void *) expansion);
			if (cstat->line_copy) {
				free((void *) cstat->line_copy);
			}
			cstat->next_char = cstat->line_copy = temp;
			return next_token(cstat);
		}
	} else {
		return (temp);
	}
}



/* skip comments */
void
do_comment(COMPSTATE * cstat)
{
	while (*cstat->next_char && *cstat->next_char != ENDCOMMENT)
		cstat->next_char++;
	if (!(*cstat->next_char)) {
		advance_line(cstat);
		if (!cstat->curr_line) {
			v_abort_compile(cstat, "Unterminated comment.");
		}
		do_comment(cstat);
	} else {
		cstat->next_char++;
		if (!(*cstat->next_char))
			advance_line(cstat);
	}
}

/* handle compiler directives */
void
do_directive(COMPSTATE * cstat, char *direct)
{
	char temp[BUFFER_LEN];
	char *tmpname;
	char *tmpptr;
	int i = 0;
	int j;

	strcpy(temp, ++direct);

	if (!(temp[0])) {
		v_abort_compile(cstat, "I don't understand that compiler directive!");
	}
	if (!string_compare(temp, "define")) {
		tmpname = (char *) next_token_raw(cstat);
		if (!tmpname)
			v_abort_compile(cstat, "Unexpected end of file looking for $define name.");
		i = 0;
		while ((tmpptr = (char *) next_token_raw(cstat)) &&
			   (string_compare(tmpptr, "$enddef"))) {
			char *cp;

			for (cp = tmpptr; i < (BUFFER_LEN / 2) && *cp;) {
				if (*tmpptr == BEGINSTRING && cp != tmpptr &&
					(*cp == ENDSTRING || *cp == BEGINESCAPE)) {
					temp[i++] = BEGINESCAPE;
				}
				temp[i++] = *cp++;
			}
			if (*tmpptr == BEGINSTRING)
				temp[i++] = ENDSTRING;
			temp[i++] = ' ';
			free(tmpptr);
			if (i > (BUFFER_LEN / 2))
				v_abort_compile(cstat, "$define definition too long.");
		}
		if (i)
			i--;
		temp[i] = '\0';
		if (!tmpptr)
			v_abort_compile(cstat, "Unexpected end of file in $define definition.");
		free(tmpptr);
		(void) insert_def(cstat, tmpname, temp);
		free(tmpname);

	} else if (!string_compare(temp, "enddef")) {
		v_abort_compile(cstat, "$enddef without a previous matching $define.");

	} else if (!string_compare(temp, "def")) {
		tmpname = (char *) next_token_raw(cstat);
		if (!tmpname)
			v_abort_compile(cstat, "Unexpected end of file looking for $def name.");
		(void) insert_def(cstat, tmpname, cstat->next_char);
		while (*cstat->next_char)
			cstat->next_char++;
		advance_line(cstat);
		free(tmpname);

	} else if (!string_compare(temp, "include")) {
		struct match_data md;

		tmpname = (char *) next_token_raw(cstat);
		if (!tmpname)
			v_abort_compile(cstat, "Unexpected end of file while doing $include.");
		{
			char tempa[BUFFER_LEN], tempb[BUFFER_LEN];

			strcpy(tempa, match_args);
			strcpy(tempb, match_cmdname);
			init_match(cstat->descr, cstat->player, tmpname, NOTYPE, &md);
			match_registered(&md);
			match_absolute(&md);
			match_me(&md);
			i = (int) match_result(&md);
			strcpy(match_args, tempa);
			strcpy(match_cmdname, tempb);
		}
		free(tmpname);
		if (((dbref) i == NOTHING) || (i < 0) || (i > db_top)
			|| (Typeof(i) == TYPE_GARBAGE))
			v_abort_compile(cstat, "I don't understand what object you want to $include.");
		include_defs(cstat, (dbref) i);

	} else if (!string_compare(temp, "undef")) {
		tmpname = (char *) next_token_raw(cstat);
		if (!tmpname)
			v_abort_compile(cstat, "Unexpected end of file looking for name to $undef.");
		kill_def(cstat, tmpname);
		free(tmpname);

	} else if (!string_compare(temp, "echo")) {
		notify_nolisten(cstat->player, cstat->next_char, 1);
		while (*cstat->next_char)
			cstat->next_char++;
		advance_line(cstat);

	} else if (!string_compare(temp, "ifdef")) {
		tmpname = (char *) next_token_raw(cstat);
		if (!tmpname)
			v_abort_compile(cstat, "Unexpected end of file looking for $ifdef condition.");
		strcpy(temp, tmpname);
		free(tmpname);
		for (i = 1; temp[i] && (temp[i] != '=') && (temp[i] != '>') && (temp[i] != '<'); i++) ;
		tmpname = &(temp[i]);
		i = (temp[i] == '>') ? 1 : ((temp[i] == '=') ? 0 : ((temp[i] == '<') ? -1 : -2));
		*tmpname = '\0';
		tmpname++;
		tmpptr = (char *) expand_def(cstat, temp);
		if (i == -2) {
			j = (!tmpptr);
			if (tmpptr)
				free(tmpptr);
		} else {
			if (!tmpptr) {
				j = 1;
			} else {
				j = string_compare(tmpptr, tmpname);
				j = !((!i && !j) || ((i * j) > 0));
				free(tmpptr);
			}
		}
		if (j) {
			i = 0;
			while ((tmpptr = (char *) next_token_raw(cstat)) &&
				   (i || ((string_compare(tmpptr, "$else"))
						  && (string_compare(tmpptr, "$endif"))))) {
				if (!string_compare(tmpptr, "$ifdef"))
					i++;
				else if (!string_compare(tmpptr, "$ifndef"))
					i++;
				else if (!string_compare(tmpptr, "$endif"))
					i--;
				free(tmpptr);
			}
			if (!tmpptr) {
				v_abort_compile(cstat, "Unexpected end of file in $ifdef clause.");
			}
			free(tmpptr);
		}
	} else if (!string_compare(temp, "ifndef")) {
		tmpname = (char *) next_token_raw(cstat);
		if (!tmpname) {
			v_abort_compile(cstat, "Unexpected end of file looking for $ifndef condition.");
		}
		strcpy(temp, tmpname);
		free(tmpname);
		for (i = 1; temp[i] && (temp[i] != '=') && (temp[i] != '>') && (temp[i] != '<'); i++) ;
		tmpname = &(temp[i]);
		i = (temp[i] == '>') ? 1 : ((temp[i] == '=') ? 0 : ((temp[i] == '<') ? -1 : -2));
		*tmpname = '\0';
		tmpname++;
		tmpptr = (char *) expand_def(cstat, temp);
		if (i == -2) {
			j = (!tmpptr);
			if (tmpptr)
				free(tmpptr);
		} else {
			if (!tmpptr) {
				j = 1;
			} else {
				j = string_compare(tmpptr, tmpname);
				j = !((!i && !j) || ((i * j) > 0));
				free(tmpptr);
			}
		}
		if (!j) {
			i = 0;
			while ((tmpptr = (char *) next_token_raw(cstat)) &&
				   (i || ((string_compare(tmpptr, "$else"))
						  && (string_compare(tmpptr, "$endif"))))) {
				if (!string_compare(tmpptr, "$ifdef"))
					i++;
				else if (!string_compare(tmpptr, "$ifndef"))
					i++;
				else if (!string_compare(tmpptr, "$endif"))
					i--;
				free(tmpptr);
			}
			if (!tmpptr) {
				v_abort_compile(cstat, "Unexpected end of file in $ifndef clause.");
			}
			free(tmpptr);
		}
	} else if (!string_compare(temp, "else")) {
		i = 0;
		while ((tmpptr = (char *) next_token_raw(cstat)) &&
			   (i || (string_compare(tmpptr, "$endif")))) {
			if (!string_compare(tmpptr, "$ifdef"))
				i++;
			else if (!string_compare(tmpptr, "$ifndef"))
				i++;
			else if (!string_compare(tmpptr, "$endif"))
				i--;
			free(tmpptr);
		}
		if (!tmpptr) {
			v_abort_compile(cstat, "Unexpected end of file in $else clause.");
		}
		free(tmpptr);

	} else if (!string_compare(temp, "endif")) {

	} else {
		v_abort_compile(cstat, "Unrecognized compiler directive.");
	}
}


/* return string */
const char *
do_string(COMPSTATE * cstat)
{
	static char buf[BUFFER_LEN];
	int i = 0, quoted = 0;

	buf[i] = *cstat->next_char;
	cstat->next_char++;
	i++;
	while ((quoted || *cstat->next_char != ENDSTRING) && *cstat->next_char) {
		if (*cstat->next_char == '\\' && !quoted) {
			quoted++;
			cstat->next_char++;
		} else if (*cstat->next_char == 'r' && quoted) {
			buf[i++] = '\r';
			cstat->next_char++;
			quoted = 0;
		} else if (*cstat->next_char == '[' && quoted) {
			buf[i++] = ESCAPE_CHAR;
			cstat->next_char++;
			quoted = 0;
		} else {
			buf[i] = *cstat->next_char;
			i++;
			cstat->next_char++;
			quoted = 0;
		}
	}
	if (!*cstat->next_char) {
		abort_compile(cstat, "Unterminated string found at end of line.");
	}
	cstat->next_char++;
	buf[i] = '\0';
	return alloc_string(buf);
}



/* process special.  Performs special processing.
   It sets up FOR and IF structures.  Remember --- for those,
   we've got to set aside an extra argument space.         */
struct INTERMEDIATE *
process_special(COMPSTATE * cstat, const char *token)
{
	static char buf[BUFFER_LEN];
	const char *tok;
	struct INTERMEDIATE *nu;

	if (!string_compare(token, ":")) {
		const char *proc_name;
/*FIXME		struct INTERMEDIATE *new2; */
		int argsflag = 0;

		if (cstat->curr_proc)
			abort_compile(cstat, "Definition within definition.");
		proc_name = next_token(cstat);
		if (!proc_name)
			abort_compile(cstat, "Unexpected end of file within procedure.");

		strcpy(buf, proc_name);
		if (proc_name)
			free((void *) proc_name);
		proc_name = buf;

		if (*proc_name && buf[strlen(buf)-1] == '[') {
			argsflag = 1;
			buf[strlen(buf)-1] = '\0';
			if (!*proc_name)
				abort_compile(cstat, "Bad procedure name.");
		}

		nu = new_inst(cstat);
		nu->no = cstat->nowords++;
		nu->in.type = PROG_FUNCTION;
		nu->in.line = cstat->lineno;
		nu->in.data.mufproc = (struct muf_proc_data*)malloc(sizeof(struct muf_proc_data));
		nu->in.data.mufproc->procname = string_dup(proc_name);
		nu->in.data.mufproc->vars = 0;
		nu->in.data.mufproc->args = 0;

		cstat->curr_proc = nu;

		if (argsflag) {
			const char* varspec;
			const char* varname;
			int argsdone = 0;
			int outflag = 0;

			do {
				varspec = next_token(cstat);
				if (!varspec)
					abort_compile(cstat, "Unexpected end of file within procedure.");

				if (!strcmp(varspec, "]")) {
					argsdone = 1;
				} else if (!strcmp(varspec, "--")) {
					outflag = 1;
				} else if (!outflag) {
					varname = index(varspec, ':');
					if (varname) {
						varname++;
					} else {
						varname = varspec;
					}
					if (add_scopedvar(cstat, varname, PROG_UNTYPED) < 0)
						abort_compile(cstat, "Variable limit exceeded.");

					nu->in.data.mufproc->vars++;
					nu->in.data.mufproc->args++;
				}
				if (varspec) {
					free((void *) varspec);
				}
			} while(!argsdone);
		}

		add_proc(cstat, proc_name, nu, PROG_UNTYPED);

		return nu;
	} else if (!string_compare(token, ";")) {
		int i;

		if (cstat->control_stack)
			abort_compile(cstat, "Unexpected end of procedure definition.");
		if (!cstat->curr_proc)
			abort_compile(cstat, "Procedure end without body.");
		cstat->curr_proc = 0;
		nu = new_inst(cstat);
		nu->no = cstat->nowords++;
		nu->in.type = PROG_PRIMITIVE;
		nu->in.line = cstat->lineno;
		nu->in.data.number = IN_RET;
		for (i = 0; i < MAX_VAR && cstat->scopedvars[i]; i++) {
			free((void *) cstat->scopedvars[i]);
			cstat->scopedvars[i] = 0;
		}
		return nu;
	} else if (!string_compare(token, "IF")) {
		nu = new_inst(cstat);
		nu->no = cstat->nowords++;
		nu->in.type = PROG_IF;
		nu->in.line = cstat->lineno;
		nu->in.data.call = 0;
		add_control_structure(cstat, CTYPE_IF, nu);
		return nu;
	} else if (!string_compare(token, "ELSE")) {
		struct INTERMEDIATE *eef;
		int ctrltype = innermost_control_type(cstat);

		switch (ctrltype) {
			case CTYPE_IF:
				break;
			case CTYPE_TRY:
				abort_compile(cstat, "Unterminated TRY-CATCH block at ELSE.");
				break;
			case CTYPE_CATCH:
				abort_compile(cstat, "Unterminated CATCH-ENDCATCH block at ELSE.");
				break;
			case CTYPE_FOR:
			case CTYPE_BEGIN:
				abort_compile(cstat, "Unterminated Loop at ELSE.");
				break;
			default:
				abort_compile(cstat, "ELSE without IF.");
				break;
		}

		nu = new_inst(cstat);
		nu->no = cstat->nowords++;
		nu->in.type = PROG_JMP;
		nu->in.line = cstat->lineno;
		nu->in.data.call = 0;

		eef = pop_control_structure(cstat, CTYPE_IF, 0);
		add_control_structure(cstat, CTYPE_ELSE, nu);
		eef->in.data.number = get_address(cstat, nu, 1);
		return nu;
	} else if (!string_compare(token, "THEN")) {
		/* can't use 'if' because it's a reserved word */
		struct INTERMEDIATE *eef;
		int ctrltype = innermost_control_type(cstat);

		switch (ctrltype) {
			case CTYPE_IF:
			case CTYPE_ELSE:
				break;
			case CTYPE_TRY:
				abort_compile(cstat, "Unterminated TRY-CATCH block at THEN.");
				break;
			case CTYPE_CATCH:
				abort_compile(cstat, "Unterminated CATCH-ENDCATCH block at THEN.");
				break;
			case CTYPE_FOR:
			case CTYPE_BEGIN:
				abort_compile(cstat, "Unterminated Loop at THEN.");
				break;
			default:
				abort_compile(cstat, "THEN without IF.");
				break;
		}

		prealloc_inst(cstat);
		eef = pop_control_structure(cstat, CTYPE_IF, CTYPE_ELSE);
		eef->in.data.number = get_address(cstat, cstat->nextinst, 0);
		return NULL;
	} else if (!string_compare(token, "BEGIN")) {
		prealloc_inst(cstat);
		add_control_structure(cstat, CTYPE_BEGIN, cstat->nextinst);
		return NULL;
	} else if (!string_compare(token, "FOR")) {
		struct INTERMEDIATE *new2, *new3;

		nu = new_inst(cstat);
		nu->no = cstat->nowords++;
		nu->in.type = PROG_PRIMITIVE;
		nu->in.line = cstat->lineno;
		nu->in.data.number = IN_FOR;
		new2 = (nu->next = new_inst(cstat));
		new2->no = cstat->nowords++;
		new2->in.type = PROG_PRIMITIVE;
		new2->in.line = cstat->lineno;
		new2->in.data.number = IN_FORITER;
		new3 = (new2->next = new_inst(cstat));
		new3->no = cstat->nowords++;
		new3->in.line = cstat->lineno;
		new3->in.type = PROG_IF;
		new3->in.data.number = 0;

		add_control_structure(cstat, CTYPE_FOR, new2);
		cstat->nested_fors++;
		return nu;
	} else if (!string_compare(token, "FOREACH")) {
		struct INTERMEDIATE *new2, *new3;

		nu = new_inst(cstat);
		nu->no = cstat->nowords++;
		nu->in.type = PROG_PRIMITIVE;
		nu->in.line = cstat->lineno;
		nu->in.data.number = IN_FOREACH;
		new2 = (nu->next = new_inst(cstat));
		new2->no = cstat->nowords++;
		new2->in.type = PROG_PRIMITIVE;
		new2->in.line = cstat->lineno;
		new2->in.data.number = IN_FORITER;
		new3 = (new2->next = new_inst(cstat));
		new3->no = cstat->nowords++;
		new3->in.line = cstat->lineno;
		new3->in.type = PROG_IF;
		new3->in.data.number = 0;

		add_control_structure(cstat, CTYPE_FOR, new2);
		cstat->nested_fors++;
		return nu;
	} else if (!string_compare(token, "UNTIL")) {
		/* can't use 'if' because it's a reserved word */
		struct INTERMEDIATE *eef;
		struct INTERMEDIATE *curr;
		int ctrltype = innermost_control_type(cstat);

		switch (ctrltype) {
			case CTYPE_FOR:
				cstat->nested_fors--;
			case CTYPE_BEGIN:
				break;
			case CTYPE_TRY:
				abort_compile(cstat, "Unterminated TRY-CATCH block at UNTIL.");
				break;
			case CTYPE_CATCH:
				abort_compile(cstat, "Unterminated CATCH-ENDCATCH block at UNTIL.");
				break;
			case CTYPE_IF:
			case CTYPE_ELSE:
				abort_compile(cstat, "Unterminated IF-THEN at UNTIL.");
				break;
			default:
				abort_compile(cstat, "Loop start not found for UNTIL.");
				break;
		}

		prealloc_inst(cstat);
		resolve_loop_addrs(cstat, get_address(cstat, cstat->nextinst, 1));
		eef = pop_control_structure(cstat, CTYPE_BEGIN, CTYPE_FOR);

		nu = new_inst(cstat);
		nu->no = cstat->nowords++;
		nu->in.type = PROG_IF;
		nu->in.line = cstat->lineno;
		nu->in.data.number = get_address(cstat, eef, 0);

		if (ctrltype == CTYPE_FOR) {
			curr = (nu->next = new_inst(cstat));
			curr->no = cstat->nowords++;
			curr->in.type = PROG_PRIMITIVE;
			curr->in.line = cstat->lineno;
			curr->in.data.number = IN_FORPOP;
		}
		return nu;
	} else if (!string_compare(token, "WHILE")) {
		struct INTERMEDIATE *curr;
		int trycount;
		if (!in_loop(cstat))
			abort_compile(cstat, "Can't have a WHILE outside of a loop.");

		trycount = count_trys_inside_loop(cstat);
		nu = curr = NULL;
		while (trycount-->0) {
			if (!nu) {
				nu = curr = new_inst(cstat);
			} else {
				nu = (nu->next = new_inst(cstat));
			}
			nu->no = cstat->nowords++;
			nu->in.type = PROG_PRIMITIVE;
			nu->in.line = cstat->lineno;
			nu->in.data.number = IN_TRYPOP;
		}
		if (nu) {
			nu = (nu->next = new_inst(cstat));
		} else {
			curr = nu = new_inst(cstat);
		}
		nu->no = cstat->nowords++;
		nu->in.type = PROG_IF;
		nu->in.line = cstat->lineno;
		nu->in.data.number = 0;

		add_loop_exit(cstat, nu);
		return curr;
	} else if (!string_compare(token, "BREAK")) {
		int trycount;
		struct INTERMEDIATE *curr;
		if (!in_loop(cstat))
			abort_compile(cstat, "Can't have a BREAK outside of a loop.");

		trycount = count_trys_inside_loop(cstat);
		nu = curr = NULL;
		while (trycount-->0) {
			if (!nu) {
				nu = curr = new_inst(cstat);
			} else {
				nu = (nu->next = new_inst(cstat));
			}
			nu->no = cstat->nowords++;
			nu->in.type = PROG_PRIMITIVE;
			nu->in.line = cstat->lineno;
			nu->in.data.number = IN_TRYPOP;
		}
		if (nu) {
			nu = (nu->next = new_inst(cstat));
		} else {
			curr = nu = new_inst(cstat);
		}
		nu->no = cstat->nowords++;
		nu->in.type = PROG_JMP;
		nu->in.line = cstat->lineno;
		nu->in.data.number = 0;

		add_loop_exit(cstat, nu);
		return curr;
	} else if (!string_compare(token, "CONTINUE")) {
		/* can't use 'if' because it's a reserved word */
		struct INTERMEDIATE *beef;
		struct INTERMEDIATE *curr;
		int trycount;

		if (!in_loop(cstat))
			abort_compile(cstat, "Can't CONTINUE outside of a loop.");

		beef = locate_control_structure(cstat, CTYPE_FOR, CTYPE_BEGIN);
		trycount = count_trys_inside_loop(cstat);
		nu = curr = NULL;
		while (trycount-->0) {
			if (!nu) {
				nu = curr = new_inst(cstat);
			} else {
				nu = (nu->next = new_inst(cstat));
			}
			nu->no = cstat->nowords++;
			nu->in.type = PROG_PRIMITIVE;
			nu->in.line = cstat->lineno;
			nu->in.data.number = IN_TRYPOP;
		}
		if (nu) {
			nu = (nu->next = new_inst(cstat));
		} else {
			curr = nu = new_inst(cstat);
		}
		nu->no = cstat->nowords++;
		nu->in.type = PROG_JMP;
		nu->in.line = cstat->lineno;
		nu->in.data.number = get_address(cstat, beef, 0);

		return curr;
	} else if (!string_compare(token, "REPEAT")) {
		/* can't use 'if' because it's a reserved word */
		struct INTERMEDIATE *eef;
		struct INTERMEDIATE *curr;
		int ctrltype = innermost_control_type(cstat);

		switch (ctrltype) {
			case CTYPE_FOR:
				cstat->nested_fors--;
			case CTYPE_BEGIN:
				break;
			case CTYPE_TRY:
				abort_compile(cstat, "Unterminated TRY-CATCH block at REPEAT.");
				break;
			case CTYPE_CATCH:
				abort_compile(cstat, "Unterminated CATCH-ENDCATCH block at REPEAT.");
				break;
			case CTYPE_IF:
			case CTYPE_ELSE:
				abort_compile(cstat, "Unterminated IF-THEN at REPEAT.");
				break;
			default:
				abort_compile(cstat, "Loop start not found for REPEAT.");
				break;
		}

		prealloc_inst(cstat);
		resolve_loop_addrs(cstat, get_address(cstat, cstat->nextinst, 1));
		eef = pop_control_structure(cstat, CTYPE_BEGIN, CTYPE_FOR);

		nu = new_inst(cstat);
		nu->no = cstat->nowords++;
		nu->in.type = PROG_JMP;
		nu->in.line = cstat->lineno;
		nu->in.data.number = get_address(cstat, eef, 0);

		if (ctrltype == CTYPE_FOR) {
			curr = (nu->next = new_inst(cstat));
			curr->no = cstat->nowords++;
			curr->in.type = PROG_PRIMITIVE;
			curr->in.line = cstat->lineno;
			curr->in.data.number = IN_FORPOP;
		}

		return nu;
	} else if (!string_compare(token, "TRY")) {
		nu = new_inst(cstat);
		nu->no = cstat->nowords++;
		nu->in.type = PROG_TRY;
		nu->in.line = cstat->lineno;
		nu->in.data.number = 0;

		add_control_structure(cstat, CTYPE_TRY, nu);
		cstat->nested_trys++;

		return nu;
	} else if (!string_compare(token, "CATCH")) {
		/* can't use 'if' because it's a reserved word */
		struct INTERMEDIATE *eef;
		struct INTERMEDIATE *curr;
		struct INTERMEDIATE *jump;
		int ctrltype = innermost_control_type(cstat);

		switch (ctrltype) {
			case CTYPE_TRY:
				break;
			case CTYPE_FOR:
			case CTYPE_BEGIN:
				abort_compile(cstat, "Unterminated Loop at CATCH.");
				break;
			case CTYPE_IF:
			case CTYPE_ELSE:
				abort_compile(cstat, "Unterminated IF-THEN at CATCH.");
				break;
			case CTYPE_CATCH:
			default:
				abort_compile(cstat, "No TRY found for CATCH.");
				break;
		}

		nu = new_inst(cstat);
		nu->no = cstat->nowords++;
		nu->in.type = PROG_PRIMITIVE;
		nu->in.line = cstat->lineno;
		nu->in.data.number = IN_TRYPOP;

		jump = (nu->next = new_inst(cstat));
		jump->no = cstat->nowords++;
		jump->in.type = PROG_JMP;
		jump->in.line = cstat->lineno;
		jump->in.data.number = 0;

		curr = (jump->next = new_inst(cstat));
		curr->no = cstat->nowords++;
		curr->in.type = PROG_PRIMITIVE;
		curr->in.line = cstat->lineno;
		curr->in.data.number = IN_CATCH;

		eef = pop_control_structure(cstat, CTYPE_TRY, 0);
		cstat->nested_trys--;
		eef->in.data.number = get_address(cstat, curr, 0);
		add_control_structure(cstat, CTYPE_CATCH, jump);

		return nu;
	} else if (!string_compare(token, "ENDCATCH")) {
		/* can't use 'if' because it's a reserved word */
		struct INTERMEDIATE *eef;
		int ctrltype = innermost_control_type(cstat);

		switch (ctrltype) {
			case CTYPE_CATCH:
				break;
			case CTYPE_FOR:
			case CTYPE_BEGIN:
				abort_compile(cstat, "Unterminated Loop at ENDCATCH.");
				break;
			case CTYPE_IF:
			case CTYPE_ELSE:
				abort_compile(cstat, "Unterminated IF-THEN at ENDCATCH.");
				break;
			case CTYPE_TRY:
			default:
				abort_compile(cstat, "No CATCH found for ENDCATCH.");
				break;
		}

		prealloc_inst(cstat);
		eef = pop_control_structure(cstat, CTYPE_CATCH, 0);
		eef->in.data.number = get_address(cstat, cstat->nextinst, 0);
		return NULL;
	} else if (!string_compare(token, "CALL")) {
		nu = new_inst(cstat);
		nu->no = cstat->nowords++;
		nu->in.type = PROG_PRIMITIVE;
		nu->in.line = cstat->lineno;
		nu->in.data.number = IN_CALL;
		return nu;
	} else if (!string_compare(token, "WIZCALL") || !string_compare(token, "PUBLIC")) {
		struct PROC_LIST *p;
		struct publics *pub;
		int wizflag = 0;

		if (!string_compare(token, "WIZCALL"))
			wizflag = 1;
		if (cstat->curr_proc)
			abort_compile(cstat, "PUBLIC  or WIZCALL declaration within procedure.");
		tok = next_token(cstat);
		if ((!tok) || !call(cstat, tok))
			abort_compile(cstat, "Subroutine unknown in PUBLIC or WIZCALL declaration.");
		for (p = cstat->procs; p; p = p->next)
			if (!string_compare(p->name, tok))
				break;
		if (!p)
			abort_compile(cstat, "Subroutine unknown in PUBLIC or WIZCALL declaration.");
		if (!cstat->currpubs) {
			cstat->currpubs = (struct publics *) malloc(sizeof(struct publics));

			cstat->currpubs->next = NULL;
			cstat->currpubs->subname = (char *) string_dup(tok);
			if (tok)
				free((void *) tok);
			cstat->currpubs->addr.no = get_address(cstat, p->code, 0);
			cstat->currpubs->mlev = wizflag ? 4 : 1;
			return 0;
		}
		for (pub = cstat->currpubs; pub;) {
			if (!string_compare(tok, pub->subname)) {
				abort_compile(cstat, "Function already declared public.");
			} else {
				if (pub->next) {
					pub = pub->next;
				} else {
					pub->next = (struct publics *) malloc(sizeof(struct publics));

					pub = pub->next;
					pub->next = NULL;
					pub->subname = (char *) string_dup(tok);
					if (tok)
						free((void *) tok);
					pub->addr.no = get_address(cstat, p->code, 0);
					pub->mlev = wizflag ? 4 : 1;
					pub = NULL;
				}
			}
		}
		return 0;
	} else if (!string_compare(token, "VAR")) {
		if (cstat->curr_proc) {
			tok = next_token(cstat);
			if (!tok)
				abort_compile(cstat, "Unexpected end of program.");
			if (add_scopedvar(cstat, tok, PROG_UNTYPED) < 0)
				abort_compile(cstat, "Variable limit exceeded.");
			if (tok)
				free((void *) tok);
			cstat->curr_proc->in.data.mufproc->vars++;
		} else {
			tok = next_token(cstat);
			if (!tok)
				abort_compile(cstat, "Unexpected end of program.");
			if (!add_variable(cstat, tok, PROG_UNTYPED))
				abort_compile(cstat, "Variable limit exceeded.");
			if (tok)
				free((void *) tok);
		}
		return 0;
	} else if (!string_compare(token, "VAR!")) {
		if (cstat->curr_proc) {
			struct INTERMEDIATE *nu;
/*FIXME			struct INTERMEDIATE *curr; */

			tok = next_token(cstat);
			if (!tok)
				abort_compile(cstat, "Unexpected end of program.");
			if (add_scopedvar(cstat, tok, PROG_UNTYPED) < 0)
				abort_compile(cstat, "Variable limit exceeded.");
			if (tok)
				free((void *) tok);

			nu = new_inst(cstat);
			nu->no = cstat->nowords++;
			nu->in.type = PROG_SVAR_BANG;
			nu->in.line = cstat->lineno;
			nu->in.data.number = cstat->curr_proc->in.data.mufproc->vars++;

			return nu;
		} else {
			abort_compile(cstat, "VAR! used outside of procedure.");
		}
		return 0;
	} else if (!string_compare(token, "LVAR")) {
		if (cstat->curr_proc)
			abort_compile(cstat, "Local variable declared within procedure.");
		tok = next_token(cstat);
		if (!tok || (add_localvar(cstat, tok, PROG_UNTYPED) == -1))
			abort_compile(cstat, "Local variable limit exceeded.");
		if (tok)
			free((void *) tok);
		return 0;
	} else {
		sprintf(buf, "Unrecognized special form %s found. (%d)", token, cstat->lineno);
		abort_compile(cstat, buf);
	}
}

/* return primitive word. */
struct INTERMEDIATE *
primitive_word(COMPSTATE * cstat, const char *token)
{
	struct INTERMEDIATE *nu, *cur;
	int pnum, loop;

	pnum = get_primitive(token);
	cur = nu = new_inst(cstat);
	if (pnum == IN_RET || pnum == IN_JMP) {
		for (loop = 0; loop < cstat->nested_trys; loop++) {
			cur->no = cstat->nowords++;
			cur->in.type = PROG_PRIMITIVE;
			cur->in.line = cstat->lineno;
			cur->in.data.number = IN_TRYPOP;
			cur->next = new_inst(cstat);
			cur = cur->next;
		}
		for (loop = 0; loop < cstat->nested_fors; loop++) {
			cur->no = cstat->nowords++;
			cur->in.type = PROG_PRIMITIVE;
			cur->in.line = cstat->lineno;
			cur->in.data.number = IN_FORPOP;
			cur->next = new_inst(cstat);
			cur = cur->next;
		}
	}

	cur->no = cstat->nowords++;
	cur->in.type = PROG_PRIMITIVE;
	cur->in.line = cstat->lineno;
	cur->in.data.number = pnum;

	return nu;
}

/* return self pushing word (string) */
struct INTERMEDIATE *
string_word(COMPSTATE * cstat, const char *token)
{
	struct INTERMEDIATE *nu;

	nu = new_inst(cstat);
	nu->no = cstat->nowords++;
	nu->in.type = PROG_STRING;
	nu->in.line = cstat->lineno;
	nu->in.data.string = alloc_prog_string(token);
	return nu;
}

/* return self pushing word (float) */
struct INTERMEDIATE *
float_word(COMPSTATE * cstat, const char *token)
{
	struct INTERMEDIATE *nu;

	nu = new_inst(cstat);
	nu->no = cstat->nowords++;
	nu->in.type = PROG_FLOAT;
	nu->in.line = cstat->lineno;
	sscanf(token, "%g", &(nu->in.data.fnumber));
	return nu;
}

/* return self pushing word (number) */
struct INTERMEDIATE *
number_word(COMPSTATE * cstat, const char *token)
{
	struct INTERMEDIATE *nu;

	nu = new_inst(cstat);
	nu->no = cstat->nowords++;
	nu->in.type = PROG_INTEGER;
	nu->in.line = cstat->lineno;
	nu->in.data.number = atoi(token);
	return nu;
}

/* do a subroutine call --- push address onto stack, then make a primitive
   CALL.
   */
struct INTERMEDIATE *
call_word(COMPSTATE * cstat, const char *token)
{
	struct INTERMEDIATE *nu;
	struct PROC_LIST *p;

	nu = new_inst(cstat);
	nu->no = cstat->nowords++;
	nu->in.type = PROG_EXEC;
	nu->in.line = cstat->lineno;
	for (p = cstat->procs; p; p = p->next)
		if (!string_compare(p->name, token))
			break;

	nu->in.data.number = get_address(cstat, p->code, 0);
	return nu;
}

struct INTERMEDIATE *
quoted_word(COMPSTATE * cstat, const char *token)
{
	struct INTERMEDIATE *nu;
	struct PROC_LIST *p;

	nu = new_inst(cstat);
	nu->no = cstat->nowords++;
	nu->in.type = PROG_ADD;
	nu->in.line = cstat->lineno;
	for (p = cstat->procs; p; p = p->next)
		if (!string_compare(p->name, token))
			break;

	nu->in.data.number = get_address(cstat, p->code, 0);
	return nu;
}

/* returns number corresponding to variable number.
   We assume that it DOES exist */
struct INTERMEDIATE *
var_word(COMPSTATE * cstat, const char *token)
{
	struct INTERMEDIATE *nu;
	int i, var_no;

	nu = new_inst(cstat);
	nu->no = cstat->nowords++;
	nu->in.type = PROG_VAR;
	nu->in.line = cstat->lineno;
	for (var_no = i = 0; i < MAX_VAR; i++) {
		if (!cstat->variables[i])
			break;
		if (!string_compare(token, cstat->variables[i]))
			var_no = i;
	}
	nu->in.data.number = var_no;

	return nu;
}

struct INTERMEDIATE *
svar_word(COMPSTATE * cstat, const char *token)
{
	struct INTERMEDIATE *nu;
	int i, var_no;

	nu = new_inst(cstat);
	nu->no = cstat->nowords++;
	nu->in.type = PROG_SVAR;
	nu->in.line = cstat->lineno;
	for (i = var_no = 0; i < MAX_VAR; i++) {
		if (!cstat->scopedvars[i])
			break;
		if (!string_compare(token, cstat->scopedvars[i]))
			var_no = i;
	}
	nu->in.data.number = var_no;

	return nu;
}

struct INTERMEDIATE *
lvar_word(COMPSTATE * cstat, const char *token)
{
	struct INTERMEDIATE *nu;
	int i, var_no;

	nu = new_inst(cstat);
	nu->no = cstat->nowords++;
	nu->in.type = PROG_LVAR;
	nu->in.line = cstat->lineno;
	for (i = var_no = 0; i < MAX_VAR; i++) {
		if (!cstat->localvars[i])
			break;
		if (!string_compare(token, cstat->localvars[i]))
			var_no = i;
	}
	nu->in.data.number = var_no;

	return nu;
}

/* check if object is in database before putting it in */
struct INTERMEDIATE *
object_word(COMPSTATE * cstat, const char *token)
{
	struct INTERMEDIATE *nu;
	int objno;

	objno = atol(token + 1);
	nu = new_inst(cstat);
	nu->no = cstat->nowords++;
	nu->in.type = PROG_OBJECT;
	nu->in.line = cstat->lineno;
	nu->in.data.objref = objno;
	return nu;
}



/* support routines for internal data structures. */

/* add procedure to procedures list */
void
add_proc(COMPSTATE * cstat, const char *proc_name, struct INTERMEDIATE *place, int rettype)
{
	struct PROC_LIST *nu;

	nu = (struct PROC_LIST *) malloc(sizeof(struct PROC_LIST));

	nu->name = alloc_string(proc_name);
	nu->returntype = rettype;
	nu->code = place;
	nu->next = cstat->procs;
	cstat->procs = nu;
}

/* add if to control stack */
void
add_control_structure(COMPSTATE * cstat, int typ, struct INTERMEDIATE *place)
{
	struct CONTROL_STACK *nu;

	nu = (struct CONTROL_STACK *) malloc(sizeof(struct CONTROL_STACK));

	nu->place = place;
	nu->type = typ;
	nu->next = cstat->control_stack;
	nu->extra = 0;
	cstat->control_stack = nu;
}

/* add while to current loop's list of exits remaining to be resolved. */
void
add_loop_exit(COMPSTATE * cstat, struct INTERMEDIATE *place)
{
	struct CONTROL_STACK *nu;
	struct CONTROL_STACK *loop;

	loop = cstat->control_stack;

	while (loop && loop->type != CTYPE_BEGIN && loop->type != CTYPE_FOR) {
		loop = loop->next;
	}

	if (!loop)
		return;

	nu = (struct CONTROL_STACK *) malloc(sizeof(struct CONTROL_STACK));

	nu->place = place;
	nu->type = CTYPE_WHILE;
	nu->next = 0;
	nu->extra = loop->extra;
	loop->extra = nu;
}

/* Returns true if a loop start is in the control structure stack. */
int
in_loop(COMPSTATE * cstat)
{
	struct CONTROL_STACK *loop;

	loop = cstat->control_stack;
	while (loop && loop->type != CTYPE_BEGIN && loop->type != CTYPE_FOR) {
		loop = loop->next;
	}
	return (loop != NULL);
}

/* Returns the type of the innermost nested control structure. */
int
innermost_control_type(COMPSTATE * cstat)
{
	struct CONTROL_STACK *ctrl;

	ctrl = cstat->control_stack;
	if (!ctrl)
		return 0;

	return ctrl->type;
}

/* Returns number of TRYs before topmost Loop */
int
count_trys_inside_loop(COMPSTATE* cstat)
{
	struct CONTROL_STACK *loop;
	int count = 0;

	loop = cstat->control_stack;

	while (loop) {
		if (loop->type == CTYPE_FOR || loop->type == CTYPE_BEGIN) {
			break;
		}
		if (loop->type == CTYPE_TRY) {
			count++;
		}
		loop = loop->next;
	}

	return count;
}

/* returns topmost begin or for off the stack */
struct INTERMEDIATE *
locate_control_structure(COMPSTATE* cstat, int type1, int type2)
{
	struct CONTROL_STACK *loop;

	loop = cstat->control_stack;

	while (loop) {
		if (loop->type == type1 || loop->type == type2) {
			return loop->place;
		}
		loop = loop->next;
	}

	return 0;
}

/* checks if topmost loop stack item is a for */
struct INTERMEDIATE *
innermost_control_place(COMPSTATE * cstat, int type1)
{
	struct CONTROL_STACK *ctrl;

	ctrl = cstat->control_stack;
	if (!ctrl)
		return 0;
	if (ctrl->type != type1)
		return 0;

	return ctrl->place;
}

/* Pops off the innermost control structure and returns the place. */
struct INTERMEDIATE *
pop_control_structure(COMPSTATE * cstat, int type1, int type2)
{
	struct CONTROL_STACK *ctrl;
	struct INTERMEDIATE *place;

	ctrl = cstat->control_stack;
	if (!ctrl)
		return NULL;
	if (ctrl->type != type1 && ctrl->type != type2)
		return NULL;

	place = ctrl->place;
	cstat->control_stack = ctrl->next;
	free(ctrl);

	return place;
}

/* pops first while off the innermost control structure, if it's a loop. */
struct INTERMEDIATE *
pop_loop_exit(COMPSTATE * cstat)
{
	struct INTERMEDIATE *temp;
	struct CONTROL_STACK *tofree;
	struct CONTROL_STACK *parent;

	parent = cstat->control_stack;

	if (!parent)
		return 0;
	if (parent->type != CTYPE_BEGIN && parent->type != CTYPE_FOR)
		return 0;
	if (!parent->extra)
		return 0;
	if (parent->extra->type != CTYPE_WHILE)
		return 0;

	temp = parent->extra->place;
	tofree = parent->extra;
	parent->extra = parent->extra->extra;
	free((void *) tofree);
	return temp;
}

void
resolve_loop_addrs(COMPSTATE * cstat, int where)
{
	struct INTERMEDIATE *eef;

	while ((eef = pop_loop_exit(cstat)))
		eef->in.data.number = where;
	eef = innermost_control_place(cstat, CTYPE_FOR);
	if (eef) {
		eef->next->in.data.number = where;
	}
}

/* adds variable.  Return 0 if no space left */
int
add_variable(COMPSTATE * cstat, const char *varname, int valtype)
{
	int i;

	for (i = RES_VAR; i < MAX_VAR; i++)
		if (!cstat->variables[i])
			break;

	if (i == MAX_VAR)
		return 0;

	cstat->variables[i] = alloc_string(varname);
	cstat->variabletypes[i] = valtype;
	return i;
}


/* adds local variable.  Return 0 if no space left */
int
add_scopedvar(COMPSTATE * cstat, const char *varname, int valtype)
{
	int i;

	for (i = 0; i < MAX_VAR; i++)
		if (!cstat->scopedvars[i])
			break;

	if (i == MAX_VAR)
		return -1;

	cstat->scopedvars[i] = alloc_string(varname);
	cstat->scopedvartypes[i] = valtype;
	return i;
}


/* adds local variable.  Return 0 if no space left */
int
add_localvar(COMPSTATE * cstat, const char *varname, int valtype)
{
	int i;

	for (i = 0; i < MAX_VAR; i++)
		if (!cstat->localvars[i])
			break;

	if (i == MAX_VAR)
		return -1;

	cstat->localvars[i] = alloc_string(varname);
	cstat->localvartypes[i] = valtype;
	return i;
}


/* predicates for procedure calls */
int
special(const char *token)
{
	return (token && !(string_compare(token, ":")
					   && string_compare(token, ";")
					   && string_compare(token, "IF")
					   && string_compare(token, "ELSE")
					   && string_compare(token, "THEN")
					   && string_compare(token, "BEGIN")
					   && string_compare(token, "FOR")
					   && string_compare(token, "FOREACH")
					   && string_compare(token, "UNTIL")
					   && string_compare(token, "WHILE")
					   && string_compare(token, "BREAK")
					   && string_compare(token, "CONTINUE")
					   && string_compare(token, "REPEAT")
					   && string_compare(token, "TRY")
					   && string_compare(token, "CATCH")
					   && string_compare(token, "ENDCATCH")
					   && string_compare(token, "CALL")
					   && string_compare(token, "PUBLIC")
					   && string_compare(token, "WIZCALL")
					   && string_compare(token, "LVAR")
					   && string_compare(token, "VAR!")
					   && string_compare(token, "VAR")));
}

/* see if procedure call */
int
call(COMPSTATE * cstat, const char *token)
{
	struct PROC_LIST *i;

	for (i = cstat->procs; i; i = i->next)
		if (!string_compare(i->name, token))
			return 1;

	return 0;
}

/* see if it's a quoted procedure name */
int
quoted(COMPSTATE * cstat, const char *token)
{
	return (*token == '\'' && call(cstat, token + 1));
}

/* see if it's an object # */
int
object(const char *token)
{
	if (*token == '#' && number(token + 1))
		return 1;
	else
		return 0;
}

/* see if string */
int
string(const char *token)
{
	return (token[0] == '"');
}

int
variable(COMPSTATE * cstat, const char *token)
{
	int i;

	for (i = 0; i < MAX_VAR && cstat->variables[i]; i++)
		if (!string_compare(token, cstat->variables[i]))
			return 1;

	return 0;
}

int
scopedvar(COMPSTATE * cstat, const char *token)
{
	int i;

	for (i = 0; i < MAX_VAR && cstat->scopedvars[i]; i++)
		if (!string_compare(token, cstat->scopedvars[i]))
			return 1;

	return 0;
}

int
localvar(COMPSTATE * cstat, const char *token)
{
	int i;

	for (i = 0; i < MAX_VAR && cstat->localvars[i]; i++)
		if (!string_compare(token, cstat->localvars[i]))
			return 1;

	return 0;
}

/* see if token is primitive */
int
primitive(const char *token)
{
	int primnum;

	primnum = get_primitive(token);
	return (primnum && primnum <= BASE_MAX - PRIMS_INTERNAL_CNT);
}

/* return primitive instruction */
int
get_primitive(const char *token)
{
	hash_data *hd;

	if ((hd = find_hash(token, primitive_list, COMP_HASH_SIZE)) == NULL)
		return 0;
	else {
		return (hd->ival);
	}
}



/* clean up as nicely as we can. */

void
cleanpubs(struct publics *mypub)
{
	struct publics *tmppub;

	while (mypub) {
		tmppub = mypub->next;
		free(mypub->subname);
		free(mypub);
		mypub = tmppub;
	}
}

void
clean_mcpbinds(struct mcp_binding *mypub)
{
	struct mcp_binding *tmppub;

	while (mypub) {
		tmppub = mypub->next;
		free(mypub->pkgname);
		free(mypub->msgname);
		free(mypub);
		mypub = tmppub;
	}
}


void
append_intermediate_chain(struct INTERMEDIATE *chain, struct INTERMEDIATE *add)
{
	while (chain->next)
		chain = chain->next;
	chain->next = add;
}


void
free_intermediate_chain(struct INTERMEDIATE *wd)
{
	struct INTERMEDIATE* tempword;
	while (wd) {
		tempword = wd->next;
		if (wd->in.type == PROG_STRING) {
			if (wd->in.data.string)
				free((void *) wd->in.data.string);
		}
		if (wd->in.type == PROG_FUNCTION) {
			free((void*)wd->in.data.mufproc->procname);
			free((void*)wd->in.data.mufproc);
		}
		free((void *) wd);
		wd = tempword;
	}
}

void
cleanup(COMPSTATE * cstat)
{
/*	struct INTERMEDIATE *wd, *tempword; */
	struct CONTROL_STACK *eef, *tempif;
	struct PROC_LIST *p, *tempp;
	int i;

	free_intermediate_chain(cstat->first_word);
	cstat->first_word = 0;

	for (eef = cstat->control_stack; eef; eef = tempif) {
		tempif = eef->next;
		free((void *) eef);
	}
	cstat->control_stack = 0;

	for (p = cstat->procs; p; p = tempp) {
		tempp = p->next;
		free((void *) p->name);
		free((void *) p);
	}
	cstat->procs = 0;

	purge_defs(cstat);
	free_addresses(cstat);

	for (i = RES_VAR; i < MAX_VAR && cstat->variables[i]; i++) {
		free((void *) cstat->variables[i]);
		cstat->variables[i] = 0;
	}

	for (i = 0; i < MAX_VAR && cstat->scopedvars[i]; i++) {
		free((void *) cstat->scopedvars[i]);
		cstat->scopedvars[i] = 0;
	}

	for (i = 0; i < MAX_VAR && cstat->localvars[i]; i++) {
		free((void *) cstat->localvars[i]);
		cstat->localvars[i] = 0;
	}
}



/* copy program to an array */
void
copy_program(COMPSTATE * cstat)
{
	/*
	 * Everything should be peachy keen now, so we don't do any error checking
	 */
	struct INTERMEDIATE *curr;
	struct inst *code;
	int i;

	if (!cstat->first_word)
		v_abort_compile(cstat, "Nothing to compile.");

	code = (struct inst *) malloc(sizeof(struct inst) * (cstat->nowords + 1));

	i = 0;
	for (curr = cstat->first_word; curr; curr = curr->next) {
		code[i].type = curr->in.type;
		code[i].line = curr->in.line;
		switch (code[i].type) {
		case PROG_PRIMITIVE:
		case PROG_INTEGER:
		case PROG_SVAR:
		case PROG_SVAR_AT:
		case PROG_SVAR_BANG:
		case PROG_LVAR:
		case PROG_VAR:
			code[i].data.number = curr->in.data.number;
			break;
		case PROG_FLOAT:
			code[i].data.fnumber = curr->in.data.fnumber;
			break;
		case PROG_STRING:
			code[i].data.string = curr->in.data.string ?
					alloc_prog_string(curr->in.data.string->data) : 0;
			break;
		case PROG_FUNCTION:
			code[i].data.mufproc = (struct muf_proc_data*)malloc(sizeof(struct muf_proc_data));
			code[i].data.mufproc->procname = string_dup(curr->in.data.mufproc->procname);
			code[i].data.mufproc->vars = curr->in.data.mufproc->vars;
			code[i].data.mufproc->args = curr->in.data.mufproc->args;
			break;
		case PROG_OBJECT:
			code[i].data.objref = curr->in.data.objref;
			break;
		case PROG_ADD:
			code[i].data.addr = alloc_addr(cstat, curr->in.data.number, code);
			break;
		case PROG_IF:
		case PROG_JMP:
		case PROG_EXEC:
		case PROG_TRY:
			code[i].data.call = code + curr->in.data.number;
			break;
		default:
			v_abort_compile(cstat, "Unknown type compile!  Internal error.");
			break;
		}
		i++;
	}
	PROGRAM_SET_CODE(cstat->program, code);
}

void
set_start(COMPSTATE * cstat)
{
	PROGRAM_SET_SIZ(cstat->program, cstat->nowords);

	/* address instr no is resolved before this gets called. */
	PROGRAM_SET_START(cstat->program, (PROGRAM_CODE(cstat->program) + cstat->procs->code->no));
}


/* allocate and initialize data linked structure. */
struct INTERMEDIATE *
alloc_inst(void)
{
	struct INTERMEDIATE *nu;

	nu = (struct INTERMEDIATE *) malloc(sizeof(struct INTERMEDIATE));

	nu->next = 0;
	nu->no = 0;
	nu->in.type = 0;
	nu->in.line = 0;
	nu->in.data.number = 0;
	return nu;
}

struct INTERMEDIATE *
prealloc_inst(COMPSTATE * cstat)
{
	struct INTERMEDIATE *ptr;
	struct INTERMEDIATE *nu;

	/* only allocate at most one extra instr */
	if (cstat->nextinst)
		return NULL;

	nu = alloc_inst();

	if (!cstat->nextinst) {
		cstat->nextinst = nu;
	} else {
		for (ptr = cstat->nextinst; ptr->next; ptr = ptr->next);
		ptr->next = nu;
	}

	nu->no = cstat->nowords;

	return nu;
}

struct INTERMEDIATE *
new_inst(COMPSTATE * cstat)
{
	struct INTERMEDIATE *nu;

	nu = cstat->nextinst;
	if (!nu) {
		nu = alloc_inst();
	}
	cstat->nextinst = nu->next;
	nu->next = NULL;

	return nu;
}

/* allocate an address */
struct prog_addr *
alloc_addr(COMPSTATE * cstat, int offset, struct inst *codestart)
{
	struct prog_addr *nu;

	nu = (struct prog_addr *) malloc(sizeof(struct prog_addr));

	nu->links = 1;
	nu->progref = cstat->program;
	nu->data = codestart + offset;
	return nu;
}

void
free_prog(dbref prog)
{
	int i;
	struct inst *c = PROGRAM_CODE(prog);
	int siz = PROGRAM_SIZ(prog);

	if (c) {
		if (PROGRAM_INSTANCES(prog)) {
			fprintf(stderr, "Freeing program %s with %d instances reported\n",
					unparse_object(GOD, prog), PROGRAM_INSTANCES(prog));
		}
		i = scan_instances(prog);
		if (i) {
			fprintf(stderr, "Freeing program %s with %d instances found\n",
					unparse_object(GOD, prog), i);
		}
		for (i = 0; i < siz; i++) {
			if (c[i].type == PROG_ADD) {
				if (c[i].data.addr->links != 1) {
					fprintf(stderr, "Freeing address in %s with link count %d\n",
							unparse_object(GOD, prog), c[i].data.addr->links);
				}
				free(c[i].data.addr);
			}
			else
			{
				CLEAR(c + i);
			}
		}
		free((void *) c);
	}
	PROGRAM_SET_CODE(prog, 0);
	PROGRAM_SET_SIZ(prog, 0);
	PROGRAM_SET_START(prog, 0);
}

long
size_prog(dbref prog)
{
	struct inst *c;
	long i, siz, byts = 0;

	c = PROGRAM_CODE(prog);
	if (!c)
		return 0;
	siz = PROGRAM_SIZ(prog);
	for (i = 0L; i < siz; i++) {
		byts += sizeof(*c);
		if (c[i].type == PROG_FUNCTION) {
			byts += strlen(c[i].data.mufproc->procname) + 1;
			byts += sizeof(struct muf_proc_data);
		} else if (c[i].type == PROG_STRING && c[i].data.string) {
			byts += strlen(c[i].data.string->data) + 1;
			byts += sizeof(struct shared_string);
		} else if ((c[i].type == PROG_ADD))
			byts += sizeof(struct prog_addr);
	}
	byts += size_pubs(PROGRAM_PUBS(prog));
	return byts;
}

static void
add_primitive(int val)
{
	hash_data hd;

	hd.ival = val;
	if (add_hash(base_inst[val - BASE_MIN], hd, primitive_list, COMP_HASH_SIZE) == NULL)
		panic("Out of memory");
	else
		return;
}

void
clear_primitives(void)
{
	kill_hash(primitive_list, COMP_HASH_SIZE, 0);
	return;
}

void
init_primitives(void)
{
	int i;

	clear_primitives();
	for (i = BASE_MIN; i <= BASE_MAX; i++) {
		add_primitive(i);
	}
	IN_FORPOP = get_primitive(" FORPOP");
	IN_FORITER = get_primitive(" FORITER");
	IN_FOR = get_primitive(" FOR");
	IN_FOREACH = get_primitive(" FOREACH");
	IN_TRYPOP = get_primitive(" TRYPOP");
	log_status("MUF: %d primitives exist.\n", BASE_MAX);
}

