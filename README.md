# LMS - Lightweight Media Server

LMS is a self-hosted media streaming software, released under the GPLv3 license.
It allows you to access your music and your videos using an https web interface.

## Features
 - Winamp-like interface, suited for large databases
 - Audio/Video transcode for maximum interoperability and low bandwith requirements
 - User management
 - LMS API available in Protocol Buffers (see src/lms-api/proto/messages.proto)

Please note LMS is still under development and will gain more features in the future.

LMS is written entirely in C++. Therefore, it is suitable to run on embedded devices, where space and/or memory are limited.
Please note some media files may require significant CPU usage to be transcoded.

## Dependencies
### Debian or Ubuntu packages
```sh
$ apt-get install g++ autoconf automake libboost-dev libboost-locale-dev libboost-iostreams-dev libboost-log-dev libavcodec-dev libwtdbosqlite-dev libwthttp-dev libwtdbo-dev libwt-dev libmagick++-dev libavcodec-dev libavformat-dev libavutil
```

If you want to enable the LMS API, you need these additional packages:

```sh
$ apt-get install libprotobuf-dev libprotobuf-dev protobuf-compiler
```

Warning: your distrib may provide an outdated wt library. Using the latest Wt (>= 3.3.3) is highly recommended (see http://www.webtoolkit.eu/wt)

## Build

```sh
$ autoreconf -vfi
$ mkdir build
$ cd build
$ ../configure
```

configure will complain if a mandatory library is missing.

The configure script allows you to disable the LMS API support, see 'configure --help' for more information.
```sh
$ make
```

## Setting up SSL materials
This is required by the SSL server and the web server.

Here is just a self signed certificate example, you could do use a CA if you want.

Generate a dh file:
```sh
$ openssl dhparam -out dh2048.pem 2048
```

Generate a self signed certificate:
```sh
$ openssl req -x509 -out cert.pem -keyout privkey.pem -newkey rsa:4096
```

## Running

In the build directory:
```sh
$ ./src/lms lms.conf
```

Create the lms.conf file using the etc/lms.sample.conf
 - "ui.resources.approot" has to refer to the ui/approot/ directory.
 - "ui.resources.docroot" has to refer to the ui/docroot/ directory
You must also link Wt's docroot (usually located in /usr/share/Wt) into that directory. Example:
```sh
ln -s /var/lms/docroot/resources /usr/share/Wt/resources
```

Depending on your SSL parameters, you may be asked for the PEM passphrase to unlock the private key.

## Credits

- Wt (http://www.webtoolkit.eu/)
- bootstrap3 (http://getbootstrap.com/)
- libav project (https://www.libav.org/).
- Protocol Buffers (https://github.com/google/protobuf/)
                                                                                                                                                                       73,1          Bot
