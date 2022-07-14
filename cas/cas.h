/* -*- C -*- */
/*
 * Copyright (c) 2016-2020 Seagate Technology LLC and/or its Affiliates
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


#pragma once

#ifndef __MOTR_CAS_CAS_H__
#define __MOTR_CAS_CAS_H__

#include "xcode/xcode_attr.h"
#include "fid/fid.h"
#include "fid/fid_xc.h"
#include "lib/types.h"
#include "lib/buf.h"
#include "lib/buf_xc.h"
#include "lib/cookie.h"
#include "lib/cookie_xc.h"
#include "rpc/at.h"             /* m0_rpc_at_buf */
#include "rpc/at_xc.h"          /* m0_rpc_at_buf_xc */
#include "fop/wire.h"           /* m0_fop_mod_rep */
#include "fop/wire_xc.h"
#include "fop/fom_interpose.h"  /* m0_fom_thralldom */
#include "dix/layout.h"
#include "dix/layout_xc.h"
#include "dtm0/tx_desc.h"	/* tx_desc */
#include "dtm0/tx_desc_xc.h"	/* xc for tx_desc */

/**
 * @page cas-fspec The catalogue service (CAS)
 *
 * - @ref cas-fspec-ds
 * - @ref cas-fspec-sub
 *
 * @section cas-fspec-ds Data Structures
 * The interface of the service for a user is a format of request/reply FOPs.
 * Request FOPs:
 * - @ref m0_cas_op
 * Reply FOPs:
 * - @ref m0_cas_rep
 *
 * Also CAS service exports predefined FID for meta-index (m0_cas_meta_fid)
 * and FID type for other ordinary indexes (m0_cas_index_fid_type).
 *
 * @section cas-fspec-sub Subroutines
 * CAS service is a request handler service and provides several callbacks to
 * the request handler, see @ref m0_cas_service_type::rst_ops().
 *
 * Also CAS service registers several FOP types during initialisation:
 * - @ref cas_get_fopt
 * - @ref cas_put_fopt
 * - @ref cas_del_fopt
 * - @ref cas_cur_fopt
 * - @ref cas_rep_fopt
 *
 * @see @ref cas_dfspec "Detailed Functional Specification"
 */

/**
 * @defgroup cas_dfspec CAS service
 * @brief Detailed functional specification.
 *
 * @{
 */

/* Import */
struct m0_sm_conf;
struct m0_fom_type_ops;
struct m0_reqh_service_type;

/**
 * CAS cookie.
 *
 * This is a hint to a location of a record in an index or
 * a location of an index in the meta-index.
 *
 * @note: not used in current implementation until profiling
 * proves that it's necessary.
 */
struct m0_cas_hint {
	/** Cookie of the leaf node. */
	struct m0_cookie ch_cookie;
	/** Position of the record within the node. */
	uint64_t         ch_index;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
 * Identifier of a CAS index.
 */
struct m0_cas_id {
	/**
	 * Catalogue fid. This is always present and can be of two types:
	 * - general (m0_cas_index_fid_type);
	 * - component catalogue (m0_cctg_fid_type).
	 */
	struct m0_fid          ci_fid;
	/** Cookie, when present is used instead of fid. */
	struct m0_cas_hint     ci_hint;
	/**
	 * Distributed index layout. Valid if type of ci_fid is component
	 * catalogue.
	 */
	struct m0_dix_layout   ci_layout;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
 * Key/value pair of RPC AT buffers.
 */
struct m0_cas_kv {
	struct m0_rpc_at_buf ck_key;
	struct m0_rpc_at_buf ck_val;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
 * Vector of key/value RPC AT buffers.
 */
struct m0_cas_kv_vec {
	uint64_t          cv_nr;
	struct m0_cas_kv *cv_rec;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc);

/**
 * CAS record version and tombstone encoded in on-disk/on-wire format.
 * Format:
 *     MSB                             LSB
 *     +----------------+----------------+
 *     | 1 bit          | 63 bits        |
 *     |<- tombstone -> | <- timestamp ->|
 *     +----------------+----------------+
 *
 * A version comprises a physical timestamp and a tombstone flag.
 * The timestamp is set on the client side whenever the corresponding
 * record was supposed to be modified by PUT or DEL request.
 * The tombstone flag is set when the corresponding record has been
 * "logicaly" removed from the storage (although, it still exists
 * there as a "dead" record).
 *
 * See ::COF_VERSIONED, ::M0_CRV_VER_NONE for details.
 */
struct m0_crv {
	uint64_t crv_encoded;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc|be);

/**
 * CAS index record.
 */
struct m0_cas_rec {
	/**
	 * Record key.
	 *
	 * Should be filled for GET, DEL, PUT, CUR requests.
	 * It's empty in replies for GET, DEL, PUT requests and non-empty for
	 * CUR reply.
	 */
	struct m0_rpc_at_buf cr_key;

