
#include <Wt/WServer>
#include <Wt/WApplication>
#include <Wt/WContainerWidget>
#include <Wt/WText>


class TestApplication : public Wt::WApplication
{
	public:
		TestApplication(const Wt::WEnvironment& env)
		: Wt::WApplication(env)
		{
			root()->addWidget(new Wt::WText("Hello, world!"));
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

