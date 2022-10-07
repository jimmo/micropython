/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013-2019 Damien P. George
 * Copyright (c) 2015 Galen Hazelwood
 * Copyright (c) 2015-2017 Paul Sokolovsky
 * Copyright (c) 2022 Jim Mussared
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "py/runtime.h"

#if MICROPY_PY_NETWORK && MICROPY_PY_LWIP && (!MICROPY_PY_LWIP_EXCLUSIVE || MICROPY_PY_LWIP_EXCLUSIVE_ENABLE_INCLUDE)

#include <string.h>

#include "lwip/dhcp.h"
#include "lwip/dns.h"
#include "lwip/igmp.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/raw.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"
#if LWIP_VERSION_MAJOR < 2
#include "lwip/timers.h"
#include "lwip/tcp_impl.h"
#else
#include "lwip/timeouts.h"
#include "lwip/priv/tcp_priv.h"
#endif
#include "lwip/apps/mdns.h"

#include "py/mphal.h"
#include "py/stream.h"

#include "shared/netutils/netutils.h"

#include "extmod/modnetwork.h"

#if 0 // print debugging info
#define DEBUG_printf DEBUG_printf
#else // don't print debugging info
#define DEBUG_printf(...) (void)0
#endif

// For compatibilily with older lwIP versions.
#ifndef ip_set_option
#define ip_set_option(pcb, opt)   ((pcb)->so_options |= (opt))
#endif
#ifndef ip_reset_option
#define ip_reset_option(pcb, opt) ((pcb)->so_options &= ~(opt))
#endif

// A port can define these hooks to provide concurrency protection
#ifndef MICROPY_PY_LWIP_ENTER
#define MICROPY_PY_LWIP_ENTER
#define MICROPY_PY_LWIP_REENTER
#define MICROPY_PY_LWIP_EXIT
#endif

// Timeout between closing a TCP socket and doing a tcp_abort on that
// socket, if the connection isn't closed cleanly in that time.
#define MICROPY_PY_LWIP_TCP_CLOSE_TIMEOUT_MS (10000)


/******************************************************************************/
// Table to convert lwIP err_t codes to socket errno codes, from the lwIP
// socket API.

// lwIP 2 changed LWIP_VERSION and it can no longer be used in macros,
// so we define our own equivalent version that can.
#define LWIP_VERSION_MACRO (LWIP_VERSION_MAJOR << 24 | LWIP_VERSION_MINOR << 16 \
        | LWIP_VERSION_REVISION << 8 | LWIP_VERSION_RC)

// Extension to lwIP error codes
#define _ERR_BADF -16
// TODO: We just know that change happened somewhere between 1.4.0 and 1.4.1,
// investigate in more detail.
#if LWIP_VERSION_MACRO < 0x01040100
static const int error_lookup_table[] = {
    0,                /* ERR_OK          0      No error, everything OK. */
    MP_ENOMEM,        /* ERR_MEM        -1      Out of memory error.     */
    MP_ENOBUFS,       /* ERR_BUF        -2      Buffer error.            */
    MP_EWOULDBLOCK,   /* ERR_TIMEOUT    -3      Timeout                  */
    MP_EHOSTUNREACH,  /* ERR_RTE        -4      Routing problem.         */
    MP_EINPROGRESS,   /* ERR_INPROGRESS -5      Operation in progress    */
    MP_EINVAL,        /* ERR_VAL        -6      Illegal value.           */
    MP_EWOULDBLOCK,   /* ERR_WOULDBLOCK -7      Operation would block.   */

    MP_ECONNABORTED,  /* ERR_ABRT       -8      Connection aborted.      */
    MP_ECONNRESET,    /* ERR_RST        -9      Connection reset.        */
    MP_ENOTCONN,      /* ERR_CLSD       -10     Connection closed.       */
    MP_ENOTCONN,      /* ERR_CONN       -11     Not connected.           */
    MP_EIO,           /* ERR_ARG        -12     Illegal argument.        */
    MP_EADDRINUSE,    /* ERR_USE        -13     Address in use.          */
    -1,               /* ERR_IF         -14     Low-level netif error    */
    MP_EALREADY,      /* ERR_ISCONN     -15     Already connected.       */
    MP_EBADF,         /* _ERR_BADF      -16     Closed socket (null pcb) */
};
#elif LWIP_VERSION_MACRO < 0x02000000
static const int error_lookup_table[] = {
    0,                /* ERR_OK          0      No error, everything OK. */
    MP_ENOMEM,        /* ERR_MEM        -1      Out of memory error.     */
    MP_ENOBUFS,       /* ERR_BUF        -2      Buffer error.            */
    MP_EWOULDBLOCK,   /* ERR_TIMEOUT    -3      Timeout                  */
    MP_EHOSTUNREACH,  /* ERR_RTE        -4      Routing problem.         */
    MP_EINPROGRESS,   /* ERR_INPROGRESS -5      Operation in progress    */
    MP_EINVAL,        /* ERR_VAL        -6      Illegal value.           */
    MP_EWOULDBLOCK,   /* ERR_WOULDBLOCK -7      Operation would block.   */

    MP_EADDRINUSE,    /* ERR_USE        -8      Address in use.          */
    MP_EALREADY,      /* ERR_ISCONN     -9      Already connected.       */
    MP_ECONNABORTED,  /* ERR_ABRT       -10     Connection aborted.      */
    MP_ECONNRESET,    /* ERR_RST        -11     Connection reset.        */
    MP_ENOTCONN,      /* ERR_CLSD       -12     Connection closed.       */
    MP_ENOTCONN,      /* ERR_CONN       -13     Not connected.           */
    MP_EIO,           /* ERR_ARG        -14     Illegal argument.        */
    -1,               /* ERR_IF         -15     Low-level netif error    */
    MP_EBADF,         /* _ERR_BADF      -16     Closed socket (null pcb) */
};
#else
// Matches lwIP 2.0.3
#undef _ERR_BADF
#define _ERR_BADF -17
static const int error_lookup_table[] = {
    0,                /* ERR_OK          0      No error, everything OK  */
    MP_ENOMEM,        /* ERR_MEM        -1      Out of memory error      */
    MP_ENOBUFS,       /* ERR_BUF        -2      Buffer error             */
    MP_EWOULDBLOCK,   /* ERR_TIMEOUT    -3      Timeout                  */
    MP_EHOSTUNREACH,  /* ERR_RTE        -4      Routing problem          */
    MP_EINPROGRESS,   /* ERR_INPROGRESS -5      Operation in progress    */
    MP_EINVAL,        /* ERR_VAL        -6      Illegal value            */
    MP_EWOULDBLOCK,   /* ERR_WOULDBLOCK -7      Operation would block    */
    MP_EADDRINUSE,    /* ERR_USE        -8      Address in use           */
    MP_EALREADY,      /* ERR_ALREADY    -9      Already connecting       */
    MP_EALREADY,      /* ERR_ISCONN     -10     Conn already established */
    MP_ENOTCONN,      /* ERR_CONN       -11     Not connected            */
    -1,               /* ERR_IF         -12     Low-level netif error    */
    MP_ECONNABORTED,  /* ERR_ABRT       -13     Connection aborted       */
    MP_ECONNRESET,    /* ERR_RST        -14     Connection reset         */
    MP_ENOTCONN,      /* ERR_CLSD       -15     Connection closed        */
    MP_EIO,           /* ERR_ARG        -16     Illegal argument.        */
    MP_EBADF,         /* _ERR_BADF      -17     Closed socket (null pcb) */
};
#endif