	/**
	 * Record value.
	 *
	 * Should be empty for GET, DEL, CUR requests.
	 * For PUT request it should be empty if record is inserted in
	 * meta-index and filled otherwise. If operation is successful then
	 * replies for non-meta GET, CUR have this field set.
	 */
	struct m0_rpc_at_buf cr_val;

	/**
	 * Vector of AT buffers to request key/value buffers in CUR request.
	 *
	 * CUR request only specifies the starting key in cr_key. If several
	 * keys/values from reply should be transferred via bulk, then request
	 * should have descriptors for them => vector is required.
	 *
	 * For now, vector size is either 0 (inline transmission is requested
	 * for all records) or equals to cr_rc (individual AT transmission
	 * method for every key/value).
	 */
	struct m0_cas_kv_vec cr_kv_bufs;

	/**
	 * Optional hint to speed up access.
	 */
	struct m0_cas_hint   cr_hint;

	/**
	 * In GET, DEL, PUT replies, return code for the operation on this
	 * record. In CUR request, the number of consecutive records to return.
	 *
	 * In CUR reply, the consecutive number of the record for the current
	 * key. For example, if there are two records in CUR request requesting
	 * 2 and 3 next records respectively, then cr_rc values in reply records
	 * will be: 1,2,1,2,3.
	 *
	 * Also, CAS service stops iteration for the current CAS-CUR key on
	 * error and proceed with the next key in the input vector. Suppose
	 * there is a CUR request, requesting 10 records starting from K0 and 10
	 * records starting with K1. Suppose iteration failed on the fifth
	 * record after K0 and all K1 records were processed successfully. In
	 * this case, the reply would contain at least 15 records. First 5 will
	 * be for K0, the last of which would contain errno, the next 10 records
	 * will be for K1. Current implementation will add final 5 zeroed
	 * records.
	 */
	uint64_t             cr_rc;

	/**
	 * Optional version of this record.
	 * The version is returned as a reply to GET and NEXT requests
	 * when COF_VERSIONED is specified in the request.
	 */
	struct m0_crv cr_ver;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
 * Vector of records.
 */
struct m0_cas_recv {
	uint64_t           cr_nr;
	struct m0_cas_rec *cr_rec;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc);

/**
 * CAS operation flags.
 */
enum m0_cas_op_flags {
	/**
	 * For NEXT operation, allows iteration to start with the smallest key
	 * following the start key if the start key is not found.
	 */
	COF_SLANT     = 1 << 0,
	/**
	 * For PUT operation, instructs it to be a no-op if the record with the
	 * given key already exists.
	 */
	COF_CREATE    = 1 << 1,
	/**
	 * For PUT operation, instructs it to silently overwrite existing record
	 * with the same key, if any.
	 */
	COF_OVERWRITE = 1 << 2,
	/**
	 * For PUT operation, instructs it to create catalogue on record PUT to
	 * non-existent catalogue.
	 * For DEL operation, instructs it to be a no-op if catalogue to be
	 * deleted does not exist.
	 */
	COF_CROW      = 1 << 3,
	/**
	 * Repair is in progress, reserve space.
	 */
	COF_RESERVE   = 1 << 4,
	/**
	 * For DEL operation, instructs CAS service to take "delete" lock for
	 * the record being deleted. It is used by DIX client to handle delete
	 * in degraded mode.
	 */
	COF_DEL_LOCK  = 1 << 5,
	/**
	 * For NEXT operation, instructs it to skip record with the given start
	 * key.
	 */
	COF_EXCLUDE_START_KEY = 1 << 6,
	/**
	 * For PUT/DEL operation, instructs it to delay reply from CAS service
	 * until BE transaction is persisted.
	 */
	COF_SYNC_WAIT = 1 << 7,
	/**
	 * For IDX CREATE, IDX DELETE and IDX LOOKUP operation, instructs it to
	 * skip layout update operation.
	 */
	COF_SKIP_LAYOUT = 1 << 8,

