/*-
 * Copyright (c) 2010-2011 Varnish Software AS
 * All rights reserved.
 *
 * Author: Poul-Henning Kamp <phk@FreeBSD.org>
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
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <syslog.h>
#include <netinet/in.h>
#include "vrt.h"
#include "../../bin/varnishd/cache.h"

#include "vcc_if.h"

void __match_proto__()
vmod_set_ip_tos(struct sess *sp, int tos)
{

	VTCP_Assert(setsockopt(sp->fd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)));
}

static const char * __match_proto__()
vmod_updown(struct sess *sp, int up, const char *s, va_list ap)
{
	unsigned u;
	char *b, *e;
	const char *p;

	CHECK_OBJ_NOTNULL(sp, SESS_MAGIC);
	u = WS_Reserve(sp->wrk->ws, 0);
	e = b = sp->wrk->ws->f;
	e += u;
	p = s;
	while (p != vrt_magic_string_end && b < e) {
		if (p != NULL) {
			for (; b < e && *p != '\0'; p++)
				if (up)
					*b++ = (char)toupper(*p);
				else
					*b++ = (char)tolower(*p);
		}
		p = va_arg(ap, const char *);
	}
	if (b < e)
		*b = '\0';
	b++;
	if (b > e) {
		WS_Release(sp->wrk->ws, 0);
		return (NULL);
	} else {
		e = b;
		b = sp->wrk->ws->f;
		WS_Release(sp->wrk->ws, e - b);
		return (b);
	}
}

const char * __match_proto__()
vmod_toupper(struct sess *sp, struct vmod_priv *priv, const char *s, ...)
{
	const char *p;
	va_list ap;

	CHECK_OBJ_NOTNULL(sp, SESS_MAGIC);
	if (priv->priv == NULL) {
		priv->priv = strdup("BAR");
		priv->free = free;
	} else {
		assert(!strcmp(priv->priv, "BAR"));
	}
	va_start(ap, s);
	p = vmod_updown(sp, 1, s, ap);
	va_end(ap);
	return (p);
}

const char * __match_proto__()
vmod_tolower(struct sess *sp, struct vmod_priv *priv, const char *s, ...)
{
	const char *p;
	va_list ap;

	CHECK_OBJ_NOTNULL(sp, SESS_MAGIC);
	assert(!strcmp(priv->priv, "FOO"));
	va_start(ap, s);
	p = vmod_updown(sp, 0, s, ap);
	va_end(ap);
	return (p);
}

int
init_function(struct vmod_priv *priv, const struct VCL_conf *cfg)
{
	(void)cfg;

	priv->priv = strdup("FOO");
	priv->free = free;
	return (0);
}

double
vmod_random(struct sess *sp, double lo, double hi)
{
	double a;

	(void)sp;

	a = drand48();
	a *= hi - lo;
	a += lo;
	return (a);
}

void __match_proto__()
vmod_log(struct sess *sp, const char *fmt, ...)
{
	char buf[8192], *p;
	va_list ap;

	va_start(ap, fmt);
	p = VRT_StringList(buf, sizeof buf, fmt, ap);
	va_end(ap);
	if (p != NULL)
		WSP(sp, SLT_VCL_Log, "%s", buf);
}

void
vmod_syslog(struct sess *sp, int fac, const char *fmt, ...)
{
	char buf[8192], *p;
	va_list ap;

	(void)sp;
	va_start(ap, fmt);
	p = VRT_StringList(buf, sizeof buf, fmt, ap);
	va_end(ap);
	if (p != NULL)
		syslog(fac, "%s", buf);
}

const char * __match_proto__()
vmod_author(struct sess *sp, const char *id)
{
	(void)sp;
	if (!strcmp(id, "phk"))
		return ("Poul-Henning");
	if (!strcmp(id, "des"))
		return ("Dag-Erling");
	if (!strcmp(id, "kristian"))
		return ("Kristian");
	if (!strcmp(id, "mithrandir"))
		return ("Tollef");
	WRONG("Illegal VMOD enum");
}

void __match_proto__()
vmod_collect(struct sess *sp, enum gethdr_e e, const char *h)
{
	(void)e;
	(void)sp;
	(void)h;
	if (e == HDR_REQ)
		http_CollectHdr(sp->http, h);
	else if (e == HDR_BERESP)
		http_CollectHdr(sp->wrk->beresp, h);
}

