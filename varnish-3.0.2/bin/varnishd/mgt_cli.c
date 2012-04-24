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
 *
 * The management process' CLI handling
 */

#include "config.h"

#include <sys/types.h>

#include <stdarg.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#ifndef HAVE_SRANDOMDEV
#include "compat/srandomdev.h"
#endif

#include "cli_priv.h"
#include "vcli.h"
#include "vsb.h"
#include "cli_common.h"
#include "cli_serve.h"
#include "vev.h"
#include "vsc.h"
#include "vlu.h"
#include "vss.h"


#include "mgt.h"
#include "heritage.h"
#include "mgt_cli.h"

static int		cli_i = -1, cli_o = -1;
static struct VCLS	*cls;
static const char	*secret_file;

#define	MCF_NOAUTH	0	/* NB: zero disables here-documents */
#define MCF_AUTH	16

/*--------------------------------------------------------------------*/

static void
mcf_banner(struct cli *cli, const char *const *av, void *priv)
{

	(void)av;
	(void)priv;
	VCLI_Out(cli, "-----------------------------\n");
	VCLI_Out(cli, "Varnish Cache CLI 1.0\n");
	VCLI_Out(cli, "-----------------------------\n");
	VCLI_Out(cli, "%s\n", VSB_data(vident) + 1);
	VCLI_Out(cli, "\n");
	VCLI_Out(cli, "Type 'help' for command list.\n");
	VCLI_Out(cli, "Type 'quit' to close CLI session.\n");
	if (child_pid < 0)
		VCLI_Out(cli, "Type 'start' to launch worker process.\n");
	VCLI_SetResult(cli, CLIS_OK);
}

/*--------------------------------------------------------------------*/

/* XXX: what order should this list be in ? */
static struct cli_proto cli_proto[] = {
	{ CLI_BANNER,		"", mcf_banner, NULL },
	{ CLI_SERVER_STATUS,	"", mcf_server_status, NULL },
	{ CLI_SERVER_START,	"", mcf_server_startstop, NULL },
	{ CLI_SERVER_STOP,	"", mcf_server_startstop, cli_proto },
	{ CLI_VCL_LOAD,		"", mcf_config_load, NULL },
	{ CLI_VCL_INLINE,	"", mcf_config_inline, NULL },
	{ CLI_VCL_USE,		"", mcf_config_use, NULL },
	{ CLI_VCL_DISCARD,	"", mcf_config_discard, NULL },
	{ CLI_VCL_LIST,		"", mcf_config_list, NULL },
	{ CLI_VCL_SHOW,		"", mcf_config_show, NULL },
	{ CLI_PARAM_SHOW,	"", mcf_param_show, NULL },
	{ CLI_PARAM_SET,	"", mcf_param_set, NULL },
	{ CLI_PANIC_SHOW,	"", mcf_panic_show, NULL },
	{ CLI_PANIC_CLEAR,	"", mcf_panic_clear, NULL },
	{ NULL }
};

/*--------------------------------------------------------------------*/

static void
mcf_panic(struct cli *cli, const char * const *av, void *priv)
{

	(void)cli;
	(void)av;
	(void)priv;
	assert(!strcmp("", "You asked for it"));
}

static struct cli_proto cli_debug[] = {
	{ "debug.panic.master", "debug.panic.master",
		"\tPanic the master process.\n",
		0, 0, "d", mcf_panic, NULL},
	{ NULL }
};

/*--------------------------------------------------------------------*/

static void
mcf_askchild(struct cli *cli, const char * const *av, void *priv)
{
	int i;
	char *q;
	unsigned u;
	struct vsb *vsb;

	(void)priv;
	/*
	 * Command not recognized in master, try cacher if it is
	 * running.
	 */
	if (cli_o <= 0) {
		if (!strcmp(av[1], "help")) {
			VCLI_Out(cli, "No help from child, (not running).\n");
			return;
		}
		VCLI_SetResult(cli, CLIS_UNKNOWN);
		VCLI_Out(cli,
		    "Unknown request in manager process "
		    "(child not running).\n"
		    "Type 'help' for more info.");
		return;
	}
	vsb = VSB_new_auto();
	for (i = 1; av[i] != NULL; i++) {
		VSB_quote(vsb, av[i], strlen(av[i]), 0);
		VSB_putc(vsb, ' ');
	}
	VSB_putc(vsb, '\n');
	AZ(VSB_finish(vsb));
	i = write(cli_o, VSB_data(vsb), VSB_len(vsb));
	if (i != VSB_len(vsb)) {
		VSB_delete(vsb);
		VCLI_SetResult(cli, CLIS_COMMS);
		VCLI_Out(cli, "CLI communication error");
		MGT_Child_Cli_Fail();
		return;
	}
	VSB_delete(vsb);
	(void)VCLI_ReadResult(cli_i, &u, &q, params->cli_timeout);
	VCLI_SetResult(cli, u);
	VCLI_Out(cli, "%s", q);
	free(q);
}

