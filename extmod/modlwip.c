/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013-2019 Damien P. George
 * Copyright (c) 2015 Galen Hazelwood
 * Copyright (c) 2015-2017 Paul Sokolovsky
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

#include <string.h>
#include <stdio.h>

#include "py/objlist.h"
#include "py/runtime.h"
#include "py/stream.h"
#include "py/mperrno.h"
#include "py/mphal.h"

#if MICROPY_PY_LWIP

#include "shared/netutils/netutils.h"

#include "lwip/init.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "lwip/raw.h"
#include "lwip/dns.h"
#include "lwip/igmp.h"
#if LWIP_VERSION_MAJOR < 2
#include "lwip/timers.h"
#include "lwip/tcp_impl.h"
#else
#include "lwip/timeouts.h"
#include "lwip/priv/tcp_priv.h"
#endif

#if 0 // print debugging info
#define DEBUG_printf DEBUG_printf
#else // don't print debugging info
#define DEBUG_printf(...) (void)0
#endif


/*******************************************************************************/
// The socket functions provided by lwip.socket.

STATIC const mp_obj_type_t lwip_socket_type;

STATIC void lwip_socket_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    lwip_socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "<socket state=%d timeout=%d incoming=%p off=%d>", self->state, self->timeout,
        self->incoming.pbuf, self->recv_offset);
}

// FIXME: Only supports two arguments at present
STATIC mp_obj_t lwip_socket_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 4, false);

    lwip_socket_obj_t *socket = m_new_obj_with_finaliser(lwip_socket_obj_t);
    socket->base.type = &lwip_socket_type;
    socket->timeout = -1;
    socket->recv_offset = 0;
    socket->domain = MOD_NETWORK_AF_INET;
    socket->type = MOD_NETWORK_SOCK_STREAM;
    socket->callback = MP_OBJ_NULL;
    socket->state = MOD_NETWORK_SS_NEW;

    if (n_args >= 1) {
        socket->domain = mp_obj_get_int(args[0]);
        if (n_args >= 2) {
            socket->type = mp_obj_get_int(args[1]);
        }
    }

    switch (socket->type) {
        case MOD_NETWORK_SOCK_STREAM:
            socket->pcb.tcp = tcp_new();
            socket->incoming.connection.alloc = 0;
            socket->incoming.connection.tcp.item = NULL;
            break;
        case MOD_NETWORK_SOCK_DGRAM:
            socket->pcb.udp = udp_new();
            socket->incoming.pbuf = NULL;
            break;
        #if MICROPY_PY_LWIP_SOCK_RAW
        case MOD_NETWORK_SOCK_RAW: {
            mp_int_t proto = n_args <= 2 ? 0 : mp_obj_get_int(args[2]);
            socket->pcb.raw = raw_new(proto);
            break;
        }
        #endif
        default:
            mp_raise_OSError(MP_EINVAL);
    }

    if (socket->pcb.tcp == NULL) {
        mp_raise_OSError(MP_ENOMEM);
    }

    switch (socket->type) {
        case MOD_NETWORK_SOCK_STREAM: {
            // Register the socket object as our callback argument.
            tcp_arg(socket->pcb.tcp, (void *)socket);
            // Register our error callback.
            tcp_err(socket->pcb.tcp, _lwip_tcp_error);
            break;
        }
        case MOD_NETWORK_SOCK_DGRAM: {
            socket->state = MOD_NETWORK_SS_ACTIVE_UDP;
            // Register our receive callback now. Since UDP sockets don't require binding or connection
            // before use, there's no other good time to do it.
            udp_recv(socket->pcb.udp, _lwip_udp_incoming, (void *)socket);
            break;
        }
        #if MICROPY_PY_LWIP_SOCK_RAW
        case MOD_NETWORK_SOCK_RAW: {
            // Register our receive callback now. Since raw sockets don't require binding or connection
            // before use, there's no other good time to do it.
            raw_recv(socket->pcb.raw, _lwip_raw_incoming, (void *)socket);
            break;
        }
        #endif
    }

    return MP_OBJ_FROM_PTR(socket);
}

