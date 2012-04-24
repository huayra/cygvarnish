/*-
 * Copyright (c) 2006 Verdens Gang AS
 * Copyright (c) 2006-2011 Varnish Software AS
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
 */

#include "config.h"

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#include "cache.h"
#include "stevedore.h"
#include "cli_priv.h"
#include "vct.h"

static unsigned fetchfrag;

/*--------------------------------------------------------------------
 * VFP_NOP
 *
 * This fetch-processor does nothing but store the object.
 * It also documents the API
 */

/*--------------------------------------------------------------------
 * VFP_BEGIN
 *
 * Called to set up stuff.
 *
 * 'estimate' is the estimate of the number of bytes we expect to receive,
 * as seen on the socket, or zero if unknown.
 */
static void __match_proto__()
vfp_nop_begin(struct sess *sp, size_t estimate)
{

	if (fetchfrag > 0) {
		estimate = fetchfrag;
		WSL(sp->wrk, SLT_Debug, sp->fd,
		    "Fetch %d byte segments:", fetchfrag);
	}
	if (estimate > 0)
		(void)FetchStorage(sp, estimate);
}

/*--------------------------------------------------------------------
 * VFP_BYTES
 *
 * Process (up to) 'bytes' from the socket.
 *
 * Return -1 on error
 * Return 0 on EOF on socket even if bytes not reached.
 * Return 1 when 'bytes' have been processed.
 */

static int __match_proto__()
vfp_nop_bytes(struct sess *sp, struct http_conn *htc, ssize_t bytes)
{
	ssize_t l, w;
	struct storage *st;

	while (bytes > 0) {
		st = FetchStorage(sp, 0);
		if (st == NULL) {
			htc->error = "Could not get storage";
			return (-1);
		}
		l = st->space - st->len;
		if (l > bytes)
			l = bytes;
		w = HTC_Read(htc, st->ptr + st->len, l);
		if (w <= 0)
			return (w);
		st->len += w;
		sp->obj->len += w;
		bytes -= w;
		if (sp->wrk->do_stream)
			RES_StreamPoll(sp);
	}
	return (1);
}

/*--------------------------------------------------------------------
 * VFP_END
 *
 * Finish & cleanup
 *
 * Return -1 for error
 * Return 0 for OK
 */

static int __match_proto__()
vfp_nop_end(struct sess *sp)
{
	struct storage *st;

	st = VTAILQ_LAST(&sp->obj->store, storagehead);
	if (st == NULL)
		return (0);

	if (st->len == 0) {
		VTAILQ_REMOVE(&sp->obj->store, st, list);
		STV_free(st);
		return (0);
	}
	if (st->len < st->space)
		STV_trim(st, st->len);
	return (0);
}

static struct vfp vfp_nop = {
	.begin	=	vfp_nop_begin,
	.bytes	=	vfp_nop_bytes,
	.end	=	vfp_nop_end,
};

/*--------------------------------------------------------------------
 * Fetch Storage
 */

struct storage *
FetchStorage(const struct sess *sp, ssize_t sz)
{
	ssize_t l;
	struct storage *st;

	st = VTAILQ_LAST(&sp->obj->store, storagehead);
	if (st != NULL && st->len < st->space)
		return (st);

	l = fetchfrag;
	if (l == 0)
		l = sz;
	if (l == 0)
		l = params->fetch_chunksize * 1024LL;
	st = STV_alloc(sp, l);
	if (st == NULL) {
		errno = ENOMEM;
		return (NULL);
	}
	AZ(st->len);
	VTAILQ_INSERT_TAIL(&sp->obj->store, st, list);
	return (st);
}

/*--------------------------------------------------------------------
 * Convert a string to a size_t safely
 */

static ssize_t
fetch_number(const char *nbr, int radix)
{
	uintmax_t cll;
	ssize_t cl;
	char *q;

	if (*nbr == '\0')
		return (-1);
	cll = strtoumax(nbr, &q, radix);
	if (q == NULL || *q != '\0')
		return (-1);

	cl = (ssize_t)cll;
	if((uintmax_t)cl != cll) /* Protect against bogusly large values */
		return (-1);
	return (cl);
}

/*--------------------------------------------------------------------*/