#if !MICROPY_PY_USOCKET_EXTENDED_STATE
#error "LWIP requires MICROPY_PY_USOCKET_EXTENDED_STATE"
#endif

typedef struct _lwip_socket_private_t {
    volatile union {
        struct tcp_pcb *tcp;
        struct udp_pcb *udp;
        struct raw_pcb *raw;
    } pcb;
    volatile union {
        struct pbuf *pbuf;
        struct {
            uint8_t alloc;
            uint8_t iget;
            uint8_t iput;
            union {
                struct tcp_pcb *item; // if alloc == 0
                struct tcp_pcb **array; // if alloc != 0
            } tcp;
        } connection;
    } incoming;
    byte peer[4];
    mp_uint_t peer_port;
    uint16_t recv_offset;
} lwip_socket_private_t;

#define LWIP_SOCKET_PRIVATE(sock) ((lwip_socket_private_t *)(sock)->_private)

static inline void poll_sockets(void) {
    #ifdef MICROPY_EVENT_POLL_HOOK
    MICROPY_EVENT_POLL_HOOK;
    #else
    mp_hal_delay_ms(1);
    #endif
}

STATIC struct tcp_pcb *volatile *lwip_socket_incoming_array(mod_network_socket_obj_t *socket) {
    if (LWIP_SOCKET_PRIVATE(socket)->incoming.connection.alloc == 0) {
        return &LWIP_SOCKET_PRIVATE(socket)->incoming.connection.tcp.item;
    } else {
        return &LWIP_SOCKET_PRIVATE(socket)->incoming.connection.tcp.array[0];
    }
}

STATIC void lwip_socket_free_incoming(mod_network_socket_obj_t *socket) {
    bool socket_is_listener =
        socket->type == MOD_NETWORK_SOCK_STREAM
        && LWIP_SOCKET_PRIVATE(socket)->pcb.tcp->state == LISTEN;

    if (!socket_is_listener) {
        if (LWIP_SOCKET_PRIVATE(socket)->incoming.pbuf != NULL) {
            pbuf_free(LWIP_SOCKET_PRIVATE(socket)->incoming.pbuf);
            LWIP_SOCKET_PRIVATE(socket)->incoming.pbuf = NULL;
        }
    } else {
        uint8_t alloc = LWIP_SOCKET_PRIVATE(socket)->incoming.connection.alloc;
        struct tcp_pcb *volatile *tcp_array = lwip_socket_incoming_array(socket);
        for (uint8_t i = 0; i < alloc; ++i) {
            // Deregister callback and abort
            if (tcp_array[i] != NULL) {
                tcp_poll(tcp_array[i], NULL, 0);
                tcp_abort(tcp_array[i]);
                tcp_array[i] = NULL;
            }
        }
    }
}

/*******************************************************************************/
// Callback functions for the lwIP raw API.

static inline void exec_user_callback(mod_network_socket_obj_t *socket) {
    if (socket->callback != MP_OBJ_NULL) {
        // Schedule the user callback to execute outside the lwIP context
        mp_sched_schedule(socket->callback, MP_OBJ_FROM_PTR(socket));
    }
}

#if MICROPY_PY_LWIP_SOCK_RAW
// Callback for incoming raw packets.
#if LWIP_VERSION_MAJOR < 2
STATIC u8_t _lwip_raw_incoming(void *arg, struct raw_pcb *pcb, struct pbuf *p, ip_addr_t *addr)
#else
STATIC u8_t _lwip_raw_incoming(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr)
#endif
{
    mod_network_socket_obj_t *socket = (mod_network_socket_obj_t *)arg;

    if (LWIP_SOCKET_PRIVATE(socket)->incoming.pbuf != NULL) {
        pbuf_free(p);
    } else {
        LWIP_SOCKET_PRIVATE(socket)->incoming.pbuf = p;
        memcpy(&LWIP_SOCKET_PRIVATE(socket)->peer, addr, sizeof(LWIP_SOCKET_PRIVATE(socket)->peer));
    }
    return 1; // we ate the packet
}
#endif

// Callback for incoming UDP packets. We simply stash the packet and the source address,
// in case we need it for recvfrom.
#if LWIP_VERSION_MAJOR < 2
STATIC void _lwip_udp_incoming(void *arg, struct udp_pcb *upcb, struct pbuf *p, ip_addr_t *addr, u16_t port)
#else
STATIC void _lwip_udp_incoming(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
#endif
{
    mod_network_socket_obj_t *socket = (mod_network_socket_obj_t *)arg;

    if (LWIP_SOCKET_PRIVATE(socket)->incoming.pbuf != NULL) {
        // That's why they call it "unreliable". No room in the inn, drop the packet.
        pbuf_free(p);
    } else {
        LWIP_SOCKET_PRIVATE(socket)->incoming.pbuf = p;
        LWIP_SOCKET_PRIVATE(socket)->peer_port = (mp_uint_t)port;
        memcpy(&LWIP_SOCKET_PRIVATE(socket)->peer, addr, sizeof(LWIP_SOCKET_PRIVATE(socket)->peer));
    }
}

// Callback for general tcp errors.
STATIC void _lwip_tcp_error(void *arg, err_t err) {
    mod_network_socket_obj_t *socket = (mod_network_socket_obj_t *)arg;

    // Free any incoming buffers or connections that are stored
    lwip_socket_free_incoming(socket);
    // Pass the error code back via the connection variable.
    socket->state = err;
    // If we got here, the lwIP stack either has deallocated or will deallocate the pcb.
    LWIP_SOCKET_PRIVATE(socket)->pcb.tcp = NULL;
}

// Callback for tcp connection requests. Error code err is unused. (See tcp.h)
STATIC err_t _lwip_tcp_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
    mod_network_socket_obj_t *socket = (mod_network_socket_obj_t *)arg;

    socket->state = MOD_NETWORK_SS_CONNECTED;
    return ERR_OK;
}

