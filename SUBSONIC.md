# Subsonic API
The API version implemented is 1.16.0 and has been tested on _Android_ using _Subsonic Player_, _Ultrasonic_, _Symfonium_, and _DSub_.

Folder navigation commands are supported. However, since _LMS_ does not store information for each folder, it is not possible to star/unstar folders considered as artists.
Given the API limitations of folder navigation commands, it is recommended to place all tracks of an album in the same folder and not to mix multiple albums in the same folder.

The Subsonic API is enabled by default.

__Note__: since _LMS_ may store hashed and salted passwords or may forward authentication requests to external services, it cannot handle the __token authentication__ method. You may need to check your client to make sure to use the __password__ authentication method. Since logins/passwords are passed in plain text through URLs, it is highly recommended to use a unique password when using the Subsonic API. Note that this may affect the use of authentication via PAM. In any case, ensure that read access to the web server logs (and to the proxy, if applicable) is well protected.

# OpenSubsonic API
OpenSubsonic is an initiative to patch and extend the legacy Subsonic API. You'll find more details in the [official documentation](https://opensubsonic.netlify.app/)

## Extra fields
The following extra fields are implemented:
* `Album` response:
  * `mediaType`
  * `played`
  * `musicBrainzId`
  * `genres`
  * `artists`
  * `displayArtist`
  * `releaseTypes`
  * `moods`
  * `originalReleaseDate`
  * `isCompilation`
  * `discTitles`: discs with no subtitle are omitted
* `Child` response:
  * `bitDepth`
  * `samplingRate`
  * `channelCount`
  * `mediaType`
  * `played`
  * `musicBrainzId`: note this is actually the recording MBID when this response refers to a song
  * `genres`
  * `artists`
  * `displayArtist`
  * `albumArtists`
  * `displayAlbumArtist`
  * `contributors`
  * `moods`
  * `replayGain`
* `Artist` response:
  * `mediaType`
  * `musicBrainzId`
  * `sortName`
  * `roles`

## Supported extensions
* [Transcode offset](https://opensubsonic.netlify.app/docs/extensions/transcodeoffset/)
