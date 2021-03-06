/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2011-2012 Los Alamos National Security, LLC. All rights
 *                         reserved.
 * Copyright (c) 2011      UT-Battelle, LLC. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ompi_config.h"

#include "btl_ugni.h"
#include "btl_ugni_frag.h"
#include "btl_ugni_smsg.h"

static int
mca_btl_ugni_setup_mpools (mca_btl_ugni_module_t *ugni_module);
static void
mca_btl_ugni_module_set_max_reg (mca_btl_ugni_module_t *ugni_module, int nlocal_procs);

int mca_btl_ugni_add_procs(struct mca_btl_base_module_t* btl,
                           size_t nprocs,
                           struct ompi_proc_t **procs,
                           struct mca_btl_base_endpoint_t **peers,
                           opal_bitmap_t *reachable) {
    mca_btl_ugni_module_t *ugni_module = (mca_btl_ugni_module_t *) btl;
    size_t ntotal_procs, nlocal_procs, i;
    bool first_time_init = (NULL == ugni_module->endpoints);
    int rc;

    if (NULL == ugni_module->endpoints) {
        (void) ompi_proc_world (&ntotal_procs);

        ugni_module->endpoints = calloc (ntotal_procs, sizeof (mca_btl_base_endpoint_t *));
        if (OPAL_UNLIKELY(NULL == ugni_module->endpoints)) {
            return OMPI_ERR_OUT_OF_RESOURCE;
        }
    }

    for (i = 0, nlocal_procs = 0 ; i < nprocs ; ++i) {
        struct ompi_proc_t *ompi_proc = procs[i];
        uint32_t rem_rank = ompi_proc->proc_name.vpid;

        if (OPAL_PROC_ON_LOCAL_NODE(ompi_proc->proc_flags)) {
            /* don't use uGNI to communicate with local procs */
            nlocal_procs++;
            continue;
        }

        /*  Create and Init endpoints */
        rc = mca_btl_ugni_init_ep (peers + i, (mca_btl_ugni_module_t *) btl, ompi_proc);
        if (OPAL_UNLIKELY(OMPI_SUCCESS != rc)) {
            BTL_ERROR(("btl/ugni error initializing endpoint"));
            return rc;
        }

        /* Set the reachable bit */
        rc = opal_bitmap_set_bit (reachable, i);

        /* Store a reference to this peer */
        ugni_module->endpoints[rem_rank] = peers[i];
    }

    if (first_time_init) {
        mca_btl_ugni_module_set_max_reg (ugni_module, nlocal_procs);

        rc = GNI_CqCreate (ugni_module->device->dev_handle, mca_btl_ugni_component.local_cq_size,
                           0, GNI_CQ_NOBLOCK, NULL, NULL, &ugni_module->rdma_local_cq);
        if (GNI_RC_SUCCESS != rc) {
            BTL_ERROR(("error creating local BTE/FMA CQ"));
            return ompi_common_rc_ugni_to_ompi (rc);
        }

        rc = GNI_CqCreate (ugni_module->device->dev_handle, mca_btl_ugni_component.local_cq_size,
                           0, GNI_CQ_NOBLOCK, NULL, NULL, &ugni_module->smsg_local_cq);
        if (GNI_RC_SUCCESS != rc) {
            BTL_ERROR(("error creating local SMSG CQ"));
            return ompi_common_rc_ugni_to_ompi (rc);
        }

        rc = GNI_CqCreate (ugni_module->device->dev_handle, mca_btl_ugni_component.remote_cq_size,
                           0, GNI_CQ_NOBLOCK, NULL, NULL, &ugni_module->smsg_remote_cq);
        if (GNI_RC_SUCCESS != rc) {
            BTL_ERROR(("error creating remote SMSG CQ"));
            return ompi_common_rc_ugni_to_ompi (rc);
        }

        rc = mca_btl_ugni_setup_mpools (ugni_module);
        if (OPAL_UNLIKELY(OMPI_SUCCESS != rc)) {
            BTL_ERROR(("btl/ugni error setting up mpools/free lists"));
            return rc;
        }

        rc = mca_btl_ugni_smsg_init (ugni_module);
        if (OPAL_UNLIKELY(OMPI_SUCCESS != rc)) {
            BTL_ERROR(("btl/ugni error initializing SMSG"));
            return rc;
        }
    }

    ugni_module->endpoint_count += nprocs;

    return OMPI_SUCCESS;
}

