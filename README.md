# LMS - Lightweight Media Server

LMS is a self-hosted media streaming software, released under the GPLv3 license.
It allows you to access your music using an http(s) web interface.

## Features
 - Audio transcode for maximum interoperability and low bandwith requirements
 - User management
 - Browse your audio collection using tag-based filters
 - Custom tags scan
 - MusicBrainzID support to handle duplicated artist and release names
 - Reverse proxy support

LMS is written entirely in C++. Therefore, it is suitable to run on embedded devices, where space and memory are limited.

## Dependencies
### Debian

```sh
apt-get install g++ autoconf automake libboost-filesystem-dev libboost-system-dev libavcodec-dev libavutil-dev libavformat-dev libav-tools libwtdbosqlite-dev libwthttp-dev libwtdbo-dev libwt-dev libmagick++-dev libpstreams-dev libconfig++-dev libpstreams-dev ffmpeg libtag1-dev
```

## Build

```sh
git clone https://github.com/epoupon/lms.git lms
cd lms
autoreconf -vfi
mkdir build
cd build
../configure --prefix=/usr --sysconfdir=/etc
```
configure will report any missing library.

```sh
make
```

## Install

```sh
make install
```
This command requires root privileges

## Configuration
LMS uses a configuration file, installed in '/etc/lms.conf'
It is recommended to edit this file and change the relevant settings (working directory, listen port, etc.)

All other settings are set using the web interface.

It is highly recommended to run LMS as a non root user. Therefore make sure the user has write permissions on the working directory.

## Running
```sh
lms [config_file]
```
Logs are output in the working directory, in the file 'lms.log'

To connect to LMS, just open your favorite browser and go to http://localhost:5082

## Credits

- Wt (http://www.webtoolkit.eu/)
- bootstrap3 (http://getbootstrap.com/)
- ffmpeg project (https://ffmpeg.org/)
- Magick++ (http://www.imagemagick.org/Magick++/)
- MetaBrainz (https://metabrainz.org/)
