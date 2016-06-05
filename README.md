# LMS - Lightweight Media Server

LMS is a self-hosted media streaming software, released under the GPLv3 license.
It allows you to access your music and your videos using an http(s) web interface.

## Features
 - Winamp-like interface, suited for large media collections
 - Audio/Video transcode for maximum interoperability and low bandwith requirements
 - User management
 - Playlist support
 - MusicBrainzID support to handle duplicated artist and release names

Tags are automatically created to facilitate music searching, using:
 - Multi genre information from metadata
 - High-level information from AcousticBrainz

Please note LMS is still under development and will gain more features in the future:
 - Radio function
 - Interface suited for mobile devices
 - Video support
 - Subsonic and/or Ampache API

LMS is written entirely in C++. Therefore, it is suitable to run on embedded devices, where space and memory are limited.
Please note some media files may require significant CPU usage to be transcoded.

## Dependencies
### Debian

```sh
$ apt-get install g++ autoconf automake libboost-dev libboost-locale-dev libboost-iostreams-dev libavcodec-dev libwtdbosqlite-dev libwthttp-dev libwtdbo-dev libwt-dev libmagick++-dev libavcodec-dev libavformat-dev libav-tools libpstreams-dev libcurl-dev libcurlpp-dev libconfig++-dev
```

## Build

```sh
$ autoreconf -vfi
$ mkdir build
$ cd build
$ ../configure --prefix=/usr --sysconfdir=/etc
```
configure will complain if a mandatory library is missing.

```sh
$ make -j 4
```

## Install

```sh
$ make install
```
This command requires root privileges

## Configuration
LMS uses a config file for low level settings:
- database file path, default is /var/lms/lms.db
- TLS settings, default is none
- listen address/port, default is 0.0.0.0 port 5081

Other settings are set using the web interface.
By default, LMS will read its configuration in the file '/etc/lms.conf'

## Running
```sh
$ /usr/bin/lms [config_file]
```
LMS needs write access to the directory used for the database.
It is highly recommended to run LMS as a non root user.

To connect to LMS, just open your favorite browser and go to http://localhost:5081

## Mobile

Add the following code in your /etc/wt/wt_config.xml file:
```
<application-settings location="/usr/bin/lms">
	<progressive-bootstrap>true</progressive-bootstrap>
</application-settings>
```

## Setting up SSL materials (optional)
Here is just a self signed certificate example, you could do use a CA if you want.

Generate a dh file:
```sh
$ openssl dhparam -out dh2048.pem 2048
```
Generate a self signed certificate:
```sh
$ openssl req -x509 -out cert.pem -keyout privkey.pem -newkey rsa:4096
```
Depending on your SSL parameters, you may be asked for the PEM passphrase to unlock the private key.

To connect to LMS, just open your favorite browser and go to https://localhost:5081

## Credits

- Wt (http://www.webtoolkit.eu/)
- bootstrap3 (http://getbootstrap.com/)
- libav project (https://www.libav.org/)
- Magick++ (http://www.imagemagick.org/Magick++/)
- MetaBrainz (https://metabrainz.org/)
