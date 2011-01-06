#include "common.h"

#include "daemon_proto.h"
#include <string.h>

static void pkg_freecontent(pkg_t* pkg);

void pkg_new_service(pkg_t* pkg, uint32_t service_id, char* usn, char* location,
                     char* service, char* server)
{
    pkg->type = PKG_NEW_SERVICE;
    assert(usn != NULL && location != NULL && service != NULL);
    pkg->content.new_service.service_id = service_id;
    pkg->content.new_service.usn = usn;
    pkg->content.new_service.location = location;
    pkg->content.new_service.service = service;
    pkg->content.new_service.server = server;
}

void pkg_old_service(pkg_t* pkg, uint32_t service_id)
{
    pkg->type = PKG_OLD_SERVICE;
    pkg->content.old_service.service_id = service_id;
}

void pkg_create_tunnel(pkg_t* pkg, uint32_t service_id, uint32_t tunnel_id, char* host)
{
    pkg->type = PKG_CREATE_TUNNEL;
    assert(host != NULL);
    pkg->content.create_tunnel.service_id = service_id;
    pkg->content.create_tunnel.tunnel_id = tunnel_id;
    pkg->content.create_tunnel.host = host;
}

void pkg_close_tunnel(pkg_t* pkg, uint32_t tunnel_id)
{
    pkg->type = PKG_CLOSE_TUNNEL;
    pkg->content.close_tunnel.tunnel_id = tunnel_id;
}

void pkg_data_tunnel(pkg_t* pkg, uint32_t tunnel_id, void* data, uint32_t len)
{
    pkg->type = PKG_DATA_TUNNEL;
    pkg->content.data_tunnel.tunnel_id = tunnel_id;
    pkg->content.data_tunnel.data = data;
    pkg->content.data_tunnel.size = len;
}

typedef struct _write_ptr_t
{
    buf_t buf;
    char* org, *ptr;
    size_t ptravail, totavail;
}* write_ptr_t;

static void write_raw(write_ptr_t wptr, const void* data, size_t len)
{
    if (wptr->ptravail >= len)
    {
        memcpy(wptr->ptr, data, len);
        wptr->ptr += len;
        wptr->ptravail -= len;
        if (wptr->ptravail > 0)
        {
            return;
        }
        else
        {
            len = 0;
        }
    }
    {
        size_t wrote = wptr->ptr - wptr->org;
        buf_wmove(wptr->buf, wrote);
        wptr->totavail -= wrote;
    }
    assert(wptr->totavail >= len);
    buf_write(wptr->buf, data, len);
    wptr->totavail -= len;
    wptr->org = wptr->ptr = buf_wptr(wptr->buf, &(wptr->ptravail));
    assert(wptr->ptravail == wptr->totavail);
}

static void write_done(write_ptr_t wptr)
{
    buf_wmove(wptr->buf, wptr->ptr - wptr->org);
}

static void write_uint32(write_ptr_t wptr, uint32_t value)
{
    uint8_t tmp[4];
    tmp[0] = value >> 24;
    tmp[1] = (value >> 16) & 0xff;
    tmp[2] = (value >> 8) & 0xff;
    tmp[3] = value & 0xff;
    write_raw(wptr, tmp, 4);
}

static void write_uint8(write_ptr_t wptr, uint8_t value)
{
    write_raw(wptr, &value, 1);
}

static void write_nstr(write_ptr_t wptr, const char* str, size_t len)
{
    assert(len <= 0xffffffff);
    write_uint32(wptr, len);
    write_raw(wptr, str, len);
}

static void write_str(write_ptr_t wptr, const char* str)
{
    write_nstr(wptr, str, strlen(str));
}

static void write_nullstr(write_ptr_t wptr, const char* str)
{
    if (str == NULL)
    {
        write_nstr(wptr, NULL, 0);
    }
    else
    {
        write_nstr(wptr, str, strlen(str));
    }
}

