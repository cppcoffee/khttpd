#ifndef _SOCKET_H
#define _SOCKET_H

#include <linux/uio.h>
#include <linux/net.h>

int socket_recv(struct socket *sock, char *buf, size_t size);

int socket_send(struct socket *sock, const char *buf, size_t size);
int socket_send_vec(struct socket *sock, struct kvec *vec, size_t count);

size_t vec_bytes(struct kvec *vec, size_t count);

#endif