// Handle errors (eg connection aborted) on TCP PCBs that have been put on the
// accept queue but are not yet actually accepted.
STATIC void _lwip_tcp_err_unaccepted(void *arg, err_t err) {
    struct tcp_pcb *pcb = (struct tcp_pcb *)arg;

    // The ->connected entry is repurposed to store the parent socket; this is safe
    // because it's only ever used by lwIP if tcp_connect is called on the TCP PCB.
    mod_network_socket_obj_t *socket = (mod_network_socket_obj_t *)pcb->connected;

    // Array is not volatile because thiss callback is executed within the lwIP context
    uint8_t alloc = LWIP_SOCKET_PRIVATE(socket)->incoming.connection.alloc;
    struct tcp_pcb **tcp_array = (struct tcp_pcb **)lwip_socket_incoming_array(socket);

    // Search for PCB on the accept queue of the parent socket
    struct tcp_pcb **shift_down = NULL;
    uint8_t i = LWIP_SOCKET_PRIVATE(socket)->incoming.connection.iget;
    do {
        if (shift_down == NULL) {
            if (tcp_array[i] == pcb) {
                shift_down = &tcp_array[i];
            }
        } else {
            *shift_down = tcp_array[i];
            shift_down = &tcp_array[i];
        }
        if (++i >= alloc) {
            i = 0;
        }
    } while (i != LWIP_SOCKET_PRIVATE(socket)->incoming.connection.iput);

    // PCB found in queue, remove it
    if (shift_down != NULL) {
        *shift_down = NULL;
        LWIP_SOCKET_PRIVATE(socket)->incoming.connection.iput = shift_down - tcp_array;
    }
}

// By default, a child socket of listen socket is created with recv
// handler which discards incoming pbuf's. We don't want to do that,
// so set this handler which requests lwIP to keep pbuf's and deliver
// them later. We cannot cache pbufs in child socket on Python side,
// until it is created in accept().
STATIC err_t _lwip_tcp_recv_unaccepted(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    return ERR_BUF;
}

// Callback for incoming tcp connections.
STATIC err_t _lwip_tcp_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    // err can be ERR_MEM to notify us that there was no memory for an incoming connection
    if (err != ERR_OK) {
        return ERR_OK;
    }

    mod_network_socket_obj_t *socket = (mod_network_socket_obj_t *)arg;
    tcp_recv(newpcb, _lwip_tcp_recv_unaccepted);

    // Search for an empty slot to store the new connection
    struct tcp_pcb *volatile *slot = &lwip_socket_incoming_array(socket)[LWIP_SOCKET_PRIVATE(socket)->incoming.connection.iput];
    if (*slot == NULL) {
        // Have an empty slot to store waiting connection
        *slot = newpcb;
        if (++LWIP_SOCKET_PRIVATE(socket)->incoming.connection.iput >= LWIP_SOCKET_PRIVATE(socket)->incoming.connection.alloc) {
            LWIP_SOCKET_PRIVATE(socket)->incoming.connection.iput = 0;
        }

        // Schedule user accept callback
        exec_user_callback(socket);

        // Set the error callback to handle the case of a dropped connection before we
        // have a chance to take it off the accept queue.
        // The ->connected entry is repurposed to store the parent socket; this is safe
        // because it's only ever used by lwIP if tcp_connect is called on the TCP PCB.
        newpcb->connected = (void *)socket;
        tcp_arg(newpcb, newpcb);
        tcp_err(newpcb, _lwip_tcp_err_unaccepted);

        return ERR_OK;
    }

    DEBUG_printf("_lwip_tcp_accept: No room to queue pcb waiting for accept\n");
    return ERR_BUF;
}

// Callback for inbound tcp packets.
STATIC err_t _lwip_tcp_recv(void *arg, struct tcp_pcb *tcpb, struct pbuf *p, err_t err) {
    mod_network_socket_obj_t *socket = (mod_network_socket_obj_t *)arg;

    if (p == NULL) {
        // Other side has closed connection.
        DEBUG_printf("_lwip_tcp_recv[%p]: other side closed connection\n", socket);
        socket->state = MOD_NETWORK_SS_PEER_CLOSED;
        exec_user_callback(socket);
        return ERR_OK;
    }

    if (LWIP_SOCKET_PRIVATE(socket)->incoming.pbuf == NULL) {
        LWIP_SOCKET_PRIVATE(socket)->incoming.pbuf = p;
    } else {
        #ifdef SOCKET_SINGLE_PBUF
        return ERR_BUF;
        #else
        pbuf_cat(LWIP_SOCKET_PRIVATE(socket)->incoming.pbuf, p);
        #endif
    }

    exec_user_callback(socket);

    return ERR_OK;
}

/*******************************************************************************/
// Functions for socket send/receive operations. Socket send/recv and friends call
// these to do the work.

// Helper function for send/sendto to handle raw/UDP packets.
STATIC mp_uint_t lwip_raw_udp_send(mod_network_socket_obj_t *socket, const byte *buf, mp_uint_t len, byte *ip, mp_uint_t port, int *_errno) {
    if (len > 0xffff) {
        // Any packet that big is probably going to fail the pbuf_alloc anyway, but may as well try
        len = 0xffff;
    }

    MICROPY_PY_LWIP_ENTER

    // FIXME: maybe PBUF_ROM?
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
    if (p == NULL) {
        MICROPY_PY_LWIP_EXIT
        *_errno = MP_ENOMEM;
        return -1;
    }

    memcpy(p->payload, buf, len);

    err_t err;
    if (ip == NULL) {
        #if MICROPY_PY_LWIP_SOCK_RAW
        if (socket->type == MOD_NETWORK_SOCK_RAW) {
            err = raw_send(LWIP_SOCKET_PRIVATE(socket)->pcb.raw, p);
        } else
        #endif
        {
            err = udp_send(LWIP_SOCKET_PRIVATE(socket)->pcb.udp, p);
        }
    } else {
        ip_addr_t dest;
        IP4_ADDR(&dest, ip[0], ip[1], ip[2], ip[3]);
        #if MICROPY_PY_LWIP_SOCK_RAW
        if (socket->type == MOD_NETWORK_SOCK_RAW) {
            err = raw_sendto(LWIP_SOCKET_PRIVATE(socket)->pcb.raw, p, &dest);
        } else
        #endif
        {
            err = udp_sendto(LWIP_SOCKET_PRIVATE(socket)->pcb.udp, p, &dest, port);
        }
    }

    pbuf_free(p);

    MICROPY_PY_LWIP_EXIT

    // udp_sendto can return 1 on occasion for ESP8266 port.  It's not known why
    // but it seems that the send actually goes through without error in this case.
    // So we treat such cases as a success until further investigation.
    if (err != ERR_OK && err != 1) {
        *_errno = error_lookup_table[-err];
        return -1;
    }

    return len;
}

// Helper function for recv/recvfrom to handle raw/UDP packets
STATIC mp_uint_t lwip_raw_udp_receive(mod_network_socket_obj_t *socket, byte *buf, mp_uint_t len, byte *ip, mp_uint_t *port, int *_errno) {

    if (LWIP_SOCKET_PRIVATE(socket)->incoming.pbuf == NULL) {
        if (socket->timeout == 0) {
            // Non-blocking socket.
            *_errno = MP_EAGAIN;
            return -1;
        }

        // Wait for data to arrive on UDP socket.
        mp_uint_t start = mp_hal_ticks_ms();
        while (LWIP_SOCKET_PRIVATE(socket)->incoming.pbuf == NULL) {
            if (socket->timeout != -1 && mp_hal_ticks_ms() - start > socket->timeout) {
                *_errno = MP_ETIMEDOUT;
                return -1;
            }
            poll_sockets();
        }
    }

    if (ip != NULL) {
        memcpy(ip, &LWIP_SOCKET_PRIVATE(socket)->peer, sizeof(LWIP_SOCKET_PRIVATE(socket)->peer));
        *port = LWIP_SOCKET_PRIVATE(socket)->peer_port;
    }

    struct pbuf *p = LWIP_SOCKET_PRIVATE(socket)->incoming.pbuf;

    MICROPY_PY_LWIP_ENTER

    u16_t result = pbuf_copy_partial(p, buf, ((p->tot_len > len) ? len : p->tot_len), 0);
    pbuf_free(p);
    LWIP_SOCKET_PRIVATE(socket)->incoming.pbuf = NULL;

    MICROPY_PY_LWIP_EXIT

    return (mp_uint_t)result;
}

