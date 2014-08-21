#ifndef DB_UPDATE_SERVICE_HPP
#define DB_UPDATE_SERVICE_HPP

#include <boost/thread.hpp>
#include <boost/asio/io_service.hpp>

#include "metadata/AvFormat.hpp"
#include "database-updater/DatabaseUpdater.hpp"

#include "Service.hpp"

namespace Service {

class DatabaseUpdateService : public Service
{
	public:

		typedef std::shared_ptr<DatabaseUpdateService>	pointer;

		DatabaseUpdateService(const boost::filesystem::path& p);

		// Service interface
		void start(void);
		void stop(void);
		void restart(void);

	private:

		MetaData::AvFormat			_metadataParser;
		DatabaseUpdater::Updater		_databaseUpdater;	// Todo use handler
};

} // namespace Service

#endif

