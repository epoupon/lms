
#include <Wt/WServer>
#include <Wt/WApplication>
#include <Wt/WContainerWidget>
#include <Wt/WStackedWidget>
#include <Wt/WComboBox>
#include <Wt/WText>
#include <Wt/WLineEdit>
#include <Wt/WAnchor>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>


class ArtistView : public Wt::WContainerWidget
{
	public:
	ArtistView(Wt::WContainerWidget *parent = 0)
	: Wt::WContainerWidget(parent)
	{
	}

	void setId(int id)
	{
		clear();

		Wt::WText *header = new Wt::WText("Artist view ID = " + std::to_string(id), this);
		header->setInline(false);

		for (int i = id; i > 0; --i)
		{
			Wt::WAnchor *anchor = new Wt::WAnchor(Wt::WLink(Wt::WLink::InternalPath, "/release/" + std::to_string(i)), this);
			Wt::WText *text = new Wt::WText("Release " + std::to_string(i), anchor);
			text->setInline(false);
		}
	}
};

class ReleaseView : public Wt::WContainerWidget
{
	public:
	ReleaseView(Wt::WContainerWidget *parent = 0)
	: Wt::WContainerWidget(parent)
	{
	}

	void setId(int id)
	{
		clear();
		Wt::WText *header = new Wt::WText("Release view ID = " + std::to_string(id), this);
		header->setInline(false);

		for (int i = id; i > 0; --i)
		{
			Wt::WAnchor *anchor = new Wt::WAnchor(Wt::WLink(Wt::WLink::InternalPath, "/artist/" + std::to_string(i)), this);
			Wt::WText *text = new Wt::WText("Artist " + std::to_string(i), anchor);
			text->setInline(false);
		}
	}
};

class TestApplication : public Wt::WApplication
{
	public:
		TestApplication(const Wt::WEnvironment& env)
		: Wt::WApplication(env)
		{

			enableInternalPaths();

			Wt::WComboBox *combo = new Wt::WComboBox(root());
			combo->addItem("Artist");
			combo->addItem("Release");

			Wt::WLineEdit *edit = new Wt::WLineEdit("Enter id", root());

			edit->changed().connect(std::bind([=]
			{
				if (combo->currentText() == "Artist")
					wApp->setInternalPath("/artist/" + edit->text().toUTF8(), true);
				else if (combo->currentText() == "Release")
					wApp->setInternalPath("/release/" + edit->text().toUTF8(), true);
			}));

			Wt::WStackedWidget *stack = new Wt::WStackedWidget(root());

			Wt::WContainerWidget *artistContainer = new Wt::WContainerWidget();
			Wt::WContainerWidget *releaseContainer = new Wt::WContainerWidget();

			stack->addWidget(releaseContainer);
			stack->addWidget(artistContainer);

			internalPathChanged().connect(std::bind([=] (std::string path)
			{
				wApp->log("info") << "Path set to '" << path << "'";

				std::vector<std::string> strings;
				boost::algorithm::split(strings, path, boost::is_any_of("/"), boost::token_compress_on);

				if (strings.size() != 3)
					return;

				std::string view = strings[1];
				int id;
				try {
					id = std::stol(strings[2]);
				}
				catch (std::exception& e) {
					return;
				}

				if (view == "release")
				{
					releaseContainer->clear();
					ReleaseView *releaseView = new ReleaseView(releaseContainer);
					releaseView->setId(id);

					stack->setCurrentIndex(0);
				}
				else if (view == "artist")
				{
					artistContainer->clear();
					ArtistView *artistView = new ArtistView(artistContainer);
					artistView->setId(id);

					stack->setCurrentIndex(1);
				}

			}, std::placeholders::_1));

			setInternalPath("/main");
		}

};

static Wt::WApplication *createTestApplication(const Wt::WEnvironment& env)
{
	return new TestApplication(env);
}

int main(int argc, char *argv[])
{
	try
	{

		Wt::WServer server(argv[0]);
		server.setServerConfiguration (argc, argv);

		server.addEntryPoint(Wt::Application, createTestApplication);

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