// For use in stream virtual methods
#define STREAM_ERROR_CHECK(socket) \
    if (socket->state < 0) { \
        *_errno = error_lookup_table[-socket->state]; \
        return MP_STREAM_ERROR; \
    } \
    assert(LWIP_SOCKET_PRIVATE(socket)->pcb.tcp);

// Version of above for use when lock is held
#define STREAM_ERROR_CHECK_WITH_LOCK(socket) \
    if (socket->state < 0) { \
        *_errno = error_lookup_table[-socket->state]; \
        MICROPY_PY_LWIP_EXIT \
        return MP_STREAM_ERROR; \
    } \
    assert(LWIP_SOCKET_PRIVATE(socket)->pcb.tcp);


// Helper function for send/sendto to handle TCP packets
STATIC mp_uint_t lwip_tcp_send(mod_network_socket_obj_t *socket, const byte *buf, mp_uint_t len, int *_errno) {
    // Check for any pending errors
    STREAM_ERROR_CHECK(socket);

    MICROPY_PY_LWIP_ENTER

    u16_t available = tcp_sndbuf(LWIP_SOCKET_PRIVATE(socket)->pcb.tcp);

    if (available == 0) {
        // Non-blocking socket
        if (socket->timeout == 0) {
            MICROPY_PY_LWIP_EXIT
            *_errno = MP_EAGAIN;
            return MP_STREAM_ERROR;
        }

        mp_uint_t start = mp_hal_ticks_ms();
        // Assume that MOD_NETWORK_SS_PEER_CLOSED may mean half-closed connection, where peer closed it
        // sending direction, but not receiving. Consequently, check for both MOD_NETWORK_SS_CONNECTED
        // and MOD_NETWORK_SS_PEER_CLOSED as normal conditions and still waiting for buffers to be sent.
        // If peer fully closed socket, we would have socket->state set to ERR_RST (connection
        // reset) by error callback.
        // Avoid sending too small packets, so wait until at least 16 bytes available
        while (socket->state >= MOD_NETWORK_SS_CONNECTED && (available = tcp_sndbuf(LWIP_SOCKET_PRIVATE(socket)->pcb.tcp)) < 16) {
            MICROPY_PY_LWIP_EXIT
            if (socket->timeout != -1 && mp_hal_ticks_ms() - start > socket->timeout) {
                *_errno = MP_ETIMEDOUT;
                return MP_STREAM_ERROR;
            }
            poll_sockets();
            MICROPY_PY_LWIP_REENTER
        }

        // While we waited, something could happen
        STREAM_ERROR_CHECK_WITH_LOCK(socket);
    }

    u16_t write_len = MIN(available, len);

    // If tcp_write returns ERR_MEM then there's currently not enough memory to
    // queue the write, so wait and keep trying until it succeeds (with 10s limit).
    // Note: if the socket is non-blocking then this code will actually block until
    // there's enough memory to do the write, but by this stage we have already
    // committed to being able to write the data.
    err_t err;
    for (int i = 0; i < 200; ++i) {
        err = tcp_write(LWIP_SOCKET_PRIVATE(socket)->pcb.tcp, buf, write_len, TCP_WRITE_FLAG_COPY);
        if (err != ERR_MEM) {
            break;
        }
        err = tcp_output(LWIP_SOCKET_PRIVATE(socket)->pcb.tcp);
        if (err != ERR_OK) {
            break;
        }
        MICROPY_PY_LWIP_EXIT
        mp_hal_delay_ms(50);
        MICROPY_PY_LWIP_REENTER
    }

    // If the output buffer is getting full then send the data to the lower layers
    if (err == ERR_OK && tcp_sndbuf(LWIP_SOCKET_PRIVATE(socket)->pcb.tcp) < TCP_SND_BUF / 4) {
        err = tcp_output(LWIP_SOCKET_PRIVATE(socket)->pcb.tcp);
    }

    MICROPY_PY_LWIP_EXIT

    if (err != ERR_OK) {
        *_errno = error_lookup_table[-err];
        return MP_STREAM_ERROR;
    }

    return write_len;
}

// Helper function for recv/recvfrom to handle TCP packets
STATIC mp_uint_t lwip_tcp_receive(mod_network_socket_obj_t *socket, byte *buf, mp_uint_t len, int *_errno) {
    // Check for any pending errors
    STREAM_ERROR_CHECK(socket);

    if (LWIP_SOCKET_PRIVATE(socket)->incoming.pbuf == NULL) {

        // Non-blocking socket
        if (socket->timeout == 0) {
            if (socket->state == MOD_NETWORK_SS_PEER_CLOSED) {
                return 0;
            }
            *_errno = MP_EAGAIN;
            return -1;
        }

        mp_uint_t start = mp_hal_ticks_ms();
        while (socket->state == MOD_NETWORK_SS_CONNECTED && LWIP_SOCKET_PRIVATE(socket)->incoming.pbuf == NULL) {
            if (socket->timeout != -1 && mp_hal_ticks_ms() - start > socket->timeout) {
                *_errno = MP_ETIMEDOUT;
                return -1;
            }
            poll_sockets();
        }

        if (socket->state == MOD_NETWORK_SS_PEER_CLOSED) {
            if (LWIP_SOCKET_PRIVATE(socket)->incoming.pbuf == NULL) {
                // socket closed and no data left in buffer
                return 0;
            }
        } else if (socket->state != MOD_NETWORK_SS_CONNECTED) {
            if (socket->state >= MOD_NETWORK_SS_NEW) {
                *_errno = MP_ENOTCONN;
            } else {
                *_errno = error_lookup_table[-socket->state];
            }
            return -1;
        }
    }

    MICROPY_PY_LWIP_ENTER

    assert(LWIP_SOCKET_PRIVATE(socket)->pcb.tcp != NULL);

    struct pbuf *p = LWIP_SOCKET_PRIVATE(socket)->incoming.pbuf;

    mp_uint_t remaining = p->len - LWIP_SOCKET_PRIVATE(socket)->recv_offset;
    if (len > remaining) {
        len = remaining;
    }

    memcpy(buf, (byte *)p->payload + LWIP_SOCKET_PRIVATE(socket)->recv_offset, len);

    remaining -= len;
    if (remaining == 0) {
        LWIP_SOCKET_PRIVATE(socket)->incoming.pbuf = p->next;
        // If we don't ref here, free() will free the entire chain,
        // if we ref, it does what we need: frees 1st buf, and decrements
        // next buf's refcount back to 1.
        pbuf_ref(p->next);
        pbuf_free(p);
        LWIP_SOCKET_PRIVATE(socket)->recv_offset = 0;
    } else {
        LWIP_SOCKET_PRIVATE(socket)->recv_offset += len;
    }
    tcp_recved(LWIP_SOCKET_PRIVATE(socket)->pcb.tcp, len);

    MICROPY_PY_LWIP_EXIT

    return len;
}