static int
fetch_straight(struct sess *sp, struct http_conn *htc, const char *b)
{
	int i;
	ssize_t cl;

	assert(sp->wrk->body_status == BS_LENGTH);

	cl = fetch_number(b, 10);
	sp->wrk->vfp->begin(sp, cl > 0 ? cl : 0);
	if (cl < 0) {
		WSP(sp, SLT_FetchError, "straight length field bogus");
		return (-1);
	} else if (cl == 0)
		return (0);

	i = sp->wrk->vfp->bytes(sp, htc, cl);
	if (i <= 0) {
		WSP(sp, SLT_FetchError, "straight read_error: %d %d (%s)",
		    i, errno, htc->error);
		return (-1);
	}
	return (0);
}

/*--------------------------------------------------------------------*/
/* XXX: Cleanup.  It must be possible somehow :-( */

#define CERR() do {						\
		if (i != 1) {					\
			WSP(sp, SLT_FetchError,			\
			    "chunked read_error: %d (%s)",	\
			    errno, htc->error);			\
			return (-1);				\
		}						\
	} while (0)

static int
fetch_chunked(struct sess *sp, struct http_conn *htc)
{
	int i;
	char buf[20];		/* XXX: 20 is arbitrary */
	unsigned u;
	ssize_t cl;

	sp->wrk->vfp->begin(sp, 0);
	assert(sp->wrk->body_status == BS_CHUNKED);
	do {
		/* Skip leading whitespace */
		do {
			i = HTC_Read(htc, buf, 1);
			CERR();
		} while (vct_islws(buf[0]));

		/* Collect hex digits, skipping leading zeros */
		for (u = 1; u < sizeof buf; u++) {
			do {
				i = HTC_Read(htc, buf + u, 1);
				CERR();
			} while (u == 1 && buf[0] == '0' && buf[u] == '0');
			if (!vct_ishex(buf[u]))
				break;
		}

		if (u >= sizeof buf) {
			WSP(sp, SLT_FetchError,	"chunked header too long");
			return (-1);
		}

		/* Skip trailing white space */
		while(vct_islws(buf[u]) && buf[u] != '\n') {
			i = HTC_Read(htc, buf + u, 1);
			CERR();
		}

		if (buf[u] != '\n') {
			WSP(sp, SLT_FetchError,	"chunked header char syntax");
			return (-1);
		}
		buf[u] = '\0';

		cl = fetch_number(buf, 16);
		if (cl < 0) {
			WSP(sp, SLT_FetchError,	"chunked header nbr syntax");
			return (-1);
		} else if (cl > 0) {
			i = sp->wrk->vfp->bytes(sp, htc, cl);
			CERR();
		}
		i = HTC_Read(htc, buf, 1);
		CERR();
		if (buf[0] == '\r') {
			i = HTC_Read(htc, buf, 1);
			CERR();
		}
		if (buf[0] != '\n') {
			WSP(sp, SLT_FetchError,	"chunked tail syntax");
			return (-1);
		}
	} while (cl > 0);
	return (0);
}

#undef CERR

/*--------------------------------------------------------------------*/

static int
fetch_eof(struct sess *sp, struct http_conn *htc)
{
	int i;

	assert(sp->wrk->body_status == BS_EOF);
	sp->wrk->vfp->begin(sp, 0);
	i = sp->wrk->vfp->bytes(sp, htc, SSIZE_MAX);
	if (i < 0) {
		WSP(sp, SLT_FetchError, "eof read_error: %d (%s)",
		    errno, htc->error);
		return (-1);
	}
	return (0);
}

/*--------------------------------------------------------------------
 * Fetch any body attached to the incoming request, and either write it
 * to the backend (if we pass) or discard it (anything else).
 * This is mainly a separate function to isolate the stack buffer and
 * to contain the complexity when we start handling chunked encoding.
 */

