/*
 * Copyright (C) 2013 Emeric Poupon
 *
 * This file is part of LMS.
 *
 * LMS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LMS.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <Wt/WPushButton>
#include <Wt/WLabel>
#include <Wt/WTable>

#include "VideoParametersDialog.hpp"

namespace UserInterface {

static const std::list<Av::Stream::Type> streamTypes = {Av::Stream::Type::Video, Av::Stream::Type::Audio, Av::Stream::Type::Subtitle};

VideoParametersDialog::VideoParametersDialog(Wt::WString windowTitle, Wt::WDialog* parent)
: Wt::WDialog(windowTitle, parent)
{

	Wt::WTable* layout = new Wt::WTable(contents());

	createStreamWidgets("Video", Av::Stream::Type::Video, layout);
	createStreamWidgets("Audio", Av::Stream::Type::Audio, layout);
	createStreamWidgets("Subtitles", Av::Stream::Type::Subtitle, layout);

	Wt::WPushButton *ok = new Wt::WPushButton("Apply", contents());
	ok->clicked().connect(this, &Wt::WDialog::accept);

	Wt::WPushButton *cancel = new Wt::WPushButton("Cancel", contents());
	cancel->clicked().connect(this, &Wt::WDialog::reject);
}

void
VideoParametersDialog::createStreamWidgets(const Wt::WString& labelString, Av::Stream::Type type, Wt::WTable* layout)
{
	int row = layout->rowCount();

	Wt::WLabel* label = new Wt::WLabel(labelString);
	Wt::WComboBox *combo = new Wt::WComboBox();
	label->setBuddy(combo);

	layout->elementAt(row, 0)->addWidget(label);
	layout->elementAt(row, 1)->addWidget(combo);

	Wt::WStringListModel* model = new Wt::WStringListModel(combo);

	combo->setModel(model);

	_streamSelection.insert( std::make_pair(type, std::make_pair(combo, model) )  );
}


void
VideoParametersDialog::handleApply()
{
	_apply.emit();
}

void
VideoParametersDialog::addStreams(Wt::WStringListModel* model, const std::vector<Av::Stream>& streams)
{
	for (const Av::Stream& stream : streams)
	{
		model->addString(stream.desc);
		model->setData(model->rowCount(), 0, stream.id,  Wt::UserRole);
	}
}

void
VideoParametersDialog::selectStream(const Wt::WStringListModel* model, int streamId, Wt::WComboBox* combo)
{
	for (int idStream = 0; idStream < model->rowCount(); ++idStream)
	{
		int id = boost::any_cast<int>( model->data( model->index(idStream, 0), Wt::UserRole));
		if (id == streamId)
		{
			combo->setCurrentIndex(idStream);
			break;
		}
	}
}


void
VideoParametersDialog::load(const Av::MediaFile& mediaFile, Av::TranscodeParameters currentParameters)
{
	// Populate the combox with the available streams
	// And then show the selected one
	for (Av::Stream::Type streamType : streamTypes)
	{
		Wt::WComboBox* combo = _streamSelection[streamType].first;
		Wt::WStringListModel* model = _streamSelection[streamType].second;

		addStreams(model, mediaFile.getStreams(streamType) );

		// Pre Select if necessary
		std::set<int> selectedStreams = currentParameters.getSelectedStreamIds();
		for (Av::Stream stream : mediaFile.getStreams(streamType))
		{
			if (selectedStreams.find(stream.id) != selectedStreams.end())
			{
				selectStream(model, stream.id, combo);
				break;
			}
		}
	}
}

void
VideoParametersDialog::save(Av::TranscodeParameters& parameters)
{
	// Get stream selected, if any
	for (Av::Stream::Type streamType : streamTypes)
	{
		Wt::WComboBox* combo = _streamSelection[streamType].first;
		Wt::WStringListModel* model = _streamSelection[streamType].second;

		if (combo->currentIndex() >= 0)
		{
			int streamId = boost::any_cast<int>( model->data(model->index(combo->currentIndex(), 0), Wt::UserRole));

			parameters.addStream(streamId);
		}

	}
}

} // namespace UserInterface

