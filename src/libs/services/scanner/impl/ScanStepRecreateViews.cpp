#include "ScanStepRecreateViews.hpp"

#include "database/Db.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "core/ILogger.hpp"

namespace lms::scanner
{
    void ScanStepRecreateViews::process(ScanContext&)
    {
        using namespace db;

        if (_abortScan)
            return;

		Session& session {_db.getTLSSession()};

		auto transaction {session.createWriteTransaction()};
        session.dropViews();
        session.createViewsIfNeeded();

        LMS_LOG(DBUPDATER, DEBUG, "Views recreated");
    }
}
