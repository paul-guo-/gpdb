/*-------------------------------------------------------------------------
 * ic_tcp.c
 *	   Interconnect code in background worker.
 *
 * Portions Copyright (c) 2019-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/motion/ic_bg.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "utils/icbg.h"

/* These are always necessary for a bgworker */
#include "miscadmin.h"
#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/lwlock.h"
#include "storage/proc.h"
#include "storage/shmem.h"
#include "storage/procarray.h"

#include "cdb/cdbvars.h"

static int icbg_idx = -1;

bool
InterconnectStartRule(Datum main_arg)
{
	return true;
}

void
InterconnectLoop(Datum main_arg)
{
	while(true)
	{
		int			rc;
		int			timeout = 20; /* TODO */

		/* */


		rc = WaitLatch(&MyProc->procLatch,
					   WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH,
					   timeout * 1000L);

		ResetLatch(&MyProc->procLatch);

		/* emergency bailout if postmaster has died */
		if (rc & WL_POSTMASTER_DEATH)
			proc_exit(1);
	}
}

/*
 * Launch interconnect workers.
 */
void
LaunchInterconnectWorkers(NULL)
{
	BackgroundWorker		worker;
	BackgroundWorkerHandle *handle;
	BgwHandleStatus			status;
	int						i;

	MemSet(&worker, 0, sizeof(BackgroundWorker));
	worker.bgw_flags = BGWORKER_SHMEM_ACCESS;
	worker.bgw_start_time = BgWorkerStart_ConsistentState;
	worker.bgw_restart_time = BGW_DEFAULT_RESTART_INTERVAL;
	worker.bgw_main = InterconnectMain;
	worker.bgw_notify_pid = MyProcPid;
	worker.bgw_function_name = "InterconnectMain";

	for (i = 0; i < Gp_max_ic_worker; ++i)
	{
		snprintf(worker.bgw_name, BGW_MAXLEN, "interconnect worker %d/%d", i, Gp_max_ic_worker);
		if (RegisterDynamicBackgroundWorker(&worker, &handle))
			elog(ERROR, "RegisterDynamicBackgroundWorker() failed for %d", i);
	}

	for (i = 0; i < Gp_max_ic_worker; ++i)
	{
		status = WaitForBackgroundWorkerStartup(handle, &pid);
		if (status != BGWH_STARTED)
			elog(ERROR, "WaitForBackgroundWorkerStartup() failed for %d", i);
	}
}
/*
 * InterconnectMain
 */
void
InterconnectMain(Datum main_arg)
{
	/* We're now ready to receive signals */
	BackgroundWorkerUnblockSignals();

	InterconnectLoop(main_arg);

	proc_exit(0);
}
