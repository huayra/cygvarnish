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
 *
 * TODO:
 *
 * There is a risk that the child process might corrupt the VSM segment
 * and we should capture that event and recover gracefully.
 *
 * A possible state diagram could be:
 *
 *	[manager start]
 *		|
 *		v
 *      Open old VSM,
 *	check pid	--------> exit/fail (-n message)
 *		|
 *		+<----------------------+
 *		|			^
 *		v			|
 *	Create new VSM			|
 *		|			|
 *		v			|
 *	Init header			|
 *	Alloc VSL			|
 *	Alloc VSC:Main			|
 *	Alloc Args etc.			|
 *		|			|
 *		+<--------------+	|
 *		|		^	|
 *		v		|	|
 *	start worker		|	|
 *		|		|	|
 *		|		|	+<---- worker crash
 *		v		|	^
 *	Reset VSL ptr.		|	|
 *	Reset VSC counters	|	|
 *		|		|	|
 *		+<------+	|	|
 *		|	^	|	|
 *		v	|	|	|
 *	alloc dynamics	|	|	|
 *	free dynamics	|	|	|
 *		|	|	|	|
 *		v	|	|	|
 *		+------>+	|	|
 *		|		|	|
 *		v		|	|
 *	stop worker		|	|
 *		|		|	|
 *		v		|	|
 *	Check consist---------- | ----->+
 *		|		|
 *		v		|
 *	Free dynamics		|
 *		|		|
 *		v		|
 *		+-------------->+
 *
 */

#include "config.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "vsc.h"
#include "vsl.h"
#include "mgt.h"
#include "heritage.h"
#include "vmb.h"
#include "vsm.h"
#include "flopen.h"

#ifndef MAP_HASSEMAPHORE
#define MAP_HASSEMAPHORE 0 /* XXX Linux */
#endif

#ifndef MAP_NOSYNC
#define MAP_NOSYNC 0 /* XXX Linux */
#endif

struct VSC_C_main	*VSC_C_main;

static int vsl_fd = -1;

/*--------------------------------------------------------------------
 * Check that we are not started with the same -n argument as an already
 * running varnishd
 */

static void
vsl_n_check(int fd)
{
	struct VSM_head slh;
	int i;
	struct stat st;
	pid_t pid;

	AZ(fstat(fd, &st));
	if (!S_ISREG(st.st_mode))
		ARGV_ERR("\tshmlog: Not a file\n");

	/* Test if the SHMFILE is locked by other Varnish */
	if (fltest(fd, &pid) > 0) {
		fprintf(stderr,
			"SHMFILE locked by running varnishd master (pid=%jd)\n",
			(intmax_t)pid);
		fprintf(stderr,
			"(Use unique -n arguments if you want multiple "
			"instances)\n");
		exit(2);
	}

	/* Read owning pid from SHMFILE */
	memset(&slh, 0, sizeof slh);	/* XXX: for flexelint */
	i = read(fd, &slh, sizeof slh);
	if (i != sizeof slh)
		return;
	if (slh.magic != VSM_HEAD_MAGIC)
		return;
	if (slh.hdrsize != sizeof slh)
		return;
	if (slh.master_pid != 0 && !kill(slh.master_pid, 0)) {
		fprintf(stderr,
			"WARNING: Taking over SHMFILE marked as owned by "
			"running process (pid=%jd)\n",
			(intmax_t)slh.master_pid);
	}
}

/*--------------------------------------------------------------------
 * Build a new shmlog file
 */

static void
vsl_buildnew(const char *fn, unsigned size, int fill)
{
	struct VSM_head slh;
	int i;
	unsigned u;
	char buf[64*1024];
	int flags;

	(void)unlink(fn);
	vsl_fd = flopen(fn, O_RDWR | O_CREAT | O_EXCL | O_NONBLOCK, 0644);
	if (vsl_fd < 0) {
		fprintf(stderr, "Could not create %s: %s\n",
		    fn, strerror(errno));
		exit (1);
	}
	flags = fcntl(vsl_fd, F_GETFL);
	assert(flags != -1);
	flags &= ~O_NONBLOCK;
	AZ(fcntl(vsl_fd, F_SETFL, flags));

	memset(&slh, 0, sizeof slh);
	slh.magic = VSM_HEAD_MAGIC;
	slh.hdrsize = sizeof slh;
	slh.shm_size = size;
	i = write(vsl_fd, &slh, sizeof slh);
	xxxassert(i == sizeof slh);

	if (fill) {
		memset(buf, 0, sizeof buf);
		for (u = sizeof slh; u < size; ) {
			i = write(vsl_fd, buf, sizeof buf);
			if (i <= 0) {
				fprintf(stderr, "Write error %s: %s\n",
				    fn, strerror(errno));
				exit (1);
			}
			u += i;
		}
	}

	AZ(ftruncate(vsl_fd, (off_t)size));
}

