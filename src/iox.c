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

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <aio.h>

#include "xtio.h"
#include "linux-sysio.h"

/*
 * Allocate internal IO context.
 */
struct io_context *
_sysio_io_context_alloc(int fd, unsigned naio, int lio_opcode)
{
	struct io_context *ioc;
	struct intnl_aiocb **aiocbp;
	unsigned u;

	ioc = malloc(sizeof(struct io_context));
	if (!ioc)
		return NULL;
	ioc->io_list = aiocbp = malloc(naio * sizeof(struct intnl_aiocb *));
	ioc->io_list_len = 0;
	ioc->io_list_size = naio;
	ioc->io_fd = fd;
	ioc->io_lio_opcode = lio_opcode;
	if (!ioc->io_list) {
		free(ioc);
		return NULL;
	}
	for (u = 0; u < naio; u++)
		*aiocbp++ = NULL;

	return ioc;
}

/*
 * Given iovec list and limit, append associated AIO control blocks to
 * the IO list in the context.
 */
ssize_t
_sysio_aiocb_build(struct iovec *iov, size_t iovlen,
		   _SYSIO_OFF_T off,
		   ssize_t limit,
		   struct io_context *ioc)
{
	ssize_t	cc, ncc;
	struct intnl_aiocb *aiocb;
	size_t	n;

	if (!iovlen) {
		errno = EINVAL;
		return -1;
	}
	if (!limit)
		return 0;

	cc = 0;
	do {
		if (!iov->iov_len)
			continue;
		n = iov->iov_len;
		if (n > (unsigned )limit)
			n = limit;
		limit -= n;
		assert(ioc->io_list_len < ioc->io_list_size);
		aiocb =
		    ioc->io_list[ioc->io_list_len++] =
		    malloc(sizeof(struct intnl_aiocb));
		if (!aiocb) {
			cc = -1;
			break;
		}
		(void )memset(aiocb, 0, sizeof(struct intnl_aiocb));
		aiocb->aio_fildes = ioc->io_fd;
		aiocb->aio_lio_opcode = ioc->io_lio_opcode;
		aiocb->aio_buf = iov->iov_base;
		aiocb->aio_nbytes = n;
		aiocb->aio_offset = off;
		off += n;
		if (off <= aiocb->aio_offset) {
			errno = EOVERFLOW;
			cc = -1;
			break;
		}
		ncc = cc + n;
		if (ncc <= cc) {
			errno = EINVAL;
			cc = -1;
			break;
		}
		cc = ncc;
		iov++;
	} while (--iovlen && limit);

	return cc;
}

/*
 * Construct and post asynch IO.
 */
struct io_context *
_sysio_iiox(struct io_context *ioc,
	    const struct iovec *iov, size_t iov_count,
	    const struct intnl_xtvec *xtv, size_t xtv_count)
{
	ssize_t	cc;

	do {
		cc =
		    _sysio_enumerate_extents(xtv, xtv_count,
					     iov, iov_count,
					     (ssize_t (*)(const struct iovec *,
							  int,
							  _SYSIO_OFF_T,
							  ssize_t,
							  void *))_sysio_aiocb_build,
					     ioc);
		if (cc < 0)
			break;
		/*
		 * Adjust the io_size field to the length. The iodone
		 * function expects the list length and size to be the
		 * same.
		 *
		 * NB: We aren't trimming the excess elements of the list. That
		 * space is just wasted while the operation is in progress.
		 */
		ioc->io_list_size = ioc->io_list_len;
		cc =
		    _sysio_lio_listio(LIO_NOWAIT,
				      (struct intnl_aiocb *const *)ioc->io_list,
				      ioc->io_list_size,
				      NULL);
		break;
	} while (0);
	if (cc < 0) {
		unsigned u;
		struct intnl_aiocb **aiocbp;

		aiocbp = ioc->io_list;
		for (u = 0; u < ioc->io_list_len; u++)
			free(*aiocbp++);
		free(ioc->io_list);
		free(ioc);
		return IOID_FAIL;
	}
	return ioc;
}

/*
 * Get status of previously posted async file IO operation.
 */
int
iodone(ioid_t ioid)
{
	struct io_context *ioc;
	struct timespec tv;
	struct intnl_aiocb **aiocbp;
	int	err;

	ioc = ioid;
	if (!ioc->io_list_len)
		return 1;
	tv.tv_sec = 0;
	tv.tv_nsec = 1000;
	if (aio_suspend((const struct aiocb *const *)ioc->io_list,
			ioc->io_list_len,
			&tv) < 0) {
		if (errno == EAGAIN) {
			errno = 0;
			return 0;
		}
		return -1;
	}
	aiocbp = ioc->io_list + ioc->io_list_len - 1;;
	do {
		err = _sysio_lio_error(*aiocbp--);
		if (err == EINPROGRESS)
			break;
		if (err != 0) {
			errno = err;
			return -1;
		}
	} while (ioc->io_list_len-- > 1);
	return ioc->io_list_len ? 0 : 1;
}

/*
 * Wait for completion of a previously posted asynch file IO request.
 */
ssize_t
iowait(ioid_t ioid)
{
	struct io_context *ioc;
	int	n;
	ssize_t	acc, cc;
	struct intnl_aiocb **aiocbp;
	int	err;

	while (!iodone(ioid))
		;
	ioc = ioid;
	acc = 0;
	err = 0;
	aiocbp = ioc->io_list;
	while (ioc->io_list_size--) {
		n = _sysio_lio_error(*aiocbp);
		cc = _sysio_lio_return(*aiocbp);
		free(*aiocbp++);
		if (acc < 0 || err)
			continue;
		if (cc < 0) {
			acc = -1;
			err = n;
			continue;
		}
		if (!cc)
			continue;
		if (acc + cc <= acc) {
			err = EOVERFLOW;
			continue;
		}
		acc += cc;
	}
	free(ioc->io_list);
	free(ioc);
	if (acc < 0) {
		errno = err;
		return -1;
	}
	return acc;
}

/*
 * Sum iovec entries, returning total found or error if range of ssize_t would
 * be exceeded.
 */
ssize_t
_sysio_sum_iovec(const struct iovec *iov, int count)
{
	ssize_t tmp, cc;

	if (count <= 0)
		return -EINVAL;

	cc = 0;
	while (count--) {
		tmp = cc;
		cc += iov->iov_len;
		if (tmp && iov->iov_len && cc <= tmp)
			return -EINVAL;
		iov++;
	}
	return cc;
}

