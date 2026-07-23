#ifndef IETF_ROUTING_PROVIDER_H
#define IETF_ROUTING_PROVIDER_H

#include <confd.h>

void * routing_alloc_dp_data(void);

int routing_get_elem(
    struct confd_trans_ctx *tctx,
    confd_hkeypath_t *kp
);

int routing_get_next(
    struct confd_trans_ctx *tctx,
    confd_hkeypath_t *kp,
    long next
);

int routing_get_case(
    struct confd_trans_ctx *tctx,
    confd_hkeypath_t *kp,
    confd_value_t *choice
);

int routing_exists_optional(
    struct confd_trans_ctx *tctx,
    confd_hkeypath_t *kp
);

#endif // IETF_ROUTING_PROVIDER_H