int
FetchReqBody(struct sess *sp)
{
	unsigned long content_length;
	char buf[8192];
	char *ptr, *endp;
	int rdcnt;

	if (http_GetHdr(sp->http, H_Content_Length, &ptr)) {

		content_length = strtoul(ptr, &endp, 10);
		/* XXX should check result of conversion */
		while (content_length) {
			if (content_length > sizeof buf)
				rdcnt = sizeof buf;
			else
				rdcnt = content_length;
			rdcnt = HTC_Read(sp->htc, buf, rdcnt);
			if (rdcnt <= 0)
				return (1);
			content_length -= rdcnt;
			if (!sp->sendbody)
				continue;
			(void)WRW_Write(sp->wrk, buf, rdcnt); /* XXX: stats ? */
			if (WRW_Flush(sp->wrk))
				return (2);
		}
	}
	if (http_GetHdr(sp->http, H_Transfer_Encoding, NULL)) {
		/* XXX: Handle chunked encoding. */
		WSL(sp->wrk, SLT_Debug, sp->fd, "Transfer-Encoding in request");
		return (1);
	}
	return (0);
}

/*--------------------------------------------------------------------
 * Send request, and receive the HTTP protocol response, but not the
 * response body.
 *
 * Return value:
 *	-1 failure, not retryable
 *	 0 success
 *	 1 failure which can be retried.
 */

int
FetchHdr(struct sess *sp)
{
	struct vbc *vc;
	struct worker *w;
	char *b;
	struct http *hp;
	int retry = -1;
	int i;

	CHECK_OBJ_NOTNULL(sp, SESS_MAGIC);
	CHECK_OBJ_NOTNULL(sp->wrk, WORKER_MAGIC);
	w = sp->wrk;

	AN(sp->director);
	AZ(sp->obj);

	if (sp->objcore != NULL) {		/* pass has no objcore */
		CHECK_OBJ_NOTNULL(sp->objcore, OBJCORE_MAGIC);
		AN(sp->objcore->flags & OC_F_BUSY);
	}

	hp = w->bereq;

	sp->vbc = VDI_GetFd(NULL, sp);
	if (sp->vbc == NULL) {
		WSP(sp, SLT_FetchError, "no backend connection");
		return (-1);
	}
	vc = sp->vbc;
	if (vc->recycled)
		retry = 1;

	/*
	 * Now that we know our backend, we can set a default Host:
	 * header if one is necessary.  This cannot be done in the VCL
	 * because the backend may be chosen by a director.
	 */
	if (!http_GetHdr(hp, H_Host, &b))
		VDI_AddHostHeader(sp);

	(void)VTCP_blocking(vc->fd);	/* XXX: we should timeout instead */
	WRW_Reserve(w, &vc->fd);
	(void)http_Write(w, hp, 0);	/* XXX: stats ? */

	/* Deal with any message-body the request might have */
	i = FetchReqBody(sp);
	if (WRW_FlushRelease(w) || i > 0) {
		WSP(sp, SLT_FetchError, "backend write error: %d (%s)",
		    errno, strerror(errno));
		VDI_CloseFd(sp);
		/* XXX: other cleanup ? */
		return (retry);
	}

	/* Checkpoint the vsl.here */
	WSL_Flush(w, 0);

	/* XXX is this the right place? */
	VSC_C_main->backend_req++;

	/* Receive response */

	HTC_Init(w->htc, w->ws, vc->fd, params->http_resp_size,
	    params->http_resp_hdr_len);

	VTCP_set_read_timeout(vc->fd, vc->first_byte_timeout);

	i = HTC_Rx(w->htc);

	if (i < 0) {
		WSP(sp, SLT_FetchError, "http first read error: %d %d (%s)",
		    i, errno, w->htc->error);
		VDI_CloseFd(sp);
		/* XXX: other cleanup ? */
		/* Retryable if we never received anything */
		return (i == -1 ? retry : -1);
	}

	VTCP_set_read_timeout(vc->fd, vc->between_bytes_timeout);

	while (i == 0) {
		i = HTC_Rx(w->htc);
		if (i < 0) {
			WSP(sp, SLT_FetchError,
			    "http first read error: %d %d (%s)",
			    i, errno, w->htc->error);
			VDI_CloseFd(sp);
			/* XXX: other cleanup ? */
			return (-1);
		}
	}

	hp = w->beresp;

	if (http_DissectResponse(w, w->htc, hp)) {
		WSP(sp, SLT_FetchError, "http format error");
		VDI_CloseFd(sp);
		/* XXX: other cleanup ? */
		return (-1);
	}
	return (0);
}

/*--------------------------------------------------------------------*/

