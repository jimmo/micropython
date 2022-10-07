/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Damien P. George
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

#if MICROPY_PY_NETWORK && MICROPY_PY_USOCKET

#include <stdio.h>
#include <string.h>

#include "py/objtuple.h"
#include "py/objlist.h"
#include "py/stream.h"
#include "py/mperrno.h"
#include "shared/netutils/netutils.h"
#include "modnetwork.h"

#if MICROPY_PY_LWIP_EXCLUSIVE
#define MICROPY_PY_LWIP_EXCLUSIVE_ENABLE_INCLUDE (1)
#include "extmod/network_lwip.c"
#undef MICROPY_PY_LWIP_EXCLUSIVE_ENABLE_INCLUDE

#define SOCKET_NIC_PROTOCOL(_socket, method) lwip_socket_##method
#define SOCKET_HAS_NIC(_socket) true
#else
#define SOCKET_NIC_PROTOCOL(socket, method) socket->protocol->method
#define SOCKET_HAS_NIC(socket) (socket->nic != MP_OBJ_NULL)
#endif

#if MICROPY_PY_LWIP && MICROPY_PY_LWIP_SLIP
#include "extmod/network_lwip_slip.h"
#endif


STATIC const mp_obj_type_t socket_type;

STATIC void socket_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    mod_network_socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "<socket fd=%d timeout=%d domain=%d type=%d proto=%d bound=%b>",
        self->fileno, self->timeout, self->domain, self->type, self->proto, self->bound);
}

// constructor socket(domain=AF_INET, type=SOCK_STREAM, proto=0)
STATIC mp_obj_t socket_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 3, false);

    // create socket object (not bound to any NIC yet)
    mod_network_socket_obj_t *s = m_new_obj_with_finaliser(mod_network_socket_obj_t);
    s->base.type = &socket_type;
    #if !MICROPY_PY_LWIP_EXCLUSIVE
    s->nic = MP_OBJ_NULL;
    s->protocol = NULL;
    #endif
    s->domain = MOD_NETWORK_AF_INET;
    s->type = MOD_NETWORK_SOCK_STREAM;
    s->proto = 0;
    s->bound = false;
    s->fileno = -1;
    if (n_args > 0) {
        s->domain = mp_obj_get_int(args[0]);
        if (s->domain != MOD_NETWORK_AF_INET && s->domain != MOD_NETWORK_AF_INET6) {
            mp_raise_OSError(MP_EINVAL);
        }
        if (n_args > 1) {
            s->type = mp_obj_get_int(args[1]);
            if (s->type != MOD_NETWORK_SOCK_STREAM && s->type != MOD_NETWORK_SOCK_DGRAM
                #if MICROPY_PY_USOCKET_RAW
                && s->type != MOD_NETWORK_SOCK_RAW
                #endif
                ) {
                mp_raise_OSError(MP_EINVAL);
            }
            if (n_args > 2) {
                s->proto = mp_obj_get_int(args[2]);
                // TODO: verify proto
            }
        }
    }
    s->timeout = -1;
    s->callback = MP_OBJ_NULL;
    s->state = MOD_NETWORK_SS_NEW;
    #if MICROPY_PY_USOCKET_EXTENDED_STATE
    s->_private = NULL;
    #endif

    #if MICROPY_PY_LWIP_EXCLUSIVE
    int ret = SOCKET_NIC_PROTOCOL(s, socket)(s);
    if (ret) {
        mp_raise_OSError(ret);
    }
    #endif

    return MP_OBJ_FROM_PTR(s);
}

