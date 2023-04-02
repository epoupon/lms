// @license magnet:?xt=urn:btih:1f739d935676111cfff4b4693e3816e664797050&dn=gpl-3.0.txt GPL-v3-or-Later

var LMS = LMS || {};

// Keep in sync with MediaPlayer::TranscodeMode cpp
const TranscodeMode = {
	Never: 0,
	Always: 1,
	IfFormatNotSupported: 2,
}

const Mode = {
	Transcode: 1,
	File: 2,
}
Object.freeze(Mode);

LMS.mediaplayer = function () {
	let _root = {};
	let _elems = {};
	let _offset = 0;
	let _trackId = null;
	let _duration = 0;
	let _audioNativeSrc;
	let _audioTranscodeSrc;
	let _settings = {};
	let _playedDuration = 0;
	let _lastStartPlaying = null;
	let _audioIsInit = false;
	let _pendingTrackParameters = null;
	let _gainNode = null;
	let _audioCtx = null;

	let _unlock = function() {
		document.removeEventListener("touchstart", _unlock);
		document.removeEventListener("touchend", _unlock);
		document.removeEventListener("click", _unlock);
		_initAudioCtx();
	};

	let _initAudioCtx = function() {
		if (_audioIsInit) {
			_audioCtx.resume(); // not sure of this
			return;
		}

		_audioIsInit = true;

		_audioCtx = new (window.AudioContext || window.webkitAudioContext)();
		_gainNode = _audioCtx.createGain();
		let source = _audioCtx.createMediaElementSource(_elems.audio);
		source.connect(_gainNode);
		_gainNode.connect(_audioCtx.destination);
		_audioCtx.resume(); // not sure of this

		if ("mediaSession" in navigator) {
			navigator.mediaSession.setActionHandler("play", function() {
				_playPause();
			});
			navigator.mediaSession.setActionHandler("pause", function() {
				_playPause();
			});
			navigator.mediaSession.setActionHandler("previoustrack", function() {
				_playPrevious();
			});
			navigator.mediaSession.setActionHandler("nexttrack", function() {
				_playNext();
			});
		}

		if (_pendingTrackParameters != null) {
			_applyAudioTrackParameters(_pendingTrackParameters);
			_pendingTrackParameters = null;
		}
	}

	let _updateControls = function() {
		const pauseClass = "fa-pause";
		const playClass = "fa-play";

		if (_elems.audio.paused) {
			_elems.playpause.firstElementChild.classList.remove(pauseClass);
			_elems.playpause.firstElementChild.classList.add(playClass);
			if ("mediaSession" in navigator) {
				navigator.mediaSession.playbackState = "paused";
			}
		}
		else {
			_elems.playpause.firstElementChild.classList.remove(playClass);
			_elems.playpause.firstElementChild.classList.add(pauseClass);
			if ("mediaSession" in navigator) {
				navigator.mediaSession.playbackState = "playing";
			}
		}
	}

	let _startTimer = function() {
		if (_lastStartPlaying == null)
			Wt.emit(_root, "scrobbleListenNow", _trackId);
		_lastStartPlaying = Date.now();
	}

	let _stopTimer = function() {
		if (_lastStartPlaying != null) {
			_playedDuration += Date.now() - _lastStartPlaying;
		}
	}

	let _resetTimer = function() {
		if (_lastStartPlaying != null)
			Wt.emit(_root, "scrobbleListenFinished", _trackId, _playedDuration);
		_playedDuration = 0;
		_lastStartPlaying = null;
	}

	let _durationToString = function (duration) {
		const seconds = parseInt(duration, 10);
		const h = Math.floor(seconds / 3600);
		const m = Math.floor((seconds % 3600) / 60);
		const s = Math.round(seconds % 60);
		return [
			h,
			m > 9 ? m : (h ? '0' + m : m || '0'),
			s > 9 ? s : '0' + s
		].filter(Boolean).join(':');
	}

	let _playTrack = function() {
		_elems.audio.play()
			.then(_ => {})
			.catch(error => { console.log("Cannot play audio: " + error); });
	}

	let _playPause = function() {
		_initAudioCtx();

		if (_elems.audio.paused && _elems.audio.children.length > 0) {
			_playTrack();
		}
		else
			_elems.audio.pause();
	}

	let _playPrevious = function() {
		_initAudioCtx();
		Wt.emit(_root, "playPrevious");
	}

	let _playNext = function() {
		_initAudioCtx();
		Wt.emit(_root, "playNext");
	}

	let _initVolume = function() {
		if (typeof(Storage) !== "undefined" && localStorage.volume) {
			_elems.volumeslider.value = Number(localStorage.volume);
		}

		_setVolume(_elems.volumeslider.value);
	}

	let _initDefaultSettings = function(defaultSettings) {
		if (typeof(Storage) !== "undefined" && localStorage.settings) {
			_settings = Object.assign(defaultSettings, JSON.parse(localStorage.settings));
		}
		else {
			_settings = defaultSettings;
		}

		Wt.emit(_root, "settingsLoaded", JSON.stringify(_settings));
	}

	let _setVolume = function(volume) {
		_elems.lastvolume = _elems.audio.volume;

		_elems.audio.volume = volume;
		_elems.volumeslider.value = volume;

		if (volume > 0.5) {
			_elems.volume.classList.remove("fa-volume-off");
			_elems.volume.classList.remove("fa-volume-down");
			_elems.volume.classList.add("fa-volume-up");
		}
		else if (volume > 0) {
			_elems.volume.classList.remove("fa-volume-off");
			_elems.volume.classList.remove("fa-volume-up");
			_elems.volume.classList.add("fa-volume-down");

		}
		else {
			_elems.volume.classList.remove("fa-volume-up");
			_elems.volume.classList.remove("fa-volume-down");
			_elems.volume.classList.add("fa-volume-off");
		}

		if (typeof(Storage) !== "undefined") {
			localStorage.volume = volume;
		}
	}

	let _setReplayGain = function (replayGain) {
		_gainNode.gain.value = Math.pow(10, (_settings.replayGain.preAmpGain + replayGain) / 20);
	}

	let init = function(root, defaultSettings) {
		_root = root;

		_elems.audio = document.getElementById("lms-mp-audio");
		_elems.playpause = document.getElementById("lms-mp-playpause");
		_elems.previous = document.getElementById("lms-mp-previous");
		_elems.next = document.getElementById("lms-mp-next");
		_elems.progress = document.getElementById("lms-mp-progress");
		_elems.seek = document.getElementById("lms-mp-seek");
		_elems.curtime = document.getElementById("lms-mp-curtime");
		_elems.duration = document.getElementById("lms-mp-duration");
		_elems.volume = document.getElementById("lms-mp-volume");
		_elems.volumeslider = document.getElementById("lms-mp-volume-slider");
		_elems.transcodingActive = document.getElementById("lms-transcoding-active");

		_elems.playpause.addEventListener("click", function() {
			_playPause();
		});

		_elems.previous.addEventListener("click", function() {
			_playPrevious();
		});
		_elems.next.addEventListener("click", function() {
			_playNext();
		});
		_elems.seek.addEventListener("change", function() {
			_initAudioCtx();
			let mode = _getAudioMode();
			if (!mode)
				return;

			let selectedOffset = parseInt(_elems.seek.value, 10);

			switch (mode) {
				case Mode.Transcode:
					_offset = selectedOffset;
					_removeAudioSources();
					_addAudioSource(_audioTranscodeSrc + "&offset=" + _offset);
					_elems.audio.load();
					_elems.audio.currentTime = 0;
					_playTrack();
					break;

				case Mode.File:
					_elems.audio.currentTime = selectedOffset;
					_playTrack();
					break;
			}
		});

		_elems.audio.addEventListener("play", _updateControls);
		_elems.audio.addEventListener("playing", _updateControls);
		_elems.audio.addEventListener("pause", _updateControls);

		_elems.audio.addEventListener("pause", _stopTimer);
		_elems.audio.addEventListener("playing", _startTimer);
		_elems.audio.addEventListener("waiting", _stopTimer);

		_elems.audio.addEventListener("timeupdate", function() {
			_elems.progress.style.width = "" + ((_offset + _elems.audio.currentTime) / _duration) * 100 + "%";
			_elems.curtime.innerHTML = _durationToString(_offset + _elems.audio.currentTime);
		});

		_elems.audio.addEventListener("ended", function() {
			Wt.emit(_root, "playbackEnded");
		});

		_elems.audio.addEventListener("canplay", function() {
			if (_getAudioMode() == Mode.Transcode) {
				_elems.transcodingActive.style.display = "inline";
			}
			else {
				_elems.transcodingActive.style.display = "none";
			}
		});

		_initVolume();
		_initDefaultSettings(defaultSettings);

		_elems.volumeslider.addEventListener("input", function() {
			_setVolume(_elems.volumeslider.value);
		});

		_elems.volume.addEventListener("click", function () {
			if (_elems.audio.volume != 0) {
				_setVolume(0);
			}
			else {
				_setVolume(_elems.lastvolume);
			}
		});

		document.addEventListener("keydown", function(event) {
			let handled = false;

			if (event.target instanceof HTMLInputElement)
				return;

			if (event.keyCode == 32) {
				_playPause();
				handled = true;
			}
			else if (event.ctrlKey && event.keyCode == 37) {
				_playPrevious();
				handled = true;
			}
			else if (event.ctrlKey && event.keyCode == 39) {
				_playNext();
				handled = true;
			}

			if (handled)
				event.preventDefault();
		});

		document.addEventListener("touchstart", _unlock);
		document.addEventListener("touchend", _unlock);
		document.addEventListener("click", _unlock);
	}

	let _removeAudioSources = function() {
		while ( _elems.audio.lastElementChild) {
			_elems.audio.removeChild( _elems.audio.lastElementChild);
		}
	}

	let _addAudioSource = function(audioSrc) {
		let source = document.createElement('source');
		source.src = audioSrc;
		_elems.audio.appendChild(source);
	}

	let _getAudioMode = function() {
		if (_elems.audio.currentSrc) {
			if (_elems.audio.currentSrc.includes("format"))
				return Mode.Transcode;
			else
				return Mode.File;
		}
		else
			return undefined;
	}

	let loadTrack = function(params, autoplay) {
		_stopTimer();
		_resetTimer();

		_trackId = params.trackId;
		_offset = 0;
		_duration = params.duration;
		_audioNativeSrc = params.nativeResource;
		_audioTranscodeSrc = params.transcodeResource + "&bitrate=" + _settings.transcode.bitrate + "&format=" + _settings.transcode.format;

		_elems.seek.max = _duration;

		_removeAudioSources();
		// ! order is important
		if (_settings.transcode.mode == TranscodeMode.Never || _settings.transcode.mode == TranscodeMode.IfFormatNotSupported)
		{
			_addAudioSource(_audioNativeSrc);
		}
		if (_settings.transcode.mode == TranscodeMode.Always || _settings.transcode.mode == TranscodeMode.IfFormatNotSupported)
		{
			_addAudioSource(_audioTranscodeSrc);
		}
		_elems.audio.load();

		_elems.curtime.innerHTML = _durationToString(_offset);
		_elems.duration.innerHTML = _durationToString(_duration);

		if (!_audioIsInit) {
			_pendingTrackParameters = params;
			return;
		}

		_applyAudioTrackParameters(params);

		if (autoplay && _audioCtx.state == "running")
			_playTrack();
	}

	let _applyAudioTrackParameters = function(params)
	{
		_setReplayGain(params.replayGain);
		if ("mediaSession" in navigator) {
			navigator.mediaSession.metadata = new MediaMetadata({
				title: params.title,
				artist: params.artist,
				album: params.release,
				artwork: params.artwork,
			});
		}
	}

	let stop = function() {
		_elems.audio.pause();
	}

	let setSettings = function(settings) {
		_settings = settings;

		if (typeof(Storage) !== "undefined") {
			localStorage.settings = JSON.stringify(_settings);
		}
	}

	return {
		init: init,
		loadTrack: loadTrack,
		stop: stop,
		setSettings: setSettings,
	};
}();

// @license-end
