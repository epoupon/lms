# LMS - Lightweight Music Server

[![Build Status](https://travis-ci.org/epoupon/lms.svg?branch=master)](https://travis-ci.org/epoupon/lms) ![GitHub release (latest by date)](https://img.shields.io/github/v/release/epoupon/lms) [![CodeFactor](https://www.codefactor.io/repository/github/epoupon/lms/badge/master)](https://www.codefactor.io/repository/github/epoupon/lms/overview/master)

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
* ReplayGain support
* Persistent play queue across sessions
* Compilation support
* Multi-value tags: artists, genres, ...
* Custom tags (ex: _mood_, _genre_, _albummood_, _albumgrouping_, ...)
* MusicBrainzID support to handle duplicated artist and release names
* Disc subtitles support
* _Systemd_ integration
* _PAM_ authentication backend
* Subsonic API, with the following additional features:
  * Playlists
  * Starred Album/Artist/Tracks
  * Bookmarks

## Music discovery

_LMS_ provides several ways to help you find the music you like:
* Tag-based filters (ex: _Rock_, _Metal_ and _Aggressive_, _Electronic_ and _Relaxed_, ...)
* Recommendations for similar artists and albums
* Radio mode, based on what is in the current playqueue
* Searches in album, artist and track names (including sort names)
* Random/Most played/Recently played/Recently added for Artist/Albums/Tracks, allowing you to search for things like:
  * Recently added _Electronic_ artists
  * Random _Metal_ and _Aggressive_ albums
  * Most played _Relaxed_ tracks
  * ...

The recommendation engine uses two different sources:
1. Tags that are present in the audio files
2. Acoustic similarities of the audio files, using a trained [Self-Organizing Map](https://en.wikipedia.org/wiki/Self-organizing_map)

__Notes on the self-organizing map__:
* training the map requires significant computation time on large collections (ex: half an hour for 40k tracks)
* audio acoustic data is pulled from [AcousticBrainz](https://acousticbrainz.org/). Therefore your audio files _must_ contain the [MusicBrainz Identifier](https://musicbrainz.org/doc/MusicBrainz_Identifier).
* to enable the audio similarity source, you have to enable it first in the settings panel.

## Subsonic API
The API version implemented is 1.12.0 and has been tested on _Android_ using the official application, _Ultrasonic_ and _DSub_.

Since _LMS_ uses metadata tags to organize music, a compatibility mode is used to navigate through the collection using the directory browsing commands.

The Subsonic API is enabled by default.

__Note__: since _LMS_ stores hashed and salted passwords, it cannot handle the __token authentication__ method defined from version 1.13.0.

## Keyboard shortcuts
* Play/pause: Space
* Previous track: Ctrl + Left
* Next track: Ctrl + Right

## Installation

### Docker
_Docker_ images are available, please see detailed instructions on https://hub.docker.com/r/epoupon/lms.

### Debian Buster packages
_Buster_ packages are provided for _amd64_ and _armhf_ architectures.

As root, trust the following debian package provider and add it in your list of repositories:
```sh
wget -O - https://debian.poupon.io/apt/debian/epoupon.gpg.key | apt-key add -
echo "deb https://debian.poupon.io/apt/debian buster main" > /etc/apt/sources.list.d/epoupon.list
```

To install or upgrade _LMS_:
```sh
apt update
apt install lms
```

The _lms_ service is started just after the package installation, run by a dedicated _lms_ system user.</br>
Please refer to [Deployment](#deployment) for further configuration options.

### From source
__Note__: this installation process and the default values of the configuration files have been written for _Debian Buster_. Therefore, you may have to adapt commands and/or paths in order to fit to your distribution.

#### Build dependencies
__Notes__:
* a C++17 compiler is needed
* ffmpeg version 4 minimum is required
```sh
apt-get install g++ cmake libboost-system-dev libavutil-dev libavformat-dev libgraphicsmagick++1-dev libconfig++-dev libpstreams-dev ffmpeg libtag1-dev libpam0g-dev
```

__Note__: package libpam0g-dev is optional (only for using PAM authentication)

You also need _Wt4_, which is not packaged yet on _Debian_. See [installation instructions](https://www.webtoolkit.eu/wt/doc/reference/html/InstallationUnix.html).</br>
No optional requirement is needed, except openSSL if you plan not to deploy behind a reverse proxy (which is not recommended).

#### Build

Get the latest stable release and build it:
```sh
git clone https://github.com/epoupon/lms.git lms
cd lms
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
```
__Note__: in order to customize the installation directory, you can use the _-DCMAKE_INSTALL_PREFIX_ option (defaults to `/usr/local`).

```sh
make
```
__Note__: you can use `make -jN` to speed up compilation time (N is the number of compilation workers to spawn).

#### Installation

__Note__: the commands of this section require root privileges.

```sh
make install
```

Create a dedicated system user:
```sh
useradd --system --group lms
```

Copy the configuration files:
```sh
cp /usr/share/lms/lms.conf /etc/lms.conf
cp /usr/share/lms/lms.service /lib/systemd/system/lms.service
```

Create the working directory and give it access to the _lms_ user:
```sh
mkdir /var/lms
chown lms:lms /var/lms
```

To make _LMS_ run automatically during startup:
```sh
systemctl enable lms
```

#### Upgrade

To upgrade _LMS_ from sources, you need to update the master branch and rebuild/install it:
```sh
cd build
git pull
make
```

Then using root privileges:
```sh
make install
systemctl restart lms
```

## Deployment

__Note__: don't forget to give the _lms_ user read access to the music directory you want to scan.

### Configuration
_LMS_ uses a configuration file, installed by default in `/etc/lms.conf`. It is recommended to edit this file and change relevant settings (listen address, listen port, working directory, Subsonic API activation, deployment path, ...).

All other settings are set using the web interface (user management, scan settings, transcode settings, ...).

If a setting is not present in the configuration file, a hardcoded default value is used (the same as in the [default.conf](https://github.com/epoupon/lms/blob/master/conf/lms.conf) file)

### Deploy on non root path
If you want to deploy on non root path (e.g. https://mydomain.com/newroot/), you have to set the `deploy-path` option accordingly in `lms.conf`.

As static resources are __not__ related to the `deploy-path` option, you have to perform the following steps if you want them to be on a non root path too:
* Create a new intermediary `newroot` directory in `/usr/share/lms/docroot` and move everything in it.
* Symlink `/usr/share/lms/docroot/newroot/resources` to `/usr/share/Wt/resources`.
* Edit `lms.conf` and set:
```
wt-resources = "" # do not comment the whole line
docroot = "/usr/share/lms/docroot/;/newroot/resources,/newroot/css,/newroot/images,/newroot/js,/newroot/favicon.ico";`
deploy-path = "/newroot/"; # ending slash is important
```

If you use nginx as a reverse proxy, you can simply replace `location /` with `location /newroot/` to achieve the same result.

### Reverse proxy settings
_LMS_ is shipped with an embedded web server, but it is recommended to deploy behind a reverse proxy. You have to set the _behind-reverse-proxy_ option to _true_ in the `lms.conf` configuration file.

Here is an example to make _LMS_ properly work on _myserver.org_ using _nginx_:
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

      proxy_pass          http://localhost:5082/;
      proxy_read_timeout  120;
    }
}
```

## Run
```sh
systemctl start lms
```

Log traces can be accessed using journactl:
```sh
journalctl -u lms.service
```

To connect to _LMS_, just open your favorite browser and go to http://localhost:5082

## Credits
* Bootstrap Notify: https://github.com/mouse0270/bootstrap-notify
* Bootstrap3 (https://getbootstrap.com/)
* Bootswatch (https://bootswatch.com/)
* Ffmpeg project (https://ffmpeg.org/)
* GraphicsMagick++ (http://www.graphicsmagick.org/)
* MetaBrainz (https://metabrainz.org/)
* Wt (http://www.webtoolkit.eu/)
