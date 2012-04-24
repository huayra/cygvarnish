/*
 * NB:  This file is machine generated, DO NOT EDIT!
 *
 * Edit and run generate.py instead
 */


#include "config.h"
#include <stdio.h>
#include <ctype.h>
#include "config.h"
#include "vcc_priv.h"
#include "vsb.h"

#define M1()	do {*q = p + 1; return (p[0]); } while (0)
#define M2(c,t)	do {if (p[1] == (c)) { *q = p + 2; return (t); }} while (0)

unsigned
vcl_fixed_token(const char *p, const char **q)
{

	switch (p[0]) {
	case '!':
		M2('=', T_NEQ);
		M2('~', T_NOMATCH);
		M1();
	case '%':
		M1();
	case '&':
		M2('&', T_CAND);
		M1();
	case '(':
		M1();
	case ')':
		M1();
	case '*':
		M2('=', T_MUL);
		M1();
	case '+':
		M2('+', T_INC);
		M2('=', T_INCR);
		M1();
	case ',':
		M1();
	case '-':
		M2('-', T_DEC);
		M2('=', T_DECR);
		M1();
	case '.':
		M1();
	case '/':
		M2('=', T_DIV);
		M1();
	case ';':
		M1();
	case '<':
		M2('<', T_SHL);
		M2('=', T_LEQ);
		M1();
	case '=':
		M2('=', T_EQ);
		M1();
	case '>':
		M2('=', T_GEQ);
		M2('>', T_SHR);
		M1();
	case 'e':
		if (p[1] == 'l' && p[2] == 's' && p[3] == 'e' &&
		    p[4] == 'i' && p[5] == 'f' && !isvar(p[6])) {
			*q = p + 6;
			return (T_ELSEIF);
		}
		if (p[1] == 'l' && p[2] == 's' && p[3] == 'i' &&
		    p[4] == 'f' && !isvar(p[5])) {
			*q = p + 5;
			return (T_ELSIF);
		}
		if (p[1] == 'l' && p[2] == 's' && p[3] == 'e' &&
		    !isvar(p[4])) {
			*q = p + 4;
			return (T_ELSE);
		}
		return (0);
	case 'i':
		if (p[1] == 'n' && p[2] == 'c' && p[3] == 'l' &&
		    p[4] == 'u' && p[5] == 'd' && p[6] == 'e' &&
		    !isvar(p[7])) {
			*q = p + 7;
			return (T_INCLUDE);
		}
		M2('f', T_IF);
		return (0);
	case '{':
		M1();
	case '|':
		M2('|', T_COR);
		M1();
	case '}':
		M1();
	case '~':
		M1();
	default:
		return (0);
	}
}

const char * const vcl_tnames[256] = {
	['!'] = "'!'",
	['%'] = "'%'",
	['&'] = "'&'",
	['('] = "'('",
	[')'] = "')'",
	['*'] = "'*'",
	['+'] = "'+'",
	[','] = "','",
	['-'] = "'-'",
	['.'] = "'.'",
	['/'] = "'/'",
	[';'] = "';'",
	['<'] = "'<'",
	['='] = "'='",
	['>'] = "'>'",
	['{'] = "'{'",
	['|'] = "'|'",
	['}'] = "'}'",
	['~'] = "'~'",
	[CNUM] = "CNUM",
	[CSRC] = "CSRC",
	[CSTR] = "CSTR",
	[EOI] = "EOI",
	[ID] = "ID",
	[T_CAND] = "&&",
	[T_COR] = "||",
	[T_DEC] = "--",
	[T_DECR] = "-=",
	[T_DIV] = "/=",
	[T_ELSE] = "else",
	[T_ELSEIF] = "elseif",
	[T_ELSIF] = "elsif",
	[T_EQ] = "==",
	[T_GEQ] = ">=",
	[T_IF] = "if",
	[T_INC] = "++",
	[T_INCLUDE] = "include",
	[T_INCR] = "+=",
	[T_LEQ] = "<=",
	[T_MUL] = "*=",
	[T_NEQ] = "!=",
	[T_NOMATCH] = "!~",
	[T_SHL] = "<<",
	[T_SHR] = ">>",
};

