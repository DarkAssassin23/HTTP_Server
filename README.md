# Multi-threaded HTTP Server

About
----------------
This is a multi-threaded HTTP server written in C, along with a client
script to connect to it. 

This HTTP server supports the following HTTP requests:
  * GET
  * HEAD
  * OPTIONS 

Building and Deploying with Docker
----------------
You can build and deploy this server with docker. To build the docker image
use the provided `Dockerfile`.
```bash
docker build -t http_server .
```

Once you build the docker image you can run it with the following command
```bash
docker run -d -p 4080:4080 http_server
```
