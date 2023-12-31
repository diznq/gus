# Gus 

Gus is a Go (also called Baduk or Weiqi) AI running on top ot `80s` web server.

It's written in C using [80s](https://github.com/diznq/80s) as webserver framework.

## Developing

Prerequisites:
- [80s](https://github.com/diznq/80s)

**Example directory hierarchy initialization**

```sh
# install Lua shared libraries using OS package manager

# download and build 80s web server with dynamic linking
git clone https://github.com/diznq/80s.git
cd ./80s
LINK=dynamic ./build.sh

# download master server source code to crymp/ directory
git clone https://github.com/diznq/gus.git i.gus/

# build the gus module
LINK=dynamic i.gus/build.sh
```

## Running
To run the environment, first run `source i.gus/env.sh` and then `bin/80s server/http.lua -c 1 -m bin/gus.so` to run the server locally.

The game is accessible at [http://localhost:8080/gus/](http://localhost:8080/gus/) after

### UI example

![Example game](docs/gus.png)