// @license magnet:?xt=urn:btih:1f739d935676111cfff4b4693e3816e664797050&dn=gpl-3.0.txt GPL-v3-or-Later

// Keep in sync with MediaPlayer::TranscodingMode cpp
LMSTranscodingMode = {
	Never: 0,
	Always: 1,
	IfFormatNotSupported: 2,
}
Object.freeze(LMSTranscodingMode);

class LMSMediaPlayer {
	// How much to increase / decrease volume when adjusting it with keyboard shortcuts
	static #volumeStepAmount = 0.05;

	// How much to seek back / forward (in seconds) with keyboard shortcuts
	static #seekAmount = 5;

	static #Mode = {
		Transcoding: 1,
		File: 2,
	}

	#root;
	#elems;
	#offset;
	#trackId;
	#duration;
	#audioNativeSrc;
	#audioTranscodingSrc;
	#settings;
	#playedDuration;
	#lastStartPlaying;
	#audioIsInit;
	#pendingTrackParameters;
	#gainNode;
	#audioCtx;

	constructor(root, defaultSettings) {
		this.#root = root;
		this.#elems = {};
		this.#offset = 0;
		this.#trackId = null;
		this.#duration = 0;
		this.#settings = {};
		this.#playedDuration = 0;
		this.#lastStartPlaying = null;
		this.#audioIsInit = false;
		this.#pendingTrackParameters = null;
		this.#gainNode = null;
		this.#audioCtx = null;

		this.#elems.audio = document.getElementById("lms-mp-audio");
		this.#elems.playpause = document.getElementById("lms-mp-playpause");
		this.#elems.previous = document.getElementById("lms-mp-previous");
		this.#elems.next = document.getElementById("lms-mp-next");
		this.#elems.progress = document.getElementById("lms-mp-progress");
		this.#elems.seek = document.getElementById("lms-mp-seek");
		this.#elems.curtime = document.getElementById("lms-mp-curtime");
		this.#elems.duration = document.getElementById("lms-mp-duration");
		this.#elems.volume = document.getElementById("lms-mp-volume");
		this.#elems.volumeslider = document.getElementById("lms-mp-volume-slider");
		this.#elems.transcodingActive = document.getElementById("lms-transcoding-active");

		this.#elems.playpause.addEventListener("click", () => {
			this.#playPause();
		});

		this.#elems.previous.addEventListener("click", () => {
			this.#playPrevious();
		});
		this.#elems.next.addEventListener("click", () => {
			this.#playNext();
		});
		this.#elems.seek.addEventListener("change", () => {
			this.#seekTo(parseInt(this.#elems.seek.value, 10));
		});

		this.#elems.audio.addEventListener("play", this.#updateControls.bind(this));
		this.#elems.audio.addEventListener("playing", this.#updateControls.bind(this));
		this.#elems.audio.addEventListener("pause", this.#updateControls.bind(this));

		this.#elems.audio.addEventListener("play", this.#updateMediaSessionState.bind(this));
		this.#elems.audio.addEventListener("playing", this.#updateMediaSessionState.bind(this));
		this.#elems.audio.addEventListener("pause", this.#updateMediaSessionState.bind(this));

		this.#elems.audio.addEventListener("pause", this.#pauseTimer.bind(this));
		this.#elems.audio.addEventListener("playing", this.#startTimer.bind(this));
		this.#elems.audio.addEventListener("waiting", this.#pauseTimer.bind(this));

		this.#elems.audio.addEventListener("timeupdate", () => {
			this.#elems.progress.style.width = "" + ((this.#offset + this.#elems.audio.currentTime) / this.#duration) * 100 + "%";
			this.#elems.curtime.innerHTML = this.#durationToString(this.#offset + this.#elems.audio.currentTime);
		});

		this.#elems.audio.addEventListener("ended", () => {
			this.#resetTimer();
			Wt.emit(this.#root, "playbackEnded");
		});

		this.#elems.audio.addEventListener("canplay", () => {
			if (this.#getAudioMode() == LMSMediaPlayer.#Mode.Transcoding) {
				this.#elems.transcodingActive.style.display = "inline";
			}
			else {
				this.#elems.transcodingActive.style.display = "none";
			}
		});

		this.#initVolume();
		this.#initDefaultSettings(defaultSettings);

		this.#elems.volumeslider.addEventListener("input", () => {
			this.#setVolume(this.#elems.volumeslider.value);
		});

		this.#elems.volume.addEventListener("click", () => {
			if (this.#elems.audio.volume != 0) {
				this.#setVolume(0);
			}
			else {
				this.#setVolume(this.#elems.lastvolume);
			}
		});

		document.addEventListener("keydown", (event) => {
			let handled = false;

			if (event.target instanceof HTMLInputElement)
				return;

			if (event.keyCode == 32) {
				this.#playPause();
				handled = true;
			}
			else if (event.ctrlKey && !event.shiftKey && event.keyCode == 37) {
				this.#playPrevious();
				handled = true;
			}
			else if (event.ctrlKey && !event.shiftKey && event.keyCode == 39) {
				this.#playNext();
				handled = true;
			}
			else if (event.ctrlKey && event.keyCode == 40) {
				this.#stepVolumeDown();
				handled = true;
			}
			else if (event.ctrlKey && event.keyCode == 38) {
				this.#stepVolumeUp();
				handled = true;
			}
			else if (event.ctrlKey && event.shiftKey && event.keyCode == 37) {
				this.#seekBack();
				handled = true;
			}
			else if (event.ctrlKey && event.shiftKey && event.keyCode == 39) {
				this.#seekForward();
				handled = true;
			}

			if (handled)
				event.preventDefault();
		});

		document.addEventListener("touchstart", this.#unlock.bind(this));
		document.addEventListener("touchend", this.#unlock.bind(this));
		document.addEventListener("click", this.#unlock.bind(this));
	}

	#unlock() {
		document.removeEventListener("touchstart", this.#unlock.bind(this));
		document.removeEventListener("touchend", this.#unlock.bind(this));
		document.removeEventListener("click", this.#unlock.bind(this));
		this.#initAudioCtx();
	};

	#initAudioCtx() {
		if (this.#audioIsInit)
			return;

		this.#audioIsInit = true;

		this.#audioCtx = new (window.AudioContext || window.webkitAudioContext)();
		this.#gainNode = this.#audioCtx.createGain();
		let source = this.#audioCtx.createMediaElementSource(this.#elems.audio);
		source.connect(this.#gainNode);
		this.#gainNode.connect(this.#audioCtx.destination);
		this.#audioCtx.resume(); // not sure of this

		if ("mediaSession" in navigator) {
			navigator.mediaSession.setActionHandler("play", () => {
				this.#playPause();
			});
			navigator.mediaSession.setActionHandler("pause", () => {
				this.#playPause();
			});
			navigator.mediaSession.setActionHandler("previoustrack", () => {
				this.#playPrevious();
			});
			navigator.mediaSession.setActionHandler("nexttrack", () => {
				this.#playNext();
			});
			navigator.mediaSession.setActionHandler("seekto", (e) => {
				this.#seekTo(e.seekTime);
			});
		}

		if (this.#pendingTrackParameters != null) {
			this.#applyAudioTrackParameters(this.#pendingTrackParameters);
			this.#pendingTrackParameters = null;
		}
	}

	#updateControls() {
		const pauseClass = "fa-pause";
		const playClass = "fa-play";

		if (this.#elems.audio.paused) {
			this.#elems.playpause.firstElementChild.classList.remove(pauseClass);
			this.#elems.playpause.firstElementChild.classList.add(playClass);
		}
		else {
			this.#elems.playpause.firstElementChild.classList.remove(playClass);
			this.#elems.playpause.firstElementChild.classList.add(pauseClass);
		}
	}

	#startTimer() {
		if (this.#lastStartPlaying == null)
			Wt.emit(this.#root, "scrobbleListenNow", this.#trackId);
		this.#lastStartPlaying = Date.now();
	}

	#pauseTimer() {
		if (this.#lastStartPlaying != null) {
			this.#playedDuration += Date.now() - this.#lastStartPlaying;
			this.#lastStartPlaying = null;
		}
	}

	#resetTimer() {
		if (this.#lastStartPlaying != null)
			this.#pauseTimer();

		if (this.#playedDuration > 0) {
			Wt.emit(this.#root, "scrobbleListenFinished", this.#trackId, this.#playedDuration);
			this.#playedDuration = 0;
		}
	}

	#durationToString(duration) {
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

	#playTrack() {
		this.#elems.audio.play()
			.then(_ => { })
			.catch(error => { console.log("Cannot play audio: " + error); });
	}

	#playPause() {
		this.#initAudioCtx();

		if (this.#elems.audio.paused && this.#elems.audio.children.length > 0) {
			this.#playTrack();
		}
		else
			this.#elems.audio.pause();
	}

	#playPrevious() {
		this.#initAudioCtx();
		Wt.emit(this.#root, "playPrevious");
	}

	#playNext() {
		this.#initAudioCtx();
		Wt.emit(this.#root, "playNext");
	}

	#initVolume() {
		if (typeof (Storage) !== "undefined" && localStorage.volume) {
			this.#elems.volumeslider.value = Number(localStorage.volume);
		}

		this.#setVolume(this.#elems.volumeslider.value);
	}

	#initDefaultSettings = function (defaultSettings) {
		if (typeof (Storage) !== "undefined" && localStorage.settings) {
			this.#settings = Object.assign(defaultSettings, JSON.parse(localStorage.settings));
		}
		else {
			this.#settings = defaultSettings;
		}

		Wt.emit(this.#root, "settingsLoaded", JSON.stringify(this.#settings));
	}

	#setVolume(volume) {
		this.#elems.lastvolume = this.#elems.audio.volume;

		this.#elems.audio.volume = volume;
		this.#elems.volumeslider.value = volume;

		if (volume > 0.5) {
			this.#elems.volume.classList.remove("fa-volume-off");
			this.#elems.volume.classList.remove("fa-volume-down");
			this.#elems.volume.classList.add("fa-volume-up");
		}
		else if (volume > 0) {
			this.#elems.volume.classList.remove("fa-volume-off");
			this.#elems.volume.classList.remove("fa-volume-up");
			this.#elems.volume.classList.add("fa-volume-down");
		}
		else {
			this.#elems.volume.classList.remove("fa-volume-up");
			this.#elems.volume.classList.remove("fa-volume-down");
			this.#elems.volume.classList.add("fa-volume-off");
		}

		if (typeof (Storage) !== "undefined") {
			localStorage.volume = volume;
		}
	}

	#stepVolumeDown() {
		let currentVolume = this.#elems.audio.volume;
		let remainder = (currentVolume * 10) % (LMSMediaPlayer.#volumeStepAmount * 10);
		let newVolume = remainder === 0 ? currentVolume - LMSMediaPlayer.#volumeStepAmount : currentVolume - (remainder / 10);
		this.#setVolume(Math.max(newVolume, 0));
	}

	#stepVolumeUp() {
		let currentVolume = this.#elems.audio.volume;
		let remainder = (currentVolume * 10) % (LMSMediaPlayer.#volumeStepAmount * 10);
		let newVolume = remainder === 0 ? currentVolume + LMSMediaPlayer.#volumeStepAmount : currentVolume + (LMSMediaPlayer.#volumeStepAmount - (remainder / 10));
		this.#setVolume(Math.min(newVolume, 1));
	}

	#setReplayGain(replayGain) {
		this.#gainNode.gain.value = Math.pow(10, (this.#settings.replayGain.preAmpGain + replayGain) / 20);
	}

	#seekTo(seekTime) {
		this.#initAudioCtx();
		let mode = this.#getAudioMode();
		if (!mode)
			return;

		switch (mode) {
			case LMSMediaPlayer.#Mode.Transcoding:
				this.#offset = seekTime;
				this.#removeAudioSources();
				this.#addAudioSource(this.#audioTranscodingSrc + "&offset=" + this.#offset);
				this.#elems.audio.load();
				this.#elems.audio.currentTime = 0;
				this.#playTrack();
				break;

			case LMSMediaPlayer.#Mode.File:
				this.#elems.audio.currentTime = seekTime;
				this.#playTrack();
				break;
		}

		this.#updateMediaSessionState();
	}

	#seekBack() {
		let currentPosition = this.#offset + this.#elems.audio.currentTime;
		let newPosition = currentPosition - LMSMediaPlayer.#seekAmount;
		this.#seekTo(Math.max(newPosition, 0));
	}

	#seekForward() {
		let currentPosition = this.#offset + this.#elems.audio.currentTime;
		let newPosition = currentPosition + LMSMediaPlayer.#seekAmount;
		this.#seekTo(Math.min(newPosition, this.#duration));
	}

	#updateMediaSessionState() {
		if ("mediaSession" in navigator) {
			navigator.mediaSession.setPositionState({
				duration: this.#duration,
				playbackRate: 1,
				position: Math.min(this.#offset + this.#elems.audio.currentTime, this.#duration),
			});

			if (this.#elems.audio.paused)
				navigator.mediaSession.playbackState = "paused";
			else
				navigator.mediaSession.playbackState = "playing";
		}
	}

	#removeAudioSources() {
		while (this.#elems.audio.lastElementChild) {
			this.#elems.audio.removeChild(this.#elems.audio.lastElementChild);
		}
	}

	#addAudioSource(audioSrc) {
		let source = document.createElement('source');
		source.src = audioSrc;
		this.#elems.audio.appendChild(source);
	}

	#getAudioMode() {
		if (this.#elems.audio.currentSrc) {
			if (this.#elems.audio.currentSrc.includes("format"))
				return LMSMediaPlayer.#Mode.Transcoding;
			else
				return LMSMediaPlayer.#Mode.File;
		}
		else
			return undefined;
	}

	#applyAudioTrackParameters(params) {
		this.#setReplayGain(params.replayGain);
		if ("mediaSession" in navigator) {
			navigator.mediaSession.metadata = new MediaMetadata({
				title: params.title,
				artist: params.artist,
				album: params.release,
				artwork: params.artwork,
			});
		}
	}

	loadTrack(params, autoplay) {
		this.#resetTimer();

		this.#trackId = params.trackId;
		this.#offset = 0;
		this.#duration = params.duration;
		this.#audioNativeSrc = params.nativeResource;
		this.#audioTranscodingSrc = params.transcodingResource + "&bitrate=" + this.#settings.transcoding.bitrate + "&format=" + this.#settings.transcoding.format;

		this.#elems.seek.max = this.#duration;

		this.#removeAudioSources();
		// ! order is important
		if (this.#settings.transcoding.mode == LMSTranscodingMode.Never || this.#settings.transcoding.mode == LMSTranscodingMode.IfFormatNotSupported) {
			this.#addAudioSource(this.#audioNativeSrc);
		}
		if (this.#settings.transcoding.mode == LMSTranscodingMode.Always || this.#settings.transcoding.mode == LMSTranscodingMode.IfFormatNotSupported) {
			this.#addAudioSource(this.#audioTranscodingSrc);
		}
		this.#elems.audio.load();

		this.#elems.curtime.innerHTML = this.#durationToString(this.#offset);
		this.#elems.duration.innerHTML = this.#durationToString(this.#duration);

		if (!this.#audioIsInit) {
			this.#pendingTrackParameters = params;
			return;
		}

		this.#applyAudioTrackParameters(params);

		if (autoplay && this.#audioCtx.state == "running")
			this.#playTrack();
	}

	stop() {
		this.#elems.audio.pause();
	}

	setSettings(settings) {
		this.#settings = settings;

		if (typeof (Storage) !== "undefined") {
			localStorage.settings = JSON.stringify(this.#settings);
		}
	}
}

// @license-end
