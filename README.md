# khttpd

The linux kernel module for HTTP static file service.

## Prepare

Installing linux kernel headers on Ubuntu:

```bash
$ sudo apt-get install linux-headers-$(uname -r)
```

## build

```bash
$ make
```

## Usage

```bash
$ sudo insmod ./khttpd.ko port=8000 root=/var/www/html
```

The module provides the following parameters with default values:

```
# Listening address
listen_addr=0.0.0.0

# Listening port
port=8000

# Pending connections queue size
backlog=1024

# Sets the root directory for requests
root_dir=/var/www/html
```

## Reference

[https://github.com/sysprog21/khttpd](https://github.com/sysprog21/khttpd)

[https://github.com/h2o/picohttpparser](https://github.com/h2o/picohttpparser)



