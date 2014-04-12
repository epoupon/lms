#ifndef VIDEO_PARAMETER_DIALOG
#define VIDEO_PARAMETER_DIALOG

#include <map>

#include <Wt/WSignal>
#include <Wt/WDialog>
#include <Wt/WComboBox>
#include <Wt/WStringListModel>
#include <Wt/WString>

#include "transcode/Parameters.hpp"

namespace UserInterface {

class VideoParametersDialog : public Wt::WDialog
{
	public:
		// parameters to be edited
		VideoParametersDialog(const Wt::WString &windowTitle, Wt::WDialog* parent = 0);

		// Populates widget contents using these paramaters
		void load(const Transcode::Parameters& parameters);

		// Save widgets contents into these parameters
		void save(Transcode::Parameters& parameters);

		// Signal to be emitted if parameters are changed
		Wt::Signal<void>& apply()	{ return _apply; }

	private:

		void handleApply(void);

		// Stream handling
		void createStreamWidgets(const Wt::WString& label, Transcode::Stream::Type type, Wt::WTable* layout);
		void addStreams(Wt::WStringListModel* model, const std::vector<Transcode::Stream>& streams);
		void selectStream(const Wt::WStringListModel* model, Transcode::Stream::Id streamId, Wt::WComboBox* combo);

		Wt::Signal<void>	_apply;

		Wt::WComboBox*						_outputFormat;
		Wt::WStringListModel*					_outputFormatModel;

		std::map<Transcode::Stream::Type, std::pair<Wt::WComboBox*, Wt::WStringListModel* > >	_streamSelection;
};

} // namespace UserInterface

#endif