STATIC void socket_select_nic(mod_network_socket_obj_t *self, const byte *ip) {
    #if !MICROPY_PY_LWIP_EXCLUSIVE
    if (!SOCKET_HAS_NIC(self)) {
        // select NIC based on IP
        self->nic = mod_network_find_nic(ip);
        self->protocol = MP_OBJ_TYPE_GET_SLOT(mp_obj_get_type(self->nic), protocol);

        // call the NIC to open the socket
        int ret = SOCKET_NIC_PROTOCOL(self, socket)(self);
        if (ret) {
            mp_raise_OSError(ret);
        }

        #if MICROPY_PY_USOCKET_EXTENDED_STATE
        // if a timeout was set before binding a NIC, call settimeout to reset it
        if (self->timeout != -1) {
            ret = SOCKET_NIC_PROTOCOL(self, settimeout)(self, self->timeout);
            if (ret) {
                mp_raise_OSError(ret);
            }
        }
        #endif
    }
    #endif
}

// method socket.bind(address)
STATIC mp_obj_t socket_bind(mp_obj_t self_in, mp_obj_t addr_in) {
    mod_network_socket_obj_t *self = MP_OBJ_TO_PTR(self_in);

    // get address
    uint8_t ip[MOD_NETWORK_IPADDR_BUF_SIZE];
    mp_uint_t port = netutils_parse_inet_addr(addr_in, ip, NETUTILS_BIG);

    // check if we need to select a NIC
    socket_select_nic(self, ip);

    // call the NIC to bind the socket
    int ret = SOCKET_NIC_PROTOCOL(self, bind)(self, ip, port);
    if (ret) {
        mp_raise_OSError(ret);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_bind_obj, socket_bind);

// method socket.listen([backlog])
STATIC mp_obj_t socket_listen(size_t n_args, const mp_obj_t *args) {
    mod_network_socket_obj_t *self = MP_OBJ_TO_PTR(args[0]);

    if (!SOCKET_HAS_NIC(self)) {
        // not connected
        // TODO I think we can listen even if not bound...
        mp_raise_OSError(MP_ENOTCONN);
    }

    if (self->type != MOD_NETWORK_SOCK_STREAM) {
        mp_raise_OSError(MP_EOPNOTSUPP);
    }

    mp_int_t backlog = MICROPY_PY_USOCKET_LISTEN_BACKLOG_DEFAULT;
    if (n_args > 1) {
        backlog = mp_obj_get_int(args[1]);
        backlog = (backlog < 0) ? 0 : backlog;
    }

    int ret = SOCKET_NIC_PROTOCOL(self, listen)(self, backlog);
    if (ret) {
        mp_raise_OSError(ret);
    }

    // set socket state
    self->state = MOD_NETWORK_SS_LISTENING;

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_listen_obj, 1, 2, socket_listen);

// method socket.accept()
STATIC mp_obj_t socket_accept(mp_obj_t self_in) {
    mod_network_socket_obj_t *self = MP_OBJ_TO_PTR(self_in);

    if (!SOCKET_HAS_NIC(self)) {
        // not bound
        mp_raise_OSError(MP_EINVAL);
    }

    if (self->type != MOD_NETWORK_SOCK_STREAM) {
        mp_raise_OSError(MP_EOPNOTSUPP);
    }

    // create new socket object
    // starts with empty NIC so that finaliser doesn't run close() method if accept() fails
    mod_network_socket_obj_t *socket2 = m_new_obj_with_finaliser(mod_network_socket_obj_t);
    socket2->base.type = &socket_type;

    // set the same address family, socket type and protocol as parent
    socket2->domain = self->domain;
    socket2->type = self->type;
    socket2->proto = self->proto;
    socket2->bound = false;
    socket2->fileno = -1;
    socket2->timeout = -1;
    socket2->callback = MP_OBJ_NULL;
    socket2->state = MOD_NETWORK_SS_NEW;
    #if MICROPY_PY_USOCKET_EXTENDED_STATE
    socket2->_private = NULL;
    #endif

    // accept incoming connection
    uint8_t ip[MOD_NETWORK_IPADDR_BUF_SIZE];
    mp_uint_t port;
    int ret = SOCKET_NIC_PROTOCOL(self, accept)(self, socket2, ip, &port);
    if (ret) {
        m_del_obj(mod_network_socket_obj_t, socket2);
        mp_raise_OSError(ret);
    }

    // new socket has valid state, so set the NIC to the same as parent
    #if !MICROPY_PY_LWIP_EXCLUSIVE
    socket2->nic = self->nic;
    socket2->protocol = self->protocol;
    #endif

    // make the return value
    mp_obj_tuple_t *client = MP_OBJ_TO_PTR(mp_obj_new_tuple(2, NULL));
    client->items[0] = MP_OBJ_FROM_PTR(socket2);
    client->items[1] = netutils_format_inet_addr(ip, port, NETUTILS_BIG);

    return MP_OBJ_FROM_PTR(client);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(socket_accept_obj, socket_accept);

// method socket.connect(address)
STATIC mp_obj_t socket_connect(mp_obj_t self_in, mp_obj_t addr_in) {
    mod_network_socket_obj_t *self = MP_OBJ_TO_PTR(self_in);

    // get address
    uint8_t ip[MOD_NETWORK_IPADDR_BUF_SIZE];
    mp_uint_t port = netutils_parse_inet_addr(addr_in, ip, NETUTILS_BIG);

    // check if we need to select a NIC
    socket_select_nic(self, ip);

    if (self->type == MOD_NETWORK_SOCK_STREAM && self->state != MOD_NETWORK_SS_NEW) {
        if (self->state == MOD_NETWORK_SS_CONNECTED) {
            mp_raise_OSError(MP_EISCONN);
        } else {
            mp_raise_OSError(MP_EALREADY);
        }
    }

    // call the NIC to connect the socket
    int ret = SOCKET_NIC_PROTOCOL(self, connect)(self, ip, port);
    if (ret) {
        mp_raise_OSError(ret);
    }

    // set socket state
    self->state = MOD_NETWORK_SS_CONNECTED;

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_connect_obj, socket_connect);

// method socket.send(bytes)
STATIC mp_obj_t socket_send(mp_obj_t self_in, mp_obj_t buf_in) {
    mod_network_socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (!SOCKET_HAS_NIC(self)) {
        // not connected
        mp_raise_OSError(MP_EPIPE);
    }
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf_in, &bufinfo, MP_BUFFER_READ);
    mp_uint_t len = bufinfo.len;
    int ret = SOCKET_NIC_PROTOCOL(self, send)(self, bufinfo.buf, &len);
    if (ret) {
        mp_raise_OSError(ret);
    }
    return mp_obj_new_int_from_uint(len);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_send_obj, socket_send);

STATIC mp_obj_t socket_sendall(mp_obj_t self_in, mp_obj_t buf_in) {
    mod_network_socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (!SOCKET_HAS_NIC(self)) {
        // not connected
        mp_raise_OSError(MP_EPIPE);
    }
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf_in, &bufinfo, MP_BUFFER_READ);

    if (self->timeout == 0) {
        mp_uint_t len = bufinfo.len;
        int ret = SOCKET_NIC_PROTOCOL(self, send)(self, bufinfo.buf, &len);
        if (ret) {
            mp_raise_OSError(ret);
        } else if (bufinfo.len > (size_t)ret) {
            mp_raise_OSError(MP_EAGAIN);
        }
        return mp_obj_new_int_from_uint(len);
    } else {
        // TODO: Now returns the total bytes written, not just the last chunk like before?
        // TODO: In CPython3.5, socket timeout should apply to the
        // entire sendall() operation, not to individual send() chunks.
        mp_uint_t total = bufinfo.len;
        while (bufinfo.len != 0) {
            mp_uint_t len = bufinfo.len;
            int ret = SOCKET_NIC_PROTOCOL(self, send)(self, bufinfo.buf, &len);
            if (ret) {
                mp_raise_OSError(ret);
            }
            bufinfo.len -= len;
            bufinfo.buf = (char *)bufinfo.buf + len;
        }
        return mp_obj_new_int_from_uint(total);
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_sendall_obj, socket_sendall);

// method socket.recv(bufsize)
STATIC mp_obj_t socket_recv(mp_obj_t self_in, mp_obj_t len_in) {
    mod_network_socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (!SOCKET_HAS_NIC(self)) {
        // not connected
        mp_raise_OSError(MP_ENOTCONN);
    }
    mp_uint_t len = mp_obj_get_int(len_in);
    vstr_t vstr;
    vstr_init_len(&vstr, len);
    int ret = SOCKET_NIC_PROTOCOL(self, recv)(self, (byte *)vstr.buf, &len);
    if (ret) {
        mp_raise_OSError(ret);
    }
    if (len == 0) {
        return mp_const_empty_bytes;
    }
    vstr.len = len;
    return mp_obj_new_bytes_from_vstr(&vstr);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_recv_obj, socket_recv);

// method socket.sendto(bytes, address)
STATIC mp_obj_t socket_sendto(mp_obj_t self_in, mp_obj_t data_in, mp_obj_t addr_in) {
    mod_network_socket_obj_t *self = MP_OBJ_TO_PTR(self_in);

    // get the data
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(data_in, &bufinfo, MP_BUFFER_READ);

    // get address
    uint8_t ip[MOD_NETWORK_IPADDR_BUF_SIZE];
    mp_uint_t port = netutils_parse_inet_addr(addr_in, ip, NETUTILS_BIG);

    // check if we need to select a NIC
    socket_select_nic(self, ip);

    // call the NIC to sendto
    mp_uint_t len = bufinfo.len;
    int ret = SOCKET_NIC_PROTOCOL(self, sendto)(self, bufinfo.buf, &len, ip, port);
    if (ret) {
        mp_raise_OSError(ret);
    }

    return mp_obj_new_int(len);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(socket_sendto_obj, socket_sendto);

// method socket.recvfrom(bufsize)
STATIC mp_obj_t socket_recvfrom(mp_obj_t self_in, mp_obj_t len_in) {
    mod_network_socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (!SOCKET_HAS_NIC(self)) {
        // not connected
        mp_raise_OSError(MP_ENOTCONN);
    }
    vstr_t vstr;
    vstr_init_len(&vstr, mp_obj_get_int(len_in));
    mp_uint_t len = vstr.len;
    byte ip[NETUTILS_IPV4ADDR_BUFSIZE];
    mp_uint_t port;
    int ret = SOCKET_NIC_PROTOCOL(self, recvfrom)(self, (byte *)vstr.buf, &len, ip, &port);
    if (ret) {
        mp_raise_OSError(ret);
    }
    mp_obj_t tuple[2];
    if (len == 0) {
        tuple[0] = mp_const_empty_bytes;
    } else {
        vstr.len = len;
        tuple[0] = mp_obj_new_bytes_from_vstr(&vstr);
    }
    tuple[1] = netutils_format_inet_addr(ip, port, NETUTILS_BIG);
    return mp_obj_new_tuple(2, tuple);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_recvfrom_obj, socket_recvfrom);

// method socket.setsockopt(level, optname, value)
STATIC mp_obj_t socket_setsockopt(size_t n_args, const mp_obj_t *args) {
    mod_network_socket_obj_t *self = MP_OBJ_TO_PTR(args[0]);

    if (!SOCKET_HAS_NIC(self)) {
        // bind to default NIC.
        uint8_t ip[NETUTILS_IPV4ADDR_BUFSIZE] = {0, 0, 0, 0};
        socket_select_nic(self, ip);
    }

    mp_int_t level = mp_obj_get_int(args[1]);
    mp_int_t opt = mp_obj_get_int(args[2]);

    const void *optval;
    mp_uint_t optlen;
    mp_int_t val;
    if (mp_obj_is_integer(args[3])) {
        val = mp_obj_get_int_truncated(args[3]);
        optval = &val;
        optlen = sizeof(val);
    } else if (opt == MOD_NETWORK_SO_CALLBACK && args[3] == mp_const_none) {
        optval = MP_OBJ_NULL;
        optlen = 0;
    } else if (opt == MOD_NETWORK_SO_CALLBACK && mp_obj_is_callable(args[3])) {
        optval = MP_OBJ_TO_PTR(args[3]);
        optlen = sizeof(optval);
    } else {
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(args[3], &bufinfo, MP_BUFFER_READ);
        optval = bufinfo.buf;
        optlen = bufinfo.len;
    }

    int ret = SOCKET_NIC_PROTOCOL(self, setsockopt)(self, level, opt, optval, optlen);
    if (ret) {
        mp_raise_OSError(ret);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_setsockopt_obj, 4, 4, socket_setsockopt);

STATIC mp_obj_t socket_makefile(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    return args[0];
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_makefile_obj, 1, 3, socket_makefile);

// method socket.settimeout(value)
// timeout=0 means non-blocking
// timeout=None means blocking
// otherwise, timeout is in seconds
STATIC mp_obj_t socket_settimeout(mp_obj_t self_in, mp_obj_t timeout_in) {
    mod_network_socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
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
    if (!SOCKET_HAS_NIC(self)) {
        #if MICROPY_PY_USOCKET_EXTENDED_STATE
        // store the timeout in the socket state until a NIC is bound
        self->timeout = timeout;
        #else
        // not connected
        mp_raise_OSError(MP_ENOTCONN);
        #endif
    } else {
        int ret = SOCKET_NIC_PROTOCOL(self, settimeout)(self, timeout);
        if (ret) {
            mp_raise_OSError(ret);
        }
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_settimeout_obj, socket_settimeout);

// method socket.setblocking(flag)
STATIC mp_obj_t socket_setblocking(mp_obj_t self_in, mp_obj_t blocking) {
    if (mp_obj_is_true(blocking)) {
        return socket_settimeout(self_in, mp_const_none);
    } else {
        return socket_settimeout(self_in, MP_OBJ_NEW_SMALL_INT(0));
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_setblocking_obj, socket_setblocking);

STATIC const mp_rom_map_elem_t socket_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&mp_stream_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&mp_stream_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_bind), MP_ROM_PTR(&socket_bind_obj) },
    { MP_ROM_QSTR(MP_QSTR_listen), MP_ROM_PTR(&socket_listen_obj) },
    { MP_ROM_QSTR(MP_QSTR_accept), MP_ROM_PTR(&socket_accept_obj) },
    { MP_ROM_QSTR(MP_QSTR_connect), MP_ROM_PTR(&socket_connect_obj) },
    { MP_ROM_QSTR(MP_QSTR_send), MP_ROM_PTR(&socket_send_obj) },
    { MP_ROM_QSTR(MP_QSTR_sendall), MP_ROM_PTR(&socket_sendall_obj) },
    { MP_ROM_QSTR(MP_QSTR_recv), MP_ROM_PTR(&socket_recv_obj) },
    { MP_ROM_QSTR(MP_QSTR_sendto), MP_ROM_PTR(&socket_sendto_obj) },
    { MP_ROM_QSTR(MP_QSTR_recvfrom), MP_ROM_PTR(&socket_recvfrom_obj) },
    { MP_ROM_QSTR(MP_QSTR_setsockopt), MP_ROM_PTR(&socket_setsockopt_obj) },
    { MP_ROM_QSTR(MP_QSTR_makefile), MP_ROM_PTR(&socket_makefile_obj) },
    { MP_ROM_QSTR(MP_QSTR_settimeout), MP_ROM_PTR(&socket_settimeout_obj) },
    { MP_ROM_QSTR(MP_QSTR_setblocking), MP_ROM_PTR(&socket_setblocking_obj) },

    { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&mp_stream_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto), MP_ROM_PTR(&mp_stream_readinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_readline), MP_ROM_PTR(&mp_stream_unbuffered_readline_obj) },
    { MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&mp_stream_write_obj) },
};