typedef struct _getaddrinfo_state_t {
    volatile int status;
    volatile ip_addr_t ipaddr;
} getaddrinfo_state_t;

// Callback for incoming DNS requests.
#if LWIP_VERSION_MAJOR < 2
STATIC void lwip_getaddrinfo_cb(const char *name, ip_addr_t *ipaddr, void *arg)
#else
STATIC void lwip_getaddrinfo_cb(const char *name, const ip_addr_t *ipaddr, void *arg)
#endif
{
    getaddrinfo_state_t *state = arg;
    if (ipaddr != NULL) {
        state->status = 1;
        state->ipaddr = *ipaddr;
    } else {
        // error
        state->status = -2;
    }
}

STATIC int lwip_gethostbyname(mp_obj_t nic, const char *name, mp_uint_t len, uint8_t *ip_out) {
    getaddrinfo_state_t state;
    state.status = 0;

    MICROPY_PY_LWIP_ENTER
    err_t ret = dns_gethostbyname(name, (ip_addr_t *)&state.ipaddr, lwip_getaddrinfo_cb, &state);
    MICROPY_PY_LWIP_EXIT

    switch (ret) {
        case ERR_OK:
            // cached
            state.status = 1;
            break;
        case ERR_INPROGRESS:
            while (state.status == 0) {
                poll_sockets();
            }
            break;
        default:
            state.status = ret;
    }

    if (state.status < 0) {
        // TODO: CPython raises gaierror, we raise with native lwIP negative error
        // values, to differentiate from normal errno's at least in such way.
        return state.status;
    }

    return 0;
}

STATIC int lwip_socket_socket(mod_network_socket_obj_t *socket) {
    LWIP_SOCKET_PRIVATE(socket)->recv_offset = 0;

    switch (socket->type) {
        case MOD_NETWORK_SOCK_STREAM:
            LWIP_SOCKET_PRIVATE(socket)->pcb.tcp = tcp_new();
            LWIP_SOCKET_PRIVATE(socket)->incoming.connection.alloc = 0;
            LWIP_SOCKET_PRIVATE(socket)->incoming.connection.tcp.item = NULL;
            break;
        case MOD_NETWORK_SOCK_DGRAM:
            LWIP_SOCKET_PRIVATE(socket)->pcb.udp = udp_new();
            LWIP_SOCKET_PRIVATE(socket)->incoming.pbuf = NULL;
            break;
        #if MICROPY_PY_LWIP_SOCK_RAW
        case MOD_NETWORK_SOCK_RAW: {
            LWIP_SOCKET_PRIVATE(socket)->pcb.raw = raw_new(socket->proto);
            break;
        }
        #endif
    }

    // Note: alias for pcb.udp and pcb.raw.
    if (LWIP_SOCKET_PRIVATE(socket)->pcb.tcp == NULL) {
        return MP_ENOMEM;
    }

    switch (socket->type) {
        case MOD_NETWORK_SOCK_STREAM: {
            // Register the socket object as our callback argument.
            tcp_arg(LWIP_SOCKET_PRIVATE(socket)->pcb.tcp, (void *)socket);
            // Register our error callback.
            tcp_err(LWIP_SOCKET_PRIVATE(socket)->pcb.tcp, _lwip_tcp_error);
            break;
        }
        case MOD_NETWORK_SOCK_DGRAM: {
            socket->state = MOD_NETWORK_SS_ACTIVE_UDP;
            // Register our receive callback now. Since UDP sockets don't require binding or connection
            // before use, there's no other good time to do it.
            udp_recv(LWIP_SOCKET_PRIVATE(socket)->pcb.udp, _lwip_udp_incoming, (void *)socket);
            break;
        }
        #if MICROPY_PY_LWIP_SOCK_RAW
        case MOD_NETWORK_SOCK_RAW: {
            // Register our receive callback now. Since raw sockets don't require binding or connection
            // before use, there's no other good time to do it.
            raw_recv(LWIP_SOCKET_PRIVATE(socket)->pcb.raw, _lwip_raw_incoming, (void *)socket);
            break;
        }
        #endif
    }

    return 0;
}

STATIC err_t _lwip_tcp_close_poll(void *arg, struct tcp_pcb *pcb) {
    // Connection has not been cleanly closed so just abort it to free up memory
    tcp_poll(pcb, NULL, 0);
    tcp_abort(pcb);
    return ERR_OK;
}

STATIC void lwip_socket_close(mod_network_socket_obj_t *socket) {
    if (LWIP_SOCKET_PRIVATE(socket)->pcb.tcp == NULL) {
        return;
    }

    MICROPY_PY_LWIP_ENTER

    // Free any incoming buffers or connections that are stored
    lwip_socket_free_incoming(socket);

    switch (socket->type) {
        case MOD_NETWORK_SOCK_STREAM: {
            // Deregister callback (pcb.tcp is set to NULL below so must deregister now)
            tcp_arg(LWIP_SOCKET_PRIVATE(socket)->pcb.tcp, NULL);
            tcp_err(LWIP_SOCKET_PRIVATE(socket)->pcb.tcp, NULL);
            tcp_recv(LWIP_SOCKET_PRIVATE(socket)->pcb.tcp, NULL);

            if (LWIP_SOCKET_PRIVATE(socket)->pcb.tcp->state != LISTEN) {
                // Schedule a callback to abort the connection if it's not cleanly closed after
                // the given timeout.  The callback must be set before calling tcp_close since
                // the latter may free the pcb; if it doesn't then the callback will be active.
                tcp_poll(LWIP_SOCKET_PRIVATE(socket)->pcb.tcp, _lwip_tcp_close_poll, MICROPY_PY_LWIP_TCP_CLOSE_TIMEOUT_MS / 500);
            }
            if (tcp_close(LWIP_SOCKET_PRIVATE(socket)->pcb.tcp) != ERR_OK) {
                DEBUG_printf("lwip_close: had to call tcp_abort()\n");
                tcp_abort(LWIP_SOCKET_PRIVATE(socket)->pcb.tcp);
            }
            break;
        }
        case MOD_NETWORK_SOCK_DGRAM:
            udp_recv(LWIP_SOCKET_PRIVATE(socket)->pcb.udp, NULL, NULL);
            udp_remove(LWIP_SOCKET_PRIVATE(socket)->pcb.udp);
            break;
        #if MICROPY_PY_LWIP_SOCK_RAW
        case MOD_NETWORK_SOCK_RAW:
            raw_recv(LWIP_SOCKET_PRIVATE(socket)->pcb.raw, NULL, NULL);
            raw_remove(LWIP_SOCKET_PRIVATE(socket)->pcb.raw);
            break;
        #endif
    }

    LWIP_SOCKET_PRIVATE(socket)->pcb.tcp = NULL;

    // TODO: this used to set state to BADF.

    MICROPY_PY_LWIP_EXIT
}

