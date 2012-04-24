/*-
 * Copyright (c) 2006 Verdens Gang AS
 * Copyright (c) 2006-2010 Varnish Software AS
 * All rights reserved.
 *
 * Author: Poul-Henning Kamp <phk@phk.freebsd.dk>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Runtime support for compiled VCL programs.
 *
 * XXX: When this file is changed, lib/libvcl/generate.py *MUST* be rerun.
 */

struct sess;
struct vsb;
struct cli;
struct director;
struct VCL_conf;
struct sockaddr_storage;

/*
 * A backend probe specification
 */

extern const void * const vrt_magic_string_end;

struct vrt_backend_probe {
	const char	*url;
	const char	*request;
	double		timeout;
	double		interval;
	unsigned	exp_status;
	unsigned	window;
	unsigned	threshold;
	unsigned	initial;
};

/*
 * A backend is a host+port somewhere on the network
 */
struct vrt_backend {
	const char			*vcl_name;
	const char			*ipv4_addr;
	const char			*ipv6_addr;
	const char			*port;

	const unsigned char		*ipv4_sockaddr;
	const unsigned char		*ipv6_sockaddr;

	const char			*hosthdr;

	double				connect_timeout;
	double				first_byte_timeout;
	double				between_bytes_timeout;
	unsigned			max_connections;
	unsigned			saintmode_threshold;
	const struct vrt_backend_probe	*probe;
};

/*
 * A director with an unpredictable reply
 */

struct vrt_dir_random_entry {
	int					host;
	double					weight;
};

struct vrt_dir_random {
	const char				*name;
	unsigned				retries;
	unsigned				nmember;
	const struct vrt_dir_random_entry	*members;
};

/*
 * A director with round robin selection
 */

struct vrt_dir_round_robin_entry {
	int					host;
};

struct vrt_dir_round_robin {
	const char				*name;
	unsigned				nmember;
	const struct vrt_dir_round_robin_entry	*members;
};

/*
 * A director with dns-based selection
 */

struct vrt_dir_dns_entry {
	int					host;
};

struct vrt_dir_dns {
	const char				*name;
	const char				*suffix;
	const double				ttl;
	unsigned				nmember;
	const struct vrt_dir_dns_entry		*members;
};

/*
 * other stuff.
 * XXX: document when bored
 */

struct vrt_ref {
	unsigned	source;
	unsigned	offset;
	unsigned	line;
	unsigned	pos;
	unsigned	count;
	const char	*token;
};

/* ACL related */
#define VRT_ACL_MAXADDR		16	/* max(IPv4, IPv6) */

void VRT_acl_log(const struct sess *, const char *msg);

/* Regexp related */
void VRT_re_init(void **, const char *);
void VRT_re_fini(void *);
int VRT_re_match(const char *, void *re);
const char *VRT_regsub(const struct sess *sp, int all, const char *,
    void *, const char *);

void VRT_panic(const struct sess *sp, const char *, ...);
void VRT_ban(struct sess *sp, char *, ...);
void VRT_ban_string(struct sess *sp, const char *);
void VRT_purge(const struct sess *sp, double ttl, double grace);

void VRT_count(const struct sess *, unsigned);
int VRT_rewrite(const char *, const char *);
void VRT_error(struct sess *, unsigned, const char *);
int VRT_switch_config(const char *);

enum gethdr_e { HDR_REQ, HDR_RESP, HDR_OBJ, HDR_BEREQ, HDR_BERESP };
char *VRT_GetHdr(const struct sess *, enum gethdr_e where, const char *);
void VRT_SetHdr(const struct sess *, enum gethdr_e where, const char *,
    const char *, ...);
void VRT_handling(struct sess *sp, unsigned hand);

void VRT_hashdata(const struct sess *sp, const char *str, ...);

/* Simple stuff */
int VRT_strcmp(const char *s1, const char *s2);
void VRT_memmove(void *dst, const void *src, unsigned len);

void VRT_ESI(const struct sess *sp);
void VRT_Rollback(struct sess *sp);

/* Synthetic pages */
void VRT_synth_page(const struct sess *sp, unsigned flags, const char *, ...);

/* Backend related */
void VRT_init_dir(struct cli *, struct director **, const char *name,
    int idx, const void *priv);
void VRT_fini_dir(struct cli *, struct director *);

/* VMOD/Modules related */
int VRT_Vmod_Init(void **hdl, void *ptr, int len, const char *nm,
    const char *path, struct cli *cli);
void VRT_Vmod_Fini(void **hdl);

struct vmod_priv;
typedef void vmod_priv_free_f(void *);
struct vmod_priv {
	void			*priv;
	vmod_priv_free_f	*free;
};

typedef int vmod_init_f(struct vmod_priv *,  const struct VCL_conf *);

static inline void
vmod_priv_fini(const struct vmod_priv *p)
{

	if (p->priv != (void*)0 && p->free != (void*)0)
		p->free(p->priv);
}

/* Stevedore related functions */
int VRT_Stv(const char *nm);

/* Convert things to string */

char *VRT_IP_string(const struct sess *sp, const struct sockaddr_storage *sa);
char *VRT_int_string(const struct sess *sp, int);
char *VRT_double_string(const struct sess *sp, double);
char *VRT_time_string(const struct sess *sp, double);
const char *VRT_bool_string(const struct sess *sp, unsigned);
const char *VRT_backend_string(const struct sess *sp, const struct director *d);

#define VRT_done(sp, hand)			\
	do {					\
		VRT_handling(sp, hand);		\
		return (1);			\
	} while (0)

const char *VRT_WrkString(const struct sess *sp, const char *p, ...);
