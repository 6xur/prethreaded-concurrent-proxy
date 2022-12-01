# Prethreaded Concurrent Proxy

This is a prethreaded concurrent web proxy with a 1MB web object cache that can handle HTTP/1.0 GET requests. The cache can handle objects up to 10KB and uses Pthread read/write locks for synchronisation. The proxy creates a fixed-sized thread pool at the start of its execution. The main thread listens for connections and pushes the connected file descriptors in a shared buffer. The worker threads then pop one descriptor at a time and handle the requests.

## Request Headers

The proxy always send the following headers.

```C
User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36;
Connection: close;
Proxy-Connection: close;
```

Specifying close as the value of the Connection and Proxy-Connection headers alert web servers that the proxy intends to close connections after the first request/response exchange is completed.

## How to Run:

The proxy can be started as follows.

```
./proxy port_number replacement_policy
```

The first argument spefies the port on which this proxy will listen for incoming connections. The second argument is the replacement policy, which can be either LRU (least recently used) or LFU (least frequently used).

For example, with the following command, the proxy should listen for connection on port 15214 and use LRU caching policy.

```
./proxy 15214 LRU
```