STATIC int lwip_socket_bind(mod_network_socket_obj_t *socket, byte *ip, mp_uint_t port) {
    ip_addr_t bind_addr;
    IP4_ADDR(&bind_addr, ip[0], ip[1], ip[2], ip[3]);

    err_t err = ERR_ARG;
    switch (socket->type) {
        case MOD_NETWORK_SOCK_STREAM: {
            err = tcp_bind(LWIP_SOCKET_PRIVATE(socket)->pcb.tcp, &bind_addr, port);
            break;
        }
        case MOD_NETWORK_SOCK_DGRAM: {
            err = udp_bind(LWIP_SOCKET_PRIVATE(socket)->pcb.udp, &bind_addr, port);
            break;
        }
    }

    return err == ERR_OK ? 0 : error_lookup_table[-err];
}

STATIC int lwip_socket_listen(mod_network_socket_obj_t *socket, mp_int_t backlog) {
    if (LWIP_SOCKET_PRIVATE(socket)->pcb.tcp == NULL) {
        return MP_EBADF;
    }

    struct tcp_pcb *new_pcb = tcp_listen_with_backlog(LWIP_SOCKET_PRIVATE(socket)->pcb.tcp, (u8_t)backlog);
    if (new_pcb == NULL) {
        return MP_ENOMEM;
    }
    LWIP_SOCKET_PRIVATE(socket)->pcb.tcp = new_pcb;

    // Allocate memory for the backlog of connections
    if (backlog <= 1) {
        LWIP_SOCKET_PRIVATE(socket)->incoming.connection.alloc = 0;
        LWIP_SOCKET_PRIVATE(socket)->incoming.connection.tcp.item = NULL;
    } else {
        LWIP_SOCKET_PRIVATE(socket)->incoming.connection.alloc = backlog;
        LWIP_SOCKET_PRIVATE(socket)->incoming.connection.tcp.array = m_new0(struct tcp_pcb *, backlog);
    }
    LWIP_SOCKET_PRIVATE(socket)->incoming.connection.iget = 0;
    LWIP_SOCKET_PRIVATE(socket)->incoming.connection.iput = 0;

    tcp_accept(new_pcb, _lwip_tcp_accept);

    // Socket is no longer considered "new" for purposes of polling
    socket->state = MOD_NETWORK_SS_LISTENING;

    return 0;
}

STATIC int lwip_socket_accept(mod_network_socket_obj_t *socket, mod_network_socket_obj_t *socket2, byte *ip, mp_uint_t *port) {
    MICROPY_PY_LWIP_ENTER

    if (LWIP_SOCKET_PRIVATE(socket)->pcb.tcp == NULL) {
        MICROPY_PY_LWIP_EXIT
        return MP_EBADF;
    }

    // I need to do this because "tcp_accepted", later, is a macro.
    struct tcp_pcb *listener = LWIP_SOCKET_PRIVATE(socket)->pcb.tcp;
    if (listener->state != LISTEN) {
        MICROPY_PY_LWIP_EXIT
        return MP_EINVAL;
    }

    // accept incoming connection
    struct tcp_pcb *volatile *incoming_connection = &lwip_socket_incoming_array(socket)[LWIP_SOCKET_PRIVATE(socket)->incoming.connection.iget];
    if (*incoming_connection == NULL) {
        if (socket->timeout == 0) {
            MICROPY_PY_LWIP_EXIT
            return MP_EAGAIN;
        } else if (socket->timeout != -1) {
            mp_uint_t retries = socket->timeout / 100;
            while (*incoming_connection == NULL) {
                MICROPY_PY_LWIP_EXIT
                if (retries-- == 0) {
                    return MP_ETIMEDOUT;
                }
                mp_hal_delay_ms(100);
                MICROPY_PY_LWIP_REENTER
            }
        } else {
            while (*incoming_connection == NULL) {
                MICROPY_PY_LWIP_EXIT
                poll_sockets();
                MICROPY_PY_LWIP_REENTER
            }
        }
    }

    // We get a new pcb handle...
    LWIP_SOCKET_PRIVATE(socket2)->pcb.tcp = *incoming_connection;
    if (++LWIP_SOCKET_PRIVATE(socket)->incoming.connection.iget >= LWIP_SOCKET_PRIVATE(socket)->incoming.connection.alloc) {
        LWIP_SOCKET_PRIVATE(socket)->incoming.connection.iget = 0;
    }
    *incoming_connection = NULL;

    // ...and set up the new socket for it.
    socket2->domain = MOD_NETWORK_AF_INET;
    socket2->type = MOD_NETWORK_SOCK_STREAM;
    LWIP_SOCKET_PRIVATE(socket2)->incoming.pbuf = NULL;
    socket2->timeout = socket->timeout;
    socket2->state = MOD_NETWORK_SS_CONNECTED;
    LWIP_SOCKET_PRIVATE(socket2)->recv_offset = 0;
    socket2->callback = MP_OBJ_NULL;
    tcp_arg(LWIP_SOCKET_PRIVATE(socket2)->pcb.tcp, (void *)socket2);
    tcp_err(LWIP_SOCKET_PRIVATE(socket2)->pcb.tcp, _lwip_tcp_error);
    tcp_recv(LWIP_SOCKET_PRIVATE(socket2)->pcb.tcp, _lwip_tcp_recv);

    tcp_accepted(listener);

    MICROPY_PY_LWIP_EXIT

    memcpy(ip, &(LWIP_SOCKET_PRIVATE(socket2)->pcb.tcp->remote_ip), NETUTILS_IPV4ADDR_BUFSIZE);
    *port = (mp_uint_t)LWIP_SOCKET_PRIVATE(socket2)->pcb.tcp->remote_port;

    return 0;
}

