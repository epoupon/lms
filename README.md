# LMS - Lightweight Music Server

[![Last Release](https://img.shields.io/github/v/release/epoupon/lms?logo=github&label=latest)](https://github.com/epoupon/lms/releases)

_LMS_ is a self-hosted music streaming software: access your music collection from anywhere using a web interface!

A [demo instance](http://lms-demo.poupon.dev) is available. Note the administration settings are not available.

## Main features
* [Subsonic/OpenSubsonic API](SUBSONIC.md) support
* Multi-valued tags: `genre`, `mood`, `artists`, ...
* Artist relationships: `composer`, `conductor`, `lyricist`, `mixer`, `performer`, `producer`, `remixer`
* [Release types](https://musicbrainz.org/doc/Release_Group/Type): `album`, `single`, `EP`, `compilation`, `live`, ...
* [Release groups](https://musicbrainz.org/doc/Release_Group) support to show different versions of albums, such as remasters, reissues, etc.
* [MusicBrainz Identifier](https://musicbrainz.org/doc/MusicBrainz_Identifier) support to handle duplicated artist and release names
* [ListenBrainz](https://listenbrainz.org) support for:
  * Scrobbling and synchronizing listens
  * Synchronizing 'love' feedbacks
* Recommendation engine
* Multi-library support
* ReplayGain support
* Audio transcoding for compatibility and reduced bandwidth
* User management, with several [authentication backends](INSTALL.md#authentication-backend)
* Playlists support
* Lyrics support

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

## Artist information folder
_LMS_ supports an Artist information folder to manage metadata and images for artists. This folder can be placed anywhere within the scanned libraries but is best located in a dedicated `ArtistInfo` directory in the root of a media library for maximum compatibility with other softwares.

The folder must follow a structure defined by Kodi, as detailed [here](https://kodi.wiki/view/Artist_information_folder). `artist.nfo` files are used to define additional artist information such as biography, sort name, and MusicBrainz ArtistID. See the format [here](https://kodi.wiki/view/NFO_files/Artists).

__Note__: If no name is provided in the `artist.nfo` file, the name of the containing folder is used.

### Filtering
It is possible to apply global filters on your collection using `genre`, `mood`, `grouping`, `language`, and by music library. More tags, including custom ones, can be added in the database administration settings.

__Note__: You can use the `lms-metadata` tool to get an idea of the tags parsed by _LMS_.

### Multiple artists
_LMS_ works best when using the default [Picard](https://picard.musicbrainz.org/) settings, where the `artist` tag contains a single display-friendly value, and the `artists` tag holds the actual artist names. This ensures a cleaner, more organized representation of artist names, when multiple artists are involved.

### Multiple album artists
While _LMS_ can manage multiple album artists using the `albumartist` tag, it works better when using the custom `albumartists` and `albumartistssort` tags, similar to how it handles regular artist tags.

__Note__: if you use Picard, add the following script to include these tags:
```
$setmulti(albumartists,%_albumartists%)
$setmulti(albumartistssort,%_albumartists_sort%)
```

### Album track grouping
The recommended way to group tracks within an album is to use the `MUSICBRAINZ_ALBUMID` tag.

When this tag is not present, _LMS_ will attempt to group them as best as possible: if the analyzed file contains a disc number and the total number of discs is greater than 1, sibling directories are also scanned to find a matching album.
Otherwise, _LMS_ will only consider albums within the current directory.  

For an album to be considered a match, the following conditions must be met:
* Same name
* Same sort name
* Same total number of discs
* Identical 'compilation' flag value
* Same record labels
* Same barcode

## Artist image lookup
The recommended method is to name the artist image file using the artist's MusicBrainz ArtistID. This file can be placed anywhere within one of the scanned libraries.

If no file with the MusicBrainz ArtistID is found, _LMS_ will first look for files named `folder` and then `thumb` in the artist information directory, where the corresponding `artist.nfo` file is located.
If neither exists, it will then search for a file named `artist` (or another name configured in `lms.conf`) in the artist's directories, using this logic:
1. Identify the artist's directory: _LMS_ selects all albums by the artist using the "AlbumArtist" link and determines the longest common path among them.
2. Scan for the image: the directory is scanned starting from this common path, moving upwards if needed, until the artist image file is found.
3. Fallback search: if no image is found, _LMS_ will then search within each individual album folder.

## Playlist support
_LMS_ supports playlist files in `m3u` and `m3u8` formats. These playlists are synced during the scan process and are available as public shared playlists.

## Lyrics support
_LMS_ supports lyrics in `lrc` files, `txt` files, and embedded track metadata. Both synchronized and unsynchronized lyrics are supported.

## Keyboard shortcuts
* Play/pause: <kbd>Space</kbd>
* Previous track: <kbd>Ctrl</kbd> + <kbd>Left</kbd>
* Next track: <kbd>Ctrl</kbd> + <kbd>Right</kbd>
* Decrease volume: <kbd>Ctrl</kbd> + <kbd>Down</kbd>
* Increase volume: <kbd>Ctrl</kbd> + <kbd>Up</kbd>
* Seek back by 5 seconds: <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>Left</kbd>
* Seek forward by 5 seconds: <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>Right</kbd>


## Installation
See [INSTALL.md](INSTALL.md) file.

## Contributing
Any feedback is welcome:
* feel free to participate in [discussions](https://github.com/epoupon/lms/discussions) if you have questions,
* report any bug or request for new features in the [issue tracker](https://github.com/epoupon/lms/issues),
* submit your pull requests based on the [develop](../../tree/develop) branch.