bool pkg_write(buf_t buf, pkg_t* pkg)
{
    struct _write_ptr_t wptr;
    uint32_t pkglen;
    uint8_t pkgtype;
    wptr.totavail = buf_wavail(buf);
    if (wptr.totavail < 6)
    {
        return false;
    }
    switch (pkg->type)
    {
    case PKG_NEW_SERVICE:
        pkgtype = 1;
        pkglen = 4 +
            2 + strlen(pkg->content.new_service.usn) +
            2 + strlen(pkg->content.new_service.location) +
            2 + strlen(pkg->content.new_service.service) + 2
            + (pkg->content.new_service.server != NULL
               ? strlen(pkg->content.new_service.server) : 0);
        break;
    case PKG_OLD_SERVICE:
        pkgtype = 2;
        pkglen = 4;
        break;
    case PKG_CREATE_TUNNEL:
        pkgtype = 10;
        pkglen = 8 + 2 + strlen(pkg->content.create_tunnel.host);
        break;
    case PKG_CLOSE_TUNNEL:
        pkgtype = 11;
        pkglen = 4;
        break;
    case PKG_DATA_TUNNEL:
        pkgtype = 12;
        if (wptr.totavail < 1024 ||
            pkg->content.data_tunnel.size < (wptr.totavail - 10))
        {
            pkglen = 4 + pkg->content.data_tunnel.size;
        }
        else
        {
            pkglen = 4 + (wptr.totavail - 10);
        }
        break;
    }
    if (pkglen > wptr.totavail)
    {
        return false;
    }
    wptr.buf = buf;
    wptr.org = wptr.ptr = buf_wptr(buf, &(wptr.ptravail));
    write_uint32(&wptr, pkglen);
    write_uint8(&wptr, pkgtype);
    write_uint8(&wptr, 0);
    switch (pkg->type)
    {
    case PKG_NEW_SERVICE:
        write_uint32(&wptr, pkg->content.new_service.service_id);
        write_str(&wptr, pkg->content.new_service.usn);
        write_str(&wptr, pkg->content.new_service.location);
        write_str(&wptr, pkg->content.new_service.service);
        write_nullstr(&wptr, pkg->content.new_service.server);
        write_done(&wptr);
        return true;
    case PKG_OLD_SERVICE:
        write_uint32(&wptr, pkg->content.old_service.service_id);
        write_done(&wptr);
        return true;
    case PKG_CREATE_TUNNEL:
        write_uint32(&wptr, pkg->content.create_tunnel.service_id);
        write_uint32(&wptr, pkg->content.create_tunnel.tunnel_id);
        write_str(&wptr, pkg->content.create_tunnel.host);
        write_done(&wptr);
        return true;
    case PKG_CLOSE_TUNNEL:
        write_uint32(&wptr, pkg->content.close_tunnel.tunnel_id);
        write_done(&wptr);
        return true;
    case PKG_DATA_TUNNEL:
        write_uint32(&wptr, pkg->content.data_tunnel.tunnel_id);
        write_uint32(&wptr, pkglen - 4);
        if (pkglen - 4 == pkg->content.data_tunnel.size)
        {
            write_raw(&wptr, pkg->content.data_tunnel.data,
                      pkg->content.data_tunnel.size);
            write_done(&wptr);
            return true;
        }
        else
        {
            assert(pkglen - 4 < pkg->content.data_tunnel.size);
            write_raw(&wptr, pkg->content.data_tunnel.data, pkglen - 4);
            pkg->content.data_tunnel.data = (char*)pkg->content.data_tunnel.data
                + (pkglen - 4);
            pkg->content.data_tunnel.size -= (pkglen - 4);
            write_done(&wptr);
            return false;
        }
    default:
        assert(false);
        return false;
    }
}

typedef struct _read_ptr_t
{
    buf_t buf;
    const char* org, *ptr;
    size_t ptravail, totavail;
}* read_ptr_t;

