# LMS - Lightweight Music Server

![GitHub release (latest by date)](https://img.shields.io/github/v/release/epoupon/lms) [![Build Status](https://travis-ci.org/epoupon/lms.svg?branch=master)](https://travis-ci.org/epoupon/lms) [![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/epoupon/lms.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/epoupon/lms/context:cpp)

_LMS_ is a self-hosted music streaming software: access your music collection from anywhere using a web interface!

A [demo instance](http://lms.demo.poupon.io) is available. Note the administration panel is not available.

## Main features
* Low memory requirements: the demo instance runs on a _Raspberry Pi Zero W_
* Recommendation engine
* Audio transcode for maximum interoperability and low bandwith requirements
* Multi-value tags: artists, genres, composers, lyricists, moods, ...
* Compilation support
* [MusicBrainz Identifier](https://musicbrainz.org/doc/MusicBrainz_Identifier) support to handle duplicated artist and release names
* Disc subtitles support
* ReplayGain support
* Persistent play queue across sessions
* _Systemd_ integration
* User management
* Subsonic API, with the following additional features:
  * Playlists
  * Bookmarks

## Music discovery
_LMS_ provides several ways to help you find the music you like:
* Tag-based filters (ex: _Rock_, _Metal_ and _Aggressive_, _Electronic_ and _Relaxed_, ...)
* Recommendations for similar artists and albums
* Radio mode, based on what is in the current playqueue
* Searches in album, artist and track names (including sort names)
* Starred Albums/Artists/Tracks
* Various tags to help you filter your music: _mood_, _albummood_, _albumgenre_, _albumgrouping_, ...
* Random/Starred/Most played/Recently played/Recently added for Artist/Albums/Tracks, allowing you to search for things like:
  * Recently added _Electronic_ artists
  * Random _Metal_ and _Aggressive_ albums
  * Most played _Relaxed_ tracks
  * Starred _Jazz_ albums
  * ...

The recommendation engine uses two different sources:
1. Tags that are present in the audio files
2. Acoustic similarities of the audio files, using a trained [Self-Organizing Map](https://en.wikipedia.org/wiki/Self-organizing_map)

__Notes on the self-organizing map__:
* training the map requires significant computation time on large collections (ex: half an hour for 40k tracks)
* audio acoustic data is pulled from [AcousticBrainz](https://acousticbrainz.org/). Therefore your audio files _must_ contain the [MusicBrainz Identifier](https://musicbrainz.org/doc/MusicBrainz_Identifier).
* to enable the audio similarity source, you have to enable it first in the administration panel.

## Subsonic API
The API version implemented is 1.12.0 and has been tested on _Android_ using the official application, _Ultrasonic_ and _DSub_.

Since _LMS_ uses metadata tags to organize music, a compatibility mode is used to navigate through the collection using the directory browsing commands.

The Subsonic API is enabled by default.

__Note__: since _LMS_ may store hashed and salted passwords or may forward authentication requests to external services, it cannot handle the __token authentication__ method defined from version 1.13.0.

## Keyboard shortcuts
* Play/pause: <kbd>Space</bbd>
* Previous track: <kbd>Ctrl</kbd> + <kbd>Left</kbd>
* Next track: <kbd>Ctrl</kbd> + <kbd>Right</kbd>

## Installation

See [INSTALL.md](INSTALL.md) file.

## Contributing

Contributions are welcome! Please submit your pull requests against the [develop](../../tree/develop) branch.
