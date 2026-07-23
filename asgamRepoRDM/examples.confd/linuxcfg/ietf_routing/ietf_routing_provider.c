#include <string.h>
#include <stdlib.h>

#include "ietf-routing.h"
#include "ietf-ipv4-unicast-routing.h"
#include "ietf-ipv6-unicast-routing.h"

#include "linuxcfg_api.h"
#include "linuxcfg_util.h"

#include "ietf_routing_system.h"

#include "ietf_routing_provider.h"
#include "ietf_routing_utils.h"
#include "if.h" //for list of interfaces

#define RIB_NAME_IPv4 "system-IPv4"
#define RIB_NAME_IPv6 "system-IPv6"

// transaction opaque data
// due to how system API is implemented, there can be only routes of specific
// family address present/mapped from the "system" sub-lib...
struct system_route_data {
    enum route_afi afi;
    const struct route_state * rs;
    // TODO - add some timestamp + refresh mechanism for huge/slow tables?
};

void * routing_alloc_dp_data(void)
{
    TRACE_ENTRY("");

    static size_t data_length = sizeof(struct system_route_data);

    void * p = malloc(data_length);

    if (NULL != p) {
        memset(p, 0x00, data_length);
    }

    TRACE_EXIT("data_length=%d", data_length);
    return p;
}

int is_next_hop_list_request(confd_hkeypath_t * kp) {
    return (NULL != kp && 12 == kp->len);
}

// retrieve N-th element form the next-hop-list linked list,
// starting with 0 as a first element index;
// return NULL if the list does not have that many elements
static void * get_nexthop_list_item(
    struct route_state * state,
    int index
) {
    TRACE_ENTRY("index == %d", index);

    void * ptr = NULL;
    int is_v4 = (ipv4 == state->afi_prefix.afi);
    if (is_v4) {
        ptr = state->nexthop.ipv4.list;
    } else {
        ptr = state->nexthop.ipv6.list;
    }

    // skip "cnt" elements
    int i;
    for (i = 0; i < index; i++) {
        if (NULL != ptr) {
            if (is_v4) {
                ptr = ((struct ipv4_nexthop_list *)ptr)->next;
            } else {
                ptr = ((struct ipv6_nexthop_list *)ptr)->next;
            }
        } else {
            break;
        }
    }

    // and return next one (or NULL, if not present)
    TRACE_EXIT("(%p)", ptr);
    return ptr;
}

// ==== get_elem() ============================================================

// set the output value to an identity reference of "address-family" type;
// see appropriate v4ur/v6ur yangs for details...
static void fill_identity_by_rib_name(
    confd_value_t * rib_name,
    confd_value_t * output
) {
    char * ptr = CONFD_GET_CBUFPTR(rib_name);

    if (0 == strcmp(ptr, RIB_NAME_IPv4)) {
        static const struct confd_identityref idr_ipv4 = {
            .id = v4ur_ipv4_unicast,
            .ns = v4ur__ns
        };
        CONFD_SET_IDENTITYREF(output, idr_ipv4);
    } else if (0 == strcmp(ptr, RIB_NAME_IPv6)) {
        static const struct confd_identityref idr_ipv6 = {
            .id = v6ur_ipv6_unicast,
            .ns = v6ur__ns
        };
        CONFD_SET_IDENTITYREF(output, idr_ipv6);
    } else {
        CONFD_SET_NOEXISTS(output);
    }
}

// set the output value to an identity reference of "routing-protocol" type;
// see ietf-routing.yang for details...
static void fill_protocol_identity(
    enum routing_protocol proto,
    confd_value_t * output
) {
    struct confd_identityref prot_ref;
    prot_ref.ns = rt__ns;

    switch (proto) {
        case prot_direct:
            prot_ref.id = rt_direct;
            break;

        case prot_static:
            prot_ref.id = rt_static;
            break;

        case prot_other:
        default:
            warn("unimplemented protocol enum! (%d)", proto);
    }

    CONFD_SET_IDENTITYREF(output, prot_ref);
}

