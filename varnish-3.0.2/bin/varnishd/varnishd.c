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
 * The management process and CLI handling
 */

#include "config.h"

#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include <sys/utsname.h>

#include "compat/daemon.h"

#include "vsb.h"
#include "vev.h"
#include "vpf.h"
#include "vsha256.h"

#include "vcli.h"
#include "cli_priv.h"
#include "cli_common.h"

#include "vin.h"
#include "heritage.h"
#include "mgt.h"
#include "hash_slinger.h"
#include "stevedore.h"

/* INFTIM indicates an infinite timeout for poll(2) */
#ifndef INFTIM
#define INFTIM -1
#endif

struct heritage		heritage;
volatile struct params	*params;
unsigned		d_flag = 0;
pid_t			mgt_pid;
struct vev_base		*mgt_evb;
int			exit_status = 0;
struct vsb		*vident;

static void
build_vident(void)
{
	struct utsname uts;

	vident = VSB_new_auto();
	AN(vident);
	if (!uname(&uts)) {
		VSB_printf(vident, ",%s", uts.sysname);
		VSB_printf(vident, ",%s", uts.release);
		VSB_printf(vident, ",%s", uts.machine);
	}
}

/*--------------------------------------------------------------------*/

const void *
pick(const struct choice *cp, const char *which, const char *kind)
{

	for(; cp->name != NULL; cp++) {
		if (!strcmp(cp->name, which))
			return (cp->ptr);
	}
	ARGV_ERR("Unknown %s method \"%s\"\n", kind, which);
}

/*--------------------------------------------------------------------*/

static unsigned long
arg_ul(const char *p)
{
	char *q;
	unsigned long ul;

	ul = strtoul(p, &q, 0);
	if (*q != '\0')
		ARGV_ERR("Invalid number: \"%s\"\n", p);
	return (ul);
}

/*--------------------------------------------------------------------*/

static void
usage(void)
{
#define FMT "    %-28s # %s\n"

	fprintf(stderr, "usage: varnishd [options]\n");
	fprintf(stderr, FMT, "-a address:port", "HTTP listen address and port");
	fprintf(stderr, FMT, "-b address:port", "backend address and port");
	fprintf(stderr, FMT, "", "   -b <hostname_or_IP>");
	fprintf(stderr, FMT, "", "   -b '<hostname_or_IP>:<port_or_service>'");
	fprintf(stderr, FMT, "-C", "print VCL code compiled to C language");
	fprintf(stderr, FMT, "-d", "debug");
	fprintf(stderr, FMT, "-f file", "VCL script");
	fprintf(stderr, FMT, "-F", "Run in foreground");
	fprintf(stderr, FMT, "-h kind[,hashoptions]", "Hash specification");
	fprintf(stderr, FMT, "", "  -h critbit [default]");
	fprintf(stderr, FMT, "", "  -h simple_list");
	fprintf(stderr, FMT, "", "  -h classic");
	fprintf(stderr, FMT, "", "  -h classic,<buckets>");
	fprintf(stderr, FMT, "-i identity", "Identity of varnish instance");
	fprintf(stderr, FMT, "-l shl,free,fill", "Size of shared memory file");
	fprintf(stderr, FMT, "", "  shl: space for SHL records [80m]");
	fprintf(stderr, FMT, "", "  free: space for other allocations [1m]");
	fprintf(stderr, FMT, "", "  fill: prefill new file [+]");
	fprintf(stderr, FMT, "-M address:port", "Reverse CLI destination.");
	fprintf(stderr, FMT, "-n dir", "varnishd working directory");
	fprintf(stderr, FMT, "-P file", "PID file");
	fprintf(stderr, FMT, "-p param=value", "set parameter");
	fprintf(stderr, FMT,
	    "-s kind[,storageoptions]", "Backend storage specification");
	fprintf(stderr, FMT, "", "  -s malloc");
#ifdef HAVE_LIBUMEM
	fprintf(stderr, FMT, "", "  -s umem");
#endif
	fprintf(stderr, FMT, "", "  -s file  [default: use /tmp]");
	fprintf(stderr, FMT, "", "  -s file,<dir_or_file>");
	fprintf(stderr, FMT, "", "  -s file,<dir_or_file>,<size>");
	fprintf(stderr, FMT, "", "  -s persist{experimenta}");
	fprintf(stderr, FMT, "",
	    "  -s file,<dir_or_file>,<size>,<granularity>");
	fprintf(stderr, FMT, "-t", "Default TTL");
	fprintf(stderr, FMT, "-S secret-file",
	    "Secret file for CLI authentication");
	fprintf(stderr, FMT, "-T address:port",
	    "Telnet listen address and port");
	fprintf(stderr, FMT, "-V", "version");
	fprintf(stderr, FMT, "-w int[,int[,int]]", "Number of worker threads");
	fprintf(stderr, FMT, "", "  -w <fixed_count>");
	fprintf(stderr, FMT, "", "  -w min,max");
	fprintf(stderr, FMT, "", "  -w min,max,timeout [default: -w2,500,300]");
	fprintf(stderr, FMT, "-u user", "Priviledge separation user id");
#undef FMT
	exit(1);
}