static struct cli_proto cli_askchild[] = {
	{ "*", "<wild-card-entry>", "\t<fall through to cacher>\n",
		0, 9999, "h*", mcf_askchild, NULL},
	{ NULL }
};

/*--------------------------------------------------------------------
 * Ask the child something over CLI, return zero only if everything is
 * happy happy.
 */

int
mgt_cli_askchild(unsigned *status, char **resp, const char *fmt, ...) {
	int i, j;
	va_list ap;
	unsigned u;
	char buf[params->cli_buffer], *p;

	if (resp != NULL)
		*resp = NULL;
	if (status != NULL)
		*status = 0;
	if (cli_i < 0|| cli_o < 0) {
		if (status != NULL)
			*status = CLIS_CANT;
		return (CLIS_CANT);
	}
	va_start(ap, fmt);
	vbprintf(buf, fmt, ap);
	va_end(ap);
	p = strchr(buf, '\0');
	assert(p != NULL && p > buf && p[-1] == '\n');
	i = p - buf;
	j = write(cli_o, buf, i);
	if (j != i) {
		if (status != NULL)
			*status = CLIS_COMMS;
		if (resp != NULL)
			*resp = strdup("CLI communication error");
		MGT_Child_Cli_Fail();
		return (CLIS_COMMS);
	}

	(void)VCLI_ReadResult(cli_i, &u, resp, params->cli_timeout);
	if (status != NULL)
		*status = u;
	if (u == CLIS_COMMS)
		MGT_Child_Cli_Fail();
	return (u == CLIS_OK ? 0 : u);
}

/*--------------------------------------------------------------------*/

void
mgt_cli_start_child(int fdi, int fdo)
{

	cli_i = fdi;
	cli_o = fdo;
}

/*--------------------------------------------------------------------*/

void
mgt_cli_stop_child(void)
{

	cli_i = -1;
	cli_o = -1;
	/* XXX: kick any users */
}

/*--------------------------------------------------------------------
 * Generate a random challenge
 */

static void
mgt_cli_challenge(struct cli *cli)
{
	int i;

	for (i = 0; i + 2L < sizeof cli->challenge; i++)
		cli->challenge[i] = (random() % 26) + 'a';
	cli->challenge[i++] = '\n';
	cli->challenge[i] = '\0';
	VCLI_Out(cli, "%s", cli->challenge);
	VCLI_Out(cli, "\nAuthentication required.\n");
	VCLI_SetResult(cli, CLIS_AUTH);
}

/*--------------------------------------------------------------------
 * Validate the authentication
 */

static void
mcf_auth(struct cli *cli, const char *const *av, void *priv)
{
	int fd;
	char buf[CLI_AUTH_RESPONSE_LEN + 1];

	AN(av[2]);
	(void)priv;
	if (secret_file == NULL) {
		VCLI_Out(cli, "Secret file not configured\n");
		VCLI_SetResult(cli, CLIS_CANT);
		return;
	}
	fd = open(secret_file, O_RDONLY);
	if (fd < 0) {
		VCLI_Out(cli, "Cannot open secret file (%s)\n",
		    strerror(errno));
		VCLI_SetResult(cli, CLIS_CANT);
		return;
	}
	mgt_got_fd(fd);
	VCLI_AuthResponse(fd, cli->challenge, buf);
	AZ(close(fd));
	if (strcasecmp(buf, av[2])) {
		mgt_cli_challenge(cli);
		return;
	}
	cli->auth = MCF_AUTH;
	memset(cli->challenge, 0, sizeof cli->challenge);
	VCLI_SetResult(cli, CLIS_OK);
	mcf_banner(cli, av, priv);
}

static struct cli_proto cli_auth[] = {
	{ CLI_HELP,		"", VCLS_func_help, NULL },
	{ CLI_PING,		"", VCLS_func_ping },
	{ CLI_AUTH,		"", mcf_auth, NULL },
	{ CLI_QUIT,		"", VCLS_func_close, NULL},
	{ NULL }
};

/*--------------------------------------------------------------------*/
static void
mgt_cli_cb_before(const struct cli *cli)
{

	if (params->syslog_cli_traffic)
		syslog(LOG_NOTICE, "CLI %s Rd %s", cli->ident, cli->cmd);
}

static void
mgt_cli_cb_after(const struct cli *cli)
{

	if (params->syslog_cli_traffic)
		syslog(LOG_NOTICE, "CLI %s Wr %03u %s",
		    cli->ident, cli->result, VSB_data(cli->sb));
}

/*--------------------------------------------------------------------*/