// create ConfD value from the system route key stuct
static void fill_ip_prefix_by_rkey(
    struct route_key * key,
    confd_value_t * output
) {
    confd_value_t v;
    CONFD_SET_NOEXISTS(&v);

    if (NULL == key
        || (ipv4 != key->afi && ipv6 != key->afi)
    ) {
        error("Invalid route key source data!");
        return;
    }

    if (ipv4 == key->afi) {
        struct confd_ipv4_prefix pref4;
        pref4.ip = key->addr.ipv4_addr;
        pref4.len = key->prefix_len;
        CONFD_SET_IPV4PREFIX(&v, pref4);
    } else {
        struct confd_ipv6_prefix pref6;
        pref6.ip6 = key->addr.ipv6_addr;
        pref6.len = key->prefix_len;
        CONFD_SET_IPV6PREFIX(&v, pref6);
    }

    confd_value_dup_to(&v, output);
}

// bind the interface name of the specific route state to the output ConfD var.
static void fill_interface_str(
    struct route_state * state,
    confd_value_t * output
) {
    char * ifptr = NULL;

    if (ipv4 == state->afi_prefix.afi) {
        ifptr = state->nexthop.ipv4.val.iface;
    } else if(ipv6 == state->afi_prefix.afi) {
        ifptr = state->nexthop.ipv6.val.iface;
    }
    if (NULL != ifptr) {
        int iflen = strnlen(ifptr, BUFSIZ);
        CONFD_SET_CBUF(output, ifptr, iflen);
    } else {
        CONFD_SET_NOEXISTS(output);
    }
}

// bind the interface name of the specific next-hop-list
// to the output ConfD variable
static void fill_list_interface_str(
    confd_hkeypath_t * kp,
    struct route_state * state,
    confd_value_t * output
) {
    TRACE_ENTRY("");
    CONFD_SET_NOEXISTS(output);

    if (NULL == kp || NULL == state || NULL == output) {
        warn("Invalid input variables for next-hop-list interface!");
        goto term;
    }

    if (list != state->nexthop_type) {
        LOG("cross nexthop-type request...");
        CONFD_SET_NOEXISTS(output);
        goto term;
    }

//   /routing-state/ribs/rib[name]/routes/route[index]/
//        next-hop/next-hop-list/next-hop[index]/outgoing-interface
    confd_value_t * vp = &kp->v[1][0];
    if (C_INT64 != vp->type) {
        error("Invalid keypath, key \"index \" type is %d!", vp->type);
        goto term;
    }

    // beware down-sizing! (long  vs int)
    int index = (int) CONFD_GET_INT64(vp);
    void * ptr = get_nexthop_list_item(state, index);

    char * ifptr;
    if (ipv4 == state->afi_prefix.afi) {
        ifptr = ((struct ipv4_nexthop *)ptr)->iface;
    } else {
        ifptr = ((struct ipv6_nexthop *)ptr)->iface;
    }

    if (NULL != ifptr) {
        LOG("got interface");
        int iflen = strnlen(ifptr, BUFSIZ);
        CONFD_SET_CBUF(output, ifptr, iflen);
    }

term:
    TRACE_EXIT("");
}