static void read_raw(read_ptr_t rptr, void* data, size_t len)
{
    if (rptr->ptravail >= len)
    {
        memcpy(data, rptr->ptr, len);
        rptr->ptr += len;
        rptr->ptravail -= len;
        if (rptr->ptravail > 0)
        {
            return;
        }
        else
        {
            len = 0;
        }
    }
    {
        size_t read = rptr->ptr - rptr->org;
        buf_rmove(rptr->buf, read);
        rptr->totavail -= read;
    }
    assert(rptr->totavail >= len);
    buf_read(rptr->buf, data, len);
    rptr->totavail -= len;
    rptr->org = rptr->ptr = buf_rptr(rptr->buf, &(rptr->ptravail));
    assert(rptr->ptravail == rptr->totavail);
}

static void read_done(read_ptr_t rptr)
{
    buf_rmove(rptr->buf, rptr->ptr - rptr->org);
}

static uint32_t read_uint32(read_ptr_t rptr)
{
    uint8_t tmp[4];
    read_raw(rptr, tmp, 4);
    return tmp[0] << 24 | tmp[1] << 16 | tmp[2] << 8 | tmp[3];
}

static char* read_str(read_ptr_t rptr)
{
    uint32_t len = read_uint32(rptr);
    char* str = malloc(len + 1);
    str[len] = '\0';
    read_raw(rptr, str, len);
    return str;
}

static char* read_nullstr(read_ptr_t rptr)
{
    uint32_t len = read_uint32(rptr);
    if (len > 0)
    {
        char* str = malloc(len + 1);
        str[len] = '\0';
        read_raw(rptr, str, len);
        return str;
    }
    return NULL;
}

