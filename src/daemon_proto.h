#ifndef DAEMON_PROTO_H
#define DAEMON_PROTO_H

/* Sent when a daemon gets notified about a new service or a new server connects
 * (then both the servers send all currently known services to each other).
 * The service_id is generated by the "owning" daemon. */
typedef struct
{
    uint32_t service_id;
    char* usn;
    char* location;
    char* service;
    char* server; /* May be NULL */
} pkg_new_service_t;

/* Sent when a daemon expires a service.
 * The service_id must earlier has been sent by the same daemon with a
 * pkg_new_service_t package. */
typedef struct
{
    uint32_t service_id;
} pkg_old_service_t;

/* Sent when a daemon accepts a connection to a service and want to start to
 * tunnel it. The tunnel_id is generated by the requesting daemon.
 * If must be unique for the connection and daemon. */
typedef struct
{
    uint32_t service_id;
    uint32_t tunnel_id;
    char* host;
} pkg_create_tunnel_t;

/* Can be sent by either daemon to signal that the tunnel now is closed */
typedef struct
{
    uint32_t tunnel_id;
} pkg_close_tunnel_t;

/* Can be sent by either daemon to signal data coming from their end */
typedef struct
{
    uint32_t tunnel_id;
    uint32_t size;
    void* data;
} pkg_data_tunnel_t;

typedef enum
{
    PKG_NEW_SERVICE,
    PKG_OLD_SERVICE,
    PKG_CREATE_TUNNEL,
    PKG_CLOSE_TUNNEL,
    PKG_DATA_TUNNEL,
} pkg_type_t;

typedef struct
{
    pkg_type_t type;
    union
    {
        pkg_new_service_t new_service;
        pkg_old_service_t old_service;
        pkg_create_tunnel_t create_tunnel;
        pkg_close_tunnel_t close_tunnel;
        pkg_data_tunnel_t data_tunnel;
    } content;
    size_t tmp1;
    uint8_t tmp2;
} pkg_t;

#include "buf.h"

/* Will only copy the pointers, not their data */
void pkg_new_service(pkg_t* pkg, uint32_t service_id, char* usn, char* location,
                     char* service, char* server);
void pkg_old_service(pkg_t* pkg, uint32_t service_id);
void pkg_create_tunnel(pkg_t* pkg, uint32_t service_id, uint32_t tunnel_id, char* host);
void pkg_close_tunnel(pkg_t* pkg, uint32_t tunnel_id);
void pkg_data_tunnel(pkg_t* pkg, uint32_t tunnel_id, void* data, uint32_t len);

/* Duplicate the given package */
pkg_t* pkg_dup(const pkg_t* pkg);
void pkg_free(pkg_t* pkg);

/* If pkg_write returns true, the pointers in pkg is now OK to free.
 * In the special case of data_tunnel, pkg_write may return false even if part
 * of the data was sent as an package, in which case the data ptr of pkg
 * is moved, so store that one somewhere else to be able to free it
 * If pkg_write returns false, the buffer is full */
bool pkg_write(buf_t buf, pkg_t* pkg);
/* If pkg_peek returns true, pkg now contains a full package.
 * If pkg_peek returns false, more data is needed to make a full package */
bool pkg_peek(buf_t buf, pkg_t* pkg);
/* If pkg_peek returned true, call pkg_read. Any pointers in pkg is now freed */
void pkg_read(buf_t buf, pkg_t* pkg);

#endif /* DAEMON_PROTO_H */
