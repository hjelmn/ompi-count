/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2006 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2006-2010 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2012-2013 Los Alamos National Security, LLC. 
 *                         All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orte_config.h"
#include "orte/types.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>
#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif
#ifdef HAVE_NET_UIO_H
#include <net/uio.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#include <signal.h>

#include "opal/mca/event/event.h"
#include "opal/opal_socket_errno.h"

#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"

#include "orte/mca/oob/tcp/oob_tcp.h"

/*
 * Local functions
 */
static void noop(int fd, short event, void *arg);

/*
 * Ping a peer to see if it is alive.
 *
 * @param peer (IN)   Opaque name of peer process.
 * @param tv (IN)     Timeout to wait for a response.
 * @return            OMPI error code (<0) on error number of bytes actually sent.
 */

int
mca_oob_tcp_ping(const orte_process_name_t* name,
                 const char* uri,
                 const struct timeval *timeout)
{
    int sd, flags, rc;
    struct sockaddr_storage inaddr;
    fd_set fdset;
    mca_oob_tcp_hdr_t hdr;
    struct timeval tv;
    struct iovec iov;
    opal_event_t *sigpipe_handler;
    socklen_t addrlen;

    /* parse uri string */
    if(ORTE_SUCCESS != (rc = mca_oob_tcp_parse_uri(uri, (struct sockaddr*) &inaddr))) {
       opal_output(0,
            "%s-%s mca_oob_tcp_ping: invalid uri: %s\n",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
            ORTE_NAME_PRINT(name),
            uri);
        return rc;
    }

    /* create socket */
    sd = socket(inaddr.ss_family, SOCK_STREAM, 0);
    if (sd < 0) {
       opal_output(0,
            "%s-%s mca_oob_tcp_ping: socket() failed: %s (%d)\n",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
            ORTE_NAME_PRINT(name),
            strerror(opal_socket_errno),
            opal_socket_errno);
        return ORTE_ERR_UNREACH;
    }

    /* setup the socket as non-blocking */
    if((flags = fcntl(sd, F_GETFL, 0)) < 0) {
        opal_output(0, "%s-%s mca_oob_tcp_ping: fcntl(F_GETFL) failed: %s (%d)\n", 
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
            ORTE_NAME_PRINT(name),
            strerror(opal_socket_errno),
            opal_socket_errno);
    } else {
        flags |= O_NONBLOCK;
        if(fcntl(sd, F_SETFL, flags) < 0) {
            opal_output(0, "%s-%s mca_oob_tcp_ping: fcntl(F_SETFL) failed: %s (%d)\n",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                ORTE_NAME_PRINT(name),
                strerror(opal_socket_errno),
                opal_socket_errno);
        }
    }

    switch (inaddr.ss_family) {
    case AF_INET:
        addrlen = sizeof(struct sockaddr_in);
        break;
    case AF_INET6:
        addrlen = sizeof(struct sockaddr_in6);
        break;
    default:
        addrlen = 0;
    }

    /* start the connect - will likely fail with EINPROGRESS */
    FD_ZERO(&fdset);
    if(connect(sd, (struct sockaddr*)&inaddr, addrlen) < 0) {
        /* connect failed? */
        if(opal_socket_errno != EINPROGRESS && opal_socket_errno != EWOULDBLOCK) {
            opal_output(0, "%s-%s mca_oob_tcp_ping: connect failed: %s (%d)\n",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        ORTE_NAME_PRINT(name),
                        strerror(opal_socket_errno),
                        opal_socket_errno);
            CLOSE_THE_SOCKET(sd);
            return ORTE_ERR_UNREACH;
        }

        /* select with timeout to wait for connect to complete */
        FD_SET(sd, &fdset);
        tv = *timeout;
        rc = select(sd+1, NULL, &fdset, NULL, &tv);
        if(rc <= 0) {
             CLOSE_THE_SOCKET(sd);
             return ORTE_ERR_UNREACH;
        }
    }

    /* set socket back to blocking */
    flags &= ~O_NONBLOCK;
    if(fcntl(sd, F_SETFL, flags) < 0) {
         opal_output(0, "%s-%s mca_oob_tcp_ping: fcntl(F_SETFL) failed: %s (%d)\n",
             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
             ORTE_NAME_PRINT(name),
             strerror(opal_socket_errno),
             opal_socket_errno);
    }

    /* send a probe message */
    memset(&hdr, 0, sizeof(hdr));
    hdr.msg_src = *ORTE_PROC_MY_NAME;

    hdr.msg_dst = *name;
    hdr.msg_type = MCA_OOB_TCP_PROBE;
    MCA_OOB_TCP_HDR_HTON(&hdr);

    /* Ignore SIGPIPE in the write -- determine success or failure in
       the ping by looking at the return code from write() */
    sigpipe_handler = opal_event_alloc();
    opal_event_signal_set(orte_event_base, sigpipe_handler, SIGPIPE,
                          noop, sigpipe_handler);
    opal_event_signal_add(sigpipe_handler, NULL);

    /* Do the write and see what happens */
    iov.iov_base = (IOVBASE_TYPE*)&hdr;
    iov.iov_len  = sizeof(hdr);
    rc = writev(sd, &iov, 1 );
    /* Now de-register the handler */
    opal_event_free(sigpipe_handler);

    if (rc != sizeof(hdr)) {
        CLOSE_THE_SOCKET(sd);
        return ORTE_ERR_UNREACH;
    }

    /* select with timeout to wait for response */
    FD_SET(sd, &fdset);
    tv = *timeout;
    rc = select(sd+1, &fdset, NULL, NULL, &tv);
    if(rc <= 0) {
        CLOSE_THE_SOCKET(sd);
        return ORTE_ERR_UNREACH;
    }
    if((rc = read(sd, &hdr, sizeof(hdr))) != sizeof(hdr)) {
        CLOSE_THE_SOCKET(sd);
        return ORTE_ERR_UNREACH;
    }
    MCA_OOB_TCP_HDR_NTOH(&hdr);
    if(hdr.msg_type != MCA_OOB_TCP_PROBE) {
        CLOSE_THE_SOCKET(sd);
        return ORTE_ERR_UNREACH;
    }
    CLOSE_THE_SOCKET(sd);
    return ORTE_SUCCESS;
}


static void noop(int fd, short event, void *arg)
{
    opal_event_t *ev = (opal_event_t*)arg;

    /* return the event */
    opal_event_free(ev);
}
