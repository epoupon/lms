# LMS - Lightweight Music Server

LMS is a self-hosted music streaming software: access your music collection from anywhere using a web interface!

A [demo](http://lms.demo.poupon.io) instance is available, with the following limitations:
- Settings cannot be saved
- No persistent playqueue
- No administration panel

## Main features
- Low memory requirement (the demo instance runs on a Raspberry Pi3B+, using less than 10% of total memory even when transcoding)
- User management
- Recommendation engine
- Audio transcode for maximum interoperability and low bandwith requirements
- Persistent play queue across sessions
- Subsonic API
- Album artist
- Multi-value tags (artists, genres, ...)
- Custom tags (ex: "mood", "genre", "albummood", "albumgrouping", ...)
- MusicBrainzID support to handle duplicated artist and release names
- Playlists (only using Subsonic API for now)
- Starred Album/Artist/Tracks (only using Subsonic API for now)

## Recommendation engine
LMS provides several ways to help you find the music you like:
- Tag-based filters (ex: "Rock", "Metal" and "Aggressive", "Electronic" and "Relaxed", ...)
- Recommendations for similar artists and albums
- Radio mode
- Searches in album, artist and track names
- Most played/Recently added music

The recommendation engine makes use of [Self-Organizing Maps](https://en.wikipedia.org/wiki/Self-organizing_map).</br>
Please note this engine:
- may require some significant computation time on very large collections
- makes use of computed data available on [AcousticBrainz](https://acousticbrainz.org/). Therefore your music files must contain the [MusicBrainz Identifier](https://musicbrainz.org/doc/MusicBrainz_Identifier) for the recommendation engine to work properly (otherwise, only tag-based recommendations are provided)

## Subsonic API
The API version implemented is 1.12.0 and has been tested on Android using the official application, Ultrasonic and DSub.

Since LMS uses metadata tags to organize data, a compatibility mode is used to navigate through the collection using the directory browsing commands.

The Subsonic API is enabled by default.

## Installation
Here are the required packages to build LMS on Debian Stretch:
```sh
apt-get install g++ autoconf automake libboost-filesystem-dev libboost-system-dev libavcodec-dev libavutil-dev libavformat-dev libav-tools libmagick++-dev libpstreams-dev libconfig++-dev libpstreams-dev ffmpeg libtag1-dev
```

You also need wt4, that is not packaged yet on Debian. See [installation instructions](https://www.webtoolkit.eu/wt/doc/reference/html/InstallationUnix.html). You may need to build Wt4 in "Release" mode if you want to compile it natively on a Raspberry Pi3B+.

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

```sh
make install
```
This command requires root privileges.

## Configuration
LMS uses a configuration file, installed in '/etc/lms.conf'. It is recommended to edit this file and change the relevant settings (working directory, listen port, etc.)

All other settings are set using the web interface.

It is highly recommended to run LMS as a non root user. Therefore make sure the user has write permissions on the working directory.

## Reverse proxy settings
You have to set the 'behind-reverse-proxy' option to 'true' in the configuration file.

Here is an example to make LMS properly work on myserver.org using nginx.
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

## Running
```sh
lms [config_file]
```
Logs are output in the working directory, in the files 'lms.log' and 'lms.access.log'.

To connect to LMS, just open your favorite browser and go to http://localhost:5082

## Credits
- Wt (http://www.webtoolkit.eu/)
- bootstrap3 (http://getbootstrap.com/)
- ffmpeg project (https://ffmpeg.org/)
- Magick++ (http://www.imagemagick.org/Magick++/)
- MetaBrainz (https://metabrainz.org/)
- Bootstrap Notify: https://github.com/mouse0270/bootstrap-notify
