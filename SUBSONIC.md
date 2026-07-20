# Subsonic API
The API version implemented is 1.16.0 and has been tested on _Android_ using _DSub_, _Subsonic Player_, _Symfonium_, _Tempo_, [_Tempus_](https://github.com/eddyizm/tempus) and  _Ultrasonic_.

Folder navigation commands are supported. However, since _LMS_ does not store information for each folder, it is not possible to star/unstar folders considered as artists.
Given the API limitations of folder navigation commands, it is recommended to place all tracks of an album in the same folder and not to mix multiple albums in the same folder.

The Subsonic API is enabled by default.

## Jukebox support
The jukebox‑control subset of the API is implemented through the `jukeboxControl` endpoint. Only the administrator may execute these actions and the service can be
enabled or disabled with the `jukebox-audio-backend` configuration option in `lms.conf`. When the PulseAudio backend is selected, _LMS_ uses the normal system‑wide environment variables (`PULSE_SERVER`, `PULSE_SINK`, `PULSE_SOURCE`, etc.) to determine where audio is sent.

__Note__: the jukebox play queue is kept entirely in memory by the service and is not persisted.

# OpenSubsonic API
OpenSubsonic is an initiative to patch and extend the legacy Subsonic API. You'll find more details in the [official documentation](https://opensubsonic.netlify.app/)

## Authentication
_LMS_ supports the [API Key Authentication](https://opensubsonic.netlify.app/docs/extensions/apikeyauth/) method. Each user has to generate their own API key on the settings page to use the Subsonic API.
If a client's login screen has no dedicated API key field, enter the API key as the password instead.

## Extra fields
The following extra fields are implemented:
* `Album` response:
  * `artists`
  * `discTitles`: discs with no subtitle are omitted
  * `displayArtist`
  * `explicitStatus`
  * `genres`
  * `groupings`
  * `isCompilation`
  * `played`
  * `mediaType`
  * `moods`
  * `musicBrainzId`
  * `originalReleaseDate`
  * `recordLabels`
  * `releaseDate`
  * `releaseTypes`
  * `sortName`
  * `userRating`
  * `version`
* `Child` response:
  * `albumArtists`
  * `artists`
  * `bitDepth`
  * `channelCount`
  * `comment`
  * `contributors`
  * `displayAlbumArtist`
  * `displayArtist`
  * `explicitStatus`
  * `genres`
  * `groupings`
  * `mediaType`
  * `moods`
  * `movements`
  * `musicBrainzId`: note this is actually the recording MBID when this response refers to a song
  * `played`
  * `replayGain`
  * `samplingRate`
  * `works`
* `Artist` response:
  * `mediaType`
  * `musicBrainzId`
  * `sortName`
  * `roles`
* `Playlist` response:
  * `readonly`

## Supported extensions
* [API Key Authentication](https://opensubsonic.netlify.app/docs/extensions/apikeyauth/)
* [getPodcastEpisode](https://opensubsonic.netlify.app/docs/extensions/getpodcastepisode/)
* [HTTP form POST](https://opensubsonic.netlify.app/docs/extensions/formpost/)
* [Index based Queue](https://opensubsonic.netlify.app/docs/extensions/indexbasedqueue/)
* [Sonic similarity](https://opensubsonic.netlify.app/docs/extensions/sonicsimilarity/)
* [Transcode offset](https://opensubsonic.netlify.app/docs/extensions/transcodeoffset/)
* [Song Lyrics](https://opensubsonic.netlify.app/docs/extensions/songlyrics/)
* [Transcoding](https://opensubsonic.netlify.app/docs/extensions/transcoding/)
