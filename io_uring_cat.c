#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <liburing.h>

static int read_file_io_uring(struct io_uring *ring, const char *file)
{
	int ret = 0;
	int fd;

	fd = open(file, O_RDONLY);
	if (fd < 0) {
		fd = errno;
		perror("open");
		return fd;
	}

	while (1) {
		struct io_uring_sqe *sqe;
		struct io_uring_cqe *cqe;
		unsigned i, head;
		char buf[10][4096];
		int tmp;

		for (i = 0; i < 10; i++) {
			sqe = io_uring_get_sqe(ring);
			if (!sqe) {
				ret = ENOMEM;
				goto out;
			}
			io_uring_prep_read(sqe, fd, buf[i], sizeof(buf[i]), -1);
			sqe->flags |= IOSQE_IO_HARDLINK;
			sqe->user_data = (__u64)i;
		}
		sqe->flags &= ~IOSQE_IO_HARDLINK;

		tmp = io_uring_submit_and_wait(ring, 1);
		if (tmp < 0) {
			ret = -tmp;
			errno = ret;
			perror("io_uring_submit");
			break;
		}

		i = 0;
		io_uring_for_each_cqe(ring, head, cqe) {
			// size_t wr_ret;
			//wr_ret =
			write(1, buf[i], cqe->res);
			// (void)wr_ret;
			// printf("res = %d; u = %u\n", cqe->res,
			// 	(unsigned)cqe->user_data);
			i++;
			if (i == 10)
				break;
			if (cqe->res <= 0)
				goto out;
		}
		// printf("i = %u\n", i);
		io_uring_cq_advance(ring, i);
	}

out:
	close(fd);
	return ret;
}

int main(int argc, char *argv[])
{
	struct io_uring ring;
	int ret;

	if (argc != 2) {
		printf("Usage: %s <file>\n", argv[0]);
		return EINVAL;
	}

	ret = io_uring_queue_init(512, &ring, 0);
	if (ret < 0) {
		errno = -ret;
		perror("io_uring_queue_init");
		return 1;
	}

	ret = read_file_io_uring(&ring, argv[1]);
	io_uring_queue_exit(&ring);
	return ret;
}
