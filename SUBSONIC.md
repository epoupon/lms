# Subsonic API
The API version implemented is 1.16.0 and has been tested on _Android_ using _Subsonic Player_, _Ultrasonic_, _Symfonium_, and _DSub_.
Since _LMS_ uses metadata tags to organize music, a compatibility mode is used to browse the collection when using the directory browsing commands.
The Subsonic API is enabled by default.

__Note__: since _LMS_ may store hashed and salted passwords or may forward authentication requests to external services, it cannot handle the __token authentication__ method. You may need to check your client to make sure to use the __password__ authentication method.

# OpenSubsonic API
OpenSubsonic is an initiative to patch and extend the legacy Subsonic API. You'll find more details in the [official documentation](https://opensubsonic.netlify.app/)

## Extra fields
The following extra fields are implemented:
* `Album` response:
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
  * `musicBrainzId`
  * `sortName`
  * `roles`

## Supported extensions
* [Transcode offset](https://opensubsonic.netlify.app/docs/extensions/transcodeoffset/)
