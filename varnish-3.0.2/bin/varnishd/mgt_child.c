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
 * The mechanics of handling the child process
 */

#include "config.h"

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "mgt.h"
#include "vsm.h"
#include "heritage.h"
#include "vcli.h"
#include "cli_priv.h"
#include "mgt_cli.h"
#include "vev.h"
#include "vlu.h"
#include "vsb.h"
#include "vss.h"
#include "vbm.h"

pid_t		child_pid = -1;


static struct vbitmap	*fd_map;

static int		child_cli_in = -1;
static int		child_VCLI_Out = -1;
static int		child_output = -1;

static enum {
	CH_STOPPED = 0,
	CH_STARTING = 1,
	CH_RUNNING = 2,
	CH_STOPPING = 3,
	CH_DIED = 4
}			child_state = CH_STOPPED;

static const char * const ch_state[] = {
	[CH_STOPPED] =	"stopped",
	[CH_STARTING] =	"starting",
	[CH_RUNNING] =	"running",
	[CH_STOPPING] =	"stopping",
	[CH_DIED] =	"died, (restarting)",
};

static struct vev	*ev_poker;
static struct vev	*ev_listen;
static struct vlu	*vlu;

static struct vsb *child_panic = NULL;

/*--------------------------------------------------------------------
 * Track the highest file descriptor the parent knows is being used.
 *
 * This allows the child process to clean/close only a small fraction
 * of the possible file descriptors after exec(2).
 *
 * This is likely to a bit on the low side, as libc and other libraries
 * has a tendency to cache file descriptors (syslog, resolver, etc.)
 * so we add a margin of 100 fds.
 */

static int		mgt_max_fd;

#define CLOSE_FD_UP_TO	(mgt_max_fd + 100)

void
mgt_got_fd(int fd)
{
	/*
	 * Assert > 0, to catch bogus opens, we know where stdin goes
	 * in the master process.
	 */
	assert(fd > 0);
	if (fd > mgt_max_fd)
		mgt_max_fd = fd;
}

/*--------------------------------------------------------------------
 * A handy little function
 */

static inline void
closex(int *fd)
{

	assert(*fd >= 0);
	AZ(close(*fd));
	*fd = -1;
}

/*--------------------------------------------------------------------
 * Keep track of which filedescriptors the child should inherit and
 * which should be closed after fork()
 */

void
mgt_child_inherit(int fd, const char *what)
{

	assert(fd >= 0);
	if (fd_map == NULL)
		fd_map = vbit_init(128);
	AN(fd_map);
	if (what != NULL)
		vbit_set(fd_map, fd);
	else
		vbit_clr(fd_map, fd);
}

/*--------------------------------------------------------------------*/

static int
child_line(void *priv, const char *p)
{
	(void)priv;

	REPORT(LOG_NOTICE, "Child (%jd) said %s", (intmax_t)child_pid, p);
	return (0);
}

/*--------------------------------------------------------------------*/

static int
child_listener(const struct vev *e, int what)
{

	(void)e;
	if ((what & ~EV_RD)) {
		ev_listen = NULL;
		return (1);
	}
	if (VLU_Fd(child_output, vlu)) {
		ev_listen = NULL;
		return (1);
	}
	return (0);
}

/*--------------------------------------------------------------------*/

static int
child_poker(const struct vev *e, int what)
{

	(void)e;
	(void)what;
	if (child_state != CH_RUNNING)
		return (1);
	if (child_pid < 0)
		return (0);
	if (!mgt_cli_askchild(NULL, NULL, "ping\n"))
		return (0);
	return (0);
}

/*--------------------------------------------------------------------
 * If CLI communications with the child process fails, there is nothing
 * for us to do but to drag it behind the barn and get it over with.
 *
 * The typical case is where the child process fails to return a reply
 * before the cli_timeout expires.  This invalidates the CLI pipes for
 * all future use, as we don't know if the child was just slow and the
 * result gets piped later on, or if the child is catatonic.
 */

void
MGT_Child_Cli_Fail(void)
{

	if (child_state != CH_RUNNING)
		return;
	if (child_pid < 0)
		return;
	REPORT(LOG_ERR, "Child (%jd) not responding to CLI, killing it.",
	    (intmax_t)child_pid);
	if (params->diag_bitmap & 0x1000)
		(void)kill(child_pid, SIGKILL);
	else
		(void)kill(child_pid, SIGQUIT);
}

