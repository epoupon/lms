
#include <Wt/WServer>
#include <Wt/WApplication>
#include <Wt/WContainerWidget>
#include <Wt/WText>
#include <Wt/WAudio>
#include <Wt/WPushButton>
#include <Wt/WBootstrapTheme>
#include <Wt/WTemplate>
#include <Wt/WLineEdit>
#include <Wt/WImage>

#include "database/DatabaseHandler.hpp"

#include "ui/resource/TranscodeResource.hpp"
#include "ui/resource/CoverResource.hpp"


class InputRange : public Wt::WWebWidget
{
	public:
	InputRange(Wt::WContainerWidget *parent = 0)
	: Wt::WWebWidget(parent)
	{
		setHtmlTagName("input");
		setAttributeValue("type", "range");
	}

	Wt::DomElementType domElementType() const
	{
		return Wt::DomElement_INPUT;
	}
};


Wt::WString MyPlayerTemplate = "${prev} ${play-pause} ${next} ${cover} ${artist} ${track} ${release} ${curtime} ${seekbar} ${duration} ${volume}";

class MyPlayer : public Wt::WContainerWidget
{
	public:

		void playbackComplete(void)
		{
			// Switch to the next track
		}

		void loadTrack(Database::Track::id_type trackId)
		{
			Wt::Dbo::Transaction transaction(_db.getSession());

			Database::Track::pointer track = Database::Track::getById(_db.getSession(), trackId);
			if (!track)
			{
				std::cerr << "no track for this id!" << std::endl;
				return;
			}

			_trackName->setText(Wt::WString::fromUTF8(track->getName()));
			_artistName->setText( Wt::WString::fromUTF8(track->getArtist()->getName()));
			_releaseName->setText( Wt::WString::fromUTF8(track->getRelease()->getName()));
			_cover->setImageLink(_coverResource->getTrackUrl(trackId, 64));
			_trackDuration->setText( boost::posix_time::to_simple_string( track->getDuration() ));

			// Analyse track, select the best media stream
			Av::MediaFile mediaFile(track->getPath());

			if (!mediaFile.open() || !mediaFile.scan())
			{
				std::cerr << "cannot open file '" << track->getPath() << std::endl;
				return;
			}


			int audioBestStreamId = mediaFile.getBestStreamId(Av::Stream::Type::Audio);
			std::vector<std::size_t> streams;
			if (audioBestStreamId != -1)
				streams.push_back(audioBestStreamId);

			this->doJavaScript("\
					document.lms.audio.state = \"loaded\";\
				        document.lms.audio.seekbar.min = " + std::to_string(0) + ";\
					document.lms.audio.seekbar.max = " + std::to_string(track->getDuration().total_seconds()) + ";\
					document.lms.audio.seekbar.value = 0;\
				        document.lms.audio.seekbar.disabled = false;\
					document.lms.audio.offset = 0;\
					document.lms.audio.curTime = 0;\
					");

			_audio->pause();
			_audio->clearSources();
			_audio->addSource(_transcodeResource->getUrl(trackId, Av::Encoding::MP3, 0, streams));
			_audio->play();
		}


