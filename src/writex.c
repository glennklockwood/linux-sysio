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

#include <stdio.h>

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/uio.h>
#include <aio.h>

#include "xtio.h"
#include "linux-sysio.h"

/*
 * Post async write from buffers mapped by iovec from regions mapped
 * by xtvec.
 *
 * NB: An adaptation of "listio" from Argonne's PVFS.
 */
static ioid_t
_sysio_iwritex(int fd,
	       const struct iovec *iov, size_t iov_count,
	       const struct intnl_xtvec *xtv, size_t xtv_count)
{
	struct io_context *ioc;

	ioc =
	    _sysio_io_context_alloc(fd,
				    xtv_count + iov_count,
				    LIO_WRITE);
	if (!ioc)
		return IOID_FAIL;

	return _sysio_iiox(ioc, iov, iov_count, xtv, xtv_count);
}

#if _LARGEFILE64_SOURCE
ioid_t
iwritex(int fd,
	const struct iovec *iov, size_t iov_count,
	const struct xtvec *xtv, size_t xtv_count)
{
	struct intnl_xtvec *ixtv;
	unsigned u;
	ioid_t	ioid;

	ixtv = malloc(xtv_count * sizeof(struct intnl_xtvec));
	if (!ixtv)
		return IOID_FAIL;

	for (u = 0; u < xtv_count; u++) {
		ixtv[u].xtv_off = xtv->xtv_off;
		ixtv[u].xtv_len = xtv->xtv_len;
		xtv++;
	}

	ioid = _sysio_iwritex(fd, iov, iov_count, ixtv, xtv_count);
	free(ixtv);
	return ioid;
}

ioid_t
iwrite64x(int fd,
	  const struct iovec *iov, size_t iov_count,
	  const struct xtvec64 *xtv, size_t xtv_count)
  __attribute__((alias("_sysio_iwritex")));
#else
ioid_t
iwritex(int fd,
	const struct iovec *iov, size_t iov_count,
	const struct xtvec *xtv, size_t xtv_count)
  __attribute__((alias("_sysio_iwritex")));
#endif

/*
 * Write from buffers mapped by iovec from regions mapped
 * by xtvec.
 *
 * NB: An adaptation of "listio" from Argonne's PVFS.
 */
ssize_t
writex(int fd,
       const struct iovec *iov, size_t iov_count,
       const struct xtvec *xtv, size_t xtv_count)
{
	ioid_t	ioid;

	ioid = iwritex(fd, iov, iov_count, xtv, xtv_count);
	if (ioid == IOID_FAIL)
		return -1;
	return iowait(ioid);
}

#if _LARGEFILE64_SOURCE
/*
 * Write from buffers mapped by iovec from regions mapped
 * by xtvec.
 *
 * NB: An adaptation of "listio" from Argonne's PVFS.
 */
ssize_t
write64x(int fd,
	 const struct iovec *iov, size_t iov_count,
	 const struct xtvec64 *xtv, size_t xtv_count)
{
	ioid_t	ioid;

	ioid = iwrite64x(fd, iov, iov_count, xtv, xtv_count);
	if (ioid == IOID_FAIL)
		return -1;
	return iowait(ioid);
}
#endif

/*
 * Post asynch write from buffers mapped by an iovec from file at given offset.
 */
ioid_t
ipwritev(int fd, const struct iovec *iov, size_t count, off_t offset)
{
	struct intnl_xtvec ixtv;

	ixtv.xtv_off = offset;
	ixtv.xtv_len = _sysio_sum_iovec(iov, count);
	return _sysio_iwritex(fd, iov, count, &ixtv, 1);
}

#if _LARGEFILE64_SOURCE
/*
 * Post asynch write from buffers mapped by an iovec from file at given offset.
 */
ioid_t
ipwrite64v(int fd, const struct iovec *iov, size_t count, off64_t offset)
{
	struct intnl_xtvec ixtv;

	ixtv.xtv_off = offset;
	ixtv.xtv_len = _sysio_sum_iovec(iov, count);
	return _sysio_iwritex(fd, iov, count, &ixtv, 1);
}
#endif

/*
 * Post asynch write from buffer from file at given offset.
 */
ioid_t
ipwrite(int fd, const void *buf, size_t count, off_t offset)
{
	struct iovec iov;

	iov.iov_base = (char *)buf;
	iov.iov_len = count;
	return ipwritev(fd, &iov, 1, offset);
}

#if _LARGEFILE64_SOURCE
/*
 * Post asynch write from buffer from file at given offset.
 */
ioid_t
ipwrite64(int fd, const void *buf, size_t count, off64_t offset)
{
	struct iovec iov;

	iov.iov_base = (char *)buf;
	iov.iov_len = count;
	return ipwrite64v(fd, &iov, 1, offset);
}
#endif

/*
 * Write from buffers mapped by an iovec from file at given offset.
 */
ssize_t
pwritev(int fd, const struct iovec *iov, size_t count, off_t offset)
{
	ioid_t	ioid;

	if ((ioid = ipwritev(fd, iov, count, offset)) == IOID_FAIL)
		return -1;
	return iowait(ioid);
}

#if _LARGEFILE64_SOURCE
/*
 * Write from buffers mapped by an iovec from file at given offset.
 */
ssize_t
pwrite64v(int fd, const struct iovec *iov, size_t count, off64_t offset)
{
	ioid_t	ioid;

	if ((ioid = ipwritev(fd, iov, count, offset)) == IOID_FAIL)
		return -1;
	return iowait(ioid);
}
#endif

/*
 * Post asynch write from buffers mapped by an iovec.
 *
 * NB: Do *not* mix with system/library routines like write and pwrite
 * and caller must pre-position the file pointer.
 */
ioid_t
iwritev(int fd, const struct iovec *iov, int count)
{
	off_t	offset;

	offset = lseek(fd, 0, SEEK_CUR);
	if (offset < 0)
		return IOID_FAIL;
	return ipwritev(fd, iov, count, offset);
}

/*
 * Write from buffer.
 *
 * NB: Do *not* mix with system/library routines like write and pwrite
 * and caller must pre-position the file pointer.
 */
ioid_t
iwrite(int fd, const void *buf, size_t count)
{
	struct iovec iov;

	iov.iov_base = (char *)buf;
	iov.iov_len = count;
	return iwritev(fd, &iov, 1);
}
