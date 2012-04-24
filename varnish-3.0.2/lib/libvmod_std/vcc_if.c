/*
 * NB:  This file is machine generated, DO NOT EDIT!
 *
 * Edit vmod.vcc and run vmod.py instead
 */

#include "vrt.h"
#include "vcc_if.h"
#include "vmod_abi.h"


typedef const char * td_std_toupper(struct sess *, struct vmod_priv *, const char *, ...);
typedef const char * td_std_tolower(struct sess *, struct vmod_priv *, const char *, ...);
typedef void td_std_set_ip_tos(struct sess *, int);
typedef double td_std_random(struct sess *, double, double);
typedef void td_std_log(struct sess *, const char *, ...);
typedef void td_std_syslog(struct sess *, int, const char *, ...);
typedef const char * td_std_fileread(struct sess *, struct vmod_priv *, const char *);
typedef const char * td_std_author(struct sess *, const char *);
typedef double td_std_duration(struct sess *, const char *, double);
typedef int td_std_integer(struct sess *, const char *, int);
typedef void td_std_collect(struct sess *, enum gethdr_e, const char *);

const char Vmod_Name[] = "std";
const struct Vmod_Func_std {
	td_std_toupper	*toupper;
	td_std_tolower	*tolower;
	td_std_set_ip_tos	*set_ip_tos;
	td_std_random	*random;
	td_std_log	*log;
	td_std_syslog	*syslog;
	td_std_fileread	*fileread;
	td_std_author	*author;
	td_std_duration	*duration;
	td_std_integer	*integer;
	td_std_collect	*collect;
	vmod_init_f	*_init;
} Vmod_Func = {
	vmod_toupper,
	vmod_tolower,
	vmod_set_ip_tos,
	vmod_random,
	vmod_log,
	vmod_syslog,
	vmod_fileread,
	vmod_author,
	vmod_duration,
	vmod_integer,
	vmod_collect,
	init_function,
};

const int Vmod_Len = sizeof(Vmod_Func);

const char Vmod_Proto[] =
	"typedef const char * td_std_toupper(struct sess *, struct vmod_priv *, const char *, ...);\n"
	"typedef const char * td_std_tolower(struct sess *, struct vmod_priv *, const char *, ...);\n"
	"typedef void td_std_set_ip_tos(struct sess *, int);\n"
	"typedef double td_std_random(struct sess *, double, double);\n"
	"typedef void td_std_log(struct sess *, const char *, ...);\n"
	"typedef void td_std_syslog(struct sess *, int, const char *, ...);\n"
	"typedef const char * td_std_fileread(struct sess *, struct vmod_priv *, const char *);\n"
	"typedef const char * td_std_author(struct sess *, const char *);\n"
	"typedef double td_std_duration(struct sess *, const char *, double);\n"
	"typedef int td_std_integer(struct sess *, const char *, int);\n"
	"typedef void td_std_collect(struct sess *, enum gethdr_e, const char *);\n"
	"\n"
	"struct Vmod_Func_std {\n"
	"	td_std_toupper	*toupper;\n"
	"	td_std_tolower	*tolower;\n"
	"	td_std_set_ip_tos	*set_ip_tos;\n"
	"	td_std_random	*random;\n"
	"	td_std_log	*log;\n"
	"	td_std_syslog	*syslog;\n"
	"	td_std_fileread	*fileread;\n"
	"	td_std_author	*author;\n"
	"	td_std_duration	*duration;\n"
	"	td_std_integer	*integer;\n"
	"	td_std_collect	*collect;\n"
	"	vmod_init_f	*_init;\n"
	"} Vmod_Func_std;\n"
	;

const char * const Vmod_Spec[] = {
	"std.toupper\0Vmod_Func_std.toupper\0STRING\0PRIV_CALL\0STRING_LIST\0",
	"std.tolower\0Vmod_Func_std.tolower\0STRING\0PRIV_VCL\0STRING_LIST\0",
	"std.set_ip_tos\0Vmod_Func_std.set_ip_tos\0VOID\0INT\0",
	"std.random\0Vmod_Func_std.random\0REAL\0REAL\0REAL\0",
	"std.log\0Vmod_Func_std.log\0VOID\0STRING_LIST\0",
	"std.syslog\0Vmod_Func_std.syslog\0VOID\0INT\0STRING_LIST\0",
	"std.fileread\0Vmod_Func_std.fileread\0STRING\0PRIV_CALL\0STRING\0",
	"std.author\0Vmod_Func_std.author\0STRING\0ENUM\0phk\0des\0kristian\0mithrandir\0\0",
	"std.duration\0Vmod_Func_std.duration\0DURATION\0STRING\0DURATION\0",
	"std.integer\0Vmod_Func_std.integer\0INT\0STRING\0INT\0",
	"std.collect\0Vmod_Func_std.collect\0VOID\0HEADER\0",
	"INIT\0Vmod_Func_std._init",
	0
};
const char Vmod_Varnish_ABI[] = VMOD_ABI_Version;
const void * const Vmod_Id = &Vmod_Id;

