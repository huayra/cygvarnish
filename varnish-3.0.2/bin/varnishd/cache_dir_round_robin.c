/*-
 * Copyright (c) 2008-2011 Varnish Software AS
 * All rights reserved.
 *
 * Author: Petter Knudsen <petter@linpro.no>
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
 */

#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cache.h"
#include "cache_backend.h"
#include "vrt.h"

/*--------------------------------------------------------------------*/

struct vdi_round_robin_host {
	struct director			*backend;
};

enum mode_e { m_round_robin, m_fallback };

struct vdi_round_robin {
	unsigned			magic;
#define VDI_ROUND_ROBIN_MAGIC		0x2114a178
	struct director			dir;
	enum mode_e			mode;
	struct vdi_round_robin_host	*hosts;
	unsigned			nhosts;
	unsigned			next_host;
};

static struct vbc *
vdi_round_robin_getfd(const struct director *d, struct sess *sp)
{
	int i;
	struct vdi_round_robin *vs;
	struct director *backend;
	struct vbc *vbe;

	CHECK_OBJ_NOTNULL(sp, SESS_MAGIC);
	CHECK_OBJ_NOTNULL(d, DIRECTOR_MAGIC);
	CAST_OBJ_NOTNULL(vs, d->priv, VDI_ROUND_ROBIN_MAGIC);

	/*
	 * In fallback mode we ignore the next_host and always grab the
	 * first healthy backend we can find.
	 */
	for (i = 0; i < vs->nhosts; i++) {
		if (vs->mode == m_round_robin) {
			backend = vs->hosts[vs->next_host].backend;
			vs->next_host = (vs->next_host + 1) % vs->nhosts;
		} else /* m_fallback */ {
			backend = vs->hosts[i].backend;
		}
		if (!VDI_Healthy(backend, sp))
			continue;
		vbe = VDI_GetFd(backend, sp);
		if (vbe != NULL)
			return (vbe);
	}

	return (NULL);
}

static unsigned
vdi_round_robin_healthy(const struct director *d, const struct sess *sp)
{
	struct vdi_round_robin *vs;
	struct director *backend;
	int i;

	CHECK_OBJ_NOTNULL(d, DIRECTOR_MAGIC);
	CAST_OBJ_NOTNULL(vs, d->priv, VDI_ROUND_ROBIN_MAGIC);

	for (i = 0; i < vs->nhosts; i++) {
		backend = vs->hosts[i].backend;
		if (VDI_Healthy(backend, sp))
			return (1);
	}
	return (0);
}

static void
vdi_round_robin_fini(const struct director *d)
{
	struct vdi_round_robin *vs;

	CHECK_OBJ_NOTNULL(d, DIRECTOR_MAGIC);
	CAST_OBJ_NOTNULL(vs, d->priv, VDI_ROUND_ROBIN_MAGIC);

	free(vs->hosts);
	free(vs->dir.vcl_name);
	vs->dir.magic = 0;
	vs->next_host = 0;
	FREE_OBJ(vs);
}

static void
vrt_init_dir(struct cli *cli, struct director **bp, int idx,
    const void *priv, enum mode_e mode)
{
	const struct vrt_dir_round_robin *t;
	struct vdi_round_robin *vs;
	const struct vrt_dir_round_robin_entry *te;
	struct vdi_round_robin_host *vh;
	int i;

	ASSERT_CLI();
	(void)cli;
	t = priv;

	ALLOC_OBJ(vs, VDI_ROUND_ROBIN_MAGIC);
	XXXAN(vs);
	vs->hosts = calloc(sizeof *vh, t->nmember);
	XXXAN(vs->hosts);

	vs->dir.magic = DIRECTOR_MAGIC;
	vs->dir.priv = vs;
	vs->dir.name = "round_robin";
	REPLACE(vs->dir.vcl_name, t->name);
	vs->dir.getfd = vdi_round_robin_getfd;
	vs->dir.fini = vdi_round_robin_fini;
	vs->dir.healthy = vdi_round_robin_healthy;

	vs->mode = mode;
	vh = vs->hosts;
	te = t->members;
	for (i = 0; i < t->nmember; i++, vh++, te++) {
		vh->backend = bp[te->host];
		AN (vh->backend);
	}
	vs->nhosts = t->nmember;
	vs->next_host = 0;

	bp[idx] = &vs->dir;
}

void
VRT_init_dir_round_robin(struct cli *cli, struct director **bp, int idx,
    const void *priv)
{
	vrt_init_dir(cli, bp, idx, priv, m_round_robin);
}

void
VRT_init_dir_fallback(struct cli *cli, struct director **bp, int idx,
    const void *priv)
{
	vrt_init_dir(cli, bp, idx, priv, m_fallback);
}