void
vcl_output_lang_h(struct vsb *sb)
{

	/* ../include/vcl.h */

	VSB_cat(sb, "/*\n * NB:  This file is machine generated, DO "
	    "NOT EDIT!\n *\n * Edit and run generate.py instead\n"
	    " */\n\nstruct sess;\nstruct cli;\n\ntypedef int vcl_init_f(struc"
	    "t cli *);\ntypedef void vcl_fini_f(struct cli *);\n"
	    "typedef int vcl_func_f(struct sess *sp);\n\n/* VCL Methods "
	    "*/\n#define VCL_MET_RECV\t\t(1U << 0)\n#define VCL_MET_PIPE\t"
	    "\t(1U << 1)\n#define VCL_MET_PASS\t\t(1U << 2)\n#define VCL_MET_"
	    "HASH\t\t(1U << 3)\n#define VCL_MET_MISS\t\t(1U << 4)\n"
	    "#define VCL_MET_HIT\t\t(1U << 5)\n#define VCL_MET_FETCH\t\t"
	    "(1U << 6)\n#define VCL_MET_DELIVER\t\t(1U << 7)\n"
	    "#define VCL_MET_ERROR\t\t(1U << 8)\n#define VCL_MET_INIT\t\t"
	    "(1U << 9)\n#define VCL_MET_FINI\t\t(1U << 10)\n\n"
	    "#define VCL_MET_MAX\t\t11\n\n#define VCL_MET_MASK\t\t0x7ff\n"
	    "\n/* VCL Returns */\n#define VCL_RET_DELIVER\t\t0\n"
	    "#define VCL_RET_ERROR\t\t1\n#define VCL_RET_FETCH\t\t2\n"
	    "#define VCL_RET_HASH\t\t3\n#define VCL_RET_HIT_FOR_PASS\t\t"
	    "4\n#define VCL_RET_LOOKUP\t\t5\n#define VCL_RET_OK\t\t6\n"
	    "#define VCL_RET_PASS\t\t7\n#define VCL_RET_PIPE\t\t8\n"
	    "#define VCL_RET_RESTART\t\t9\n\n#define VCL_RET_MAX\t\t10\n"
	    "\nstruct VCL_conf {\n\tunsigned\tmagic;\n#define VCL_CONF_MAGIC\t"
	    "0x7406c509\t/* from /dev/random */\n\n\tstruct director\t**direc"
	    "tor;\n\tunsigned\tndirector;\n\tstruct vrt_ref\t*ref;\n"
	    "\tunsigned\tnref;\n\tunsigned\tbusy;\n\tunsigned\tdiscard;\n"
	    "\n\tunsigned\tnsrc;\n\tconst char\t**srcname;\n\tconst char\t"
	    "**srcbody;\n\n\tvcl_init_f\t*init_vcl;\n\tvcl_fini_f\t*fini_vcl;"
	    "\n\tvcl_func_f\t*recv_func;\n\tvcl_func_f\t*pipe_func;\n"
	    "\tvcl_func_f\t*pass_func;\n\tvcl_func_f\t*hash_func;\n"
	    "\tvcl_func_f\t*miss_func;\n\tvcl_func_f\t*hit_func;\n"
	    "\tvcl_func_f\t*fetch_func;\n\tvcl_func_f\t*deliver_func;\n"
	    "\tvcl_func_f\t*error_func;\n\tvcl_func_f\t*init_func;\n"
	    "\tvcl_func_f\t*fini_func;\n\n};\n"	);

	/* ../include/vrt.h */

	VSB_cat(sb, "/*-\n * Copyright (c) 2006 Verdens Gang AS\n"
	    " * Copyright (c) 2006-2010 Varnish Software AS\n * All rights "
	    "reserved.\n *\n * Author: Poul-Henning Kamp <phk@phk.freebsd.dk>"
	    "\n *\n * Redistribution and use in source and binary forms, "
	    "with or without\n * modification, are permitted provided that "
	    "the following conditions\n * are met:\n * 1. Redistributions "
	    "of source code must retain the above copyright\n *    notice, "
	    "this list of conditions and the following disclaimer.\n"
	    " * 2. Redistributions in binary form must reproduce the above "
	    "copyright\n *    notice, this list of conditions and the followi"
	    "ng disclaimer in the\n *    documentation and/or other materials"
	    " provided with the distribution.\n *\n * THIS SOFTWARE IS "
	    "PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND\n"
	    " * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED"
	    " TO, THE\n * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS "
	    "FOR A PARTICULAR PURPOSE\n * ARE DISCLAIMED.  IN NO EVENT "
	    "SHALL AUTHOR OR CONTRIBUTORS BE LIABLE\n * FOR ANY DIRECT, "
	    "INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL\n"
	    " * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF "
	    "SUBSTITUTE GOODS\n * OR SERVICES; LOSS OF USE, DATA, OR PROFITS;"
	    " OR BUSINESS INTERRUPTION)\n * HOWEVER CAUSED AND ON ANY THEORY "
	    "OF LIABILITY, WHETHER IN CONTRACT, STRICT\n * LIABILITY, OR "
	    "TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY\n"
	    " * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE "
	    "POSSIBILITY OF\n * SUCH DAMAGE.\n *\n * Runtime support for "
	    "compiled VCL programs.\n *\n * XXX: When this file is changed, "
	    "lib/libvcl/generate.py *MUST* be rerun.\n */\n\nstruct sess;\n"
	    "struct vsb;\nstruct cli;\nstruct director;\nstruct VCL_conf;\n"
	    "struct sockaddr_storage;\n\n/*\n * A backend probe specification"
	    "\n */\n\nextern const void * const vrt_magic_string_end;\n"
	    "\nstruct vrt_backend_probe {\n\tconst char\t*url;\n"
	    "\tconst char\t*request;\n\tdouble\t\ttimeout;\n\tdouble\t\t"
	    "interval;\n\tunsigned\texp_status;\n\tunsigned\twindow;\n"
	    "\tunsigned\tthreshold;\n\tunsigned\tinitial;\n};\n"
	    "\n/*\n * A backend is a host+port somewhere on the network\n"
	    " */\nstruct vrt_backend {\n\tconst char\t\t\t*vcl_name;\n"
	    "\tconst char\t\t\t*ipv4_addr;\n\tconst char\t\t\t*ipv6_addr;\n"
	    "\tconst char\t\t\t*port;\n\n\tconst unsigned char\t\t*ipv4_socka"
	    "ddr;\n\tconst unsigned char\t\t*ipv6_sockaddr;\n\n"
	    "\tconst char\t\t\t*hosthdr;\n\n\tdouble\t\t\t\tconnect_timeout;\n"
	    "\tdouble\t\t\t\tfirst_byte_timeout;\n\tdouble\t\t\t\tbetween_byt"
	    "es_timeout;\n\tunsigned\t\t\tmax_connections;\n\tunsigned\t"
	    "\t\tsaintmode_threshold;\n\tconst struct vrt_backend_probe\t"
	    "*probe;\n};\n\n/*\n * A director with an unpredictable reply\n"
	    " */\n\nstruct vrt_dir_random_entry {\n\tint\t\t\t\t\thost;\n"
	    "\tdouble\t\t\t\t\tweight;\n};\n\nstruct vrt_dir_random {\n"
	    "\tconst char\t\t\t\t*name;\n\tunsigned\t\t\t\tretries;\n"
	    "\tunsigned\t\t\t\tnmember;\n\tconst struct vrt_dir_random_entry\t"
	    "*members;\n};\n\n/*\n * A director with round robin selection\n"
	    " */\n\nstruct vrt_dir_round_robin_entry {\n\tint\t\t\t\t\thost;\n"
	    "};\n\nstruct vrt_dir_round_robin {\n\tconst char\t\t\t\t*name;\n"
	    "\tunsigned\t\t\t\tnmember;\n\tconst struct vrt_dir_round_robin_e"
	    "ntry\t*members;\n};\n\n/*\n * A director with dns-based selectio"
	    "n\n */\n\nstruct vrt_dir_dns_entry {\n\tint\t\t\t\t\thost;\n"
	    "};\n\nstruct vrt_dir_dns {\n\tconst char\t\t\t\t*name;\n"
	    "\tconst char\t\t\t\t*suffix;\n\tconst double\t\t\t\tttl;\n"
	    "\tunsigned\t\t\t\tnmember;\n\tconst struct vrt_dir_dns_entry\t"
	    "\t*members;\n};\n\n/*\n * other stuff.\n * XXX: document when "
	    "bored\n */\n\nstruct vrt_ref {\n\tunsigned\tsource;\n"
	    "\tunsigned\toffset;\n\tunsigned\tline;\n\tunsigned\tpos;\n"
	    "\tunsigned\tcount;\n\tconst char\t*token;\n};\n\n"
	    "/* ACL related */\n#define VRT_ACL_MAXADDR\t\t16\t/* max(IPv4, "
	    "IPv6) */\n\nvoid VRT_acl_log(const struct sess *, const char "
	    "*msg);\n\n/* Regexp related */\nvoid VRT_re_init(void **, "
	    "const char *);\nvoid VRT_re_fini(void *);\nint VRT_re_match(cons"
	    "t char *, void *re);\nconst char *VRT_regsub(const struct "
	    "sess *sp, int all, const char *,\n    void *, const char *);\n"
	    "\nvoid VRT_panic(const struct sess *sp, const char *, ...);\n"
	    "void VRT_ban(struct sess *sp, char *, ...);\nvoid VRT_ban_string"
	    "(struct sess *sp, const char *);\nvoid VRT_purge(const struct "
	    "sess *sp, double ttl, double grace);\n\nvoid VRT_count(const "
	    "struct sess *, unsigned);\nint VRT_rewrite(const char *, const "
	    "char *);\nvoid VRT_error(struct sess *, unsigned, const char "
	    "*);\nint VRT_switch_config(const char *);\n\nenum gethdr_e "
	    "{ HDR_REQ, HDR_RESP, HDR_OBJ, HDR_BEREQ, HDR_BERESP };\n"
	    "char *VRT_GetHdr(const struct sess *, enum gethdr_e where, "
	    "const char *);\nvoid VRT_SetHdr(const struct sess *, enum "
	    "gethdr_e where, const char *,\n    const char *, ...);\n"
	    "void VRT_handling(struct sess *sp, unsigned hand);\n"
	    "\nvoid VRT_hashdata(const struct sess *sp, const char *str, "
	    "...);\n\n/* Simple stuff */\nint VRT_strcmp(const char *s1, "
	    "const char *s2);\nvoid VRT_memmove(void *dst, const void *src, "
	    "unsigned len);\n\nvoid VRT_ESI(const struct sess *sp);\n"
	    "void VRT_Rollback(struct sess *sp);\n\n/* Synthetic pages "
	    "*/\nvoid VRT_synth_page(const struct sess *sp, unsigned flags, "
	    "const char *, ...);\n\n/* Backend related */\nvoid VRT_init_dir("
	    "struct cli *, struct director **, const char *name,\n"
	    "    int idx, const void *priv);\nvoid VRT_fini_dir(struct "
	    "cli *, struct director *);\n\n/* VMOD/Modules related */\n"
	    "int VRT_Vmod_Init(void **hdl, void *ptr, int len, const char "
	    "*nm,\n    const char *path, struct cli *cli);\nvoid VRT_Vmod_Fin"
	    "i(void **hdl);\n\nstruct vmod_priv;\ntypedef void vmod_priv_free"
	    "_f(void *);\nstruct vmod_priv {\n\tvoid\t\t\t*priv;\n"
	    "\tvmod_priv_free_f\t*free;\n};\n\ntypedef int vmod_init_f(struct"
	    " vmod_priv *,  const struct VCL_conf *);\n\nstatic inline "
	    "void\nvmod_priv_fini(const struct vmod_priv *p)\n"
	    "{\n\n\tif (p->priv != (void*)0 && p->free != (void*)0)\n"
	    "\t\tp->free(p->priv);\n}\n\n/* Stevedore related functions "
	    "*/\nint VRT_Stv(const char *nm);\n\n/* Convert things to string "
	    "*/\n\nchar *VRT_IP_string(const struct sess *sp, const struct "
	    "sockaddr_storage *sa);\nchar *VRT_int_string(const struct "
	    "sess *sp, int);\nchar *VRT_double_string(const struct sess "
	    "*sp, double);\nchar *VRT_time_string(const struct sess *sp, "
	    "double);\nconst char *VRT_bool_string(const struct sess *sp, "
	    "unsigned);\nconst char *VRT_backend_string(const struct sess "
	    "*sp, const struct director *d);\n\n#define VRT_done(sp, hand)\t"
	    "\t\t\\\n\tdo {\t\t\t\t\t\\\n\t\tVRT_handling(sp, hand);\t\t"
	    "\\\n\t\treturn (1);\t\t\t\\\n\t} while (0)\n\nconst char *VRT_Wr"
	    "kString(const struct sess *sp, const char *p, ...);\n"
	);

	/* ../include/vrt_obj.h */

	VSB_cat(sb, "/*\n * NB:  This file is machine generated, DO "
	    "NOT EDIT!\n *\n * Edit and run generate.py instead\n"
	    " */\nstruct sockaddr_storage * VRT_r_client_ip(const struct "
	    "sess *);\nconst char * VRT_r_client_identity(struct sess *);\n"
	    "void VRT_l_client_identity(struct sess *, const char *, ...);\n"
	    "struct sockaddr_storage * VRT_r_server_ip(struct sess *);\n"
	    "const char * VRT_r_server_hostname(struct sess *);\n"
	    "const char * VRT_r_server_identity(struct sess *);\n"
	    "int VRT_r_server_port(struct sess *);\nconst char * VRT_r_req_re"
	    "quest(const struct sess *);\nvoid VRT_l_req_request(const "
	    "struct sess *, const char *, ...);\nconst char * VRT_r_req_url(c"
	    "onst struct sess *);\nvoid VRT_l_req_url(const struct sess "
	    "*, const char *, ...);\nconst char * VRT_r_req_proto(const "
	    "struct sess *);\nvoid VRT_l_req_proto(const struct sess *, "
	    "const char *, ...);\nstruct director * VRT_r_req_backend(struct "
	    "sess *);\nvoid VRT_l_req_backend(struct sess *, struct director "
	    "*);\nint VRT_r_req_restarts(const struct sess *);\n"
	    "int VRT_r_req_esi_level(const struct sess *);\ndouble VRT_r_req_"
	    "ttl(struct sess *);\nvoid VRT_l_req_ttl(struct sess *, double);\n"
	    "double VRT_r_req_grace(struct sess *);\nvoid VRT_l_req_grace(str"
	    "uct sess *, double);\ndouble VRT_r_req_keep(struct sess *);\n"
	    "void VRT_l_req_keep(struct sess *, double);\nconst char * "
	    "VRT_r_req_xid(struct sess *);\nunsigned VRT_r_req_esi(struct "
	    "sess *);\nvoid VRT_l_req_esi(struct sess *, unsigned);\n"
	    "unsigned VRT_r_req_can_gzip(struct sess *);\nunsigned VRT_r_req_"
	    "backend_healthy(const struct sess *);\nunsigned VRT_r_req_hash_i"
	    "gnore_busy(struct sess *);\nvoid VRT_l_req_hash_ignore_busy(stru"
	    "ct sess *, unsigned);\nunsigned VRT_r_req_hash_always_miss(struc"
	    "t sess *);\nvoid VRT_l_req_hash_always_miss(struct sess *, "
	    "unsigned);\nconst char * VRT_r_bereq_request(const struct "
	    "sess *);\nvoid VRT_l_bereq_request(const struct sess *, const "
	    "char *, ...);\nconst char * VRT_r_bereq_url(const struct sess "
	    "*);\nvoid VRT_l_bereq_url(const struct sess *, const char "
	    "*, ...);\nconst char * VRT_r_bereq_proto(const struct sess "
	    "*);\nvoid VRT_l_bereq_proto(const struct sess *, const char "
	    "*, ...);\ndouble VRT_r_bereq_connect_timeout(struct sess *);\n"
	    "void VRT_l_bereq_connect_timeout(struct sess *, double);\n"
	    "double VRT_r_bereq_first_byte_timeout(struct sess *);\n"
	    "void VRT_l_bereq_first_byte_timeout(struct sess *, double);\n"
	    "double VRT_r_bereq_between_bytes_timeout(struct sess *);\n"
	    "void VRT_l_bereq_between_bytes_timeout(struct sess *, double);\n"
	    "const char * VRT_r_beresp_proto(const struct sess *);\n"
	    "void VRT_l_beresp_proto(const struct sess *, const char *, "
	    "...);\nvoid VRT_l_beresp_saintmode(const struct sess *, double);"
	    "\nint VRT_r_beresp_status(const struct sess *);\n"
	    "void VRT_l_beresp_status(const struct sess *, int);\n"
	    "const char * VRT_r_beresp_response(const struct sess *);\n"
	    "void VRT_l_beresp_response(const struct sess *, const char "
	    "*, ...);\nunsigned VRT_r_beresp_do_esi(const struct sess *);\n"
	    "void VRT_l_beresp_do_esi(const struct sess *, unsigned);\n"
	    "unsigned VRT_r_beresp_do_stream(const struct sess *);\n"
	    "void VRT_l_beresp_do_stream(const struct sess *, unsigned);\n"
	    "unsigned VRT_r_beresp_do_gzip(const struct sess *);\n"
	    "void VRT_l_beresp_do_gzip(const struct sess *, unsigned);\n"
	    "unsigned VRT_r_beresp_do_gunzip(const struct sess *);\n"
	    "void VRT_l_beresp_do_gunzip(const struct sess *, unsigned);\n"
	    "double VRT_r_beresp_ttl(struct sess *);\nvoid VRT_l_beresp_ttl(s"
	    "truct sess *, double);\ndouble VRT_r_beresp_grace(struct sess "
	    "*);\nvoid VRT_l_beresp_grace(struct sess *, double);\n"
	    "double VRT_r_beresp_keep(struct sess *);\nvoid VRT_l_beresp_keep"
	    "(struct sess *, double);\nconst char * VRT_r_beresp_backend_name"
	    "(const struct sess *);\nstruct sockaddr_storage * VRT_r_beresp_b"
	    "ackend_ip(const struct sess *);\nint VRT_r_beresp_backend_port(c"
	    "onst struct sess *);\nconst char * VRT_r_beresp_storage(struct "
	    "sess *);\nvoid VRT_l_beresp_storage(struct sess *, const char "
	    "*, ...);\nconst char * VRT_r_obj_proto(const struct sess *);\n"
	    "void VRT_l_obj_proto(const struct sess *, const char *, ...);\n"
	    "int VRT_r_obj_status(const struct sess *);\nvoid VRT_l_obj_statu"
	    "s(const struct sess *, int);\nconst char * VRT_r_obj_response(co"
	    "nst struct sess *);\nvoid VRT_l_obj_response(const struct "
	    "sess *, const char *, ...);\nint VRT_r_obj_hits(const struct "
	    "sess *);\ndouble VRT_r_obj_ttl(struct sess *);\nvoid VRT_l_obj_t"
	    "tl(struct sess *, double);\ndouble VRT_r_obj_grace(struct "
	    "sess *);\nvoid VRT_l_obj_grace(struct sess *, double);\n"
	    "double VRT_r_obj_keep(struct sess *);\nvoid VRT_l_obj_keep(struc"
	    "t sess *, double);\ndouble VRT_r_obj_lastuse(const struct "
	    "sess *);\nconst char * VRT_r_resp_proto(const struct sess "
	    "*);\nvoid VRT_l_resp_proto(const struct sess *, const char "
	    "*, ...);\nint VRT_r_resp_status(const struct sess *);\n"
	    "void VRT_l_resp_status(const struct sess *, int);\n"
	    "const char * VRT_r_resp_response(const struct sess *);\n"
	    "void VRT_l_resp_response(const struct sess *, const char *, "
	    "...);\ndouble VRT_r_now(const struct sess *);\ndouble VRT_Stv_fr"
	    "ee_space(const char *);\ndouble VRT_Stv_used_space(const char "
	    "*);\nunsigned VRT_Stv_happy(const char *);\n"	);

}
