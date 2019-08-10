# LMS - Lightweight Music Server

_LMS_ is a self-hosted music streaming software: access your music collection from anywhere using a web interface!

A [demo](http://lms.demo.poupon.io) instance is available, with the following limitations:
- Settings cannot be saved
- No persistent playqueue
- No administration panel

## Main features

* Low memory requirement (the demo instance runs on a Raspberry Pi3B+, using less than 10% of total memory even when transcoding)
* User management
* Recommendation engine
* Audio transcode for maximum interoperability and low bandwith requirements
* Persistent play queue across sessions
* Subsonic API
* Album artist
* Multi-value tags: artists, genres, ...
* Custom tags (ex: _mood_, _genre_, _albummood_, _albumgrouping_, ...)
* MusicBrainzID support to handle duplicated artist and release names
* Playlists, (only using Subsonic API for now)
* Starred Album/Artist/Tracks (only using Subsonic API for now)
* _Systemd_ integration

## Recommendation engine

_LMS_ provides several ways to help you find the music you like:
* Tag-based filters (ex: _Rock_, _Metal_ and _Aggressive_, _Electronic_ and _Relaxed_, ...)
* Recommendations for similar artists and albums
* Radio mode
* Searches in album, artist and track names
* Most played/Recently added music

The recommendation engine makes use of [Self-Organizing Maps](https://en.wikipedia.org/wiki/Self-organizing_map).</br>
Please note this engine:
* may require some significant computation time on large collections (ex: half a hour for 40k tracks)
* makes use of computed data available on [AcousticBrainz](https://acousticbrainz.org/). Therefore your music files must contain the [MusicBrainz Identifier](https://musicbrainz.org/doc/MusicBrainz_Identifier) for the recommendation engine to work properly (otherwise, only tag-based recommendations are provided)

## Subsonic API
The API version implemented is 1.12.0 and has been tested on _Android_ using the official application, _Ultrasonic_ and _DSub_.

Since _LMS_ uses metadata tags to organize music, a compatibility mode is used to navigate through the collection using the directory browsing commands.

The Subsonic API is enabled by default.

__Note__: since _LMS_ stores hashed and salted passwords, it cannot handle the __token authentication__ method defined from 1.13.0.

## Installation

__Note__: this installation process and the default values of the configuration file and the _systemd_ service file have been written for _Debian Stretch_. Therefore, you may have to adapt commands and/or paths in order to fit to your distribution.

### Build dependencies
```sh
apt-get install g++ autoconf automake libboost-filesystem-dev libboost-system-dev libavcodec-dev libavutil-dev libavformat-dev libav-tools libmagick++-dev libpstreams-dev libconfig++-dev libpstreams-dev ffmpeg libtag1-dev
```

You also need _W4_, that is not packaged yet on _Debian_. See [installation instructions](https://www.webtoolkit.eu/wt/doc/reference/html/InstallationUnix.html).

### Build

Get the latest stable release and build it:
```sh
git clone https://github.com/epoupon/lms.git lms
cd lms
autoreconf -vfi
mkdir build
cd build
../configure --prefix=/usr
```
configure will report any missing library.

__Note__: in order to customize the installation directories, you can use the following options of the `configure` script:
* _--prefix_ (defaults to `/usr/local`).
* _--bindir_ (defaults to `$PREFIX/bin`).

```sh
make
```
__Note__: you can use `make -jN` to speed up compilation time (N is the number of compilation workers to spawn).

#### Raspberry 3 build notes

Due to memory limitations, you may need to build _Wt4_ in _Release_ mode if you want to compile it natively on a Raspberry Pi3B+.

__Note__: the build process is really long, roughly 1 hour to build _Wt4_ + _LMS_.

### Deployment

__Note__: the commands of this section require root privileges.

```sh
make install
```

Create a dedicated system user:
```sh
useradd --system lms
```

Copy the configuration files:
```sh
cp /usr/share/lms/default.conf /etc/lms.conf
cp /usr/share/lms/default.service /lib/systemd/system/lms.service
```

Create working directories and give access to the _lms_ user:
```sh
mkdir /var/lms
touch /var/log/lms.log
touch /var/log/lms.access.log
chown lms:lms /var/lms /var/log/lms.log /var/log/lms.access.log
```

__Note__: don't forget to give the _lms_ user read access to the music directory you want to scan.

### Configuration
_LMS_ uses a configuration file, installed by default in `/etc/lms.conf`. It is recommended to edit this file and change relevant settings (listen address, listen port, working directory, Subsonic API activation, ...).

All other settings are set using the web interface (user management, scan  settings, transcode settings, ...).

### Reverse proxy settings
_LMS_ is shipped with an embedded web server, but it is recommended to deploy behind a reverse proxy. You have to set the _behind-reverse-proxy_ option to _true_ in the `lms.conf` configuration file.

Here is an example to make _LMS_ properly work on _myserver.org_ using _nginx_.
```
server {
    listen 80;

    server_name myserver.org;

    access_log            /var/log/nginx/myserver.access.log;

    proxy_request_buffering off;
    proxy_buffering off;
    proxy_buffer_size 4k;

    location / {

      proxy_set_header        Host $host;
      proxy_set_header        X-Real-IP $remote_addr;
      proxy_set_header        X-Forwarded-For $proxy_add_x_forwarded_for;
      proxy_set_header        X-Forwarded-Proto $scheme;

      proxy_pass          http://localhost:5082;
      proxy_read_timeout  120;
    }
}
```

## Run
```sh
systemctl start lms
```
Two log files are used:
* `/var/log/lms.log`: logs of the `lms` daemon
* `/var/log/lms.access.log`: access logs of the embedded web server

To connect to _LMS_, just open your favorite browser and go to http://localhost:5082

To make _LMS_ run automatically during startup:
```sh
systemctl enable lms
```

## Upgrade

To upgrade `LMS`, you need to update the master branch and rebuild/install it:
```sh
cd build
git pull
make
```

Then using root privileges:
```sh
make install
systemctl lms restart
```

Read the [release notes](https://github.com/epoupon/lms/releases) to check if any relevant setting has been added into the configuration file.

Any change in the database schema is made automatically by the application during startup.

## Credits
* Wt (http://www.webtoolkit.eu/)
* bootstrap3 (http://getbootstrap.com/)
* ffmpeg project (https://ffmpeg.org/)
* Magick++ (http://www.imagemagick.org/Magick++/)
* MetaBrainz (https://metabrainz.org/)
* Bootstrap Notify: https://github.com/mouse0270/bootstrap-notify
