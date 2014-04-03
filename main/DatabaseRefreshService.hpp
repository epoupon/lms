#ifndef DB_REFRESH_SERVICE_HPP
#define DB_REFRESH_SERVICE_HPP

#include "metadata/AvFormat.hpp"
#include "database/Database.hpp"

#include "Service.hpp"

class DatabaseRefreshService : public Service
{
	public:

		DatabaseRefreshService(const boost::filesystem::path& p);

		void start(void);
		void stop(void);
		void restart(void);

	private:

		MetaData::AvFormat	_metadataParser;
		Database		_database;
};

#endif

