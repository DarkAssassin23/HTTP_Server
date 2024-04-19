# Multi-threaded HTTP Server

## Table of Contents
  * [About](#about)
  * [Building](#building)
  * [Using Docker](#building-and-deploying-with-docker)
  * [Configuration](#configuring)
    * [Examples](#config-examples)
    * [Configuration with Docker](#additional-docker-configuration-steps)
  * [Using the Client Script](#using-the-client-script)

## About
This is a multi-threaded HTTP server written in C, along with a client
script to connect to it for testing. 

This HTTP server supports the following HTTP requests:
  * GET
  * HEAD
  * OPTIONS 

## Building
The repo includes a `makefile`, which will build the server from source.
To build the server from source, run `make`. If you wish to build the
release version, run `make release`.

## Building and Deploying with Docker
The easiest way to get this server up and running is by using the included
`docker-compose.yml` file. All you need to do to get the server running is
run:
```bash
docker-compose up -d
```
Then, to shutdown the container:
```bash
docker-compose down
```
If you make changes to the source and need to rebuild the container, you can
run:
```bash
docker-compose build
```
When you spin up the container for the first time, by default, it will create
an `html` folder in your current directory, if one doesn't already exist. This
is where you will put all of your pages for your web server for the server to
display them.

## Configuring
The server will create an `http.conf` file upon running for the first time,
assuming one doesn't already exist. This file allows you to configure things
such as how many threads the server should run with, the default location in
the file system the server should look for files, the name of the server, etc.

If you wish to change one of the configuration options, uncomment the value
and input your desired value.

### Config Examples
```conf
...
# The number of threads you want the server to run with.
# threads 20
...
```
```conf
...
# The number of threads you want the server to run with.
threads 16
...
```
In this example, the server will now run with 16 threads in its thread pool
rather than the default of 20.

```conf
...
# The name you wish to call the server.
# name HTTP Server
...
```
```conf
...
# The name you wish to call the server.
name The Beast
...
```
In this example, the name of the server will now be `The Beast`. This change
is reflected in the HTTP response messages from the server.

> [!NOTE]
> In order for config changes to take effect, you need to restart the
> server/container.

### Additional Docker Configuration Steps
Like the `html` folder, a `cfg` folder gets created, which houses the server's
`http.conf` file. If you make changes to this config file, you may need to 
take additional steps in addition to just restarting the container. 

If you modify the `port` or `html_root` in the `http.conf` file, you **will**
need to update the `docker-compose.yml` file, and, if you change the `port`,
the `Dockerfile`. Modifying the `port` will require you to change the
`EXPOSE` value in the `Dockerfile` and `ports` value in the 
`docker-compose.yml` file. Modifying the `html_root` will require you to
change ` - ./html:/var/www/html` under `volumes` in the `docker-compose.yml`
to be ` - ./html:[html_root]`

## Using the Client Script
As mentioned in the [About](#about) section, this repo contains a client
script. The script is a python script which will randomly select from a series
of possible HTTP requests to send to the server. Before you run the script,
make sure the `HOST` matches the IP address of your server, and the `PORT`
also matches the port your server is running on.

With that configured, you can run the script via `python basic_client.py`.

In addition to the Python script, there is also a Bash script to simulate many
simultaneous requests. By default, it will run the `basic_client.py` script
150 times. However, you can add more requests by a command line argument. 
For example, `./many_clients.sh 200` will create 200 simultaneous requests,
rather than 150.
