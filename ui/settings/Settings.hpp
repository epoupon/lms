#include <Wt/WContainerWidget>

#include "common/SessionData.hpp"

namespace UserInterface {
namespace Settings {

class Settings : public Wt::WContainerWidget
{
	public:
		Settings(SessionData& sessionData, Wt::WContainerWidget* parent = 0);

	private:

		void handleDatabaseDirectoriesChanged();

		void restartDatabaseUpdateService(void);

		SessionData&	_sessionData;

};

} // namespace Settings
} // namespace UserInterface
