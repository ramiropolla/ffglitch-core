/* SPDX-License-Identifier: MPL-2.0 */

#ifndef __ZMQ_FD_HPP_INCLUDED__
#define __ZMQ_FD_HPP_INCLUDED__

#if defined _WIN32
#include "windows.hpp"
#endif

namespace zmq
{
typedef zmq_fd_t fd_t;

#ifdef ZMQ_HAVE_WINDOWS
enum : fd_t
{
    retired_fd = INVALID_SOCKET
};
#else
enum
{
    retired_fd = -1
};
#endif
}
#endif