STATIC mp_obj_t lwip_socket_bind(mp_obj_t self_in, mp_obj_t addr_in) {
    lwip_socket_obj_t *socket = MP_OBJ_TO_PTR(self_in);

    uint8_t ip[NETUTILS_IPV4ADDR_BUFSIZE];
    mp_uint_t port = netutils_parse_inet_addr(addr_in, ip, NETUTILS_BIG);

    ip_addr_t bind_addr;
    IP4_ADDR(&bind_addr, ip[0], ip[1], ip[2], ip[3]);

    err_t err = ERR_ARG;
    switch (socket->type) {
        case MOD_NETWORK_SOCK_STREAM: {
            err = tcp_bind(socket->pcb.tcp, &bind_addr, port);
            break;
        }
        case MOD_NETWORK_SOCK_DGRAM: {
            err = udp_bind(socket->pcb.udp, &bind_addr, port);
            break;
        }
    }

    if (err != ERR_OK) {
        mp_raise_OSError(error_lookup_table[-err]);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lwip_socket_bind_obj, lwip_socket_bind);

STATIC mp_obj_t lwip_socket_listen(size_t n_args, const mp_obj_t *args) {
    lwip_socket_obj_t *socket = MP_OBJ_TO_PTR(args[0]);

    mp_int_t backlog = MICROPY_PY_USOCKET_LISTEN_BACKLOG_DEFAULT;
    if (n_args > 1) {
        backlog = mp_obj_get_int(args[1]);
        backlog = (backlog < 0) ? 0 : backlog;
    }

    if (socket->pcb.tcp == NULL) {
        mp_raise_OSError(MP_EBADF);
    }
    if (socket->type != MOD_NETWORK_SOCK_STREAM) {
        mp_raise_OSError(MP_EOPNOTSUPP);
    }

    struct tcp_pcb *new_pcb = tcp_listen_with_backlog(socket->pcb.tcp, (u8_t)backlog);
    if (new_pcb == NULL) {
        mp_raise_OSError(MP_ENOMEM);
    }
    socket->pcb.tcp = new_pcb;

    // Allocate memory for the backlog of connections
    if (backlog <= 1) {
        socket->incoming.connection.alloc = 0;
        socket->incoming.connection.tcp.item = NULL;
    } else {
        socket->incoming.connection.alloc = backlog;
        socket->incoming.connection.tcp.array = m_new0(struct tcp_pcb *, backlog);
    }
    socket->incoming.connection.iget = 0;
    socket->incoming.connection.iput = 0;

    tcp_accept(new_pcb, _lwip_tcp_accept);

    // Socket is no longer considered "new" for purposes of polling
    socket->state = MOD_NETWORK_SS_LISTENING;

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lwip_socket_listen_obj, 1, 2, lwip_socket_listen);

STATIC mp_obj_t lwip_socket_accept(mp_obj_t self_in) {
    lwip_socket_obj_t *socket = MP_OBJ_TO_PTR(self_in);

    if (socket->type != MOD_NETWORK_SOCK_STREAM) {
        mp_raise_OSError(MP_EOPNOTSUPP);
    }

    // Create new socket object, do it here because we must not raise an out-of-memory
    // exception when the LWIP concurrency lock is held
    lwip_socket_obj_t *socket2 = m_new_obj_with_finaliser(lwip_socket_obj_t);
    socket2->base.type = &lwip_socket_type;

    MICROPY_PY_LWIP_ENTER

    if (socket->pcb.tcp == NULL) {
        MICROPY_PY_LWIP_EXIT
        m_del_obj(lwip_socket_obj_t, socket2);
        mp_raise_OSError(MP_EBADF);
    }

    // I need to do this because "tcp_accepted", later, is a macro.
    struct tcp_pcb *listener = socket->pcb.tcp;
    if (listener->state != LISTEN) {
        MICROPY_PY_LWIP_EXIT
        m_del_obj(lwip_socket_obj_t, socket2);
        mp_raise_OSError(MP_EINVAL);
    }

    // accept incoming connection
    struct tcp_pcb *volatile *incoming_connection = &lwip_socket_incoming_array(socket)[socket->incoming.connection.iget];
    if (*incoming_connection == NULL) {
        if (socket->timeout == 0) {
            MICROPY_PY_LWIP_EXIT
            m_del_obj(lwip_socket_obj_t, socket2);
            mp_raise_OSError(MP_EAGAIN);
        } else if (socket->timeout != -1) {
            mp_uint_t retries = socket->timeout / 100;
            while (*incoming_connection == NULL) {
                MICROPY_PY_LWIP_EXIT
                if (retries-- == 0) {
                    m_del_obj(lwip_socket_obj_t, socket2);
                    mp_raise_OSError(MP_ETIMEDOUT);
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
    socket2->pcb.tcp = *incoming_connection;
    if (++socket->incoming.connection.iget >= socket->incoming.connection.alloc) {
        socket->incoming.connection.iget = 0;
    }
    *incoming_connection = NULL;

    // ...and set up the new socket for it.
    socket2->domain = MOD_NETWORK_AF_INET;
    socket2->type = MOD_NETWORK_SOCK_STREAM;
    socket2->incoming.pbuf = NULL;
    socket2->timeout = socket->timeout;
    socket2->state = MOD_NETWORK_SS_CONNECTED;
    socket2->recv_offset = 0;
    socket2->callback = MP_OBJ_NULL;
    tcp_arg(socket2->pcb.tcp, (void *)socket2);
    tcp_err(socket2->pcb.tcp, _lwip_tcp_error);
    tcp_recv(socket2->pcb.tcp, _lwip_tcp_recv);

    tcp_accepted(listener);

    MICROPY_PY_LWIP_EXIT

    // make the return value
    uint8_t ip[NETUTILS_IPV4ADDR_BUFSIZE];
    memcpy(ip, &(socket2->pcb.tcp->remote_ip), sizeof(ip));
    mp_uint_t port = (mp_uint_t)socket2->pcb.tcp->remote_port;
    mp_obj_tuple_t *client = MP_OBJ_TO_PTR(mp_obj_new_tuple(2, NULL));
    client->items[0] = MP_OBJ_FROM_PTR(socket2);
    client->items[1] = netutils_format_inet_addr(ip, port, NETUTILS_BIG);

    return MP_OBJ_FROM_PTR(client);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lwip_socket_accept_obj, lwip_socket_accept);

STATIC mp_obj_t lwip_socket_connect(mp_obj_t self_in, mp_obj_t addr_in) {
    lwip_socket_obj_t *socket = MP_OBJ_TO_PTR(self_in);

    if (socket->pcb.tcp == NULL) {
        mp_raise_OSError(MP_EBADF);
    }

    // get address
    uint8_t ip[NETUTILS_IPV4ADDR_BUFSIZE];
    mp_uint_t port = netutils_parse_inet_addr(addr_in, ip, NETUTILS_BIG);

    ip_addr_t dest;
    IP4_ADDR(&dest, ip[0], ip[1], ip[2], ip[3]);

    err_t err = ERR_ARG;
    switch (socket->type) {
        case MOD_NETWORK_SOCK_STREAM: {
            if (socket->state != MOD_NETWORK_SS_NEW) {
                if (socket->state == MOD_NETWORK_SS_CONNECTED) {
                    mp_raise_OSError(MP_EISCONN);
                } else {
                    mp_raise_OSError(MP_EALREADY);
                }
            }

            // Register our receive callback.
            MICROPY_PY_LWIP_ENTER
            tcp_recv(socket->pcb.tcp, _lwip_tcp_recv);
            socket->state = MOD_NETWORK_SS_CONNECTING;
            err = tcp_connect(socket->pcb.tcp, &dest, port, _lwip_tcp_connected);
            if (err != ERR_OK) {
                MICROPY_PY_LWIP_EXIT
                socket->state = MOD_NETWORK_SS_NEW;
                mp_raise_OSError(error_lookup_table[-err]);
            }
            socket->peer_port = (mp_uint_t)port;
            memcpy(socket->peer, &dest, sizeof(socket->peer));
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
            err = udp_connect(socket->pcb.udp, &dest, port);
            break;
        }
        #if MICROPY_PY_LWIP_SOCK_RAW
        case MOD_NETWORK_SOCK_RAW: {
            err = raw_connect(socket->pcb.raw, &dest);
            break;
        }
        #endif
    }

    if (err != ERR_OK) {
        mp_raise_OSError(error_lookup_table[-err]);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lwip_socket_connect_obj, lwip_socket_connect);

STATIC void lwip_socket_check_connected(lwip_socket_obj_t *socket) {
    if (socket->pcb.tcp == NULL) {
        // not connected
        int _errno = error_lookup_table[-socket->state];
        socket->state = _ERR_BADF;
        mp_raise_OSError(_errno);
    }
}

STATIC mp_obj_t lwip_socket_send(mp_obj_t self_in, mp_obj_t buf_in) {
    lwip_socket_obj_t *socket = MP_OBJ_TO_PTR(self_in);
    int _errno;

    lwip_socket_check_connected(socket);

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf_in, &bufinfo, MP_BUFFER_READ);

    mp_uint_t ret = 0;
    switch (socket->type) {
        case MOD_NETWORK_SOCK_STREAM: {
            ret = lwip_tcp_send(socket, bufinfo.buf, bufinfo.len, &_errno);
            break;
        }
        case MOD_NETWORK_SOCK_DGRAM:
        #if MICROPY_PY_LWIP_SOCK_RAW
        case MOD_NETWORK_SOCK_RAW:
        #endif
            ret = lwip_raw_udp_send(socket, bufinfo.buf, bufinfo.len, NULL, 0, &_errno);
            break;
    }
    if (ret == -1) {
        mp_raise_OSError(_errno);
    }

    return mp_obj_new_int_from_uint(ret);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lwip_socket_send_obj, lwip_socket_send);

STATIC mp_obj_t lwip_socket_recv(mp_obj_t self_in, mp_obj_t len_in) {
    lwip_socket_obj_t *socket = MP_OBJ_TO_PTR(self_in);
    int _errno;

    lwip_socket_check_connected(socket);

    mp_int_t len = mp_obj_get_int(len_in);
    vstr_t vstr;
    vstr_init_len(&vstr, len);

    mp_uint_t ret = 0;
    switch (socket->type) {
        case MOD_NETWORK_SOCK_STREAM: {
            ret = lwip_tcp_receive(socket, (byte *)vstr.buf, len, &_errno);
            break;
        }
        case MOD_NETWORK_SOCK_DGRAM:
        #if MICROPY_PY_LWIP_SOCK_RAW
        case MOD_NETWORK_SOCK_RAW:
        #endif
            ret = lwip_raw_udp_receive(socket, (byte *)vstr.buf, len, NULL, NULL, &_errno);
            break;
    }
    if (ret == -1) {
        mp_raise_OSError(_errno);
    }

    if (ret == 0) {
        return mp_const_empty_bytes;
    }
    vstr.len = ret;
    return mp_obj_new_bytes_from_vstr(&vstr);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lwip_socket_recv_obj, lwip_socket_recv);

STATIC mp_obj_t lwip_socket_sendto(mp_obj_t self_in, mp_obj_t data_in, mp_obj_t addr_in) {
    lwip_socket_obj_t *socket = MP_OBJ_TO_PTR(self_in);
    int _errno;

    lwip_socket_check_connected(socket);

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(data_in, &bufinfo, MP_BUFFER_READ);

    uint8_t ip[NETUTILS_IPV4ADDR_BUFSIZE];
    mp_uint_t port = netutils_parse_inet_addr(addr_in, ip, NETUTILS_BIG);

    mp_uint_t ret = 0;
    switch (socket->type) {
        case MOD_NETWORK_SOCK_STREAM: {
            ret = lwip_tcp_send(socket, bufinfo.buf, bufinfo.len, &_errno);
            break;
        }
        case MOD_NETWORK_SOCK_DGRAM:
        #if MICROPY_PY_LWIP_SOCK_RAW
        case MOD_NETWORK_SOCK_RAW:
        #endif
            ret = lwip_raw_udp_send(socket, bufinfo.buf, bufinfo.len, ip, port, &_errno);
            break;
    }
    if (ret == -1) {
        mp_raise_OSError(_errno);
    }

    return mp_obj_new_int_from_uint(ret);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lwip_socket_sendto_obj, lwip_socket_sendto);

STATIC mp_obj_t lwip_socket_recvfrom(mp_obj_t self_in, mp_obj_t len_in) {
    lwip_socket_obj_t *socket = MP_OBJ_TO_PTR(self_in);
    int _errno;

    lwip_socket_check_connected(socket);

    mp_int_t len = mp_obj_get_int(len_in);
    vstr_t vstr;
    vstr_init_len(&vstr, len);
    byte ip[4];
    mp_uint_t port;

    mp_uint_t ret = 0;
    switch (socket->type) {
        case MOD_NETWORK_SOCK_STREAM: {
            memcpy(ip, &socket->peer, sizeof(socket->peer));
            port = (mp_uint_t)socket->peer_port;
            ret = lwip_tcp_receive(socket, (byte *)vstr.buf, len, &_errno);
            break;
        }
        case MOD_NETWORK_SOCK_DGRAM:
        #if MICROPY_PY_LWIP_SOCK_RAW
        case MOD_NETWORK_SOCK_RAW:
        #endif
            ret = lwip_raw_udp_receive(socket, (byte *)vstr.buf, len, ip, &port, &_errno);
            break;
    }
    if (ret == -1) {
        mp_raise_OSError(_errno);
    }

    mp_obj_t tuple[2];
    if (ret == 0) {
        tuple[0] = mp_const_empty_bytes;
    } else {
        vstr.len = ret;
        tuple[0] = mp_obj_new_bytes_from_vstr(&vstr);
    }
    tuple[1] = netutils_format_inet_addr(ip, port, NETUTILS_BIG);
    return mp_obj_new_tuple(2, tuple);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lwip_socket_recvfrom_obj, lwip_socket_recvfrom);

STATIC mp_obj_t lwip_socket_sendall(mp_obj_t self_in, mp_obj_t buf_in) {
    lwip_socket_obj_t *socket = MP_OBJ_TO_PTR(self_in);
    lwip_socket_check_connected(socket);

    int _errno;
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf_in, &bufinfo, MP_BUFFER_READ);

    mp_uint_t ret = 0;
    switch (socket->type) {
        case MOD_NETWORK_SOCK_STREAM: {
            if (socket->timeout == 0) {
                // Behavior of sendall() for non-blocking sockets isn't explicitly specified.
                // But it's specified that "On error, an exception is raised, there is no
                // way to determine how much data, if any, was successfully sent." Then, the
                // most useful behavior is: check whether we will be able to send all of input
                // data without EAGAIN, and if won't be, raise it without sending any.
                if (bufinfo.len > tcp_sndbuf(socket->pcb.tcp)) {
                    mp_raise_OSError(MP_EAGAIN);
                }
            }
            // TODO: In CPython3.5, socket timeout should apply to the
            // entire sendall() operation, not to individual send() chunks.
            while (bufinfo.len != 0) {
                ret = lwip_tcp_send(socket, bufinfo.buf, bufinfo.len, &_errno);
                if (ret == -1) {
                    mp_raise_OSError(_errno);
                }
                bufinfo.len -= ret;
                bufinfo.buf = (char *)bufinfo.buf + ret;
            }
            break;
        }
        case MOD_NETWORK_SOCK_DGRAM:
            mp_raise_NotImplementedError(NULL);
            break;
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lwip_socket_sendall_obj, lwip_socket_sendall);

STATIC mp_obj_t lwip_socket_settimeout(mp_obj_t self_in, mp_obj_t timeout_in) {
    lwip_socket_obj_t *socket = MP_OBJ_TO_PTR(self_in);
    mp_uint_t timeout;
    if (timeout_in == mp_const_none) {
        timeout = -1;
    } else {
        #if MICROPY_PY_BUILTINS_FLOAT
        timeout = (mp_uint_t)(MICROPY_FLOAT_CONST(1000.0) * mp_obj_get_float(timeout_in));
        #else
        timeout = 1000 * mp_obj_get_int(timeout_in);
        #endif
    }
    socket->timeout = timeout;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lwip_socket_settimeout_obj, lwip_socket_settimeout);

STATIC mp_obj_t lwip_socket_setblocking(mp_obj_t self_in, mp_obj_t flag_in) {
    lwip_socket_obj_t *socket = MP_OBJ_TO_PTR(self_in);
    bool val = mp_obj_is_true(flag_in);
    if (val) {
        socket->timeout = -1;
    } else {
        socket->timeout = 0;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lwip_socket_setblocking_obj, lwip_socket_setblocking);

STATIC mp_obj_t lwip_socket_setsockopt(size_t n_args, const mp_obj_t *args) {
    (void)n_args; // always 4
    lwip_socket_obj_t *socket = MP_OBJ_TO_PTR(args[0]);

    int opt = mp_obj_get_int(args[2]);
    if (opt == 20) {
        if (args[3] == mp_const_none) {
            socket->callback = MP_OBJ_NULL;
        } else {
            socket->callback = args[3];
        }
        return mp_const_none;
    }

    switch (opt) {
        // level: SOL_SOCKET
        case SOF_REUSEADDR: {
            mp_int_t val = mp_obj_get_int(args[3]);
            // Options are common for UDP and TCP pcb's.
            if (val) {
                ip_set_option(socket->pcb.tcp, SOF_REUSEADDR);
            } else {
                ip_reset_option(socket->pcb.tcp, SOF_REUSEADDR);
            }
            break;
        }

        // level: IPPROTO_IP
        case IP_ADD_MEMBERSHIP:
        case IP_DROP_MEMBERSHIP: {
            mp_buffer_info_t bufinfo;
            mp_get_buffer_raise(args[3], &bufinfo, MP_BUFFER_READ);
            if (bufinfo.len != sizeof(ip_addr_t) * 2) {
                mp_raise_ValueError(NULL);
            }

            // POSIX setsockopt has order: group addr, if addr, lwIP has it vice-versa
            err_t err;
            if (opt == IP_ADD_MEMBERSHIP) {
                err = igmp_joingroup((ip_addr_t *)bufinfo.buf + 1, bufinfo.buf);
            } else {
                err = igmp_leavegroup((ip_addr_t *)bufinfo.buf + 1, bufinfo.buf);
            }
            if (err != ERR_OK) {
                mp_raise_OSError(error_lookup_table[-err]);
            }
            break;
        }

        default:
            printf("Warning: lwip.setsockopt() not implemented\n");
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lwip_socket_setsockopt_obj, 4, 4, lwip_socket_setsockopt);

STATIC mp_obj_t lwip_socket_makefile(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    return args[0];
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lwip_socket_makefile_obj, 1, 3, lwip_socket_makefile);

STATIC mp_uint_t lwip_socket_read(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {
    lwip_socket_obj_t *socket = MP_OBJ_TO_PTR(self_in);

    switch (socket->type) {
        case MOD_NETWORK_SOCK_STREAM:
            return lwip_tcp_receive(socket, buf, size, errcode);
        case MOD_NETWORK_SOCK_DGRAM:
        #if MICROPY_PY_LWIP_SOCK_RAW
        case MOD_NETWORK_SOCK_RAW:
        #endif
            return lwip_raw_udp_receive(socket, buf, size, NULL, NULL, errcode);
    }
    // Unreachable
    return MP_STREAM_ERROR;
}

STATIC mp_uint_t lwip_socket_write(mp_obj_t self_in, const void *buf, mp_uint_t size, int *errcode) {
    lwip_socket_obj_t *socket = MP_OBJ_TO_PTR(self_in);

    switch (socket->type) {
        case MOD_NETWORK_SOCK_STREAM:
            return lwip_tcp_send(socket, buf, size, errcode);
        case MOD_NETWORK_SOCK_DGRAM:
        #if MICROPY_PY_LWIP_SOCK_RAW
        case MOD_NETWORK_SOCK_RAW:
        #endif
            return lwip_raw_udp_send(socket, buf, size, NULL, 0, errcode);
    }
    // Unreachable
    return MP_STREAM_ERROR;
}

STATIC err_t _lwip_tcp_close_poll(void *arg, struct tcp_pcb *pcb) {
    // Connection has not been cleanly closed so just abort it to free up memory
    tcp_poll(pcb, NULL, 0);
    tcp_abort(pcb);
    return ERR_OK;
}

STATIC mp_uint_t lwip_socket_ioctl(mp_obj_t self_in, mp_uint_t request, uintptr_t arg, int *errcode) {
    lwip_socket_obj_t *socket = MP_OBJ_TO_PTR(self_in);
    mp_uint_t ret;

    MICROPY_PY_LWIP_ENTER

    if (request == MP_STREAM_POLL) {
        uintptr_t flags = arg;
        ret = 0;

        if (flags & MP_STREAM_POLL_RD) {
            if (socket->state == MOD_NETWORK_SS_LISTENING) {
                // Listening TCP socket may have one or multiple connections waiting
                if (lwip_socket_incoming_array(socket)[socket->incoming.connection.iget] != NULL) {
                    ret |= MP_STREAM_POLL_RD;
                }
            } else {
                // Otherwise there is just one slot for incoming data
                if (socket->incoming.pbuf != NULL) {
                    ret |= MP_STREAM_POLL_RD;
                }
            }
        }

        if (flags & MP_STREAM_POLL_WR) {
            if (socket->type == MOD_NETWORK_SOCK_DGRAM && socket->pcb.udp != NULL) {
                // UDP socket is writable
                ret |= MP_STREAM_POLL_WR;
            #if MICROPY_PY_LWIP_SOCK_RAW
            } else if (socket->type == MOD_NETWORK_SOCK_RAW && socket->pcb.raw != NULL) {
                // raw socket is writable
                ret |= MP_STREAM_POLL_WR;
            #endif
            } else if (socket->pcb.tcp != NULL && tcp_sndbuf(socket->pcb.tcp) > 0) {
                // TCP socket is writable
                // Note: pcb.tcp==NULL if state<0, and in this case we can't call tcp_sndbuf
                ret |= MP_STREAM_POLL_WR;
            }
        }

        if (socket->state == MOD_NETWORK_SS_NEW) {
            // New sockets are not connected so set HUP
            ret |= MP_STREAM_POLL_HUP;
        } else if (socket->state == MOD_NETWORK_SS_PEER_CLOSED) {
            // Peer-closed socket is both readable and writable: read will
            // return EOF, write - error. Without this poll will hang on a
            // socket which was closed by peer.
            ret |= flags & (MP_STREAM_POLL_RD | MP_STREAM_POLL_WR);
        } else if (socket->state == ERR_RST) {
            // Socket was reset by peer, a write will return an error
            ret |= flags & MP_STREAM_POLL_WR;
            ret |= MP_STREAM_POLL_HUP;
        } else if (socket->state == _ERR_BADF) {
            ret |= MP_STREAM_POLL_NVAL;
        } else if (socket->state < 0) {
            // Socket in some other error state, use catch-all ERR flag
            // TODO: may need to set other return flags here
            ret |= MP_STREAM_POLL_ERR;
        }

    } else if (request == MP_STREAM_CLOSE) {
        if (socket->pcb.tcp == NULL) {
            MICROPY_PY_LWIP_EXIT
            return 0;
        }

        // Free any incoming buffers or connections that are stored
        lwip_socket_free_incoming(socket);

        switch (socket->type) {
            case MOD_NETWORK_SOCK_STREAM: {
                // Deregister callback (pcb.tcp is set to NULL below so must deregister now)
                tcp_arg(socket->pcb.tcp, NULL);
                tcp_err(socket->pcb.tcp, NULL);
                tcp_recv(socket->pcb.tcp, NULL);

                if (socket->pcb.tcp->state != LISTEN) {
                    // Schedule a callback to abort the connection if it's not cleanly closed after
                    // the given timeout.  The callback must be set before calling tcp_close since
                    // the latter may free the pcb; if it doesn't then the callback will be active.
                    tcp_poll(socket->pcb.tcp, _lwip_tcp_close_poll, MICROPY_PY_LWIP_TCP_CLOSE_TIMEOUT_MS / 500);
                }
                if (tcp_close(socket->pcb.tcp) != ERR_OK) {
                    DEBUG_printf("lwip_close: had to call tcp_abort()\n");
                    tcp_abort(socket->pcb.tcp);
                }
                break;
            }
            case MOD_NETWORK_SOCK_DGRAM:
                udp_recv(socket->pcb.udp, NULL, NULL);
                udp_remove(socket->pcb.udp);
                break;
            #if MICROPY_PY_LWIP_SOCK_RAW
            case MOD_NETWORK_SOCK_RAW:
                raw_recv(socket->pcb.raw, NULL, NULL);
                raw_remove(socket->pcb.raw);
                break;
            #endif
        }

        socket->pcb.tcp = NULL;
        socket->state = _ERR_BADF;
        ret = 0;

    } else {
        *errcode = MP_EINVAL;
        ret = MP_STREAM_ERROR;
    }

    MICROPY_PY_LWIP_EXIT

    return ret;
}

STATIC const mp_rom_map_elem_t lwip_socket_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&mp_stream_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&mp_stream_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_bind), MP_ROM_PTR(&lwip_socket_bind_obj) },
    { MP_ROM_QSTR(MP_QSTR_listen), MP_ROM_PTR(&lwip_socket_listen_obj) },
    { MP_ROM_QSTR(MP_QSTR_accept), MP_ROM_PTR(&lwip_socket_accept_obj) },
    { MP_ROM_QSTR(MP_QSTR_connect), MP_ROM_PTR(&lwip_socket_connect_obj) },
    { MP_ROM_QSTR(MP_QSTR_send), MP_ROM_PTR(&lwip_socket_send_obj) },
    { MP_ROM_QSTR(MP_QSTR_recv), MP_ROM_PTR(&lwip_socket_recv_obj) },
    { MP_ROM_QSTR(MP_QSTR_sendto), MP_ROM_PTR(&lwip_socket_sendto_obj) },
    { MP_ROM_QSTR(MP_QSTR_recvfrom), MP_ROM_PTR(&lwip_socket_recvfrom_obj) },
    { MP_ROM_QSTR(MP_QSTR_sendall), MP_ROM_PTR(&lwip_socket_sendall_obj) },
    { MP_ROM_QSTR(MP_QSTR_settimeout), MP_ROM_PTR(&lwip_socket_settimeout_obj) },
    { MP_ROM_QSTR(MP_QSTR_setblocking), MP_ROM_PTR(&lwip_socket_setblocking_obj) },
    { MP_ROM_QSTR(MP_QSTR_setsockopt), MP_ROM_PTR(&lwip_socket_setsockopt_obj) },
    { MP_ROM_QSTR(MP_QSTR_makefile), MP_ROM_PTR(&lwip_socket_makefile_obj) },

    { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&mp_stream_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto), MP_ROM_PTR(&mp_stream_readinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_readline), MP_ROM_PTR(&mp_stream_unbuffered_readline_obj) },
    { MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&mp_stream_write_obj) },
};
STATIC MP_DEFINE_CONST_DICT(lwip_socket_locals_dict, lwip_socket_locals_dict_table);

STATIC const mp_stream_p_t lwip_socket_stream_p = {
    .read = lwip_socket_read,
    .write = lwip_socket_write,
    .ioctl = lwip_socket_ioctl,
};

STATIC MP_DEFINE_CONST_OBJ_TYPE(
    lwip_socket_type,
    MP_QSTR_socket,
    MP_TYPE_FLAG_NONE,
    make_new, lwip_socket_make_new,
    print, lwip_socket_print,
    protocol, &lwip_socket_stream_p,
    locals_dict, &lwip_socket_locals_dict
    );

/******************************************************************************/
// Support functions for memory protection. lwIP has its own memory management
// routines for its internal structures, and since they might be called in
// interrupt handlers, they need some protection.
sys_prot_t sys_arch_protect() {
    return (sys_prot_t)MICROPY_BEGIN_ATOMIC_SECTION();
}

void sys_arch_unprotect(sys_prot_t state) {
    MICROPY_END_ATOMIC_SECTION((mp_uint_t)state);
}

/******************************************************************************/
// Polling callbacks for the interfaces connected to lwIP. Right now it calls
// itself a "list" but isn't; we only support a single interface.

typedef struct nic_poll {
    void (*poll)(void *arg);
    void *poll_arg;
} nic_poll_t;

STATIC nic_poll_t lwip_poll_list;

void mod_lwip_register_poll(void (*poll)(void *arg), void *poll_arg) {
    lwip_poll_list.poll = poll;
    lwip_poll_list.poll_arg = poll_arg;
}

void mod_lwip_deregister_poll(void (*poll)(void *arg), void *poll_arg) {
    lwip_poll_list.poll = NULL;
}


// LWIP global functions.

STATIC mp_obj_t mod_lwip_reset() {
    lwip_init();
    lwip_poll_list.poll = NULL;
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(mod_lwip_reset_obj, mod_lwip_reset);

STATIC mp_obj_t mod_lwip_callback() {
    if (lwip_poll_list.poll != NULL) {
        lwip_poll_list.poll(lwip_poll_list.poll_arg);
    }
    sys_check_timeouts();
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(mod_lwip_callback_obj, mod_lwip_callback);

// Debug functions

STATIC mp_obj_t lwip_print_pcbs() {
    tcp_debug_print_pcbs();
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(lwip_print_pcbs_obj, lwip_print_pcbs);

#endif // MICROPY_PY_LWIP
