/* -*- C -*- */
/*
 * Copyright (c) 2013-2020 Seagate Technology LLC and/or its Affiliates
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For any questions about this software or licensing,
 * please email opensource@seagate.com or cortx-questions@seagate.com.
 *
 */

/**
 * @addtogroup dtm0
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "dtm0/drlink.h"

#include "ut/ut.h"       /* M0_UT_ASSERT */
#include "rpc/rpclib.h"  /* m0_rpc_client_start */


/* XXX remove after transition to libfabric */
#define M0_NET_XPRT_PREFIX_DEFAULT "lnet"
/* XXX copy-paste */
#define SERVER_ENDPOINT_ADDR   "0@lo:12345:34:1"
#define SERVER_ENDPOINT        M0_NET_XPRT_PREFIX_DEFAULT":"SERVER_ENDPOINT_ADDR
#define DTM0_UT_CONF_PROCESS   "<0x7200000000000001:5>"
#define DTM0_UT_LOG            "dtm0_ut_server.log"

static char *dtm0_ut_argv[] = { "m0d", "-T", "linux",
			       "-D", "dtm0_sdb", "-S", "dtm0_stob",
			       "-A", "linuxstob:dtm0_addb_stob",
			       "-e", SERVER_ENDPOINT,
			       "-H", SERVER_ENDPOINT_ADDR,
			       "-w", "10",
			       "-f", DTM0_UT_CONF_PROCESS,
			       "-c", M0_SRC_PATH("dtm0/conf.xc")};

void m0_dtm0_ut_drlink_simple()
{
	int rc;
	// struct cl_ctx            cctx = {};
	struct m0_rpc_server_ctx sctx = {
		.rsx_xprts         = m0_net_all_xprt_get(),
		.rsx_xprts_nr      = m0_net_xprt_nr(),
		.rsx_argv          = dtm0_ut_argv,
		.rsx_argc          = ARRAY_SIZE(dtm0_ut_argv),
		.rsx_log_file_name = DTM0_UT_LOG,
	};
	/*
	struct m0_reqh_service  *cli_srv;
	struct m0_reqh_service  *srv_srv;
	struct m0_reqh          *srv_reqh;

	srv_reqh = &sctx.rsx_motr_ctx.cc_reqh_ctx.rc_reqh;
	*/

	// m0_fi_enable("m0_dtm0_in_ut", "ut");

	rc = m0_rpc_server_start(&sctx);
	M0_UT_ASSERT(rc == 0);

	/*
	dtm0_ut_client_init(&cctx, cl_ep_addr, srv_ep_addr,
			    m0_net_xprt_default_get());
	rc = m0_dtm_client_service_start(&cctx.cl_ctx.rcx_reqh,
					 &cli_srv_fid, &cli_srv);
	M0_UT_ASSERT(rc == 0);
	srv_srv = m0_reqh_service_lookup(srv_reqh, &srv_dtm0_fid);
	rc = m0_dtm0_service_process_connect(srv_srv, &cli_srv_fid, cl_ep_addr,
					     false);
	M0_UT_ASSERT(rc == 0);

	dtm0_cli_srv_reqh = &cctx.cl_ctx.rcx_reqh;

	dtm0_ut_send_fops(&cctx.cl_ctx.rcx_session);

	rc = m0_dtm0_service_process_disconnect(srv_srv, &cli_srv_fid);
	M0_UT_ASSERT(rc == 0);
	(void)srv_srv;

	m0_dtm_client_service_stop(cli_srv);
	dtm0_ut_client_fini(&cctx);
	*/
	m0_rpc_server_stop(&sctx);
	// m0_fi_disable("m0_dtm0_in_ut", "ut");
}


#undef M0_TRACE_SUBSYSTEM

/** @} end of dtm0 group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