// handle the get_elem() callback for the elements specific to main routing yang
int switch_rt_leaves(
    struct route_state * state,
    confd_hkeypath_t * kp,
    confd_value_t * output
) {
    TRACE_ENTRY("");
    int ret_code = CONFD_OK;

    confd_value_t v;
    CONFD_SET_NOEXISTS(&v);

    int leaf_tag = CONFD_GET_XML(&kp->v[0][0]);
    if_entry_list_t* if_list = NULL;
    int n_ifaces = 0;
    confd_value_t *ifaces = NULL;

    switch (leaf_tag) {
        case rt_router_id:
            // TODO - set to something?
            CONFD_SET_NOEXISTS(&v);
            break;
        case rt_interface:
            if_list = get_if_entry_list();
            for (int ix = 0; ix < if_list->size; ix++) {
                if_entry_t * entry = GET_ENTRY(if_list, ix);
                LOG("Interface %i %s", ix, entry->ifName);
                ifaces = realloc(ifaces, (n_ifaces + 1)
                        * sizeof(confd_value_t));
                CONFD_SET_CBUF(&ifaces[n_ifaces], strdup(entry->ifName),
                        strlen(entry->ifName));
                n_ifaces++;
            }
            CONFD_SET_LIST(&v, ifaces, n_ifaces);
            break;
        case rt_name:
            confd_value_dup_to(&kp->v[kp->len-4][0], &v);
            break;

        case rt_default_rib:
            // TODO - does not make sense on default linux config
            CONFD_SET_NOEXISTS(&v);
            break;

        case rt_address_family:
            fill_identity_by_rib_name(&kp->v[kp->len-4][0], &v);
            break;

        case rt_route_preference:
            if (state->has_priority) {
                CONFD_SET_UINT32(&v, state->priority);
            } else {
                CONFD_SET_NOEXISTS(&v);
            }
            break;

        case rt_last_updated:
            // TODO - is not tracked on default linux
            CONFD_SET_NOEXISTS(&v);
            break;

        case rt_source_protocol:
            fill_protocol_identity(state->protocol, &v);
            break;

        case rt_outgoing_interface:
            if (is_next_hop_list_request(kp)) {
                fill_list_interface_str(kp, state, &v);
            } else {
//   /routing-state/ribs/rib[name]/routes/route[index]/
//        next-hop/outgoing-interface
                fill_interface_str(state, &v);
            }
            break;

        case rt_special_next_hop:
            if (special == state->nexthop_type) {
                CONFD_SET_ENUM_VALUE(&v,
                    spec_nexthop_system_to_hash(state->nexthop.special)
                );
            } else {
                CONFD_SET_NOEXISTS(&v);
            }
            break;

        default:
            warn("unimplemented yet: %d", leaf_tag);
            ret_code = CONFD_ERR;
            break;
    }

    if (CONFD_OK == ret_code) {
        confd_value_dup_to(&v, output);
    }

    TRACE_EXIT("(%d)", ret_code);
    return ret_code;
}

// TODO - workaround for "when" statements commented out from yang files!
// see "TODO RT-XXX" comments in the files "ietf-ipv[4/6]-unicast-routing.yang"
static int is_matching_kp_key(
    confd_hkeypath_t * kp,
    struct route_key * rkey
) {
    int res = 0;
    TRACE_ENTRY("");

    if (NULL == kp
        || NULL == rkey
        || kp->len < 4
    ) {
        goto term;
    }

    char * ptr = CONFD_GET_CBUFPTR(&kp->v[kp->len-4][0]);

    int leaf_ns = CONFD_GET_XML_NS(&kp->v[0][0]);

    if (0 == strcmp(ptr, RIB_NAME_IPv4)) {
        if (ipv4 == rkey->afi && v4ur__ns == leaf_ns) {
            res = 1;
        }
    } else if (0 == strcmp(ptr, RIB_NAME_IPv6)) {
        if (ipv6 == rkey->afi && v6ur__ns == leaf_ns) {
            res = 1;
        }
    }

term:
    TRACE_EXIT("(%d)", res);
    return res;
}

