#pragma once

#include "enet.h"
#include <cstring>
#include <cstdlib>

namespace enet_impl {

// [MACRO] enet_address_set_host is a #define, invisible to jank
inline ENetPeer* connect_to_host(ENetHost* host, const char* hostname, uint16_t port) {
    ENetAddress addr;
    enet_address_set_host(&addr, hostname);
    addr.port = port;
    return enet_host_connect(host, &addr, 2, 0);
}

// [STRING-TO-VOID*] enet_packet_create takes const void*, jank strings
// auto-convert to const char* but not to const void*
inline int send_packet(ENetPeer* peer, const char* data, size_t len, int channel, bool reliable) {
    uint32_t flags = reliable ? ENET_PACKET_FLAG_RELIABLE : 0;
    ENetPacket* packet = enet_packet_create(data, len, flags);
    if (!packet) return -1;
    return enet_peer_send(peer, channel, packet);
}

// [STRING-TO-VOID*] same as send_packet
inline void broadcast_packet(ENetHost* host, const char* data, size_t len, int channel, bool reliable) {
    uint32_t flags = reliable ? ENET_PACKET_FLAG_RELIABLE : 0;
    ENetPacket* packet = enet_packet_create(data, len, flags);
    if (packet) {
      enet_host_broadcast(host, channel, packet);
    }
}

// [POINTER FIELD + MALLOC] event->packet is non-convertible ENetPacket*&,
// plus needs malloc+memcpy+null-terminate pattern
inline char* get_event_data_copy(ENetEvent* event) {
    if (event->packet && event->packet->data && event->packet->dataLength > 0) {
      size_t len = event->packet->dataLength;
      char* copy = (char*)malloc(len + 1);
      if (copy) {
        memcpy(copy, event->packet->data, len);
        copy[len] = '\0';
      }
      return copy;
    }
    return nullptr;
}

// [POINTER ARITHMETIC] peer - peer->host->peers
inline uint32_t get_peer_id(ENetPeer* peer) {
    if (peer && peer->host) {
      return (uint32_t)(peer - peer->host->peers);
    }
    return 0;
}

// [POINTER FIELD] event->packet returns non-convertible ENetPacket*&
inline void destroy_event_packet(ENetEvent* event) {
    if (event->packet) {
      enet_packet_destroy(event->packet);
    }
}

// [POINTER FIELD] data is a char* from get_event_data_copy, needs free(void*)
inline void free_data_copy(char* data) {
    if (data) {
      free(data);
    }
}

} // namespace enet_impl