int mca_btl_ugni_del_procs (struct mca_btl_base_module_t *btl,
                            size_t nprocs, struct ompi_proc_t **procs,
                            struct mca_btl_base_endpoint_t **peers) {
    mca_btl_ugni_module_t *ugni_module = (mca_btl_ugni_module_t *) btl;
    size_t i;

    /* NTH: this function destroys the endpoint list which will cause bad
       things to happen if the caller only wants to delete a few procs. */

    for (i = 0 ; i < nprocs ; ++i) {
        struct ompi_proc_t *ompi_proc = procs[i];
        uint32_t rem_rank = ompi_proc->proc_name.vpid;

        if (ugni_module->endpoints[rem_rank]) {
            mca_btl_ugni_release_ep (ugni_module->endpoints[rem_rank]);
        }

        ugni_module->endpoints[rem_rank] = NULL;
    }

    ugni_module->endpoint_count -= nprocs;

    if (0 == ugni_module->endpoint_count) {
        free (ugni_module->endpoints);
        ugni_module->endpoints = NULL;
    }

    return OMPI_SUCCESS;
}

static int ugni_reg_rdma_mem (void *reg_data, void *base, size_t size,
                              mca_mpool_base_registration_t *reg)
{
    mca_btl_ugni_module_t *ugni_module = (mca_btl_ugni_module_t *) reg_data;
    mca_btl_ugni_reg_t *ugni_reg = (mca_btl_ugni_reg_t *) reg;
    gni_return_t rc;

    if (ugni_module->reg_count >= ugni_module->reg_max) {
        return OMPI_ERR_OUT_OF_RESOURCE;
    }
 
    rc = GNI_MemRegister (ugni_module->device->dev_handle, (uint64_t) base,
                          size, NULL, GNI_MEM_READWRITE | GNI_MEM_RELAXED_PI_ORDERING,
                          -1, &(ugni_reg->memory_hdl));
    if (OPAL_UNLIKELY(GNI_RC_SUCCESS != rc)) {
        return OMPI_ERR_OUT_OF_RESOURCE;
    }

    ugni_module->reg_count++;  

    return OMPI_SUCCESS;
}


static int ugni_reg_smsg_mem (void *reg_data, void *base, size_t size,
                              mca_mpool_base_registration_t *reg)
{
    mca_btl_ugni_module_t *ugni_module = (mca_btl_ugni_module_t *) reg_data;
    mca_btl_ugni_reg_t *ugni_reg = (mca_btl_ugni_reg_t *) reg;
    gni_return_t rc;

    rc = GNI_MemRegister (ugni_module->device->dev_handle, (uint64_t) base,
                          size, ugni_module->smsg_remote_cq, GNI_MEM_READWRITE, -1,
                          &(ugni_reg->memory_hdl));
    return ompi_common_rc_ugni_to_ompi (rc);
}

static int
ugni_dereg_mem (void *reg_data, mca_mpool_base_registration_t *reg)
{
    mca_btl_ugni_module_t *ugni_module = (mca_btl_ugni_module_t *) reg_data;
    mca_btl_ugni_reg_t *ugni_reg = (mca_btl_ugni_reg_t *)reg;
    gni_return_t rc;

    rc = GNI_MemDeregister (ugni_module->device->dev_handle, &ugni_reg->memory_hdl);
    if (GNI_RC_SUCCESS != rc) {
        return OMPI_ERROR;
    }

    ugni_module->reg_count--;

    return OMPI_SUCCESS;
}