/*--------------------------------------------------------------------*/

static int
open_sockets(void)
{
	struct listen_sock *ls, *ls2;
	int good = 0;

	VTAILQ_FOREACH_SAFE(ls, &heritage.socks, list, ls2) {
		if (ls->sock >= 0) {
			good++;
			continue;
		}
		ls->sock = VSS_bind(ls->addr);
		if (ls->sock < 0)
			continue;

		mgt_child_inherit(ls->sock, "sock");

		/*
		 * Set nonblocking mode to avoid a race where a client
		 * closes before we call accept(2) and nobody else are in
		 * the listen queue to release us.
		 */
		(void)VTCP_nonblocking(ls->sock);
		(void)VTCP_filter_http(ls->sock);
		good++;
	}
	if (!good)
		return (1);
	return (0);
}

/*--------------------------------------------------------------------*/

static void
close_sockets(void)
{
	struct listen_sock *ls;

	VTAILQ_FOREACH(ls, &heritage.socks, list) {
		if (ls->sock < 0)
			continue;
		mgt_child_inherit(ls->sock, NULL);
		closex(&ls->sock);
	}
}

/*--------------------------------------------------------------------*/

static void
start_child(struct cli *cli)
{
	pid_t pid;
	unsigned u;
	char *p;
	struct vev *e;
	int i, cp[2];

	if (child_state != CH_STOPPED && child_state != CH_DIED)
		return;

	if (open_sockets() != 0) {
		child_state = CH_STOPPED;
		if (cli != NULL) {
			VCLI_SetResult(cli, CLIS_CANT);
			VCLI_Out(cli, "Could not open sockets");
			return;
		}
		REPORT0(LOG_ERR,
		    "Child start failed: could not open sockets");
		return;
	}

	child_state = CH_STARTING;

	/* Open pipe for mgr->child CLI */
	AZ(pipe(cp));
	heritage.cli_in = cp[0];
	mgt_child_inherit(heritage.cli_in, "cli_in");
	child_VCLI_Out = cp[1];

	/* Open pipe for child->mgr CLI */
	AZ(pipe(cp));
	heritage.VCLI_Out = cp[1];
	mgt_child_inherit(heritage.VCLI_Out, "VCLI_Out");
	child_cli_in = cp[0];

	/*
	 * Open pipe for child stdout/err
	 * NB: not inherited, because we dup2() it to stdout/stderr in child
	 */
	AZ(pipe(cp));
	heritage.std_fd = cp[1];
	child_output = cp[0];

	MCF_ParamSync();
	if ((pid = fork()) < 0) {
		perror("Could not fork child");
		exit(1);
	}
	if (pid == 0) {

		/* Redirect stdin/out/err */
		AZ(close(STDIN_FILENO));
		assert(open("/dev/null", O_RDONLY) == STDIN_FILENO);
		assert(dup2(heritage.std_fd, STDOUT_FILENO) == STDOUT_FILENO);
		assert(dup2(heritage.std_fd, STDERR_FILENO) == STDERR_FILENO);

		/* Close anything we shouldn't know about */
		closelog();
		for (i = STDERR_FILENO + 1; i < CLOSE_FD_UP_TO; i++) {
			if (vbit_test(fd_map, i))
				continue;
			(void)(close(i) == 0);
		}
#ifdef HAVE_SETPROCTITLE
		setproctitle("Varnish-Chld %s", heritage.name);
#endif

		(void)signal(SIGINT, SIG_DFL);
		(void)signal(SIGTERM, SIG_DFL);

		mgt_sandbox();

		child_main();

		exit(1);
	}
	REPORT(LOG_NOTICE, "child (%jd) Started", (intmax_t)pid);

	/* Close stuff the child got */
	closex(&heritage.std_fd);

	mgt_child_inherit(heritage.cli_in, NULL);
	closex(&heritage.cli_in);

	mgt_child_inherit(heritage.VCLI_Out, NULL);
	closex(&heritage.VCLI_Out);

	close_sockets();

	vlu = VLU_New(NULL, child_line, 0);
	AN(vlu);

	AZ(ev_listen);
	e = vev_new();
	XXXAN(e);
	e->fd = child_output;
	e->fd_flags = EV_RD;
	e->name = "Child listener";
	e->callback = child_listener;
	AZ(vev_add(mgt_evb, e));
	ev_listen = e;
	AZ(ev_poker);
	if (params->ping_interval > 0) {
		e = vev_new();
		XXXAN(e);
		e->timeout = params->ping_interval;
		e->callback = child_poker;
		e->name = "child poker";
		AZ(vev_add(mgt_evb, e));
		ev_poker = e;
	}

	mgt_cli_start_child(child_cli_in, child_VCLI_Out);
	child_pid = pid;
	if (mgt_push_vcls_and_start(&u, &p)) {
		REPORT(LOG_ERR, "Pushing vcls failed:\n%s", p);
		free(p);
		child_state = CH_RUNNING;
		mgt_stop_child();
	} else
		child_state = CH_RUNNING;
}