bool pkg_peek(buf_t buf, pkg_t* pkg)
{
    struct _read_ptr_t rptr;
    uint32_t pkglen;
    uint8_t pkgtype, pkgversion;
    uint8_t header[6];
    for (;;)
    {
        rptr.totavail = buf_ravail(buf);
        if (rptr.totavail < 6)
        {
            return false;
        }
        buf_peek(buf, header, 6);
        pkglen = (header[0] << 24) | (header[1] << 16) | (header[2] << 8)
            | header[3];
        pkgtype = header[4];
        pkgversion = header[5];
        if (pkgversion != 0 ||
            !((pkgtype >= 1 && pkgtype <= 2) ||
              (pkgtype >= 10 && pkgtype <= 12)))
        {
            if (rptr.totavail < 6 + pkglen)
            {
                /* need more data */
                return false;
            }
            else
            {
                /* skip package */
                buf_skip(buf, 6 + pkglen);
            }
        }

        if (rptr.totavail < 6 + pkglen)
        {
            if (buf_wavail(buf) == 0 && pkgtype == 12 &&
                rptr.totavail >= 20)
            {
                /* If we got one big data package larger than the buffer,
                 * return it in parts */
                size_t avail = rptr.totavail - 10;
                buf_skip(buf, 6);
                rptr.totavail -= 6;
                rptr.buf = buf;
                rptr.ptr = rptr.org = buf_rptr(buf, &(rptr.ptravail));

                pkg->type = PKG_DATA_TUNNEL;
                pkg->content.data_tunnel.size = read_uint32(&rptr);

                assert(avail == rptr.ptravail || (avail - rptr.ptravail) >= 10);
                pkg->content.data_tunnel.data = (char*)rptr.ptr;
                pkg->tmp1 = pkg->content.data_tunnel.size - rptr.ptravail;
                pkg->tmp2 = 0x81;
                pkg->content.data_tunnel.size = rptr.ptravail;
                read_done(&rptr);
                return true;
            }
            else
            {
                /* need more data */
                return false;
            }
        }

        buf_skip(buf, 6);
        rptr.totavail -= 6;
        rptr.buf = buf;
        rptr.ptr = rptr.org = buf_rptr(buf, &(rptr.ptravail));

        pkg->tmp1 = 0;
        pkg->tmp2 = 0;

        switch (pkgtype)
        {
        case 1:
            pkg->type = PKG_NEW_SERVICE;
            pkg->content.new_service.service_id = read_uint32(&rptr);
            if (rptr.ptravail >= pkglen - 4)
            {
                uint8_t* ptr = (uint8_t*)rptr.ptr;
                uint16_t len;
                len = ptr[0] << 24 | ptr[1] << 16 | ptr[2] << 8 | ptr[3];
                ptr += 4;
                pkg->content.new_service.usn = (char*)ptr;
                ptr += len;
                len = ptr[0] << 24 | ptr[1] << 16 | ptr[2] << 8 | ptr[3];
                ptr[0] = '\0';
                ptr += 4;
                pkg->content.new_service.location = (char*)ptr;
                ptr += len;
                len = ptr[0] << 24 | ptr[1] << 16 | ptr[2] << 8 | ptr[3];
                ptr[0] = '\0';
                ptr += 4;
                pkg->content.new_service.service = (char*)ptr;
                ptr += len;
                len = ptr[0] << 24 | ptr[1] << 16 | ptr[2] << 8 | ptr[3];
                ptr[0] = '\0';
                ptr += 4;
                if (len > 0)
                {
                    pkg->content.new_service.server = malloc(len + 1);
                    memcpy(pkg->content.new_service.server, ptr, len);
                    pkg->content.new_service.server[len] = '\0';
                    ptr += len;
                }
                else
                {
                    pkg->content.new_service.server = NULL;
                }
                pkg->tmp1 = (char*)ptr - rptr.org;
                pkg->tmp2 = 0x80;
                return true;
            }
            else
            {
                pkg->content.new_service.usn = read_str(&rptr);
                pkg->content.new_service.location = read_str(&rptr);
                pkg->content.new_service.service = read_str(&rptr);
                pkg->content.new_service.server = read_nullstr(&rptr);
                read_done(&rptr);
            }
            return true;
        case 2:
            pkg->type = PKG_OLD_SERVICE;
            pkg->content.old_service.service_id = read_uint32(&rptr);
            read_done(&rptr);
            return true;
        case 10:
            pkg->type = PKG_CREATE_TUNNEL;
            pkg->content.create_tunnel.service_id = read_uint32(&rptr);
            pkg->content.create_tunnel.tunnel_id = read_uint32(&rptr);
            pkg->content.create_tunnel.host = read_str(&rptr);
            read_done(&rptr);
            return true;
        case 11:
            pkg->type = PKG_CLOSE_TUNNEL;
            pkg->content.close_tunnel.tunnel_id = read_uint32(&rptr);
            read_done(&rptr);
            return true;
        case 12:
            pkg->type = PKG_DATA_TUNNEL;
            pkg->content.data_tunnel.tunnel_id = read_uint32(&rptr);
            pkg->content.data_tunnel.size = read_uint32(&rptr);
            if (rptr.ptravail >= pkg->content.data_tunnel.size)
            {
                pkg->content.data_tunnel.data = (char*)rptr.ptr;
                pkg->tmp1 =
                    (rptr.ptr - rptr.org) + pkg->content.data_tunnel.size;
                pkg->tmp2 = 0x80;
                return true;
            }
            else if (rptr.ptravail > 10)
            {
                pkg->content.data_tunnel.data = (char*)rptr.ptr;
                pkg->tmp1 = pkg->content.data_tunnel.size - rptr.ptravail;
                pkg->tmp2 = 0x81;
                pkg->content.data_tunnel.size = rptr.ptravail;
                read_done(&rptr);
                return true;
            }
            else
            {
                pkg->content.data_tunnel.data
                    = malloc(pkg->content.data_tunnel.size);
                read_raw(&rptr, pkg->content.data_tunnel.data,
                         pkg->content.data_tunnel.size);
                read_done(&rptr);
                return true;
            }
            break;
        default:
            assert(false);
            buf_skip(buf, pkglen);
            continue;
        }
    }
}