static int
mca_btl_ugni_setup_mpools (mca_btl_ugni_module_t *ugni_module)
{
    struct mca_mpool_base_resources_t mpool_resources;
    int mbox_increment, rc;
    size_t nprocs;

    rc = opal_pointer_array_init (&ugni_module->pending_smsg_frags_bb, 0,
                                  1 << 30, 32768);
    if (OPAL_SUCCESS != rc) {
        return rc;
    }

    (void) ompi_proc_world (&nprocs);

    rc = ompi_free_list_init_ex_new (&ugni_module->smsg_frags,
                                     sizeof (mca_btl_ugni_smsg_frag_t),
                                     opal_cache_line_size, OBJ_CLASS(mca_btl_ugni_smsg_frag_t),
                                     mca_btl_ugni_component.ugni_smsg_limit,
                                     opal_cache_line_size,
                                     mca_btl_ugni_component.ugni_free_list_num,
                                     mca_btl_ugni_component.ugni_free_list_max,
                                     mca_btl_ugni_component.ugni_free_list_inc,
                                     NULL, (ompi_free_list_item_init_fn_t) mca_btl_ugni_frag_init,
                                     (void *) ugni_module);
    if (OPAL_UNLIKELY(OMPI_SUCCESS != rc)) {
        BTL_ERROR(("error creating smsg fragment free list"));
        return rc;
    }

    rc = ompi_free_list_init_ex_new (&ugni_module->rdma_frags,
                                     sizeof (mca_btl_ugni_rdma_frag_t), 64,
                                     OBJ_CLASS(mca_btl_ugni_rdma_frag_t),
                                     0, opal_cache_line_size,
                                     mca_btl_ugni_component.ugni_free_list_num,
                                     mca_btl_ugni_component.ugni_free_list_max,
                                     mca_btl_ugni_component.ugni_free_list_inc,
                                     NULL, (ompi_free_list_item_init_fn_t) mca_btl_ugni_frag_init,
                                     (void *) ugni_module);
    if (OPAL_UNLIKELY(OMPI_SUCCESS != rc)) {
        return rc;
    }

    rc = ompi_free_list_init_ex_new (&ugni_module->rdma_int_frags,
                                     sizeof (mca_btl_ugni_rdma_frag_t), 8,
                                     OBJ_CLASS(mca_btl_ugni_rdma_frag_t),
                                     0, opal_cache_line_size, 0, -1, 64,
                                     NULL, (ompi_free_list_item_init_fn_t) mca_btl_ugni_frag_init,
                                     (void *) ugni_module);
    if (OPAL_UNLIKELY(OMPI_SUCCESS != rc)) {
        return rc;
    }

    mpool_resources.pool_name      = "ugni";
    mpool_resources.reg_data       = (void *) ugni_module;
    mpool_resources.sizeof_reg     = sizeof (mca_btl_ugni_reg_t);
    mpool_resources.register_mem   = ugni_reg_rdma_mem;
    mpool_resources.deregister_mem = ugni_dereg_mem;
    ugni_module->super.btl_mpool =
        mca_mpool_base_module_create("grdma", ugni_module->device,
                                     &mpool_resources);
    if (NULL == ugni_module->super.btl_mpool) {
        BTL_ERROR(("error creating rdma mpool"));
        return OMPI_ERROR;
    }

    mpool_resources.register_mem   = ugni_reg_smsg_mem;

    ugni_module->smsg_mpool =
        mca_mpool_base_module_create("grdma", ugni_module->device,
                                     &mpool_resources);
    if (NULL == ugni_module->smsg_mpool) {
        BTL_ERROR(("error creating smsg mpool"));
        return OMPI_ERROR;
    }

    rc = ompi_free_list_init_ex_new (&ugni_module->eager_frags_send,
                                     sizeof (mca_btl_ugni_eager_frag_t), 8,
                                     OBJ_CLASS(mca_btl_ugni_eager_frag_t),
                                     ugni_module->super.btl_eager_limit, 64,
                                     mca_btl_ugni_component.ugni_eager_num,
                                     mca_btl_ugni_component.ugni_eager_max,
                                     mca_btl_ugni_component.ugni_eager_inc,
                                     ugni_module->super.btl_mpool,
                                     (ompi_free_list_item_init_fn_t) mca_btl_ugni_frag_init,
                                     (void *) ugni_module);
    if (OPAL_UNLIKELY(OMPI_SUCCESS != rc)) {
        BTL_ERROR(("error creating eager send fragment free list"));
        return rc;
    }

    rc = ompi_free_list_init_ex_new (&ugni_module->eager_frags_recv,
                                     sizeof (mca_btl_ugni_eager_frag_t), 8,
                                     OBJ_CLASS(mca_btl_ugni_eager_frag_t),
                                     ugni_module->super.btl_eager_limit, 64,
                                     mca_btl_ugni_component.ugni_eager_num,
                                     mca_btl_ugni_component.ugni_eager_max,
                                     mca_btl_ugni_component.ugni_eager_inc,
                                     ugni_module->super.btl_mpool,
                                     (ompi_free_list_item_init_fn_t) mca_btl_ugni_frag_init,
                                     (void *) ugni_module);
    if (OPAL_UNLIKELY(OMPI_SUCCESS != rc)) {
        BTL_ERROR(("error creating eager receive fragment free list"));
        return rc;
    }

    OBJ_CONSTRUCT(&ugni_module->smsg_mboxes, ompi_free_list_t);

    mbox_increment = nprocs;

    /* limit mailbox allocations to either 12.5% of available registrations 
       or 2MiB per allocation */
    if (nprocs * mca_btl_ugni_component.smsg_mbox_size > 2097152) {
        mbox_increment = (int) (2097152.0 / (float)mca_btl_ugni_component.smsg_mbox_size);
    }

    if (nprocs/mbox_increment > ugni_module->reg_max / 8) {
        mbox_increment = nprocs / (ugni_module->reg_max >> 3);
    }

    rc = ompi_free_list_init_new (&ugni_module->smsg_mboxes,
                                  sizeof (mca_btl_ugni_smsg_mbox_t), 8,
                                  OBJ_CLASS(mca_btl_ugni_smsg_mbox_t),
                                  mca_btl_ugni_component.smsg_mbox_size, 128,
                                  32, nprocs, mbox_increment,
                                  ugni_module->smsg_mpool);
    if (OPAL_UNLIKELY(OMPI_SUCCESS != rc)) {
        BTL_ERROR(("error creating smsg mailbox free list"));
        return rc;
    }

    return OMPI_SUCCESS;
}

static void
mca_btl_ugni_module_set_max_reg (mca_btl_ugni_module_t *ugni_module, int nlocal_procs)
{
    if (0 == mca_btl_ugni_component.max_mem_reg) {
#if defined(HAVE_GNI_GETJOBRESINFO)
        gni_job_res_desc_t res_des;
        gni_return_t grc;

        grc = GNI_GetJobResInfo (ugni_module->device->dev_id, ompi_common_ugni_module.ptag,
                                 GNI_JOB_RES_MDD, &res_des);
        if (GNI_RC_SUCCESS == grc) {
            ugni_module->reg_max = (res_des.limit - res_des.used) / nlocal_procs;
        }
#else
        /* no way to determine the maximum registration count */
        ugni_module->reg_max = 1200 / nlocal_procs;
#endif
    } else if (-1 == mca_btl_ugni_component.max_mem_reg) {
        ugni_module->reg_max = INT_MAX;
    } else {
        ugni_module->reg_max = mca_btl_ugni_component.max_mem_reg;
    }

    ugni_module->reg_count = 0;
}

