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

#include <aio.h>
#include <unistd.h>
#include <errno.h>

#include "sysio.h"

/* Retrieve error status associated with AIOCBP.  */
int 
aio_error (const struct aiocb *aiocbp) 
{
	return aiocbp->__error_code;
}

/* Return status associated with AIOCBP.  */
ssize_t 
aio_return (struct aiocb *aiocbp)
{
	return aiocbp->__return_value;
}


/* Retrieve error status associated with AIOCBP.  */
int 
aio_error64 (const struct aiocb64 *aiocbp) 
{
	return aiocbp->__error_code;
}


/* Return status associated with AIOCBP.  */
ssize_t 
aio_return64 (struct aiocb64 *aiocbp)
{
	return aiocbp->__return_value;
}


int
aio_suspend(const struct aiocb * const cblist[] __IS_UNUSED,
	    int n __IS_UNUSED,
	    const struct timespec *timeout __IS_UNUSED)
{

	return 0;
}

static ssize_t
perform(int opcode, int fd, void *buf, ssize_t nbytes, off64_t offset)
{
	ssize_t	cc;

	switch (opcode) {
	case LIO_READ:
		cc = pread64(fd, buf, nbytes, offset);
		break;
	case LIO_WRITE:
		cc = pwrite64(fd, buf, nbytes, offset);
		break;
	default:
		cc = -1;
		errno = EINVAL;
	}

	return cc;
}

int 
lio_listio(int mode __IS_UNUSED,
	   struct aiocb * const list[],
	   int nent,
	   struct sigevent * sig __IS_UNUSED)
{
	int i;
	struct aiocb *acb;

	/* 
	 * for each of the nent entries in list, perform requested 
	 * operation
	 */
	for (i=0; i < nent; i++) {
		acb = list[i];
		if (!acb)
			continue;
		acb->__return_value =
		    perform(acb->aio_lio_opcode,
			    acb->aio_fildes,
			    (void *)acb->aio_buf,
			    acb->aio_nbytes,
			    acb->aio_offset);
		acb->__error_code = acb->__return_value < 0 ? errno : 0;
	}
	return 0;
}

int 
lio_listio64(int mode __IS_UNUSED,
	     struct aiocb64 * const list[],
	     int nent,
	     struct sigevent * sig __IS_UNUSED)
{
	int i;
	struct aiocb64 *acb;

	/* 
	 * for each of the nent entries in list, perform requested 
	 * operation
	 */
	for (i=0; i < nent; i++) {
		acb = list[i];
		if (!acb)
			continue;
		acb->__return_value =
		    perform(acb->aio_lio_opcode,
			    acb->aio_fildes,
			    (void *)acb->aio_buf,
			    acb->aio_nbytes,
			    acb->aio_offset);
		acb->__error_code = acb->__return_value < 0 ? errno : 0;
	}
	return 0;
}