static void fill_list_nexthop_addr(
    confd_hkeypath_t * kp,
    struct route_state * state,
    confd_value_t * output
) {
    TRACE_ENTRY("");
    CONFD_SET_NOEXISTS(output);

    if (NULL == kp || NULL == state || NULL == output) {
        warn("Invalid input variables for next-hop-list address!");
        goto term;
    }

//   /routing-state/ribs/rib[name]/routes/route[index]/
//        next-hop/next-hop-list/next-hop[index]/outgoing-interface
    // beware down-sizing! (long  vs int)
    int index = (int) CONFD_GET_INT64(&kp->v[1][0]);
    void * ptr = get_nexthop_list_item(state, index);

    if (ipv4 == state->afi_prefix.afi ) {
        struct ipv4_nexthop * v4ptr = (struct ipv4_nexthop *)ptr;
        if (v4ptr->has_addr) {
            CONFD_SET_IPV4(output, v4ptr->addr);
        }
    } else {
        struct ipv6_nexthop * v6ptr = (struct ipv6_nexthop *)ptr;
        if (v6ptr->has_addr) {
            CONFD_SET_IPV6(output, v6ptr->addr);
        }
    }

term:
    TRACE_EXIT("");
}

// handle the get_elem() callback for the elements specific to IPv4 yang
int switch_ipv4_leaves(
    struct route_state * state,
    confd_hkeypath_t *kp,
    confd_value_t * output
) {
    TRACE_ENTRY("");
    int ret_code = CONFD_OK;

    confd_value_t v;
    CONFD_SET_NOEXISTS(&v);

    if (0 == is_matching_kp_key(kp, &state->afi_prefix)) {
        CONFD_SET_NOEXISTS(&v);
        goto term;
    }

    int leaf_tag = CONFD_GET_XML(&kp->v[0][0]);

    switch (leaf_tag) {

        case v4ur_destination_prefix:
            fill_ip_prefix_by_rkey(&state->afi_prefix, &v);
            break;

        case v4ur_next_hop_address:
            if (simple == state->nexthop_type
                && state->nexthop.ipv4.val.has_addr
            ) {
                CONFD_SET_IPV4(&v, state->nexthop.ipv4.val.addr);
            } else {
                CONFD_SET_NOEXISTS(&v);
            }
            break;

        case v4ur_address:
            if (list == state->nexthop_type) {
                fill_list_nexthop_addr(kp, state, &v);
            } else {
                CONFD_SET_NOEXISTS(&v);
            }
            break;

        default:
            warn("unimplemented yet: %d", leaf_tag);
            ret_code = CONFD_ERR;

    }

term:
    if (CONFD_OK == ret_code) {
        // TODO - memleak!
        confd_value_dup_to(&v, output);
    }

    TRACE_EXIT("(%d)", ret_code);
    return ret_code;
}

// handle the get_elem() callback for the elements specific to IPv6 yang
int switch_ipv6_leaves(
    struct route_state * state,
    confd_hkeypath_t *kp,
    confd_value_t * output
) {
    TRACE_ENTRY("");
    int ret_code = CONFD_OK;

    confd_value_t v;
    CONFD_SET_NOEXISTS(&v);

    if (0 == is_matching_kp_key(kp, &state->afi_prefix)) {
        CONFD_SET_NOEXISTS(&v);
        goto term;
    }

    int leaf_tag = CONFD_GET_XML(&kp->v[0][0]);

    switch (leaf_tag) {

        case v6ur_destination_prefix:
            fill_ip_prefix_by_rkey(&state->afi_prefix, &v);
            break;

        case v6ur_next_hop_address:
            if (simple == state->nexthop_type
                && state->nexthop.ipv6.val.has_addr
            ) {
                CONFD_SET_IPV6(&v, state->nexthop.ipv6.val.addr);
            } else {
                CONFD_SET_NOEXISTS(&v);
            }
            break;

        case v6ur_address:
            if (list == state->nexthop_type) {
                fill_list_nexthop_addr(kp, state, &v);
            } else {
                CONFD_SET_NOEXISTS(&v);
            }
            break;

        default:
            warn("unimplemented yet: %d", leaf_tag);
            ret_code = CONFD_ERR;

    }

term:
    if (CONFD_OK == ret_code) {
        confd_value_dup_to(&v, output);
    }

    TRACE_EXIT("(%d)", ret_code);
    return ret_code;
}