/*--------------------------------------------------------------------
 * Exit handler that clears the owning pid from the SHMLOG
 */

static
void
mgt_shm_atexit(void)
{
	if (getpid() == VSM_head->master_pid)
		VSM_head->master_pid = 0;
}

void
mgt_SHM_Init(const char *l_arg)
{
	int i, fill;
	struct params *pp;
	const char *q;
	uintmax_t size, s1, s2, ps;
	char **av, **ap;
	uint32_t *vsl_log_start;

	if (l_arg == NULL)
		l_arg = "";

	av = VAV_Parse(l_arg, NULL, ARGV_COMMA);
	AN(av);
	if (av[0] != NULL)
		ARGV_ERR("\t-l ...: %s", av[0]);

	ap = av + 1;

	/* Size of SHMLOG */
	if (*ap != NULL && **ap != '\0') {
		q = str2bytes(*ap, &s1, 0);
		if (q != NULL)
			ARGV_ERR("\t-l[1] ...:  %s\n", q);
	} else {
		s1 = 80 * 1024 * 1024;
	}
	if (*ap != NULL)
		ap++;

	/* Size of space for other stuff */
	if (*ap != NULL && **ap != '\0') {
		q = str2bytes(*ap, &s2, 0);
		if (q != NULL)
			ARGV_ERR("\t-l[2] ...:  %s\n", q);
	} else {
		s2 = 1024 * 1024;
	}
	if (*ap != NULL)
		ap++;

	/* Fill or not ? */
	if (*ap != NULL) {
		if (**ap == '\0')
			fill = 1;
		else if (!strcmp(*ap, "-"))
			fill = 0;
		else if (!strcmp(*ap, "+"))
			fill = 1;
		else
			ARGV_ERR("\t-l[3] ...:  Must be \"-\" or \"+\"\n");
		ap++;
	} else {
		fill = 1;
	}

	if (*ap != NULL)
		ARGV_ERR("\t-l ...:  Too many sub-args\n");

	VAV_Free(av);

	size = s1 + s2;
	ps = getpagesize();
	size += ps - 1;
	size &= ~(ps - 1);

	i = open(VSM_FILENAME, O_RDWR, 0644);
	if (i >= 0) {
		vsl_n_check(i);
		(void)close(i);
	}
	vsl_buildnew(VSM_FILENAME, size, fill);

	VSM_head = (void *)mmap(NULL, size,
	    PROT_READ|PROT_WRITE,
	    MAP_HASSEMAPHORE | MAP_NOSYNC | MAP_SHARED,
	    vsl_fd, 0);
	VSM_head->master_pid = getpid();
	AZ(atexit(mgt_shm_atexit));
	xxxassert(VSM_head != MAP_FAILED);
	(void)mlock((void*)VSM_head, size);

	memset(&VSM_head->head, 0, sizeof VSM_head->head);
	VSM_head->head.magic = VSM_CHUNK_MAGIC;
	VSM_head->head.len =
	    (uint8_t*)(VSM_head) + size - (uint8_t*)&VSM_head->head;
	bprintf(VSM_head->head.class, "%s", VSM_CLASS_FREE);
	VWMB();

	vsm_end = (void*)((uint8_t*)VSM_head + size);

	VSC_C_main = VSM_Alloc(sizeof *VSC_C_main,
	    VSC_CLASS, VSC_TYPE_MAIN, "");
	AN(VSC_C_main);

	pp = VSM_Alloc(sizeof *pp, VSM_CLASS_PARAM, "", "");
	AN(pp);
	*pp = *params;
	params = pp;

	vsl_log_start = VSM_Alloc(s1, VSL_CLASS, "", "");
	AN(vsl_log_start);
	vsl_log_start[1] = VSL_ENDMARKER;
	VWMB();

	do
		*vsl_log_start = random() & 0xffff;
	while (*vsl_log_start == 0);

	VWMB();

	do
		VSM_head->alloc_seq = random();
	while (VSM_head->alloc_seq == 0);

}

void
mgt_SHM_Pid(void)
{

	VSM_head->master_pid = getpid();
}