STATIC MP_DEFINE_CONST_DICT(socket_locals_dict, socket_locals_dict_table);

mp_uint_t socket_read(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {
    mod_network_socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (!SOCKET_HAS_NIC(self)) {
        return MP_STREAM_ERROR;
    }
    int ret = SOCKET_NIC_PROTOCOL(self, recv)(self, (byte *)buf, &size);
    if (ret) {
        *errcode = ret;
        return MP_STREAM_ERROR;
    }
    return size;
}

mp_uint_t socket_write(mp_obj_t self_in, const void *buf, mp_uint_t size, int *errcode) {
    mod_network_socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (!SOCKET_HAS_NIC(self)) {
        return MP_STREAM_ERROR;
    }
    int ret = SOCKET_NIC_PROTOCOL(self, send)(self, buf, &size);
    if (ret) {
        *errcode = ret;
        return MP_STREAM_ERROR;
    }
    return size;
}

mp_uint_t socket_ioctl(mp_obj_t self_in, mp_uint_t request, uintptr_t arg, int *errcode) {
    mod_network_socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (request == MP_STREAM_CLOSE) {
        if (SOCKET_HAS_NIC(self)) {
            SOCKET_NIC_PROTOCOL(self, close)(self);
            #if !MICROPY_PY_LWIP_EXCLUSIVE
            self->nic = MP_OBJ_NULL;
            self->protocol = NULL;
            #endif
        }
        self->state = MOD_NETWORK_SS_CLOSED;
        return 0;
    }
    if (!SOCKET_HAS_NIC(self)) {
        if (request == MP_STREAM_POLL) {
            if (self->state == MOD_NETWORK_SS_NEW) {
                // New sockets are writable and not connected.
                return MP_STREAM_POLL_HUP | MP_STREAM_POLL_WR;
            } else if (self->state == MOD_NETWORK_SS_CLOSED) {
                // Closed socket, return invalid.
                return MP_STREAM_POLL_NVAL;
            }
        }
        *errcode = MP_EINVAL;
        return MP_STREAM_ERROR;
    }
    mp_uint_t result;
    int ret = SOCKET_NIC_PROTOCOL(self, ioctl)(self, request, arg, &result);
    if (ret) {
        *errcode = ret;
        return MP_STREAM_ERROR;
    }
    return result;
}

STATIC const mp_stream_p_t socket_stream_p = {
    .read = socket_read,
    .write = socket_write,
    .ioctl = socket_ioctl,
    .is_text = false,
};

STATIC MP_DEFINE_CONST_OBJ_TYPE(
    socket_type,
    MP_QSTR_socket,
    MP_TYPE_FLAG_NONE,
    make_new, socket_make_new,
    protocol, &socket_stream_p,
    locals_dict, &socket_locals_dict,
    print, socket_print
    );

// function usocket.getaddrinfo(host, port)
STATIC mp_obj_t mod_usocket_getaddrinfo(size_t n_args, const mp_obj_t *args) {
    mp_obj_t host_in = args[0], port_in = args[1];
    size_t hlen;
    const char *host = mp_obj_str_get_data(host_in, &hlen);
    mp_int_t port = mp_obj_get_int(port_in);
    uint8_t out_ip[MOD_NETWORK_IPADDR_BUF_SIZE];
    bool have_ip = false;

    // if constraints were passed then check they are compatible with the supported params
    if (n_args > 2) {
        mp_int_t family = mp_obj_get_int(args[2]);
        mp_int_t type = 0;
        mp_int_t proto = 0;
        mp_int_t flags = 0;
        if (n_args > 3) {
            type = mp_obj_get_int(args[3]);
            if (n_args > 4) {
                proto = mp_obj_get_int(args[4]);
                if (n_args > 5) {
                    flags = mp_obj_get_int(args[5]);
                }
            }
        }
        if (!((family == 0 || family == MOD_NETWORK_AF_INET)
              && (type == 0 || type == MOD_NETWORK_SOCK_STREAM)
              && proto == 0
              && flags == 0)) {
            mp_warning(MP_WARN_CAT(RuntimeWarning), "unsupported getaddrinfo constraints");
        }

        // TODO? Should these flags get used?
    }

    if (hlen > 0) {
        // check if host is already in IP form
        nlr_buf_t nlr;
        if (nlr_push(&nlr) == 0) {
            netutils_parse_ipv4_addr(args[0], out_ip, NETUTILS_BIG);
            have_ip = true;
            nlr_pop();
        } else {
            // swallow exception: host was not in IP form so need to do DNS lookup
        }
    }

    #if MICROPY_PY_LWIP_EXCLUSIVE
    int ret = lwip_gethostbyname(MP_OBJ_NULL, host, hlen, out_ip);
    if (ret) {
        mp_raise_OSError(ret);
    }
    have_ip = true;
    #else
    if (!have_ip) {
        // find a NIC that can do a name lookup
        for (mp_uint_t i = 0; i < MP_STATE_PORT(mod_network_nic_list).len; i++) {
            mp_obj_t nic = MP_STATE_PORT(mod_network_nic_list).items[i];
            const mp_network_nic_p_t *protocol = MP_OBJ_TYPE_GET_SLOT(mp_obj_get_type(nic), protocol);
            if (!protocol->gethostbyname) {
                continue;
            }
            int ret = protocol->gethostbyname(nic, host, hlen, out_ip);
            if (ret) {
                mp_raise_OSError(ret);
            }
            have_ip = true;
            break;
        }
    }
    #endif

    if (!have_ip) {
        mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("no available NIC"));
    }

    mp_obj_tuple_t *tuple = MP_OBJ_TO_PTR(mp_obj_new_tuple(5, NULL));
    tuple->items[0] = MP_OBJ_NEW_SMALL_INT(MOD_NETWORK_AF_INET);
    tuple->items[1] = MP_OBJ_NEW_SMALL_INT(MOD_NETWORK_SOCK_STREAM);
    tuple->items[2] = MP_OBJ_NEW_SMALL_INT(0);
    tuple->items[3] = MP_OBJ_NEW_QSTR(MP_QSTR_);
    tuple->items[4] = netutils_format_inet_addr(out_ip, port, NETUTILS_BIG);
    return mp_obj_new_list(1, (mp_obj_t *)&tuple);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_usocket_getaddrinfo_obj, 2, 6, mod_usocket_getaddrinfo);