void pkg_read(buf_t buf, pkg_t* pkg)
{
    if ((pkg->tmp2 & 0x80) == 0)
    {
        pkg_freecontent(pkg);
    }
    pkg->tmp2 &= 0x7f;
    if (pkg->tmp2 == 0)
    {
        buf_rmove(buf, pkg->tmp1);
    }
    else
    {
        /* pkg_peek cut a DATA_TUNNEL package in two. So write a new header
         * so the next call to pkg_peek collects the next one */
        uint32_t pkglen = 10 + pkg->tmp1;
        uint8_t header[10];
        struct _write_ptr_t wptr;
        assert(pkg->tmp2 == 1);
        assert(pkg->content.data_tunnel.size >= 10);
        buf_rmove(buf, pkg->content.data_tunnel.size - 10);
        assert(buf_ravail(buf) >= pkglen);
        wptr.ptr = wptr.org = (char*)header;
        wptr.buf = NULL;
        wptr.ptravail = 10 + 1; /* the last byte is to trick write_raw not to
                                 * sync */
        wptr.totavail = wptr.ptravail;
        write_uint32(&wptr, pkglen);
        write_uint8(&wptr, 12);
        write_uint8(&wptr, 0);
        write_uint32(&wptr, pkglen - 10);
        assert(wptr.ptravail == 1);
        buf_replace(buf, header, 10);
    }
}

pkg_t* pkg_dup(const pkg_t* pkg)
{
    pkg_t* ret = calloc(1, sizeof(pkg_t));
    if (ret == NULL)
        return ret;
    switch (pkg->type)
    {
    case PKG_NEW_SERVICE:
        pkg_new_service(ret, pkg->content.new_service.service_id,
                        strdup(pkg->content.new_service.usn),
                        strdup(pkg->content.new_service.location),
                        strdup(pkg->content.new_service.service),
                        pkg->content.new_service.server != NULL ?
                        strdup(pkg->content.new_service.server) : NULL);
        break;
    case PKG_OLD_SERVICE:
        pkg_old_service(ret, pkg->content.old_service.service_id);
        break;
    case PKG_CREATE_TUNNEL:
        pkg_create_tunnel(ret, pkg->content.create_tunnel.service_id,
                          pkg->content.create_tunnel.tunnel_id,
                          pkg->content.create_tunnel.host);
        break;
    case PKG_CLOSE_TUNNEL:
        pkg_close_tunnel(ret, pkg->content.close_tunnel.tunnel_id);
        break;
    case PKG_DATA_TUNNEL:
    {
        void* data = malloc(pkg->content.data_tunnel.size);
        memcpy(data, pkg->content.data_tunnel.data,
               pkg->content.data_tunnel.size);
        pkg_data_tunnel(ret, pkg->content.data_tunnel.tunnel_id, data,
                        pkg->content.data_tunnel.size);
        break;
    }
    }

    return ret;
}

void pkg_freecontent(pkg_t* pkg)
{
    switch (pkg->type)
    {
    case PKG_NEW_SERVICE:
        free(pkg->content.new_service.usn);
        free(pkg->content.new_service.location);
        free(pkg->content.new_service.service);
        free(pkg->content.new_service.server);
        break;
    case PKG_CREATE_TUNNEL:
        free(pkg->content.create_tunnel.host);
        break;
    case PKG_OLD_SERVICE:
    case PKG_CLOSE_TUNNEL:
        break;
    case PKG_DATA_TUNNEL:
        free(pkg->content.data_tunnel.data);
        break;
    }
}

void pkg_free(pkg_t* pkg)
{
    if (pkg == NULL)
        return;

    pkg_freecontent(pkg);
    free(pkg);
}
