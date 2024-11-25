# Subsonic API
The API version implemented is 1.16.0 and has been tested on _Android_ using _DSub_, _Subsonic Player_, _Symfonium_, _Tempo_ and  _Ultrasonic_.

Folder navigation commands are supported. However, since _LMS_ does not store information for each folder, it is not possible to star/unstar folders considered as artists.
Given the API limitations of folder navigation commands, it is recommended to place all tracks of an album in the same folder and not to mix multiple albums in the same folder.

The Subsonic API is enabled by default.

# OpenSubsonic API
OpenSubsonic is an initiative to patch and extend the legacy Subsonic API. You'll find more details in the [official documentation](https://opensubsonic.netlify.app/)

## Authentication
_LMS_ supports the newer [API Key Authentication](https://opensubsonic.netlify.app/docs/extensions/apikeyauth/). Each user can generate, regenerate, or delete their API key in the settings page.

By default, API keys can also be used as passwords, and the `user` parameter must match the owner of the API key. This fallback authentication method can be disabled by setting `api-subsonic-support-user-password-auth = false;` in `lms.conf`.

__Note__: the token+salt authentication method is not supported.

## Extra fields
The following extra fields are implemented:
* `Album` response:
  * `artists`
  * `discTitles`: discs with no subtitle are omitted
  * `displayArtist`
  * `genres`
  * `isCompilation`
  * `played`
  * `mediaType`
  * `moods`
  * `musicBrainzId`
  * `originalReleaseDate`
  * `recordLabels`
  * `releaseTypes`
  * `userRating`
* `Child` response:
  * `albumArtists`
  * `artists`
  * `bitDepth`
  * `channelCount`
  * `comment`
  * `contributors`
  * `displayAlbumArtist`
  * `displayArtist`
  * `genres`
  * `mediaType`
  * `moods`
  * `musicBrainzId`: note this is actually the recording MBID when this response refers to a song
  * `played`
  * `replayGain`
  * `samplingRate`
* `Artist` response:
  * `mediaType`
  * `musicBrainzId`
  * `sortName`
  * `roles`

## Supported extensions
* [API Key Authentication](https://opensubsonic.netlify.app/docs/extensions/apikeyauth/)
* [HTTP form POST](https://opensubsonic.netlify.app/docs/extensions/formpost/)
* [Transcode offset](https://opensubsonic.netlify.app/docs/extensions/transcodeoffset/)
* [Song Lyrics](https://opensubsonic.netlify.app/docs/extensions/songlyrics/)
