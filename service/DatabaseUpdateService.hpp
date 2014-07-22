#ifndef DB_UPDATE_SERVICE_HPP
#define DB_UPDATE_SERVICE_HPP

#include <boost/thread.hpp>
#include <boost/asio/io_service.hpp>

#include "metadata/AvFormat.hpp"
#include "database-updater/DatabaseUpdater.hpp"

#include "Service.hpp"

class DatabaseUpdateService : public Service
{
	public:

		typedef std::shared_ptr<DatabaseUpdateService>	pointer;

		DatabaseUpdateService(boost::asio::io_service& ioService, const boost::filesystem::path& p);

		// Service interface
		void start(void);
		void stop(void);
		void restart(void);

		// Specific interface
		bool	isScanning(void) const;	//return if the service is currently scanning the db

	private:

		boost::thread				_thread;

		MetaData::AvFormat			_metadataParser;
		DatabaseUpdater::Updater		_databaseUpdater;	// Todo use handler
};

#endif