		MyPlayer(Database::Handler &db, Wt::WContainerWidget *parent = 0)
		: Wt::WContainerWidget(parent),
		_db(db)
	{

		_transcodeResource = new UserInterface::TranscodeResource(db, this);
		_coverResource = new UserInterface::CoverResource(db, this);

		Wt::WTemplate *t = new Wt::WTemplate(MyPlayerTemplate, this);

		_audio = new Wt::WAudio(this);

		_cover = new Wt::WImage();
		t->bindWidget("cover", _cover);
		_cover->setImageLink(_coverResource->getUnknownTrackUrl(64));

		InputRange *seekbar = new InputRange();
		t->bindWidget("seekbar", seekbar);

		_trackName = new Wt::WText();
		t->bindWidget("track", _trackName);

		_artistName = new Wt::WText();
		t->bindWidget("artist", _artistName);

		_releaseName = new Wt::WText();
		t->bindWidget("release", _releaseName);

		InputRange *volumeSlider = new InputRange();
		t->bindWidget("volume", volumeSlider);

		Wt::WPushButton *prevBtn = new Wt::WPushButton("<<");
		t->bindWidget("prev", prevBtn);

		Wt::WPushButton *nextBtn = new Wt::WPushButton(">>");
		t->bindWidget("next", nextBtn);

		Wt::WPushButton *playPauseBtn = new Wt::WPushButton("play/pause");
		t->bindWidget("play-pause", playPauseBtn);

		Wt::WText *trackCurrentTime = new Wt::WText("00:00");
		t->bindWidget("curtime", trackCurrentTime);

		_trackDuration = new Wt::WText("00:00");
		t->bindWidget("duration", _trackDuration);

		//Wt::WTemplate *_volumeSlider = new Wt::WTemplate(VolumeSliderTemplate, this);

		this->doJavaScript(
"\
	document.lms = {};\
	document.lms.audio = {};\
        document.lms.audio.audio = " + _audio->jsRef() + ";\
        document.lms.audio.seekbar = " + seekbar->jsRef() +";\
        document.lms.audio.volumeSlider = " + volumeSlider->jsRef() + ";\
        document.lms.audio.curTimeText = " + trackCurrentTime->jsRef() + ";\
        document.lms.audio.playPause = " + playPauseBtn->jsRef() + ";\
\
	document.lms.audio.offset = 0;\
	document.lms.audio.curTime = 0;\
	document.lms.audio.state = \"init\";\
	document.lms.audio.volume = 1;\
\
        document.lms.audio.seekbar.value = 0;\
        document.lms.audio.seekbar.disabled = true;\
\
	document.lms.audio.volumeSlider.min = 0;\
	document.lms.audio.volumeSlider.max = 100;\
	document.lms.audio.volumeSlider.value = 100;\
\
        function updateUI() {\
		document.lms.audio.curTimeText.innerHTML = document.lms.audio.curTime;\
		document.lms.audio.seekbar.value = document.lms.audio.curTime;\
	}\
\
	var mouseDown = 0;\
	function seekMouseDown(e) {\
		++mouseDown;\
	}\
	function seekMouseUp(e) {\
		--mouseDown;\
	}\
\
	function seeking(e) {\
		if (document.lms.audio.state == \"init\")\
			return;\
\
		document.lms.audio.curTimeText.innerHTML = document.lms.audio.seekbar.value;\
	}\
\
        function seek(e) {\
		if (document.lms.audio.state == \"init\")\
			return;\
\
		document.lms.audio.audio.pause(); \
		document.lms.audio.offset = parseInt(document.lms.audio.seekbar.value);\
		document.lms.audio.curTime = document.lms.audio.seekbar.value;\
		var audioSource = document.lms.audio.audio.getElementsByTagName(\"source\")[0];\
		var src = audioSource.src;\
		src = src.slice(0, src.lastIndexOf(\"=\") + 1);\
		audioSource.src = src + document.lms.audio.seekbar.value;\
		document.lms.audio.audio.load(); \
		document.lms.audio.audio.play(); \
		document.lms.audio.curTimeText.innerHTML = ~~document.lms.audio.curTime + \"        \";\
	}\
\
	function volumeChanged() {\
		document.lms.audio.audio.volume = document.lms.audio.volumeSlider.value / 100;\
	}\
\
	function updateCurTime() {\
		document.lms.audio.curTime = document.lms.audio.offset + ~~document.lms.audio.audio.currentTime; \
		if (mouseDown == 0)\
			updateUI();\
	} \
\
	function playPause() {\
		if (document.lms.audio.state == \"init\") \
			return;\
\
		if (document.lms.audio.audio.paused)\
			document.lms.audio.audio.play();\
		else\
			document.lms.audio.audio.pause();\
\
	}\
\
	document.lms.audio.audio.addEventListener('timeupdate', updateCurTime); \
	document.lms.audio.seekbar.addEventListener('change', seek);\
	document.lms.audio.seekbar.addEventListener('input', seeking);\
	document.lms.audio.seekbar.addEventListener('mousedown', seekMouseDown);\
	document.lms.audio.seekbar.addEventListener('mouseup', seekMouseUp);\
	document.lms.audio.volumeSlider.addEventListener('input', volumeChanged);\
	document.lms.audio.playPause.addEventListener('click', playPause);\
"
);
	}

	UserInterface::TranscodeResource* _transcodeResource;
	UserInterface::CoverResource* _coverResource;
	Database::Handler&	_db;
	Wt::WAudio*	_audio;
	Wt::WText*	_trackDuration;
	Wt::WText*	_trackName;
	Wt::WText*	_artistName;
	Wt::WText*	_releaseName;
	Wt::WImage*	_cover;
};

class TestApplication : public Wt::WApplication
{
	public:
		TestApplication(const Wt::WEnvironment& env, Wt::Dbo::SqlConnectionPool& connectionPool)
		: Wt::WApplication(env)
		  , _db(connectionPool)
		{
			Wt::WBootstrapTheme *bootstrapTheme = new Wt::WBootstrapTheme(this);
			bootstrapTheme->setVersion(Wt::WBootstrapTheme::Version3);
			bootstrapTheme->setResponsive(true);
			setTheme(bootstrapTheme);

			Wt::WLineEdit*	trackSelector = new Wt::WLineEdit();
			MyPlayer*	player = new MyPlayer(_db);

			trackSelector->changed().connect(std::bind([=] {
				player->loadTrack(Wt::asNumber(trackSelector->valueText()));
			}));

			root()->addWidget(trackSelector);
			root()->addWidget(player);
		}

	private:
		Database::Handler _db;

};

static Wt::WApplication *createTestApplication(const Wt::WEnvironment& env, Wt::Dbo::SqlConnectionPool& connectionPool )
{
	return new TestApplication(env, connectionPool);
}

int main(int argc, char *argv[])
{
	try
	{
		Av::AvInit();
		Av::Transcoder::init();

		Wt::WServer server(argv[0]);
		server.setServerConfiguration (argc, argv);

		// Make pstream work with ffmpeg
		close(STDIN_FILENO);

		Database::Handler::configureAuth();
		std::unique_ptr<Wt::Dbo::SqlConnectionPool> connectionPool( Database::Handler::createConnectionPool("/var/lms/lms.db"));

		server.addEntryPoint(Wt::Application, boost::bind(createTestApplication, _1, boost::ref(*connectionPool)));

		server.start();

		Wt::WServer::waitForShutdown(argv[0]);

		server.stop();

		return EXIT_SUCCESS;
	}
	catch (std::exception& e)
	{
		std::cerr << "Caught exception: " << e.what();
		return EXIT_FAILURE;
	}

}