// main callback registered for data provider .get_elem
int routing_get_elem(
    struct confd_trans_ctx *tctx,
    confd_hkeypath_t *kp
) {
    int ret_code = CONFD_OK;
    TRACE_ENTRY("path == %s", KPSTR(kp));

    int kplen = kp->len;

    // TODO - add separate procedures, or just ignore non-rib elements?
    uint32_t branch_tag = CONFD_GET_XMLTAG(&kp->v[kplen-2][0]);
    switch (branch_tag) {
        case rt_control_plane_protocols:
            confd_data_reply_not_found(tctx);
            goto term;
            break;

        case rt_ribs:
        default:
            break;
    }

    struct route_state * rstate = NULL;
    if (kp->len > 7 && C_INT64 == kp->v[kp->len-7][0].type) {
        long elem_key = CONFD_GET_INT64(&kp->v[kp->len-7][0]);
        rstate = (struct route_state *)elem_key;
    }

    confd_value_t rv;
    CONFD_SET_NOEXISTS(&rv);

    int leaf_ns = CONFD_GET_XML_NS(&kp->v[0][0]);

    switch (leaf_ns) {
        case rt__ns:
            ret_code = switch_rt_leaves(rstate, kp, &rv);
            break;

        case v4ur__ns:
            ret_code = switch_ipv4_leaves(rstate, kp, &rv);
            break;

        case v6ur__ns:
            ret_code = switch_ipv6_leaves(rstate, kp, &rv);
            break;

        default:
            error("Unsupported namespace! (%d)", leaf_ns);
            ret_code = CONFD_ERR;
            break;
    }

    if (C_NOEXISTS != rv.type) {
        LOG("returning value == %s", VALSTRNN(&rv));
        confd_data_reply_value(tctx, &rv);
        confd_free_value(&rv);
    } else {
        confd_data_reply_not_found(tctx);
    }
term:
    TRACE_EXIT("(%d)", ret_code);
    return ret_code;
}

// ==== get_next() ============================================================

// service the get_next() request for the sub-tree:
//      /routing-state/control-plane-protocols/control-plane-protocol{}
static int reply_next_protocol(
    struct confd_trans_ctx *tctx,
    confd_hkeypath_t *kp,
    long next
) {
    confd_data_reply_next_key(tctx, NULL, -1, -1);
    return CONFD_OK;
}

// update the target system_route_data by re-reading it form the system API
static int refresh_route_data(
    enum route_afi afi,
    struct system_route_data * data
) {
    if (NULL == data) {
        error("Missing transaction opaque data!");
        return CONFD_ERR;
    }

// TODO - beware!! any refresh in the middle of show command can lead to
// problems due to memory pointers to allocated routes will be invalid between
// the .get_next() / .get_elem() calls!

    if (NULL == data->rs
        || afi != data->afi) {
        LOG("update the cached routes");
        if (state_ok != get_routes(afi, &data->rs)) {
            error("Couldn't retrieve routes from system!");
            data->rs = NULL;
            return CONFD_ERR;
        }
        data->afi = afi;
    }

    return CONFD_OK;
}

// check whether some/any route exists for a specific family address;
// beware that this refreshes the opaque data with system routes!
static int check_afi_exists(
    enum route_afi afi,
    struct system_route_data * data
) {
    TRACE_ENTRY("afi == %d", afi);
    int ret = 0;

    if (CONFD_OK != refresh_route_data(afi, data)) {
        warn("Failed to get system routes");
    } else {
        ret = (NULL != data->rs);
    }

    TRACE_EXIT("(%d)", ret);
    return ret;
}