/*--------------------------------------------------------------------*/

void
mgt_stop_child(void)
{

	if (child_state != CH_RUNNING)
		return;

	child_state = CH_STOPPING;

	REPORT0(LOG_DEBUG, "Stopping Child");
	if (ev_poker != NULL) {
		vev_del(mgt_evb, ev_poker);
		free(ev_poker);
	}
	ev_poker = NULL;

	mgt_cli_stop_child();

	/* We tell the child to die gracefully by closing the CLI */
	closex(&child_VCLI_Out);
	closex(&child_cli_in);
}

/*--------------------------------------------------------------------*/

static void
mgt_report_panic(pid_t r)
{

	if (VSM_head->panicstr[0] == '\0')
		return;
	REPORT(LOG_ERR, "Child (%jd) Panic message: %s",
	    (intmax_t)r, VSM_head->panicstr);
}

static void
mgt_save_panic(void)
{
	char time_str[30];
	if (VSM_head->panicstr[0] == '\0')
		return;

	if (child_panic)
		VSB_delete(child_panic);
	child_panic = VSB_new_auto();
	XXXAN(child_panic);
	TIM_format(TIM_real(), time_str);
	VSB_printf(child_panic, "Last panic at: %s\n", time_str);
	VSB_cat(child_panic, VSM_head->panicstr);
	AZ(VSB_finish(child_panic));
}

static void
mgt_clear_panic(void)
{
	VSB_delete(child_panic);
	child_panic = NULL;
}

/*--------------------------------------------------------------------*/

static int
mgt_sigchld(const struct vev *e, int what)
{
	int status;
	struct vsb *vsb;
	pid_t r;

	(void)e;
	(void)what;

	if (ev_poker != NULL) {
		vev_del(mgt_evb, ev_poker);
		free(ev_poker);
	}
	ev_poker = NULL;

	r = waitpid(child_pid, &status, WNOHANG);
	if (r == 0 || (r == -1 && errno == ECHILD))
		return (0);
	assert(r == child_pid);
	vsb = VSB_new_auto();
	XXXAN(vsb);
	VSB_printf(vsb, "Child (%d) %s", r, status ? "died" : "ended");
	if (WIFEXITED(status) && WEXITSTATUS(status)) {
		VSB_printf(vsb, " status=%d", WEXITSTATUS(status));
		exit_status |= 0x20;
	}
	if (WIFSIGNALED(status)) {
		VSB_printf(vsb, " signal=%d", WTERMSIG(status));
		exit_status |= 0x40;
	}
#ifdef WCOREDUMP
	if (WCOREDUMP(status)) {
		VSB_printf(vsb, " (core dumped)");
		exit_status |= 0x80;
	}
#endif
	AZ(VSB_finish(vsb));
	REPORT(LOG_INFO, "%s", VSB_data(vsb));
	VSB_delete(vsb);

	mgt_report_panic(r);
	mgt_save_panic();

	child_pid = -1;

	if (child_state == CH_RUNNING) {
		child_state = CH_DIED;
		mgt_cli_stop_child();
		closex(&child_VCLI_Out);
		closex(&child_cli_in);
	}

	if (ev_listen != NULL) {
		vev_del(mgt_evb, ev_listen);
		free(ev_listen);
		ev_listen = NULL;
	}
	/* Pick up any stuff lingering on stdout/stderr */
	(void)child_listener(NULL, EV_RD);
	closex(&child_output);

	REPORT0(LOG_DEBUG, "Child cleanup complete");

	if (child_state == CH_DIED && params->auto_restart)
		start_child(NULL);
	else if (child_state == CH_DIED) {
		child_state = CH_STOPPED;
	} else if (child_state == CH_STOPPING)
		child_state = CH_STOPPED;

	return (0);
}