int
FetchBody(struct sess *sp)
{
	int cls;
	struct storage *st;
	struct worker *w;
	int mklen;

	CHECK_OBJ_NOTNULL(sp, SESS_MAGIC);
	CHECK_OBJ_NOTNULL(sp->wrk, WORKER_MAGIC);
	w = sp->wrk;
	CHECK_OBJ_NOTNULL(sp->obj, OBJECT_MAGIC);
	CHECK_OBJ_NOTNULL(sp->obj->http, HTTP_MAGIC);

	if (w->vfp == NULL)
		w->vfp = &vfp_nop;

	AN(sp->director);
	AssertObjCorePassOrBusy(sp->obj->objcore);

	AZ(w->vgz_rx);
	AZ(VTAILQ_FIRST(&sp->obj->store));
	switch (w->body_status) {
	case BS_NONE:
		cls = 0;
		mklen = 0;
		break;
	case BS_ZERO:
		cls = 0;
		mklen = 1;
		break;
	case BS_LENGTH:
		cls = fetch_straight(sp, w->htc,
		    w->h_content_length);
		mklen = 1;
		XXXAZ(w->vfp->end(sp));
		break;
	case BS_CHUNKED:
		cls = fetch_chunked(sp, w->htc);
		mklen = 1;
		XXXAZ(w->vfp->end(sp));
		break;
	case BS_EOF:
		cls = fetch_eof(sp, w->htc);
		mklen = 1;
		XXXAZ(w->vfp->end(sp));
		break;
	case BS_ERROR:
		cls = 1;
		mklen = 0;
		break;
	default:
		cls = 0;
		mklen = 0;
		INCOMPL();
	}
	AZ(w->vgz_rx);

	/*
	 * It is OK for ->end to just leave the last storage segment
	 * sitting on w->storage, we will always call vfp_nop_end()
	 * to get it trimmed or thrown out if empty.
	 */
	AZ(vfp_nop_end(sp));

	WSL(w, SLT_Fetch_Body, sp->vbc->fd, "%u(%s) cls %d mklen %u",
	    w->body_status, body_status(w->body_status),
	    cls, mklen);

	if (w->body_status == BS_ERROR) {
		VDI_CloseFd(sp);
		return (__LINE__);
	}

	if (cls < 0) {
		w->stats.fetch_failed++;
		/* XXX: Wouldn't this store automatically be released ? */
		while (!VTAILQ_EMPTY(&sp->obj->store)) {
			st = VTAILQ_FIRST(&sp->obj->store);
			VTAILQ_REMOVE(&sp->obj->store, st, list);
			STV_free(st);
		}
		VDI_CloseFd(sp);
		sp->obj->len = 0;
		return (__LINE__);
	}

	if (cls == 0 && w->do_close)
		cls = 1;

	WSL(w, SLT_Length, sp->vbc->fd, "%u", sp->obj->len);

	{
	/* Sanity check fetch methods accounting */
		ssize_t uu;

		uu = 0;
		VTAILQ_FOREACH(st, &sp->obj->store, list)
			uu += st->len;
		if (sp->objcore == NULL || (sp->objcore->flags & OC_F_PASS))
			/* Streaming might have started freeing stuff */
			assert (uu <= sp->obj->len);
		else
			assert(uu == sp->obj->len);
	}

	if (mklen > 0) {
		http_Unset(sp->obj->http, H_Content_Length);
		http_PrintfHeader(w, sp->fd, sp->obj->http,
		    "Content-Length: %jd", (intmax_t)sp->obj->len);
	}

	if (cls)
		VDI_CloseFd(sp);
	else
		VDI_RecycleFd(sp);

	return (0);
}

/*--------------------------------------------------------------------
 * Debugging aids
 */

static void
debug_fragfetch(struct cli *cli, const char * const *av, void *priv)
{
	(void)priv;
	(void)cli;
	fetchfrag = strtoul(av[2], NULL, 0);
}

static struct cli_proto debug_cmds[] = {
	{ "debug.fragfetch", "debug.fragfetch",
		"\tEnable fetch fragmentation\n", 1, 1, "d", debug_fragfetch },
	{ NULL }
};

/*--------------------------------------------------------------------
 *
 */

void
Fetch_Init(void)
{

	CLI_AddFuncs(debug_cmds);
}
