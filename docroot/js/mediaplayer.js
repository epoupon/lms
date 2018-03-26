// @license magnet:?xt=urn:btih:1f739d935676111cfff4b4693e3816e664797050&dn=gpl-3.0.txt GPL-v3-or-Later

var LMS = LMS || {};

LMS.mediaplayer = function () {

	var _root = {};
	var _elems = {};

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

		_elems.audio.addEventListener("ended", function() {
			Wt.emit(_root, "playbackEnded");
		});
	}

	var loadTrack = function(params, autoplay) {
		console.log("Loading track '" + params.name + "', release = '" + params.release + "', artist = '" + params.artist + "', resource = '" + params.resource + "', imgResource = '" + params.imgResource);

		_elems.title.innerHTML = params.name;
		_elems.artist.innerHTML = params.artist;
		_elems.release.innerHTML = params.release;
		_elems.audio.src = params.resource;
		_elems.duration.innerHTML = params.duration;

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
