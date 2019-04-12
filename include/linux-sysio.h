/*
 *    This Cplant(TM) source code is the property of Sandia National
 *    Laboratories.
 *
 *    This Cplant(TM) source code is copyrighted by Sandia National
 *    Laboratories.
 *
 *    The redistribution of this Cplant(TM) source code is subject to the
 *    terms of the GNU Lesser General Public License
 *    (see cit/LGPL or http://www.gnu.org/licenses/lgpl.html)
 *
 *    Cplant(TM) Copyright 1998-2004 Sandia Corporation. 
 *    Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 *    license for use of this work by or on behalf of the US Government.
 *    Export of this program may require a license from the United States
 *    Government.
 */

/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Questions or comments about this library should be sent to:
 *
 * Lee Ward
 * Sandia National Laboratories, New Mexico
 * P.O. Box 5800
 * Albuquerque, NM 87185-1110
 *
 * lee@sandia.gov
 */

#include "sysio-cmn.h"

#if defined(_LARGEFILE64_SOURCE) && _LARGEFILE64_SOURCE
#define intnl_aiocb	aiocb64

#define _i_lio_listio	lio_listio64
#define _i_lio_error	aio_error64
#define _i_lio_return	aio_return64
#else
#define intnl_aiocb	aiocb

#define _i_lio_listio	lio_listio
#define _i_lio_error	aio_error
#define _i_lio_return	aio_return
#endif

#define _i_lio_suspend	aio_suspend

#ifndef EMULATE_AIO
#define _u_lio_listio	_i_lio_listio
#define _u_lio_error	_i_lio_error
#define _u_lio_return	_i_lio_return
#define _u_lio_suspend	_i_lio_suspend
#else
#define _u_lio_listio	_emulate_lio_listio
#define _u_lio_error	_emulate_lio_error
#define _u_lio_return	_emulate_lio_return
#define _u_lio_suspend	_emulate_lio_suspend
#endif

#define _sysio_lio_listio(_m, _l, _n, _sig) \
	_u_lio_listio((_m), (_l), (_n), (_sig))

#define _sysio_lio_error(aiocb) \
	_u_lio_error(aiocb)

#define _sysio_lio_return(aiocb) \
	_u_lio_return(aiocb)

#define _sysio_lio_suspend(aiocb, _l, _n, _timeo) \
	_u_lio_suspend((aiocb), (_l), (_n), (_timeo))

struct io_context {
	struct intnl_aiocb **io_list;
	size_t	io_list_len;
	size_t	io_list_size;
	int	io_fd;
	int	io_lio_opcode;
};

extern struct io_context *_sysio_io_context_alloc(int fd,
						  unsigned naio,
						  int lio_opcode);
extern ssize_t _sysio_aiocb_build(struct iovec *iov,
				  size_t iovlen,
				  _SYSIO_OFF_T off,
				  ssize_t limit,
				  struct io_context *ioc);
extern struct io_context *_sysio_iiox(struct io_context *ioc,
				      const struct iovec *iov,
				      size_t iov_count,
				      const struct intnl_xtvec *xtv,
				      size_t xtv_count);
extern int iodone(ioid_t ioid);
extern ssize_t iowait(ioid_t ioid);
extern ssize_t _sysio_sum_iovec(const struct iovec *iov, int count);