static int process_next_hop(
    struct confd_trans_ctx * tctx,
    confd_hkeypath_t * kp,
    long next
) {
    TRACE_ENTRY("path == %s", KPSTR(kp));
    int ret_code = CONFD_OK;

    int kplen = kp->len;

    if ((kp->len < 8) || (C_INT64 != kp->v[kplen-7][0].type)) {
        ret_code = CONFD_ERR;
        goto term;
    }

    struct route_state * state = (struct route_state *)
                                    CONFD_GET_INT64(&kp->v[kplen-7][0]);

    if (list != state->nexthop_type) {
        confd_data_reply_next_key(tctx, NULL, -1, -1);
        goto term;
    }

    // how many next-hop elements have we returned already
    int steps_cnt = next - (-1);

    // get the "next" item from list
    void * ptr = get_nexthop_list_item(state, steps_cnt);

    // and return it, if present
    if (NULL == ptr) {
        confd_data_reply_next_key(tctx, NULL, -1, -1);
    } else {
        confd_value_t v;
        CONFD_SET_INT64(&v, steps_cnt);
        confd_data_reply_next_key(tctx, &v, 1, next+1);
    }

term:
    TRACE_EXIT("(%d)", ret_code);
    return ret_code;
}

// service the get_next() request for the sub-tree:
//      /routing-state/ribs/rib{}
static int reply_next_rib(
    struct confd_trans_ctx *tctx,
    confd_hkeypath_t *kp,
    long next
) {
    TRACE_ENTRY("path == %s, next == %ld", KPSTR(kp), next);
    int ret_code = CONFD_OK;

    uint32_t list_tag = CONFD_GET_XML(&kp->v[0][0]);
    LOG("list tag == %d", list_tag);

    struct system_route_data * data = tctx->t_opaque;

    switch (list_tag) {
        case rt_rib:
        {
            // for RIB names, return sequentially two "default" RIBs,
            // one for IPv4 routes, another for IPv6, if at least one route in
            // the specific category exists...
            char * str = NULL;
            switch (next) {

                case -1:
                    if (NULL == str) {
                        str = RIB_NAME_IPv4;
                    }
                    // intentional fall-through

                case 0:
                    if (NULL == str) {
                        str = RIB_NAME_IPv6;
                    }
                    // intentional fall-through

                default:
                    if (NULL == str) {
                        // no more items to iterate
                        confd_data_reply_next_key(tctx, NULL, -1, -1);
                    } else {
                        confd_value_t vkey;
                        CONFD_SET_CBUF(&vkey, str, strlen(str));
                        confd_data_reply_next_key(tctx, &vkey, 1, next+1);
                    }
            }
        }
            break;

        case rt_route:
        {
            // Inspired by: "examples.confd/intro/8-c_stats_no_key" example;
            // No keys for the "route" list, so just return a pointer to the
            // specific structure to use in get_elem() callback
            const struct route_state * rs_ptr = NULL;
            if (-1 == next) {
                // need to read routes from system
                enum route_afi afi;
                char * ptr = CONFD_GET_CBUFPTR(&kp->v[kp->len-4][0]);
                if (0 == strcmp(ptr, RIB_NAME_IPv4)) {
                    afi = ipv4;
                } else if (0 == strcmp(ptr, RIB_NAME_IPv6)) {
                    afi = ipv6;
                }
                if (check_afi_exists(afi, data)) {
                    rs_ptr = data->rs;
                }
            } else if (0 != next) {
                LOG("next == %ld", next);
                rs_ptr = ((struct route_state *)next)->next;
            }

            if (NULL != rs_ptr) {
                confd_value_t v;
                CONFD_SET_INT64(&v, (long) rs_ptr);
                LOG("returning value == %ld", (long)rs_ptr);
                trace_route_key(&rs_ptr->afi_prefix);
                LOG("nexthop_type == %d", rs_ptr->nexthop_type);
                confd_data_reply_next_key(tctx, &v, 1, (long)rs_ptr);
            } else {
                // no more elements in the list
                confd_data_reply_next_key(tctx, NULL, -1, -1);
            }
        }
            break;

        case rt_next_hop:
            ret_code = process_next_hop(tctx, kp, next);
            break;

        default:
            warn("Unsupported list entry! (%d)", list_tag);
            ret_code = CONFD_ERR;
    }

    TRACE_EXIT("(%d)", ret_code);
    return ret_code;
}

