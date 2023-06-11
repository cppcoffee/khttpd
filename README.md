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

First load `khttpd.ko` kernel model:

```bash
$ sudo insmod ./khttpd.ko port=8000 root=/home/ubuntu/khttpd
```

it is now listening to `0.0.0.0:8000` and can send requests.

Sending http request file:

```shell
$ curl http://10.211.55.4:8000/src/http_request.c -voa
  % Total    % Received % Xferd  Average Speed   Time    Time     Time  Current
                                 Dload  Upload   Total   Spent    Left  Speed
  0     0    0     0    0     0      0      0 --:--:-- --:--:-- --:--:--     0*   Trying 10.211.55.4:8000...
* Connected to 10.211.55.4 (10.211.55.4) port 8000 (#0)
> GET /src/http_request.c HTTP/1.1
> Host: 10.211.55.4:8000
> User-Agent: curl/7.88.1
> Accept: */*
>
< HTTP/1.1 200 OK
< Server: khttpd/0.0.1
< Connection: close
< Content-Length: 4007
<
{ [2896 bytes data]
100  4007  100  4007    0     0   522k      0 --:--:-- --:--:-- --:--:-- 1956k
* Closing connection 0
```

File SHA256

```shell
$ openssl dgst -sha256 ./a
SHA256(./a)= 2b9470d3437c346ff716bd9109f7541b286b67651e5eeec01b2629b236255b75

$ openssl dgst -sha256 /home/ubuntu/khttpd/src/http_request.c
SHA2-256(/home/ubuntu/khttpd/src/http_request.c)= 2b9470d3437c346ff716bd9109f7541b286b67651e5eeec01b2629b236255b75
```

## Parameters

The `khttpd.ko` module provides the following parameters with default values:

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