static void
mgt_cli_init_cls(void)
{

	cls = VCLS_New(mgt_cli_cb_before, mgt_cli_cb_after, params->cli_buffer);
	AN(cls);
	AZ(VCLS_AddFunc(cls, MCF_NOAUTH, cli_auth));
	AZ(VCLS_AddFunc(cls, MCF_AUTH, cli_proto));
	AZ(VCLS_AddFunc(cls, MCF_AUTH, cli_debug));
	AZ(VCLS_AddFunc(cls, MCF_AUTH, cli_stv));
	AZ(VCLS_AddFunc(cls, MCF_AUTH, cli_askchild));
}

/*--------------------------------------------------------------------
 * Get rid of all CLI sessions
 */

void
mgt_cli_close_all(void)
{

	VCLS_Destroy(&cls);
}

/*--------------------------------------------------------------------
 * Callback whenever something happens to the input fd of the session.
 */

static int
mgt_cli_callback2(const struct vev *e, int what)
{
	int i;

	(void)e;
	(void)what;
	i = VCLS_PollFd(cls, e->fd, 0);
	return (i);
}

/*--------------------------------------------------------------------*/

void
mgt_cli_setup(int fdi, int fdo, int verbose, const char *ident, mgt_cli_close_f *closefunc, void *priv)
{
	struct cli *cli;
	struct vev *ev;

	(void)ident;
	(void)verbose;
	if (cls == NULL)
		mgt_cli_init_cls();

	cli = VCLS_AddFd(cls, fdi, fdo, closefunc, priv);

	cli->ident = strdup(ident);

	/* Deal with TELNET options */
	if (fdi != 0)
		VLU_SetTelnet(cli->vlu, fdo);

	if (fdi != 0 && secret_file != NULL) {
		cli->auth = MCF_NOAUTH;
		mgt_cli_challenge(cli);
	} else {
		cli->auth = MCF_AUTH;
		mcf_banner(cli, NULL, NULL);
	}
	AZ(VSB_finish(cli->sb));
	(void)VCLI_WriteResult(fdo, cli->result, VSB_data(cli->sb));


	ev = vev_new();
	AN(ev);
	ev->name = cli->ident;
	ev->fd = fdi;
	ev->fd_flags = EV_RD;
	ev->callback = mgt_cli_callback2;
	ev->priv = cli;
	AZ(vev_add(mgt_evb, ev));
}

/*--------------------------------------------------------------------*/

static struct vsb *
sock_id(const char *pfx, int fd)
{
	struct vsb *vsb;

	char abuf1[VTCP_ADDRBUFSIZE], abuf2[VTCP_ADDRBUFSIZE];
	char pbuf1[VTCP_PORTBUFSIZE], pbuf2[VTCP_PORTBUFSIZE];

	vsb = VSB_new_auto();
	AN(vsb);
	VTCP_myname(fd, abuf1, sizeof abuf1, pbuf1, sizeof pbuf1);
	VTCP_hisname(fd, abuf2, sizeof abuf2, pbuf2, sizeof pbuf2);
	VSB_printf(vsb, "%s %s %s %s %s", pfx, abuf2, pbuf2, abuf1, pbuf1);
	AZ(VSB_finish(vsb));
	return (vsb);
}

/*--------------------------------------------------------------------*/

struct telnet {
	unsigned		magic;
#define TELNET_MAGIC		0x53ec3ac0
	int			fd;
	struct vev		*ev;
};

static void
telnet_close(void *priv)
{
	struct telnet *tn;

	CAST_OBJ_NOTNULL(tn, priv, TELNET_MAGIC);
	(void)close(tn->fd);
	FREE_OBJ(tn);
}

static struct telnet *
telnet_new(int fd)
{
	struct telnet *tn;

	ALLOC_OBJ(tn, TELNET_MAGIC);
	AN(tn);
	tn->fd = fd;
	return (tn);
}

static int
telnet_accept(const struct vev *ev, int what)
{
	struct vsb *vsb;
	struct sockaddr_storage addr;
	socklen_t addrlen;
	struct telnet *tn;
	int i;

	(void)what;
	addrlen = sizeof addr;
	i = accept(ev->fd, (void *)&addr, &addrlen);
	if (i < 0 && errno == EBADF)
		return (1);
	if (i < 0)
		return (0);

	mgt_got_fd(i);
	tn = telnet_new(i);
	vsb = sock_id("telnet", i);
	mgt_cli_setup(i, i, 0, VSB_data(vsb), telnet_close, tn);
	VSB_delete(vsb);
	return (0);
}

