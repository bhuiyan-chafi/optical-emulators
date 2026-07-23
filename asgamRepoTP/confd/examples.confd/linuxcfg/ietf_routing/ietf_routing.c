/*
 * Copyright 2017 Tail-F Systems AB
 * Tail-F customers are permitted to redistribute in binary form, with
 * or without modification, for use in customer products.
 */

#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <confd.h>
#include <confd_maapi.h>
#include <confd_cdb.h>

#include "ietf-routing.h"
//#include "iana-if-type.h"

#include "linuxcfg_api.h"
#include "linuxcfg_util.h"
#include "ietf_routing_system.h"
#include "ietf_routing_subscriber.h"
#include "ietf_routing_provider.h"

static void setup(struct confd_daemon_ctx *dctx, int rsock);

static struct confd_data_cbs transpoints[] = {
    {   .callpoint = rt__callpointid_routing_state_dp,
        .get_elem = routing_get_elem,
        .get_next = routing_get_next,
        .get_case = routing_get_case,
        .exists_optional = routing_exists_optional
    },
    { .callpoint = "" }
};

static int init_data(struct confd_trans_ctx *tctx)
{
    tctx->t_opaque = routing_alloc_dp_data();
    return CONFD_OK;
}

static void finish_data(struct confd_trans_ctx *tctx)
{
    // TODO - clean internals first if something dynamic happening inside...
    XFREE(tctx->t_opaque);
}

const struct component ietf_routing = {
    NULL,         /* init */
    NULL,         /* setup0 */
    setup,        /* setup */
    NULL,         /* valpoints */
    transpoints,  /* transpoints */
    NULL,         /* actionpoints */
    NULL,         /* init_validation */
    NULL,         /* stop_validation */
    init_data,    /* init_data */
    finish_data,  /* finish_data */
};

/* Setup this component on startup */
static void setup(struct confd_daemon_ctx *dctx, int rsock)
{
    routing_subs_setup(dctx, rsock);
}