STATIC const mp_rom_map_elem_t mp_module_usocket_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_usocket) },

    { MP_ROM_QSTR(MP_QSTR_socket), MP_ROM_PTR(&socket_type) },
    { MP_ROM_QSTR(MP_QSTR_getaddrinfo), MP_ROM_PTR(&mod_usocket_getaddrinfo_obj) },

    // class constants
    { MP_ROM_QSTR(MP_QSTR_AF_INET), MP_ROM_INT(MOD_NETWORK_AF_INET) },
    { MP_ROM_QSTR(MP_QSTR_AF_INET6), MP_ROM_INT(MOD_NETWORK_AF_INET6) },

    { MP_ROM_QSTR(MP_QSTR_SOCK_STREAM), MP_ROM_INT(MOD_NETWORK_SOCK_STREAM) },
    { MP_ROM_QSTR(MP_QSTR_SOCK_DGRAM), MP_ROM_INT(MOD_NETWORK_SOCK_DGRAM) },
    #if MICROPY_PY_USOCKET_RAW
    { MP_ROM_QSTR(MP_QSTR_SOCK_RAW), MP_ROM_INT(MOD_NETWORK_SOCK_RAW) },
    #endif

    { MP_ROM_QSTR(MP_QSTR_SOL_SOCKET), MP_ROM_INT(MOD_NETWORK_SOL_SOCKET) }, // TODO: LWIP this is 1
    { MP_ROM_QSTR(MP_QSTR_SO_REUSEADDR), MP_ROM_INT(MOD_NETWORK_SO_REUSEADDR) }, // TODO: LWIP this in SOF_REUSEADDR
    { MP_ROM_QSTR(MP_QSTR_SO_KEEPALIVE), MP_ROM_INT(MOD_NETWORK_SO_KEEPALIVE) },
    { MP_ROM_QSTR(MP_QSTR_SO_SNDTIMEO), MP_ROM_INT(MOD_NETWORK_SO_SNDTIMEO) },
    { MP_ROM_QSTR(MP_QSTR_SO_RCVTIMEO), MP_ROM_INT(MOD_NETWORK_SO_RCVTIMEO) },

    // TODO: MP_ROM_QSTR(MP_QSTR_SOL_IP)

    // { MP_ROM_QSTR(MP_QSTR_IPPROTO_ICMP), MP_ROM_INT(MOD_NETWORK_IPPROTO_ICMP) },
    // { MP_ROM_QSTR(MP_QSTR_IPPROTO_IPV4), MP_ROM_INT(MOD_NETWORK_IPPROTO_IPV4) },
    // { MP_ROM_QSTR(MP_QSTR_IPPROTO_TCP), MP_ROM_INT(MOD_NETWORK_IPPROTO_TCP) },
    // { MP_ROM_QSTR(MP_QSTR_IPPROTO_UDP), MP_ROM_INT(MOD_NETWORK_IPPROTO_UDP) },
    // { MP_ROM_QSTR(MP_QSTR_IPPROTO_IPV6), MP_ROM_INT(MOD_NETWORK_IPPROTO_IPV6) },
    // { MP_ROM_QSTR(MP_QSTR_IPPROTO_RAW), MP_ROM_INT(MOD_NETWORK_IPPROTO_RAW) },

    // LWIP
    { MP_ROM_QSTR(MP_QSTR_IPPROTO_IP), MP_ROM_INT(MOD_NETWORK_IPPROTO_IP) },
    { MP_ROM_QSTR(MP_QSTR_IP_ADD_MEMBERSHIP), MP_ROM_INT(MOD_NETWORK_IP_ADD_MEMBERSHIP) },
    { MP_ROM_QSTR(MP_QSTR_IP_DROP_MEMBERSHIP), MP_ROM_INT(MOD_NETWORK_IP_DROP_MEMBERSHIP) },

    // #ifdef MICROPY_PY_LWIP
    // { MP_ROM_QSTR(MP_QSTR_reset), MP_ROM_PTR(&mod_lwip_reset_obj) },
    // { MP_ROM_QSTR(MP_QSTR_callback), MP_ROM_PTR(&mod_lwip_callback_obj) },
    // { MP_ROM_QSTR(MP_QSTR_print_pcbs), MP_ROM_PTR(&lwip_print_pcbs_obj) },
    // #if MICROPY_PY_LWIP_SLIP
    // { MP_ROM_QSTR(MP_QSTR_slip), MP_ROM_PTR(&lwip_slip_type) },
    // #endif
    // #endif
};

STATIC MP_DEFINE_CONST_DICT(mp_module_usocket_globals, mp_module_usocket_globals_table);

const mp_obj_module_t mp_module_usocket = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_usocket_globals,
};

MP_REGISTER_MODULE(MP_QSTR_usocket, mp_module_usocket);

#ifdef MICROPY_PY_LWIP
// Backwards compatibility, alias the `lwip` module to `socket`.
MP_REGISTER_MODULE(MP_QSTR_lwip, mp_module_usocket);
#endif

#endif // MICROPY_PY_NETWORK && MICROPY_PY_USOCKET