/*--------------------------------------------------------------------*/

static void
tackle_warg(const char *argv)
{
	char **av;
	unsigned int u;

	av = VAV_Parse(argv, NULL, ARGV_COMMA);
	AN(av);

	if (av[0] != NULL)
		ARGV_ERR("%s\n", av[0]);

	if (av[1] == NULL)
		usage();

	u = arg_ul(av[1]);
	if (u < 1)
		usage();
	params->wthread_max = params->wthread_min = u;

	if (av[2] != NULL) {
		u = arg_ul(av[2]);
		if (u < params->wthread_min)
			usage();
		params->wthread_max = u;

		if (av[3] != NULL) {
			u = arg_ul(av[3]);
			params->wthread_timeout = u;
		}
	}
	VAV_Free(av);
}

/*--------------------------------------------------------------------*/

static void
cli_check(const struct cli *cli)
{
	if (cli->result == CLIS_OK) {
		VSB_clear(cli->sb);
		return;
	}
	AZ(VSB_finish(cli->sb));
	fprintf(stderr, "Error:\n%s\n", VSB_data(cli->sb));
	exit (2);
}

/*--------------------------------------------------------------------
 * All praise POSIX!  Thanks to our glorious standards there are no
 * standard way to get a back-trace of the stack, and even if we hack
 * that together from spit and pieces of string, there is no way no
 * standard way to translate a pointer to a symbol, which returns anything
 * usable.  (See for instance FreeBSD PR-134391).
 *
 * Attempt to run nm(1) on our binary during startup, hoping it will
 * give us a usable list of symbols.
 */

struct symbols {
	uintptr_t		a;
	char			*n;
	VTAILQ_ENTRY(symbols)	list;
};

static VTAILQ_HEAD(,symbols) symbols = VTAILQ_HEAD_INITIALIZER(symbols);

int
Symbol_Lookup(struct vsb *vsb, void *ptr)
{
	struct symbols *s, *s0;
	uintptr_t pp;

	pp = (uintptr_t)ptr;
	s0 = NULL;
	VTAILQ_FOREACH(s, &symbols, list) {
		if (s->a > pp)
			continue;
		if (s0 == NULL || s->a > s0->a)
			s0 = s;
	}
	if (s0 == NULL)
		return (-1);
	VSB_printf(vsb, "%p: %s+%jx", ptr, s0->n, (uintmax_t)pp - s0->a);
	return (0);
}

static void
Symbol_hack(const char *a0)
{
	char buf[BUFSIZ], *p, *e;
	FILE *fi;
	uintptr_t a;
	struct symbols *s;

	bprintf(buf, "nm -an %s 2>/dev/null", a0);
	fi = popen(buf, "r");
	if (fi == NULL)
		return;
	while (fgets(buf, sizeof buf, fi)) {
		if (buf[0] == ' ')
			continue;
		p = NULL;
		a = strtoul(buf, &p, 16);
		if (p == NULL)
			continue;
		if (a == 0)
			continue;
		if (*p++ != ' ')
			continue;
		p++;
		if (*p++ != ' ')
			continue;
		if (*p <= ' ')
			continue;
		e = strchr(p, '\0');
		AN(e);
		while (e > p && isspace(e[-1]))
			e--;
		*e = '\0';
		s = malloc(sizeof *s + strlen(p) + 1);
		AN(s);
		s->a = a;
		s->n = (void*)(s + 1);
		strcpy(s->n, p);
		VTAILQ_INSERT_TAIL(&symbols, s, list);
	}
	(void)pclose(fi);
}

/*--------------------------------------------------------------------
 * This function is called when the CLI on stdin is closed.
 */

static void
cli_stdin_close(void *priv)
{

	(void)priv;
	(void)close(0);
	(void)close(1);
	(void)close(2);
	assert(open("/dev/null", O_RDONLY) == 0);
	assert(open("/dev/null", O_WRONLY) == 1);
	assert(open("/dev/null", O_WRONLY) == 2);

	if (d_flag) {
		mgt_stop_child();
		mgt_cli_close_all();
		exit(0);
	}
}

