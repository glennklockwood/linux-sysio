#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/queue.h>
#include <sched.h>
#include <assert.h>

#include "xtio.h"

/*
 * Stripe width.
 */
#define STRIPE_SIZE	(8 * 1024 * 1024)

/*
 * Asynchronous IO operation record.
 *
 * One for each posted operation.
 */
struct aop {
	TAILQ_ENTRY(aop) link;				/* posted ops link */
	enum {
		R,
		W
	} direction;					/* read/write */
	size_t	nbytes;					/* xfer count */
	struct iovec *iov;				/* memory regions */
	size_t	niov;					/* iov length */
	struct xtvec *xtv;				/* file regions */
	size_t	nxtv;					/* xtv length */
	ioid_t	ioid;					/* async IO ID */
};

/*
 * Queue of posted operations.
 */
TAILQ_HEAD(, aop) posted;

/*
 * Number of posted operations.
 */
unsigned nposted;

/*
 * Desired maximum posted queue length.
 */
unsigned noutstanding = 3;

void usage(void);
unsigned long lcm(unsigned long x, unsigned long y);

int
main(int argc, char *const argv[])
{
	int	i;
	int	keep = 0;
	int	itmp;
	const char *ipath, *opath;
	int	flags;
	int	ifd, ofd;
	size_t	bufsiz, rgsiz;
	size_t niov, nrg;
	off_t	off, fsiz;
	int	eof;
	struct aop *r;
	ssize_t	cc;
	size_t	n;
	struct iovec *iov;
	struct xtvec *xtv;

	/*
	 * Parse options.
	 */
	while ((i = getopt(argc, argv, "kn:")) != -1)
		switch (i) {
		case 'k':
			keep = 1;
			break;
		case 'n':
			itmp = atoi(optarg);
			if (itmp < 1) {
				(void )fprintf(stderr, "n >= 1\n");
				exit(1);
			}
			noutstanding = (unsigned )itmp;
			break;
		default:
			usage();
		}

	if (!(argc - optind))
		usage();
	/*
	 * Input file name.
	 */
	ipath = argv[optind++];
	if (!(argc - optind))
		usage();
	/*
	 * Output file name.
	 */
	opath = argv[optind++];
	if (argc - optind)
		usage();

	/*
	 * Initialize.
	 */
	TAILQ_INIT(&posted);
	nposted = 0;

	/*
	 * Open input file.
	 */
	flags = O_RDONLY;
#if _LARGEFILE64_SOURCE
	flags |= O_LARGEFILE;
#endif
	ifd = open(ipath, flags);
	if (ifd < 0) {
		perror(ipath);
		exit(1);
	}

	/*
	 * Open output file.
	 */
	flags = O_CREAT|O_WRONLY;
	flags |= keep ? O_EXCL : O_TRUNC;
#if _LARGEFILE64_SOURCE
	flags |= O_LARGEFILE;
#endif
	ofd = open(opath, flags, 0666);
	if (ofd < 0) {
		perror(opath);
		exit(1);
	}

	/*
	 * Parameterize. We want things mismatched.
	 */
	bufsiz = lcm(STRIPE_SIZE / 32, getpagesize());
	niov = STRIPE_SIZE / bufsiz;
	rgsiz = STRIPE_SIZE / 25;
	nrg = STRIPE_SIZE / rgsiz;
	off = 0;
	eof = 0;
	fsiz = 0;
	for (;;) {
		while (nposted >= noutstanding - 1 || (nposted && eof)) {
			/*
			 * Reap readers and post again as writes.
			 */
			r = posted.tqh_first;
			assert(r);
			i = iodone(r->ioid);
			if (!i) {
				sched_yield();
				break;
			}
			if (i < 0) {
				perror("iodone");
				exit(1);
			}
			cc = iowait(r->ioid);
			if (cc < 0) {
				perror(r->direction == R ? ipath : opath);
				exit(1);
			}
			if (r->direction == W && (size_t )cc != r->nbytes) {
				(void )fprintf(stderr,
					       "%s: short write (%d/%d)\n",
					       opath,
					       cc,
					       r->nbytes);
				exit(1);
			}
			if (cc == 0)
				eof = 1;
			else if (r->direction == R) {
				r->direction = W;
				fsiz += (unsigned long )cc;
				r->ioid =
				    iwritex(ofd,
					    r->iov, r->niov,
					    r->xtv, r->nxtv);
				if (r->ioid == IOID_FAIL) {
					perror(opath);
					exit(1);
				}
				/*
				 * Move it to the end of the queue.
				 */
				TAILQ_REMOVE(&posted, r, link);
				TAILQ_INSERT_TAIL(&posted, r, link);
				continue;
			}
			nposted--;

			TAILQ_REMOVE(&posted, r, link);
			for (n = 0, iov = r->iov; n < niov; n++, iov++)
				free(iov->iov_base);
			free(r->iov);
			free(r->xtv);
			free(r);
		}
		if (eof)
			break;
		if (nposted >= noutstanding)
			continue;

		/*
		 * Generate next read request.
		 */
		r = malloc(sizeof(struct aop));
		if (!r) {
			perror("malloc[aop]");
			exit(1);
		}
		r->direction = R;
		r->nbytes = 0;
		r->iov = malloc(niov * sizeof(struct iovec));
		if (!r->iov) {
			perror("malloc[iov]");
			exit(1);
		}
		n = r->niov = niov;
		while (n--) {
			iov = &r->iov[n];
			iov->iov_base = malloc(iov->iov_len = bufsiz);
			if (!iov->iov_base) {
				perror("malloc[iov_base]");
				exit(1);
			}
		}
		r->xtv = malloc(nrg * sizeof(struct xtvec));
		if (!r->xtv) {
			perror("malloc[xtv]");
			break;
		}
		n = r->nxtv = nrg;
		while (n--) {
			xtv = &r->xtv[n];
			xtv->xtv_off = off;
			r->nbytes += rgsiz;
			off += (ssize_t )(xtv->xtv_len = rgsiz);
		}

		r->ioid = ireadx(ifd, r->iov, r->niov, r->xtv, r->nxtv);
		if (r->ioid == IOID_FAIL) {
			perror(ipath);
			exit(1);
		}
		TAILQ_INSERT_TAIL(&posted, r, link);
		nposted++;
	}

	assert(!nposted);

	/*
	 * In the end, we almost certainly wrote a bunch of garbage at
	 * the tail of the file. Truncate now to the proper size, keeping
	 * only valid data.
	 */
	if (ftruncate(ofd, (off_t )fsiz) != 0) {
		perror(opath);
		exit(1);
	}

	/*
	 * Close files.
	 */
	if (close(ifd) != 0) {
		perror(ipath);
		exit(1);
	}
	if (close(ofd) != 0) {
		perror(opath);
		exit(1);
	}

	return 0;
}

void
usage()
{

	(void )fprintf(stderr,
		       "Usage: a.out [-k] [-n #] <in-path> <out-path>\n");
	exit(1);
}

/*
 * Greatest common denominator.
 */
unsigned long
gcd(unsigned long x, unsigned long y)
{
	unsigned long a, b, tmp;

	if (!(x && y)) {
		errno = EINVAL;
		return 0;
	}

	/*
	 * Euler's to find GCD.
	 */
	if (y > x) {
		tmp = y;
		x = y;
		y = tmp;
	}
	a = x;
	b = y;
	for (;;) {
		a %= (tmp = b);
		if (!a)
			break;
		a = b;
		b = tmp;
	}

	return b;
}

/*
 * Lowest common multiple.
 */
unsigned long
lcm(unsigned long x, unsigned long y)
{
	unsigned long tmp;

	if (y > x) {
		tmp = y;
		x = y;
		y = tmp;
	}

	tmp = gcd(x, y);
	if (tmp < 1 || tmp == ULONG_MAX) {
		errno = EINVAL;
		return 0;
	}
	return (x / gcd(x, y)) * y;
}