/*--------------------------------------------------------------------*/

static int
mgt_sigint(const struct vev *e, int what)
{

	(void)e;
	(void)what;
	REPORT0(LOG_ERR, "Manager got SIGINT");
	(void)fflush(stdout);
	if (child_pid >= 0)
		mgt_stop_child();
	exit (2);
}

/*--------------------------------------------------------------------
 * This thread is the master thread in the management process.
 * The relatively simple task is to start and stop the child process
 * and to reincarnate it in case of trouble.
 */

void
MGT_Run(void)
{
	struct sigaction sac;
	struct vev *e;
	int i;

	e = vev_new();
	XXXAN(e);
	e->sig = SIGTERM;
	e->callback = mgt_sigint;
	e->name = "mgt_sigterm";
	AZ(vev_add(mgt_evb, e));

	e = vev_new();
	XXXAN(e);
	e->sig = SIGINT;
	e->callback = mgt_sigint;
	e->name = "mgt_sigint";
	AZ(vev_add(mgt_evb, e));

	e = vev_new();
	XXXAN(e);
	e->sig = SIGCHLD;
	e->sig_flags = SA_NOCLDSTOP;
	e->callback = mgt_sigchld;
	e->name = "mgt_sigchild";
	AZ(vev_add(mgt_evb, e));

#ifdef HAVE_SETPROCTITLE
	setproctitle("Varnish-Mgr %s", heritage.name);
#endif

	memset(&sac, 0, sizeof sac);
	sac.sa_handler = SIG_IGN;
	sac.sa_flags = SA_RESTART;

	AZ(sigaction(SIGPIPE, &sac, NULL));
	AZ(sigaction(SIGHUP, &sac, NULL));

	if (!d_flag && !mgt_has_vcl())
		REPORT0(LOG_ERR, "No VCL loaded yet");
	else if (!d_flag) {
		start_child(NULL);
		if (child_state == CH_STOPPED) {
			exit_status = 2;
			return;
		}
	}

	i = vev_schedule(mgt_evb);
	if (i != 0)
		REPORT(LOG_ERR, "vev_schedule() = %d", i);

	REPORT0(LOG_ERR, "manager dies");
}

/*--------------------------------------------------------------------*/

void __match_proto__(cli_func_t)
mcf_server_startstop(struct cli *cli, const char * const *av, void *priv)
{

	(void)av;
	if (priv != NULL && child_state == CH_RUNNING)
		mgt_stop_child();
	else if (priv == NULL && child_state == CH_STOPPED) {
		if (mgt_has_vcl()) {
			start_child(cli);
		} else {
			VCLI_SetResult(cli, CLIS_CANT);
			VCLI_Out(cli, "No VCL available");
		}
	} else {
		VCLI_SetResult(cli, CLIS_CANT);
		VCLI_Out(cli, "Child in state %s", ch_state[child_state]);
	}
}

/*--------------------------------------------------------------------*/

void
mcf_server_status(struct cli *cli, const char * const *av, void *priv)
{
	(void)av;
	(void)priv;
	VCLI_Out(cli, "Child in state %s", ch_state[child_state]);
}

void
mcf_panic_show(struct cli *cli, const char * const *av, void *priv)
{
	(void)av;
	(void)priv;

	if (!child_panic) {
	  VCLI_SetResult(cli, CLIS_CANT);
	  VCLI_Out(cli, "Child has not panicked or panic has been cleared");
	  return;
	}

	VCLI_Out(cli, "%s\n", VSB_data(child_panic));
}

void
mcf_panic_clear(struct cli *cli, const char * const *av, void *priv)
{
	(void)av;
	(void)priv;

	if (!child_panic) {
	  VCLI_SetResult(cli, CLIS_CANT);
	  VCLI_Out(cli, "No panic to clear");
	  return;
	}

	mgt_clear_panic();
}
