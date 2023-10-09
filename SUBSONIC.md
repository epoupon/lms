# Subsonic API
The API version implemented is 1.16.0 and has been tested on _Android_ using _Subsonic Player_, _Ultrasonic_, _Symfonium_, and _DSub_.
Since _LMS_ uses metadata tags to organize music, a compatibility mode is used to browse the collection when using the directory browsing commands.
The Subsonic API is enabled by default.

__Note__: since _LMS_ may store hashed and salted passwords or may forward authentication requests to external services, it cannot handle the __token authentication__ method. You may need to check your client to make sure to use the __password__ authentication method.

# OpenSubsonic API
## Extra fields
The following extra fields are implemented:
* `Album` response:
  * `musicBrainzId`
  * `genres`
  * `artists`
  * `releaseTypes`
  * `moods`
  * `originalReleaseDate`
  * `isCompilation`
  * `discTitles`
* `Child` response:
  * `musicBrainzId`: note this is actually the recording MBID
  * `genres`
  * `artists`
  * `albumArtists`
  * `contributors`
  * `moods`
  * `replayGain`
* `Artist` response:
  * `musicBrainzId`
  * `sortName`
  * `roles`

## Supported extensions
* [Transcode offset](https://opensubsonic.netlify.app/docs/extensions/transcodeoffset/)
