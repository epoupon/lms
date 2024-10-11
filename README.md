# LMS - Lightweight Music Server

[![Last Release](https://img.shields.io/github/v/release/epoupon/lms?logo=github&label=latest)](https://github.com/epoupon/lms/releases)

_LMS_ is a self-hosted music streaming software: access your music collection from anywhere using a web interface!

A [demo instance](http://lms-demo.poupon.dev) is available. Note the administration settings are not available.

## Main features
* Recommendation engine
* Multi-valued tags: `genre`, `mood`, `artists`, ...
* Artist relationships: `composer`, `conductor`, `lyricist`, `mixer`, `performer`, `producer`, `remixer`
* [Release types](https://musicbrainz.org/doc/Release_Group/Type): `album`, `single`, `EP`, `compilation`, `live`, ...
* [MusicBrainz Identifier](https://musicbrainz.org/doc/MusicBrainz_Identifier) support to handle duplicated artist and release names
* [ListenBrainz](https://listenbrainz.org) support for:
  * Scrobbling and synchronizing listens
  * Synchronizing 'love' feedbacks
* Audio transcoding for maximum interoperability and reduced bandwith requirements
* Multi-library support
* ReplayGain support
* User management, with several [authentication backends](INSTALL.md#authentication-backend)
* [Subsonic/OpenSubsonic API](SUBSONIC.md) support

## Music discovery
_LMS_ provides several ways to help you find the music you like:
* Tag-based filters (ex: "_Rock_", "_Metal_ and _Aggressive_", "_Electronic_ and _Relaxed_", ...)
* Recommendations for similar artists and albums
* Radio mode, with endless filling of the play queue with tracks similar to what is there
* Searches in album, artist and track names (including sort names)
* Starred Albums/Artists/Tracks
* Random/Starred/Most played/Recently played/Recently added for Artist/Albums/Tracks, allowing you to search for things like:
  * Recently added _Electronic_ artists
  * Random _Metal_ and _Aggressive_ albums
  * Most played _Relaxed_ tracks
  * Starred _Jazz_ albums
  * ...

__Note__: depending on your database size and/or your hardware, the tag-based recommendation engine may significantly slow down the user interface. You can disable it in the administration settings.

## About tags
_LMS_ primarily relies on tags to organize your music collection but also supports browsing by directory using the [Subsonic/OpenSubsonic API](SUBSONIC.md).

### Filtering
It is possible to apply global filters on your collection using `genre`, `mood`, `grouping`, `language`, and by music library. More tags, including custom ones, can be added in the database administration settings.

__Note__: You can use the `lms-metadata` tool to get an idea of the tags parsed by _LMS_.

### Multiple artists
_LMS_ works best when using the default [Picard](https://picard.musicbrainz.org/) settings, where the `artist` tag contains a single display-friendly value, and the `artists` tag holds the actual artist names. This ensures a cleaner, more organized representation of artist names, when multiple artists are involved.

### Multiple album artists
While LMS can manage multiple album artists using the `albumartist` tag, it works better when using the custom `albumartists` and `albumartistssort` tags, similar to how it handles regular artist tags.

__Note__: if you use Picard, add the following script to include these tags:
```
$setmulti(albumartists,%_albumartists%)
$setmulti(albumartistssort,%_albumartists_sort%)
```

## Keyboard shortcuts
* Play/pause: <kbd>Space</kbd>
* Previous track: <kbd>Ctrl</kbd> + <kbd>Left</kbd>
* Next track: <kbd>Ctrl</kbd> + <kbd>Right</kbd>

## Installation
See [INSTALL.md](INSTALL.md) file.

## Contributing
Any feedback is welcome:
* feel free to participate in [discussions](https://github.com/epoupon/lms/discussions) if you have questions,
* report any bug or request for new features in the [issue tracker](https://github.com/epoupon/lms/issues),
* submit your pull requests based on the [develop](../../tree/develop) branch.
