// @license magnet:?xt=urn:btih:1f739d935676111cfff4b4693e3816e664797050&dn=gpl-3.0.txt GPL-v3-or-Later

var LMS = LMS || {};

LMS.mediaplayer = function () {

	var _root = {};
	var _elems = {};
	var _offset = 0;
	var _duration = 0;

	var _updateControls = function() {
		if (_elems.audio.paused) {
			_elems.play.style.display = "block";
			_elems.pause.style.display = "none";
		}
		else {
			_elems.pause.style.display = "block";
			_elems.play.style.display = "none";
		}
	}

	var _durationToString = function (duration, displayHours) {
			var hours = parseInt(duration / 3600, 10) % 24;
			var minutes = parseInt(duration / 60, 10) % 60;
			var seconds = parseInt(duration, 10) % 60;

			var res = "";

			if (displayHours)
				res += (hours < 10 ? "0" + hours : hours) + ":";

			res += (minutes < 10 ? "0" + minutes : minutes) + ":";
			res += (seconds  < 10 ? "0" + seconds : seconds);

			return res;
	}

	var init = function(root) {
		_root = root;

		_elems.audio = document.getElementById("lms-mp-audio");
		_elems.play = document.getElementById("lms-mp-play");
		_elems.pause = document.getElementById("lms-mp-pause");
		_elems.previous = document.getElementById("lms-mp-previous");
		_elems.next = document.getElementById("lms-mp-next");
		_elems.title = document.getElementById("lms-mp-title");
		_elems.artist = document.getElementById("lms-mp-artist");
		_elems.release = document.getElementById("lms-mp-release");
		_elems.duration = document.getElementById("lms-mp-duration");
		_elems.time = document.getElementById("lms-mp-time");
		// Hack cannot add a img element into the template
		// since a /img is considered missing!
		_elems.cover = document.createElement("img");
		root.appendChild(_elems.cover);

		_elems.play.addEventListener("click", function() {
			_elems.audio.play();
		});
		_elems.pause.addEventListener("click", function() {
			_elems.audio.pause();
		});

		_elems.previous.addEventListener("click", function() {
			Wt.emit(_root, "playPrevious");
		});
		_elems.next.addEventListener("click", function() {
			Wt.emit(_root, "playNext");
		});

		_elems.audio.addEventListener("play", _updateControls);
		_elems.audio.addEventListener("playing", _updateControls);
		_elems.audio.addEventListener("pause", _updateControls);

		_elems.audio.addEventListener("timeupdate", function() {
			_elems.time.innerHTML = _durationToString(_offset + _elems.audio.currentTime, _duration > 3600)
		});

		_elems.audio.addEventListener("ended", function() {
			Wt.emit(_root, "playbackEnded");
		});
	}

	var loadTrack = function(params, autoplay) {
		console.log("Loading track '" + params.name + "', release = '" + params.release + "', artist = '" + params.artist + "', resource = '" + params.resource + "', imgResource = '" + params.imgResource);

		_offset = 0;
		_duration = params.duration;

		_elems.title.innerHTML = params.name;
		_elems.artist.innerHTML = params.artist;
		_elems.release.innerHTML = params.release;
		_elems.audio.src = params.resource;
		_elems.duration.innerHTML = _durationToString(_duration, _duration > 3600);
		_elems.time.innerHTML = _durationToString(0, _duration > 3600)
		_elems.cover.src = params.imgResource;

		if (autoplay)
			_elems.audio.play();
	}

	var stop = function() {
		_elems.audio.pause();
	}

	return {
		init: init,
		loadTrack: loadTrack,
		stop: stop,
	};
}();

// @license-end