	/**
	 * Enables version-aware behavior for PUT, DEL, GET, and NEXT requests.
	 *
	 * Overview
	 * --------
	 *   Versions are taken from m0_cas_op::cg_txd. When this flag is set,
	 * the following change happens in the logic of the mentioned request
	 * types:
	 *     - PUT does not overwrite "newest" (version-wise) records.
	 *       Requirements:
	 *         x COF_OVERWRITE is set.
	 *         x Transaction descriptor has a valid DTX ID.
	 *     - DEL puts a tombstone instead of an actual removal. In this
	 *       mode, DEL does not return -ENOENT in the same way as
	 *       PUT-with-COF_OVERWRITE does not return -EEXIST.
	 *       Requirements:
	 *         x Transaction descriptor has a valid DTX ID.
	 *     - GET returns only "alive" entries (without tombstones).
	 *     - NEXT also skips entries with tombstones.
	 *
	 * How to enable/disable
	 * ---------------------
	 *   This operation mode is enabled only if all the requirements are
	 * satisfied (the list above). For example, if m0_cas_op::cg_txd does
	 * not have a valid transaction ID then the request is getting executed
	 * in the usual manner, without version-aware behavior. Additional
	 * restictions may be imposed later on, but at this moment it is as
	 * flexible as possible.
	 *
	 * Relation with DTM0
	 * ------------------
	 *   The flag should always be enabled if DTM0 is enabled because it
	 * enables CRDT-ness for catalogues. If DTM0 is not enabled then this
	 * flag may or may not be enabled (for example, the version-related UTs
	 * use the flag but they do not depend on enabled DTM0).
	 */
	COF_VERSIONED = 1 << 9,

	/**
	 * Makes NEXT return "dead" records (with tombstones) when
	 * version-aware behavior is specified (see ::COF_VERSIONED).
	 * By default, COF_VERSIONED does not return dead records for NEXT
	 * requests but when both flags are specified (VERSIONED | SHOW_DEAD)
	 * then it yields all records whether they are alive or not.
	 * It might be used by the client when it wants to analyse the records
	 * received from several catalogue services, so that it could properly
	 * merge the results of NEXT operations eliminating inconsistencies.
	 * In other words, it might be used in degraded mode.
	 */
	COF_SHOW_DEAD = 1 << 10,
	/**
	 * NO DTM is needed for this operation.
	 */
	COF_NO_DTM = 1 << 11,
};

enum m0_cas_opcode {
	CO_GET,
	CO_PUT,
	CO_DEL,
	CO_CUR,
	CO_REP,
	CO_GC,
	CO_MIN,
	CO_TRUNC,
	CO_DROP,
	CO_MEM_PLACE,
	CO_MEM_FREE,
	CO_NR
} M0_XCA_ENUM;

enum m0_cas_type {
	CT_META,
	CT_BTREE,
	CT_DEAD_INDEX,
	CT_MEM
} M0_XCA_ENUM;


/**
 * CAS-GET, CAS-PUT, CAS-DEL and CAS-CUR fops.
 *
 * Performs an operation on a number of records.
 *
 * @see m0_fop_cas_rep
 */
struct m0_cas_op {
	/** Index to make operation in. */
	struct m0_cas_id       cg_id;

	/**
	 * Array of input records.
	 *
	 * For CAS-GET, CAS-DEL and CAS-CUR this describes input keys.
	 * For CAS-PUT this describes input keys and values.
	 *
	 * Array should be non-empty.
	 */
	struct m0_cas_recv     cg_rec;

	/**
	 * CAS operation flags.
	 *
	 * It's a bitmask of flags from m0_cas_op_flags enumeration.
	 */
	uint32_t               cg_flags;

	/**
	 * Transaction descriptor associated with CAS operation.
	 */
	struct m0_dtm0_tx_desc cg_txd;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
 * The structure to be passed in DTM0 log as a payload
 */
struct m0_cas_dtm0_log_payload  {
   /**  CAS op */
	struct m0_cas_op  cdg_cas_op;
	/** CAS operation code */
	uint32_t          cdg_cas_opcode;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
 * CAS-GET, CAS-PUT, CAS-DEL and CAS-CUR reply fops.
 *
 * @note CAS-CUR reply may contain less records than requested.
 *
 * @see m0_fop_cas_op
 */
struct m0_cas_rep {
	/**
	 * Status code of operation.
	 * If code != 0, then no input record from request is processed
	 * successfully and contents of m0_cas_rep::cgr_rep field is undefined.
	 *
	 * Zero status code means that at least one input record is processed
	 * successfully. Operation status code of individual input record can
	 * be obtained through m0_cas_rep::cgr_rep::cg_rec::cr_rc.
	 */
	int32_t                 cgr_rc;

	/**
	 * Array of operation results for each input record.
	 *
	 * For CAS-GET this describes output values.
	 *
	 * For CAS-PUT and, CAS-DEL this describes return codes.
	 *
	 * For CAS-CUR this describes next records.
	 */
	struct m0_cas_recv      cgr_rep;