STATIC int lwip_socket_connect(mod_network_socket_obj_t *socket, byte *ip, mp_uint_t port) {
    if (LWIP_SOCKET_PRIVATE(socket)->pcb.tcp == NULL) {
        return MP_EBADF;
    }

    ip_addr_t dest;
    IP4_ADDR(&dest, ip[0], ip[1], ip[2], ip[3]);

    err_t err = ERR_ARG;
    switch (socket->type) {
        case MOD_NETWORK_SOCK_STREAM: {
            // Register our receive callback.
            MICROPY_PY_LWIP_ENTER
            tcp_recv(LWIP_SOCKET_PRIVATE(socket)->pcb.tcp, _lwip_tcp_recv);
            socket->state = MOD_NETWORK_SS_CONNECTING;
            err = tcp_connect(LWIP_SOCKET_PRIVATE(socket)->pcb.tcp, &dest, port, _lwip_tcp_connected);
            if (err != ERR_OK) {
                MICROPY_PY_LWIP_EXIT
                socket->state = MOD_NETWORK_SS_NEW;
                return error_lookup_table[-err];
            }
            LWIP_SOCKET_PRIVATE(socket)->peer_port = (mp_uint_t)port;
            memcpy(LWIP_SOCKET_PRIVATE(socket)->peer, &dest, sizeof(LWIP_SOCKET_PRIVATE(socket)->peer));
            MICROPY_PY_LWIP_EXIT

            // And now we wait...
            if (socket->timeout != -1) {
                for (mp_uint_t retries = socket->timeout / 100; retries--;) {
                    mp_hal_delay_ms(100);
                    if (socket->state != MOD_NETWORK_SS_CONNECTING) {
                        break;
                    }
                }
                if (socket->state == MOD_NETWORK_SS_CONNECTING) {
                    mp_raise_OSError(MP_EINPROGRESS);
                }
            } else {
                while (socket->state == MOD_NETWORK_SS_CONNECTING) {
                    poll_sockets();
                }
            }
            if (socket->state == MOD_NETWORK_SS_CONNECTED) {
                err = ERR_OK;
            } else {
                err = socket->state;
            }
            break;
        }
        case MOD_NETWORK_SOCK_DGRAM: {
            err = udp_connect(LWIP_SOCKET_PRIVATE(socket)->pcb.udp, &dest, port);
            break;
        }
        #if MICROPY_PY_LWIP_SOCK_RAW
        case MOD_NETWORK_SOCK_RAW: {
            err = raw_connect(LWIP_SOCKET_PRIVATE(socket)->pcb.raw, &dest);
            break;
        }
        #endif
    }

    return err == ERR_OK ? 0 : error_lookup_table[-err];
}

STATIC int lwip_socket_check_connected(mod_network_socket_obj_t *socket) {
    if (LWIP_SOCKET_PRIVATE(socket)->pcb.tcp == NULL) {
        // not connected
        // int _errno = error_lookup_table[-socket->state];
        // socket->state = _ERR_BADF;
        // mp_raise_OSError(_errno);
        // TODO: figure out how this is supposed to work with badf

        if (socket->state < 0) {
            // TODO: change state
            return error_lookup_table[-socket->state];
        } else {
            return MP_EBADF;
        }
    }
    return 0;
}

STATIC int lwip_socket_send(mod_network_socket_obj_t *socket, const byte *buf, mp_uint_t *len) {
    int err = lwip_socket_check_connected(socket);
    if (err) {
        return err;
    }

    mp_uint_t ret = 0;
    switch (socket->type) {
        case MOD_NETWORK_SOCK_STREAM: {
            ret = lwip_tcp_send(socket, buf, *len, &err);
            break;
        }
        case MOD_NETWORK_SOCK_DGRAM:
        #if MICROPY_PY_LWIP_SOCK_RAW
        case MOD_NETWORK_SOCK_RAW:
        #endif
            ret = lwip_raw_udp_send(socket, buf, *len, NULL, 0, &err);
            break;
    }

    if (ret == -1) {
        return err;
    } else {
        *len = ret;
        return 0;
    }
}

STATIC int lwip_socket_recv(mod_network_socket_obj_t *socket, byte *buf, mp_uint_t *len) {
    int err = lwip_socket_check_connected(socket);
    if (err) {
        return err;
    }

    mp_uint_t ret = 0;
    switch (socket->type) {
        case MOD_NETWORK_SOCK_STREAM: {
            ret = lwip_tcp_receive(socket, buf, *len, &err);
            break;
        }
        case MOD_NETWORK_SOCK_DGRAM:
        #if MICROPY_PY_LWIP_SOCK_RAW
        case MOD_NETWORK_SOCK_RAW:
        #endif
            ret = lwip_raw_udp_receive(socket, buf, *len, NULL, NULL, &err);
            break;
    }

    if (ret == -1) {
        return err;
    } else {
        *len = ret;
        return 0;
    }
}

STATIC int lwip_socket_sendto(mod_network_socket_obj_t *socket, const byte *buf, mp_uint_t *len, byte *ip, mp_uint_t port) {
    int err = lwip_socket_check_connected(socket);
    if (err) {
        return err;
    }

    mp_uint_t ret = 0;
    switch (socket->type) {
        case MOD_NETWORK_SOCK_STREAM: {
            ret = lwip_tcp_send(socket, buf, *len, &err);
            break;
        }
        case MOD_NETWORK_SOCK_DGRAM:
        #if MICROPY_PY_LWIP_SOCK_RAW
        case MOD_NETWORK_SOCK_RAW:
        #endif
            ret = lwip_raw_udp_send(socket, buf, *len, ip, port, &err);
            break;
    }
    if (ret == -1) {
        return err;
    } else {
        *len = ret;
        return 0;
    }
}

STATIC int lwip_socket_recvfrom(mod_network_socket_obj_t *socket, byte *buf, mp_uint_t *len, byte *ip, mp_uint_t *port) {
    int err = lwip_socket_check_connected(socket);
    if (err) {
        return err;
    }

    mp_uint_t ret = 0;
    switch (socket->type) {
        case MOD_NETWORK_SOCK_STREAM: {
            memcpy(ip, &LWIP_SOCKET_PRIVATE(socket)->peer, sizeof(LWIP_SOCKET_PRIVATE(socket)->peer));
            *port = (mp_uint_t)LWIP_SOCKET_PRIVATE(socket)->peer_port;
            ret = lwip_tcp_receive(socket, buf, *len, &err);
            break;
        }
        case MOD_NETWORK_SOCK_DGRAM:
        #if MICROPY_PY_LWIP_SOCK_RAW
        case MOD_NETWORK_SOCK_RAW:
        #endif
            ret = lwip_raw_udp_receive(socket, buf, *len, ip, port, &err);
            break;
    }

    if (ret == -1) {
        mp_raise_OSError(err);
    } else {
        *len = ret;
        return 0;
    }
}

// TODO: sendall?

STATIC int lwip_socket_setsockopt(mod_network_socket_obj_t *socket, mp_uint_t level, mp_uint_t opt, const void *optval, mp_uint_t optlen) {
    if (opt == MOD_NETWORK_SO_CALLBACK) {
        // Note: level ignored.
        socket->callback = MP_OBJ_FROM_PTR(optval);
        return 0;
    }

    if (level == MOD_NETWORK_SOL_SOCKET) {
        if (opt == MOD_NETWORK_SO_REUSEADDR) {
            mp_int_t val = *(mp_int_t *)optval;
            // Options are common for UDP and TCP pcb's.
            if (val) {
                ip_set_option(LWIP_SOCKET_PRIVATE(socket)->pcb.tcp, SOF_REUSEADDR);
            } else {
                ip_reset_option(LWIP_SOCKET_PRIVATE(socket)->pcb.tcp, SOF_REUSEADDR);
            }
            return 0;
        }
    }

    if (level == MOD_NETWORK_IPPROTO_IP) {
        if (opt == MOD_NETWORK_IP_ADD_MEMBERSHIP || opt == MOD_NETWORK_IP_DROP_MEMBERSHIP) {
            if (optlen != sizeof(ip_addr_t) * 2) {
                return MP_EINVAL;
            }

            // POSIX setsockopt has order: group addr, if addr, lwIP has it vice-versa
            err_t err;
            if (opt == MOD_NETWORK_IP_ADD_MEMBERSHIP) {
                err = igmp_joingroup((ip_addr_t *)optval + 1, optval);
            } else {
                err = igmp_leavegroup((ip_addr_t *)optval + 1, optval);
            }
            return err == ERR_OK ? 0 : error_lookup_table[-err];
        }
    }

    return MP_EINVAL;
}

