#include "DatabaseHandler.hpp"

#include "AudioTypes.hpp"
#include "FileTypes.hpp"


DatabaseHandler::DatabaseHandler(boost::filesystem::path db)
:
_path(db),
_dbBackend( db.string() )
{
	_session.setConnection(_dbBackend);
	_session.mapClass<Genre>("genre");
	_session.mapClass<Track>("track");
	_session.mapClass<Artist>("artist");
	_session.mapClass<Release>("release");
	_session.mapClass<Release>("release");
	_session.mapClass<Path>("path");
	_session.mapClass<Video>("video");

	try {
	        _session.createTables();
	}
	catch(std::exception& e) {
		std::cerr << "Cannot create tables: " << e.what() << std::endl;
	}

	_dbBackend.executeSql("pragma journal_mode=WAL");
}

