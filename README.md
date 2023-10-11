# LMS - Lightweight Music Server

[![Last Release](https://img.shields.io/github/v/release/epoupon/lms?logo=github&label=latest)](https://github.com/epoupon/lms/releases)

_LMS_ is a self-hosted music streaming software: access your music collection from anywhere using a web interface!

A [demo instance](http://lms-demo.poupon.dev) is available. Note the administration panel is not available.

## Main features
* Recommendation engine
* Audio transcoding for maximum interoperability and reduced bandwith requirements
* Multi-value tags: `genre`, `albumgenre`, `mood`, `albummood`, `albumgrouping`, ...
* Artist relationships: `composer`, `conductor`, `lyricist`, `mixer`, `performer`, `producer`, `remixer`
* [Release types](https://musicbrainz.org/doc/Release_Group/Type): `album`, `single`, `EP`, `compilation`, `live`, ...
* [MusicBrainz Identifier](https://musicbrainz.org/doc/MusicBrainz_Identifier) support to handle duplicated artist and release names
* [ListenBrainz](https://listenbrainz.org) support for:
  * Scrobbling and synchronizing listens
  * Synchronizing 'love' feedbacks
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

## About tags
_LMS_ relies exclusively on tags to organize your music collection.

### Filtering
You can specify the tags you want to use to filter your collection. By default, `genre`, `albumgrouping`, `mood` and `albummood` tags are used.
In the administration  panel, you can set whatever tags you want, even custom tags.

__Note__: you can use the `lms-metadata` tool to have an idea of the tags parsed by _LMS_ using [TagLib](https://github.com/taglib/taglib).

### Multiple album artists
_LMS_ requires the `ALBUMARTISTS` and `ALBUMARTISTSSORT` tags to properly handle multiple album artists on the same album. As they are a custom tags, you may need to setup your favorite tagger to add them.

__Note__: if you use [Picard](https://picard.musicbrainz.org/), add the following script to include these tags:
```
$setmulti(albumartists,%_albumartists%)
$setmulti(albumartistssort,%_albumartists_sort%)
```

## Keyboard shortcuts
* Play/pause: <kbd>Space</bbd>
* Previous track: <kbd>Ctrl</kbd> + <kbd>Left</kbd>
* Next track: <kbd>Ctrl</kbd> + <kbd>Right</kbd>

## Installation
See [INSTALL.md](INSTALL.md) file.

## Contributing
Any feedback is welcome:
* feel free to participate in [discussions](https://github.com/epoupon/lms/discussions) if you have questions,
* report any bug or request for new features in the [issue tracker](https://github.com/epoupon/lms/issues),
* submit your pull requests based on the [develop](../../tree/develop) branch.