STATIC int lwip_socket_settimeout(mod_network_socket_obj_t *socket, mp_uint_t timeout_ms) {
    socket->timeout = timeout_ms;
    return 0;
}

STATIC int lwip_socket_ioctl(mod_network_socket_obj_t *socket, mp_uint_t request, mp_uint_t arg, mp_uint_t *result) {
    *result = 0;

    if (request == MP_STREAM_POLL) {
        MICROPY_PY_LWIP_ENTER

        uintptr_t flags = arg;

        if (flags & MP_STREAM_POLL_RD) {
            if (socket->state == MOD_NETWORK_SS_LISTENING) {
                // Listening TCP socket may have one or multiple connections waiting
                if (lwip_socket_incoming_array(socket)[LWIP_SOCKET_PRIVATE(socket)->incoming.connection.iget] != NULL) {
                    *result |= MP_STREAM_POLL_RD;
                }
            } else {
                // Otherwise there is just one slot for incoming data
                if (LWIP_SOCKET_PRIVATE(socket)->incoming.pbuf != NULL) {
                    *result |= MP_STREAM_POLL_RD;
                }
            }
        }

        if (flags & MP_STREAM_POLL_WR) {
            if (socket->type == MOD_NETWORK_SOCK_DGRAM && LWIP_SOCKET_PRIVATE(socket)->pcb.udp != NULL) {
                // UDP socket is writable
                *result |= MP_STREAM_POLL_WR;
            #if MICROPY_PY_LWIP_SOCK_RAW
            } else if (socket->type == MOD_NETWORK_SOCK_RAW && LWIP_SOCKET_PRIVATE(socket)->pcb.raw != NULL) {
                // raw socket is writable
                *result |= MP_STREAM_POLL_WR;
            #endif
            } else if (LWIP_SOCKET_PRIVATE(socket)->pcb.tcp != NULL && tcp_sndbuf(LWIP_SOCKET_PRIVATE(socket)->pcb.tcp) > 0) {
                // TCP socket is writable
                // Note: pcb.tcp==NULL if state<0, and in this case we can't call tcp_sndbuf
                *result |= MP_STREAM_POLL_WR;
            }
        }

        if (socket->state == MOD_NETWORK_SS_NEW) {
            // New sockets are not connected so set HUP
            *result |= MP_STREAM_POLL_HUP;
        } else if (socket->state == MOD_NETWORK_SS_PEER_CLOSED) {
            // Peer-closed socket is both readable and writable: read will
            // return EOF, write - error. Without this poll will hang on a
            // socket which was closed by peer.
            *result |= flags & (MP_STREAM_POLL_RD | MP_STREAM_POLL_WR);
        } else if (socket->state == ERR_RST) {
            // Socket was reset by peer, a write will return an error
            *result |= flags & MP_STREAM_POLL_WR;
            *result |= MP_STREAM_POLL_HUP;
        } else if (socket->state == _ERR_BADF) {
            *result |= MP_STREAM_POLL_NVAL;
        } else if (socket->state < 0) {
            // Socket in some other error state, use catch-all ERR flag
            // TODO: may need to set other return flags here
            *result |= MP_STREAM_POLL_ERR;
        }

        MICROPY_PY_LWIP_EXIT

        return 0;
    } else {
        return MP_EINVAL;
    }
}

#if !MICROPY_PY_LWIP_EXCLUSIVE
const mp_network_nic_p_t mp_network_nic_protocol_lwip = {
    .gethostbyname = lwip_gethostbyname,
    .socket = lwip_socket_socket,
    .close = lwip_socket_close,
    .bind = lwip_socket_bind,
    .listen = lwip_socket_listen,
    .accept = lwip_socket_accept,
    .connect = lwip_socket_connect,
    .send = lwip_socket_send,
    .recv = lwip_socket_recv,
    .sendto = lwip_socket_sendto,
    .recvfrom = lwip_socket_recvfrom,
    .setsockopt = lwip_socket_setsockopt,
    .settimeout = lwip_socket_settimeout,
    .ioctl = lwip_socket_ioctl,
};
#endif

mp_obj_t mod_network_nic_ifconfig(struct netif *netif, size_t n_args, const mp_obj_t *args) {
    if (n_args == 0) {
        // Get IP addresses
        const ip_addr_t *dns = dns_getserver(0);
        mp_obj_t tuple[4] = {
            netutils_format_ipv4_addr((uint8_t *)&netif->ip_addr, NETUTILS_BIG),
            netutils_format_ipv4_addr((uint8_t *)&netif->netmask, NETUTILS_BIG),
            netutils_format_ipv4_addr((uint8_t *)&netif->gw, NETUTILS_BIG),
            netutils_format_ipv4_addr((uint8_t *)dns, NETUTILS_BIG),
        };
        return mp_obj_new_tuple(4, tuple);
    } else if (args[0] == MP_OBJ_NEW_QSTR(MP_QSTR_dhcp)) {
        // Start the DHCP client
        if (dhcp_supplied_address(netif)) {
            dhcp_renew(netif);
        } else {
            dhcp_stop(netif);
            dhcp_start(netif);
        }

        // Wait for DHCP to get IP address
        uint32_t start = mp_hal_ticks_ms();
        while (!dhcp_supplied_address(netif)) {
            if (mp_hal_ticks_ms() - start > 10000) {
                mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("timeout waiting for DHCP to get IP address"));
            }
            mp_hal_delay_ms(100);
        }

        return mp_const_none;
    } else {
        // Release and stop any existing DHCP
        dhcp_release(netif);
        dhcp_stop(netif);
        // Set static IP addresses
        mp_obj_t *items;
        mp_obj_get_array_fixed_n(args[0], 4, &items);
        netutils_parse_ipv4_addr(items[0], (uint8_t *)&netif->ip_addr, NETUTILS_BIG);
        netutils_parse_ipv4_addr(items[1], (uint8_t *)&netif->netmask, NETUTILS_BIG);
        netutils_parse_ipv4_addr(items[2], (uint8_t *)&netif->gw, NETUTILS_BIG);
        ip_addr_t dns;
        netutils_parse_ipv4_addr(items[3], (uint8_t *)&dns, NETUTILS_BIG);
        dns_setserver(0, &dns);
        return mp_const_none;
    }
}

#endif // MICROPY_PY_NETWORK && MICROPY_PY_LWIP
