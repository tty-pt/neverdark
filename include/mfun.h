#ifndef _MFUN_H
#define _MFUN_H

/* Include definition of MFUNARGS */
#include "msgparse.h"

const char *mfn_abs(MFUNARGS);
const char *mfn_add(MFUNARGS);
const char *mfn_and(MFUNARGS);
const char *mfn_attr(MFUNARGS);
const char *mfn_awake(MFUNARGS);
const char *mfn_bless(MFUNARGS);
const char *mfn_center(MFUNARGS);
const char *mfn_commas(MFUNARGS);
const char *mfn_concat(MFUNARGS);
const char *mfn_contains(MFUNARGS);
const char *mfn_contents(MFUNARGS);
const char *mfn_controls(MFUNARGS);
const char *mfn_convsecs(MFUNARGS);
const char *mfn_convtime(MFUNARGS);
const char *mfn_count(MFUNARGS);
const char *mfn_created(MFUNARGS);
const char *mfn_date(MFUNARGS);
const char *mfn_dbeq(MFUNARGS);
const char *mfn_debug(MFUNARGS);
const char *mfn_debugif(MFUNARGS);
const char *mfn_dec(MFUNARGS);
const char *mfn_default(MFUNARGS);
const char *mfn_delay(MFUNARGS);
const char *mfn_delprop(MFUNARGS);
const char *mfn_dice(MFUNARGS);
const char *mfn_dist(MFUNARGS);
const char *mfn_div(MFUNARGS);
const char *mfn_eq(MFUNARGS);
const char *mfn_escape(MFUNARGS);
const char *mfn_eval(MFUNARGS);
const char *mfn_evalbang(MFUNARGS);
const char *mfn_exec(MFUNARGS);
const char *mfn_execbang(MFUNARGS);
const char *mfn_exits(MFUNARGS);
const char *mfn_filter(MFUNARGS);
const char *mfn_flags(MFUNARGS);
const char *mfn_fold(MFUNARGS);
const char *mfn_for(MFUNARGS);
const char *mfn_force(MFUNARGS);
const char *mfn_foreach(MFUNARGS);
const char *mfn_fox(MFUNARGS);
const char *mfn_ftime(MFUNARGS);
const char *mfn_fullname(MFUNARGS);
const char *mfn_func(MFUNARGS);
const char *mfn_ge(MFUNARGS);
const char *mfn_gt(MFUNARGS);
const char *mfn_holds(MFUNARGS);
const char *mfn_idle(MFUNARGS);
const char *mfn_if(MFUNARGS);
const char *mfn_inc(MFUNARGS);
const char *mfn_index(MFUNARGS);
const char *mfn_indexbang(MFUNARGS);
const char *mfn_instr(MFUNARGS);
const char *mfn_isdbref(MFUNARGS);
const char *mfn_isnum(MFUNARGS);
const char *mfn_istype(MFUNARGS);
const char *mfn_kill(MFUNARGS);
const char *mfn_lastused(MFUNARGS);
const char *mfn_lcommon(MFUNARGS);
const char *mfn_le(MFUNARGS);
const char *mfn_left(MFUNARGS);
const char *mfn_lexec(MFUNARGS);
const char *mfn_links(MFUNARGS);
const char *mfn_list(MFUNARGS);
const char *mfn_listprops(MFUNARGS);
const char *mfn_lit(MFUNARGS);
const char *mfn_lmember(MFUNARGS);
const char *mfn_loc(MFUNARGS);
const char *mfn_locked(MFUNARGS);
const char *mfn_log(MFUNARGS);
const char *mfn_lrand(MFUNARGS);
const char *mfn_lremove(MFUNARGS);
const char *mfn_lsort(MFUNARGS);
const char *mfn_lt(MFUNARGS);
const char *mfn_ltimestr(MFUNARGS);
const char *mfn_lunion(MFUNARGS);
const char *mfn_lunique(MFUNARGS);
const char *mfn_max(MFUNARGS);
const char *mfn_midstr(MFUNARGS);
const char *mfn_min(MFUNARGS);
const char *mfn_mklist(MFUNARGS);
const char *mfn_mod(MFUNARGS);
const char *mfn_modified(MFUNARGS);
const char *mfn_money(MFUNARGS);
const char *mfn_muckname(MFUNARGS);
const char *mfn_muf(MFUNARGS);
const char *mfn_mult(MFUNARGS);
const char *mfn_name(MFUNARGS);
const char *mfn_ne(MFUNARGS);
const char *mfn_nearby(MFUNARGS);
const char *mfn_nl(MFUNARGS);
const char *mfn_not(MFUNARGS);
const char *mfn_null(MFUNARGS);
const char *mfn_online(MFUNARGS);
const char *mfn_ontime(MFUNARGS);
const char *mfn_or(MFUNARGS);
const char *mfn_otell(MFUNARGS);
const char *mfn_owner(MFUNARGS);
const char *mfn_parse(MFUNARGS);
const char *mfn_pronouns(MFUNARGS);
const char *mfn_prop(MFUNARGS);
const char *mfn_propbang(MFUNARGS);
const char *mfn_propdir(MFUNARGS);
const char *mfn_rand(MFUNARGS);
const char *mfn_ref(MFUNARGS);
const char *mfn_revoke(MFUNARGS);
const char *mfn_right(MFUNARGS);
const char *mfn_secs(MFUNARGS);
const char *mfn_select(MFUNARGS);
const char *mfn_set(MFUNARGS);
const char *mfn_sign(MFUNARGS);
const char *mfn_smatch(MFUNARGS);
const char *mfn_stimestr(MFUNARGS);
const char *mfn_store(MFUNARGS);
const char *mfn_strip(MFUNARGS);
const char *mfn_strlen(MFUNARGS);
const char *mfn_sublist(MFUNARGS);
const char *mfn_subst(MFUNARGS);
const char *mfn_subt(MFUNARGS);
const char *mfn_tell(MFUNARGS);
const char *mfn_testlock(MFUNARGS);
const char *mfn_time(MFUNARGS);
const char *mfn_timing(MFUNARGS);
const char *mfn_timestr(MFUNARGS);
const char *mfn_timesub(MFUNARGS);
const char *mfn_tolower(MFUNARGS);
const char *mfn_toupper(MFUNARGS);
const char *mfn_type(MFUNARGS);
const char *mfn_tzoffset(MFUNARGS);
const char *mfn_unbless(MFUNARGS);
const char *mfn_usecount(MFUNARGS);
const char *mfn_v(MFUNARGS);
const char *mfn_version(MFUNARGS);
const char *mfn_while(MFUNARGS);
const char *mfn_with(MFUNARGS);
const char *mfn_xor(MFUNARGS);

#endif /* _MFUN_H */

#ifdef DEFINE_HEADER_VERSIONS


const char *mfun_h_version = "$RCSfile$ $Revision: 1.9 $";

#else
extern const char *mfun_h_version;
#endif

