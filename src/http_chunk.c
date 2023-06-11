#include "http_chunk.h"
#include "http_response.h"
#include "socket.h"

int http_response_chunk_vec(struct http_worker *worker, struct kvec *vec,
			    int vlen)
{
	int n, pos;
	char buf[32];
	struct kvec *dst;

	pos = 0;
	n = vlen + 2;

	dst = kmalloc(sizeof(struct kvec) * n, GFP_KERNEL);
	if (dst == NULL) {
		return -ENOMEM;
	}

	// chunk data size
	n = snprintf(buf, sizeof(buf), "%zx" CRLF, vec_bytes(vec, vlen));
	buf[n] = '\0';

	dst[pos].iov_base = buf;
	dst[pos].iov_len = n;
	pos += 1;

	// copy data
	memcpy(dst + pos, vec, sizeof(struct kvec) * vlen);
	pos += vlen;

	// chunk data end
	dst[pos].iov_base = CRLF;
	dst[pos].iov_len = 2;
	pos += 1;

	n = socket_send_vec(worker->socket, dst, pos);

	kfree(dst);

	return n;
}

int http_response_chunk_data(struct http_worker *worker, void *data, size_t len)
{
	struct kvec vec;

	vec.iov_base = data;
	vec.iov_len = len;

	return http_response_chunk_vec(worker, &vec, 1);
}

int http_response_chunk_end(struct http_worker *worker)
{
	return socket_send(worker->socket, "0" CRLF CRLF, 5);
}
