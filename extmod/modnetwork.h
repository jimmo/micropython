/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014 Damien P. George
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
#ifndef MICROPY_INCLUDED_MODNETWORK_H
#define MICROPY_INCLUDED_MODNETWORK_H

#define MOD_NETWORK_IPADDR_BUF_SIZE (4)

#define MOD_NETWORK_AF_INET (2)
#define MOD_NETWORK_AF_INET6 (10)

#define MOD_NETWORK_SOCK_STREAM (1)
#define MOD_NETWORK_SOCK_DGRAM (2)
#define MOD_NETWORK_SOCK_RAW (3)

#define MOD_NETWORK_IPPROTO_IP      (0)

#define MOD_NETWORK_WLAN_STA_IF (0)
#define MOD_NETWORK_WLAN_AP_IF (1)

// Socket level option.
#define MOD_NETWORK_SOL_SOCKET      (0x0FFF)

// Common option flags per-socket.
#define MOD_NETWORK_SO_REUSEADDR    (0x0004)
#define MOD_NETWORK_SO_KEEPALIVE    (0x0008)
#define MOD_NETWORK_SO_SNDTIMEO     (0x1005)
#define MOD_NETWORK_SO_RCVTIMEO     (0x1006)

#define MOD_NETWORK_SO_CALLBACK     (20)

#define MOD_NETWORK_SS_NEW          (0)
#define MOD_NETWORK_SS_LISTENING    (1)
#define MOD_NETWORK_SS_CONNECTING   (2)
#define MOD_NETWORK_SS_CONNECTED    (3)
#define MOD_NETWORK_SS_CLOSED       (4)
#define MOD_NETWORK_SS_PEER_CLOSED  (5)
#define MOD_NETWORK_SS_ACTIVE_UDP   (6)

// All socket options should be globally distinct,
// because we ignore option levels for efficiency.
#define MOD_NETWORK_IP_ADD_MEMBERSHIP (0x400) // 3 & 4
#define MOD_NETWORK_IP_DROP_MEMBERSHIP (0x401)

typedef struct _mod_network_socket_obj_t {
    mp_obj_base_t base;
    #if !MICROPY_PY_LWIP_EXCLUSIVE
    mp_obj_t nic;
    const struct _mp_network_nic_p_t *protocol;
    #endif
    uint32_t domain : 5;
    uint32_t type   : 5;
    uint32_t proto  : 5;
    uint32_t bound  : 1;
    int32_t state   : 8;
    int32_t fileno;
    int32_t timeout;
    mp_obj_t callback;
    #if MICROPY_PY_USOCKET_EXTENDED_STATE
    // Extended socket state for NICs/ports that need it.
    void *_private;
    #endif
} mod_network_socket_obj_t;

// NIC protocol
typedef struct _mp_network_nic_p_t {
    // API for non-socket operations
    int (*gethostbyname)(mp_obj_t nic, const char *name, mp_uint_t len, uint8_t *ip_out);

    // API for socket operations -- all return errno.
    int (*socket)(mod_network_socket_obj_t *socket);
    void (*close)(mod_network_socket_obj_t *socket);
    int (*bind)(mod_network_socket_obj_t *socket, byte *ip, mp_uint_t port);
    int (*listen)(mod_network_socket_obj_t *socket, mp_int_t backlog);
    int (*accept)(mod_network_socket_obj_t *socket, mod_network_socket_obj_t *socket2, byte *ip, mp_uint_t *port);
    int (*connect)(mod_network_socket_obj_t *socket, byte *ip, mp_uint_t port);
    int (*send)(mod_network_socket_obj_t *socket, const byte *buf, mp_uint_t *len);
    int (*recv)(mod_network_socket_obj_t *socket, byte *buf, mp_uint_t *len);
    int (*sendto)(mod_network_socket_obj_t *socket, const byte *buf, mp_uint_t *len, byte *ip, mp_uint_t port);
    int (*recvfrom)(mod_network_socket_obj_t *socket, byte *buf, mp_uint_t *len, byte *ip, mp_uint_t *port);
    int (*setsockopt)(mod_network_socket_obj_t *socket, mp_uint_t level, mp_uint_t opt, const void *optval, mp_uint_t optlen);
    int (*settimeout)(mod_network_socket_obj_t *socket, mp_uint_t timeout_ms);
    int (*ioctl)(mod_network_socket_obj_t *socket, mp_uint_t request, mp_uint_t arg, mp_uint_t *result);
} mp_network_nic_p_t;

#if MICROPY_PY_LWIP_EXCLUSIVE
STATIC inline void mod_network_init(void) {
}
STATIC inline void mod_network_deinit(void) {
}
STATIC inline void mod_network_register_nic(mp_obj_t) {
}
#else
void mod_network_init(void);
void mod_network_deinit(void);
void mod_network_register_nic(mp_obj_t nic);
mp_obj_t mod_network_find_nic(const uint8_t *ip);
#endif

#endif // MICROPY_INCLUDED_MODNETWORK_H