/*--------------------------------------------------------------------*/

int
main(int argc, char * const *argv)
{
	int o;
	unsigned C_flag = 0;
	unsigned F_flag = 0;
	const char *b_arg = NULL;
	const char *f_arg = NULL;
	const char *i_arg = NULL;
	const char *l_arg = NULL;	/* default in mgt_shmem.c */
	const char *h_arg = "critbit";
	const char *M_arg = NULL;
	const char *n_arg = NULL;
	const char *P_arg = NULL;
	const char *S_arg = NULL;
	const char *s_arg = "file";
	int s_arg_given = 0;
	const char *T_arg = NULL;
	char *p, *vcl = NULL;
	struct cli cli[1];
	struct vpf_fh *pfh = NULL;
	char *dirname;

	/*
	 * Start out by closing all unwanted file descriptors we might
	 * have inherited from sloppy process control daemons.
	 */
	for (o = getdtablesize(); o > STDERR_FILENO; o--)
		(void)close(o);

	AZ(seed_random());

	mgt_got_fd(STDERR_FILENO);

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	build_vident();

	Symbol_hack(argv[0]);

	/* for ASSERT_MGT() */
	mgt_pid = getpid();

	/*
	 * Run in UTC timezone, on the off-chance that this operating
	 * system does not have a timegm() function, and translates
	 * timestamps on the local timescale.
	 * See lib/libvarnish/time.c
	 */
	AZ(setenv("TZ", "UTC", 1));
	tzset();
	assert(TIM_parse("Sun, 06 Nov 1994 08:49:37 GMT") == 784111777);
	assert(TIM_parse("Sunday, 06-Nov-94 08:49:37 GMT") == 784111777);
	assert(TIM_parse("Sun Nov  6 08:49:37 1994") == 784111777);

	/*
	 * Check that our SHA256 works
	 */
	SHA256_Test();

	memset(cli, 0, sizeof cli);
	cli[0].sb = VSB_new_auto();
	XXXAN(cli[0].sb);
	cli[0].result = CLIS_OK;

	VTAILQ_INIT(&heritage.socks);

	MCF_ParamInit(cli);

	if (sizeof(void *) < 8) {
		/*
		 * Adjust default parameters for 32 bit systems to conserve
		 * VM space.
		 */
		MCF_ParamSet(cli, "sess_workspace", "16384");
		cli_check(cli);

		MCF_ParamSet(cli, "thread_pool_workspace", "16384");
		cli_check(cli);

		MCF_ParamSet(cli, "http_resp_size", "8192");
		cli_check(cli);

		MCF_ParamSet(cli, "http_req_size", "12288");
		cli_check(cli);

		MCF_ParamSet(cli, "thread_pool_stack", "32bit");
		cli_check(cli);

		MCF_ParamSet(cli, "gzip_stack_buffer", "4096");
		cli_check(cli);
	}

	cli_check(cli);

	while ((o = getopt(argc, argv,
	    "a:b:Cdf:Fg:h:i:l:L:M:n:P:p:S:s:T:t:u:Vx:w:")) != -1)
		switch (o) {
		case 'a':
			MCF_ParamSet(cli, "listen_address", optarg);
			cli_check(cli);
			break;
		case 'b':
			b_arg = optarg;
			break;
		case 'C':
			C_flag = 1 - C_flag;
			break;
		case 'd':
			d_flag++;
			break;
		case 'f':
			f_arg = optarg;
			break;
		case 'F':
			F_flag = 1 - F_flag;
			break;
		case 'g':
			MCF_ParamSet(cli, "group", optarg);
			break;
		case 'h':
			h_arg = optarg;
			break;
		case 'i':
			i_arg = optarg;
			break;
		case 'l':
			l_arg = optarg;
			break;
		case 'M':
			M_arg = optarg;
			break;
		case 'n':
			n_arg = optarg;
			break;
		case 'P':
			P_arg = optarg;
			break;
		case 'p':
			p = strchr(optarg, '=');
			if (p == NULL)
				usage();
			AN(p);
			*p++ = '\0';
			MCF_ParamSet(cli, optarg, p);
			cli_check(cli);
			break;
		case 's':
			s_arg_given = 1;
			STV_Config(optarg);
			break;
		case 't':
			MCF_ParamSet(cli, "default_ttl", optarg);
			break;
		case 'S':
			S_arg = optarg;
			break;
		case 'T':
			T_arg = optarg;
			break;
		case 'u':
			MCF_ParamSet(cli, "user", optarg);
			break;
		case 'V':
			/* XXX: we should print the ident here */
			VCS_Message("varnishd");
			exit(0);
		case 'x':
			if (!strcmp(optarg, "dumprst")) {
				MCF_DumpRst();
				exit (0);
			}
			usage();
			break;
		case 'w':
			tackle_warg(optarg);
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	mgt_vcc_init();

	if (argc != 0) {
		fprintf(stderr, "Too many arguments (%s...)\n", argv[0]);
		usage();
	}

	/* XXX: we can have multiple CLI actions above, is this enough ? */
	if (cli[0].result != CLIS_OK) {
		fprintf(stderr, "Parameter errors:\n");
		AZ(VSB_finish(cli[0].sb));
		fprintf(stderr, "%s\n", VSB_data(cli[0].sb));
		exit(1);
	}

	if (d_flag && F_flag) {
		fprintf(stderr, "Only one of -d or -F can be specified\n");
		usage();
	}

	if (b_arg != NULL && f_arg != NULL) {
		fprintf(stderr, "Only one of -b or -f can be specified\n");
		usage();
	}
	if (S_arg == NULL && T_arg == NULL && d_flag == 0 && b_arg == NULL &&
	    f_arg == NULL && M_arg == NULL) {
		fprintf(stderr,
		    "At least one of -d, -b, -f, -M, -S or -T "
		    "must be specified\n");
		usage();
	}

	if (f_arg != NULL) {
		vcl = vreadfile(NULL, f_arg, NULL);
		if (vcl == NULL) {
			fprintf(stderr, "Cannot read '%s': %s\n",
			    f_arg, strerror(errno));
			exit(1);
		}
	}

	if (VIN_N_Arg(n_arg, &heritage.name, &dirname, NULL) != 0) {
		fprintf(stderr, "Invalid instance name: %s\n",
		    strerror(errno));
		exit(1);
	}

	if (i_arg != NULL) {
		if (snprintf(heritage.identity, sizeof heritage.identity,
		    "%s", i_arg) > sizeof heritage.identity) {
			fprintf(stderr, "Invalid identity name: %s\n",
			    strerror(ENAMETOOLONG));
			exit(1);
		}
	}

	if (n_arg != NULL)
		openlog(n_arg, LOG_PID, LOG_LOCAL0);
	else
		openlog("varnishd", LOG_PID, LOG_LOCAL0);

	if (mkdir(dirname, 0755) < 0 && errno != EEXIST) {
		fprintf(stderr, "Cannot create working directory '%s': %s\n",
		    dirname, strerror(errno));
		exit(1);
	}

	if (chdir(dirname) < 0) {
		fprintf(stderr, "Cannot change to working directory '%s': %s\n",
		    dirname, strerror(errno));
		exit(1);
	}

	/* XXX: should this be relative to the -n arg ? */
	if (P_arg && (pfh = VPF_Open(P_arg, 0644, NULL)) == NULL) {
		perror(P_arg);
		exit(1);
	}

	if (b_arg != NULL || f_arg != NULL)
		if (mgt_vcc_default(b_arg, f_arg, vcl, C_flag))
			exit (2);

	if (C_flag)
		exit (0);

	/* If no -s argument specified, process default -s argument */
	if (!s_arg_given)
		STV_Config(s_arg);

	/* Configure Transient storage, if user did not */
	STV_Config_Transient();

	HSH_config(h_arg);

	mgt_SHM_Init(l_arg);

	AZ(VSB_finish(vident));

	if (!d_flag && !F_flag)
		AZ(varnish_daemon(1, 0));

	mgt_SHM_Pid();

	if (pfh != NULL && VPF_Write(pfh))
		fprintf(stderr, "NOTE: Could not write PID file\n");

	if (d_flag)
		fprintf(stderr, "Platform: %s\n", VSB_data(vident) + 1);
	syslog(LOG_NOTICE, "Platform: %s\n", VSB_data(vident) + 1);

	/* Do this again after debugstunt and daemon has run */
	mgt_pid = getpid();

	mgt_evb = vev_new_base();
	XXXAN(mgt_evb);

	if (d_flag)
		mgt_cli_setup(0, 1, 1, "debug", cli_stdin_close, NULL);
	if (S_arg != NULL)
		mgt_cli_secret(S_arg);
	if (M_arg != NULL)
		mgt_cli_master(M_arg);
	if (T_arg != NULL)
		mgt_cli_telnet(T_arg);

	AN(VSM_Alloc(0, VSM_CLASS_MARK, "", ""));

	MGT_Run();

	if (pfh != NULL)
		(void)VPF_Remove(pfh);
	exit(exit_status);
}