void
mgt_cli_secret(const char *S_arg)
{
	int i, fd;
	char buf[BUFSIZ];
	char *p;

	/* Save in shmem */
	i = strlen(S_arg);
	p = VSM_Alloc(i + 1, "Arg", "-S", "");
	AN(p);
	strcpy(p, S_arg);

	srandomdev();
	fd = open(S_arg, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Can not open secret-file \"%s\"\n", S_arg);
		exit (2);
	}
	mgt_got_fd(fd);
	i = read(fd, buf, sizeof buf);
	if (i == 0) {
		fprintf(stderr, "Empty secret-file \"%s\"\n", S_arg);
		exit (2);
	}
	if (i < 0) {
		fprintf(stderr, "Can not read secret-file \"%s\"\n", S_arg);
		exit (2);
	}
	AZ(close(fd));
	secret_file = S_arg;
}

void
mgt_cli_telnet(const char *T_arg)
{
	struct vss_addr **ta;
	int i, n, sock, good;
	struct telnet *tn;
	char *p;
	struct vsb *vsb;
	char abuf[VTCP_ADDRBUFSIZE];
	char pbuf[VTCP_PORTBUFSIZE];

	n = VSS_resolve(T_arg, NULL, &ta);
	if (n == 0) {
		REPORT(LOG_ERR, "-T %s Could not be resolved\n", T_arg);
		exit(2);
	}
	good = 0;
	vsb = VSB_new_auto();
	XXXAN(vsb);
	for (i = 0; i < n; ++i) {
		sock = VSS_listen(ta[i], 10);
		if (sock < 0)
			continue;
		VTCP_myname(sock, abuf, sizeof abuf, pbuf, sizeof pbuf);
		VSB_printf(vsb, "%s %s\n", abuf, pbuf);
		good++;
		tn = telnet_new(sock);
		tn->ev = vev_new();
		XXXAN(tn->ev);
		tn->ev->fd = sock;
		tn->ev->fd_flags = POLLIN;
		tn->ev->callback = telnet_accept;
		AZ(vev_add(mgt_evb, tn->ev));
		free(ta[i]);
		ta[i] = NULL;
	}
	free(ta);
	if (good == 0) {
		REPORT(LOG_ERR, "-T %s could not be listened on.", T_arg);
		exit(2);
	}
	AZ(VSB_finish(vsb));
	/* Save in shmem */
	p = VSM_Alloc(VSB_len(vsb) + 1, "Arg", "-T", "");
	AN(p);
	strcpy(p, VSB_data(vsb));
	VSB_delete(vsb);
}

/* Reverse CLI ("Master") connections --------------------------------*/

static int M_fd = -1;
static struct vev *M_poker, *M_conn;
static struct vss_addr **M_ta;
static int M_nta, M_nxt;
static double M_poll = 0.1;

static void
Marg_closer(void *priv)
{

	(void)priv;
	(void)close(M_fd);
	M_fd = -1;
}

static int
Marg_poker(const struct vev *e, int what)
{
	struct vsb *vsb;
	int s, k;
	socklen_t l;

	(void)what;	/* XXX: ??? */

	if (e == M_conn) {
		/* Our connect(2) returned, check result */
		l = sizeof k;
		AZ(getsockopt(M_fd, SOL_SOCKET, SO_ERROR, &k, &l));
		if (k) {
			errno = k;
			syslog(LOG_INFO, "Could not connect to CLI-master: %m");
			(void)close(M_fd);
			M_fd = -1;
			/* Try next address */
			if (++M_nxt >= M_nta) {
				M_nxt = 0;
				if (M_poll < 10)
					M_poll *= 2;
			}
			return (1);
		}
		vsb = sock_id("master", M_fd);
		mgt_cli_setup(M_fd, M_fd, 0, VSB_data(vsb), Marg_closer, NULL);
		VSB_delete(vsb);
		M_poll = 1;
		return (1);
	}

	assert(e == M_poker);

	M_poker->timeout = M_poll;	/* XXX nasty ? */
	if (M_fd >= 0)
		return (0);

	/* Try to connect asynchronously */
	s = VSS_connect(M_ta[M_nxt], 1);
	if (s < 0)
		return (0);

	mgt_got_fd(s);

	M_conn = vev_new();
	AN(M_conn);
	M_conn->callback = Marg_poker;
	M_conn->name = "-M connector";
	M_conn->fd_flags = EV_WR;
	M_conn->fd = s;
	M_fd = s;
	AZ(vev_add(mgt_evb, M_conn));
	return (0);
}

void
mgt_cli_master(const char *M_arg)
{
	(void)M_arg;

	M_nta = VSS_resolve(M_arg, NULL, &M_ta);
	if (M_nta <= 0) {
		fprintf(stderr, "Could resolve -M argument to address\n");
		exit (1);
	}
	M_nxt = 0;
	AZ(M_poker);
	M_poker = vev_new();
	AN(M_poker);
	M_poker->timeout = M_poll;
	M_poker->callback = Marg_poker;
	M_poker->name = "-M poker";
	AZ(vev_add(mgt_evb, M_poker));
}