	/** Returned values for an UPDATE operation, such as CAS-PUT. */
	struct m0_fop_mod_rep   cgr_mod_rep;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

M0_EXTERN struct m0_reqh_service_type m0_cas_service_type;
M0_EXTERN const struct m0_fid         m0_cas_meta_fid;
M0_EXTERN const struct m0_fid         m0_cas_dead_index_fid;
M0_EXTERN const struct m0_fid         m0_cas_ctidx_fid;
M0_EXTERN const struct m0_fid_type    m0_cas_index_fid_type;
M0_EXTERN const struct m0_fid_type    m0_dix_fid_type;
M0_EXTERN const struct m0_fid_type    m0_cctg_fid_type;

M0_EXTERN struct m0_fop_type cas_get_fopt;
M0_EXTERN struct m0_fop_type cas_put_fopt;
M0_EXTERN struct m0_fop_type cas_del_fopt;
M0_EXTERN struct m0_fop_type cas_cur_fopt;
M0_EXTERN struct m0_fop_type cas_rep_fopt;
M0_EXTERN struct m0_fop_type cas_gc_fopt;
extern    struct m0_fop_type m0_fop_fsync_cas_fopt;

/**
 * CAS server side is able to compile in user-space only. Use stubs in kernel
 * mode for service initialisation and deinitalisation. Also use NULLs for
 * service-specific fields in CAS FOP types.
 */
#ifndef __KERNEL__
M0_INTERNAL void m0_cas_svc_init(void);
M0_INTERNAL void m0_cas_svc_fini(void);
M0_INTERNAL void m0_cas_svc_fop_args(struct m0_sm_conf            **sm_conf,
				     const struct m0_fom_type_ops **fom_ops,
				     struct m0_reqh_service_type  **svctype);
M0_INTERNAL void m0_cas__ut_svc_be_set(struct m0_reqh_service *svc,
				       struct m0_be_domain *dom);
M0_INTERNAL struct m0_be_domain *
m0_cas__ut_svc_be_get(struct m0_reqh_service *svc);
M0_INTERNAL int m0_cas_fom_spawn(
	struct m0_fom           *lead,
	struct m0_fom_thralldom *thrall,
	struct m0_fop           *cas_fop,
	void                   (*on_fom_complete)(struct m0_fom_thralldom *,
						  struct m0_fom           *));
M0_INTERNAL uint32_t m0_cas_svc_device_id_get(
	const struct m0_reqh_service_type *stype,
	const struct m0_reqh              *reqh);
#else
#define m0_cas_svc_init()
#define m0_cas_svc_fini()
#define m0_cas_svc_fop_args(sm_conf, fom_ops, svctype) \
do {                                                   \
	*(sm_conf) = NULL;                             \
	*(fom_ops) = NULL;                             \
	*(svctype) = NULL;                             \
} while (0);
#endif /* __KERNEL__ */

M0_INTERNAL void m0_cas_id_fini(struct m0_cas_id *cid);
M0_INTERNAL bool m0_cas_id_invariant(const struct m0_cas_id *cid);

M0_INTERNAL bool cas_in_ut(void);

enum {
	/* Tombstone flag: marks a dead kv pair. We use the MSB here. */
	M0_CRV_TBS = 1L << (sizeof(uint64_t) * CHAR_BIT - 1),
	/*
	 * A special value for the empty version.
	 * A record with the empty version is always overwritten by any
	 * PUT or DEL operation that has a valid non-empty version.
	 * A PUT or DEL operation with the empty version always ignores
	 * the version-aware behavior: records are actually removed by DEL,
	 * and overwritten by PUT, no matter what was stored in the catalogue.
	 */
	M0_CRV_VER_NONE = 0,
	/* The maximum possible value of a version. */
	M0_CRV_VER_MAX = (UINT64_MAX & ~M0_CRV_TBS) - 1,
	/* The minimum possible value of a version. */
	M0_CRV_VER_MIN = M0_CRV_VER_NONE + 1,
};

/*
 * 100:a == alive record with version 100
 * 123:d == dead record with version 123
 */
#define CRV_F "%" PRIu64 ":%c"
#define CRV_P(__crv) m0_crv_ts(__crv).dts_phys, m0_crv_tbs(__crv) ? 'd' : 'a'

#define M0_CRV_INIT_NONE ((struct m0_crv) { .crv_encoded = M0_CRV_VER_NONE })

M0_INTERNAL void m0_crv_init(struct m0_crv           *crv,
			     const struct m0_dtm0_ts *ts,
			     bool                     tbs);

M0_INTERNAL bool m0_crv_is_none(const struct m0_crv *crv);
M0_INTERNAL int m0_crv_cmp(const struct m0_crv *left,
			   const struct m0_crv *right);

M0_INTERNAL bool m0_crv_tbs(const struct m0_crv *crv);
M0_INTERNAL void m0_crv_tbs_set(struct m0_crv *crv, bool tbs);

M0_INTERNAL struct m0_dtm0_ts m0_crv_ts(const struct m0_crv *crv);
M0_INTERNAL void m0_crv_ts_set(struct m0_crv           *crv,
			       const struct m0_dtm0_ts *ts);

/** @} end of cas_dfspec */
#endif /* __MOTR_CAS_CAS_H__ */

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