// main callback registered for data provider .get_next
int routing_get_next(
    struct confd_trans_ctx *tctx,
    confd_hkeypath_t *kp,
    long next
) {
    int ret_code = CONFD_OK;
    TRACE_ENTRY("path == %s; next == %d", KPSTR(kp), next);

    uint32_t kplen = kp->len;

    uint32_t branch_tag = CONFD_GET_XMLTAG(&kp->v[kplen-2][0]);

    switch (branch_tag) {

        case rt_control_plane_protocols:
            ret_code = reply_next_protocol(tctx, kp, next);
            break;

        case rt_ribs:
            ret_code = reply_next_rib(tctx, kp, next);
            break;

        default:
            warn("Unsupported tag! (%d)", branch_tag);
    }

    TRACE_EXIT("(%d)", ret_code);
    return ret_code;
}

// ==== get_case() ============================================================

// browse the input keypath and return the pointer to route state data if
// applicable (if keypath includes specific route[] list key)
struct route_state * get_route_state_ptr(confd_hkeypath_t * kp)
{
    struct route_state * ptr = NULL;

    if (NULL == kp) {
        goto term;
    }

    int i;
    for (i = 0; i < kp->len; i++) {
        if (C_XMLTAG == kp->v[i][0].type
            && rt_route == kp->v[i][0].val.xmltag.tag)
        {
            if (i>1) {
               long elem_key = CONFD_GET_INT64(&kp->v[i-1][0]);
               ptr = (struct route_state *) elem_key;
            }
        }
    }

term:
    return ptr;
}

// main callback registered for data provider .get_case
int routing_get_case(
    struct confd_trans_ctx *tctx,
    confd_hkeypath_t *kp,
    confd_value_t *choice
) {
    int ret_code = CONFD_OK;
    TRACE_ENTRY("path == %s; choice == %s", KPSTR(kp), VALSTR(choice));

    struct route_state * ptr = NULL;

    ptr = get_route_state_ptr(kp);

    int tag = -1;

    if (NULL != ptr) {
        switch (ptr->nexthop_type) {
            case simple:
                tag = rt_simple_next_hop;
                break;

            case special:
                tag = rt_special_next_hop;
                break;

            case list:
                tag = rt_next_hop_list;
                break;

            default:
                warn("Unsupported next-hop type! (%d)", ptr->nexthop_type);
        }

        ret_code = (-1 == tag) ? CONFD_ERR : CONFD_OK;
    }

    if (CONFD_OK == ret_code) {
        confd_value_t res_val;
        CONFD_SET_XMLTAG(&res_val, tag, rt__ns);
        LOG("returning case == %s", VALSTRNN(&res_val));
        confd_data_reply_value(tctx, &res_val);
    }

    TRACE_EXIT("(%d)", ret_code);
    return ret_code;
}

// ==== exists_optional() =====================================================

// main callback registered for data provider .exists_optional
int routing_exists_optional(
    struct confd_trans_ctx *tctx,
    confd_hkeypath_t *kp
) {
    int ret_code = CONFD_OK;
    TRACE_ENTRY("path == %s", KPSTR(kp));

    struct route_state * ptr = get_route_state_ptr(kp);
    if (NULL == ptr) {
        error("Failed to get route key!");
        ret_code = CONFD_ERR;
        goto term;
    }

    // TODO - implement via lowest metric route with specific prefix...
//    if (ptr->is_active) {
//        confd_data_reply_found(tctx);
//    } else {
        confd_data_reply_not_found(tctx);
//    }

term:
    TRACE_EXIT("(%d)", ret_code);
    return ret_code;
}
