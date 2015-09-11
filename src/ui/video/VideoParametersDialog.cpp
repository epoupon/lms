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

#include <boost/foreach.hpp>

#include <Wt/WPushButton>
#include <Wt/WLabel>
#include <Wt/WTable>

#include "VideoParametersDialog.hpp"

using namespace Transcode;

namespace UserInterface {

static const std::list<Stream::Type> streamTypes = {Stream::Video, Stream::Audio, Stream::Subtitle};

VideoParametersDialog::VideoParametersDialog(const Wt::WString &windowTitle, Wt::WDialog* parent)
: Wt::WDialog(windowTitle, parent)
{

	Wt::WTable* layout = new Wt::WTable(contents());
	int row = 0;

	{
		Wt::WLabel* label = new Wt::WLabel("Format");
		_outputFormat = new Wt::WComboBox();
		label->setBuddy(_outputFormat);

		layout->elementAt(row, 0)->addWidget(label);
		layout->elementAt(row, 1)->addWidget(_outputFormat);

		_outputFormatModel = new Wt::WStringListModel(_outputFormat);

		std::vector<Format> formats ; // TODO Format::get( Format::Video );

		for(std::size_t idFormat = 0; idFormat < formats.size(); ++idFormat)
		{
			_outputFormatModel->addString(formats[idFormat].getDesc());
			_outputFormatModel->setData(idFormat, 0, formats[idFormat].getEncoding(), Wt::UserRole);
		}

		_outputFormat->setModel(_outputFormatModel);

		row++;
	}

	createStreamWidgets("Video", Stream::Video, layout);
	createStreamWidgets("Audio", Stream::Audio, layout);
	createStreamWidgets("Subtitles", Stream::Subtitle, layout);

	Wt::WPushButton *ok = new Wt::WPushButton("Apply", contents());
	ok->clicked().connect(this, &Wt::WDialog::accept);

	Wt::WPushButton *cancel = new Wt::WPushButton("Cancel", contents());
	cancel->clicked().connect(this, &Wt::WDialog::reject);
}

void
VideoParametersDialog::createStreamWidgets(const Wt::WString& labelString, Transcode::Stream::Type type, Wt::WTable* layout)
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
VideoParametersDialog::addStreams(Wt::WStringListModel* model, const std::vector<Stream>& streams)
{
	for (std::size_t idStream = 0; idStream < streams.size(); ++idStream)
	{
		const Stream& stream = streams[idStream];

		std::ostringstream oss;
		if (!stream.getLanguage().empty())
			oss << "[" << stream.getLanguage() << "] ";

		oss << stream.getDesc();

		model->addString(oss.str());
		model->setData(idStream, 0, stream.getId(),  Wt::UserRole);
	}
}

void
VideoParametersDialog::selectStream(const Wt::WStringListModel* model, Stream::Id streamId, Wt::WComboBox* combo)
{
	for (int idStream = 0; idStream < model->rowCount(); ++idStream)
	{
		Stream::Id id = boost::any_cast<Stream::Id>( model->data( model->index(idStream, 0), Wt::UserRole));
		if (id == streamId)
		{
			combo->setCurrentIndex(idStream);
			break;
		}
	}
}


void
VideoParametersDialog::load(const Transcode::Parameters& parameters)
{
	// Select proper encoding
	for (int row = 0; row < _outputFormat->count(); ++row)
	{
		Format::Encoding encoding = boost::any_cast<Format::Encoding>( _outputFormatModel->data(_outputFormatModel->index(row,0), Wt::UserRole));
		if (encoding == parameters.getOutputFormat().getEncoding()) {
			_outputFormat->setCurrentIndex(row);
			break;
		}
	}

	// Get the current selected input streams
	Parameters::StreamMap streamMap = parameters.getInputStreams();;

	// Populate the combox with the available streams
	// And then show the selected one
	BOOST_FOREACH(Stream::Type streamType, streamTypes)
	{
		Wt::WComboBox* combo = _streamSelection[streamType].first;
		Wt::WStringListModel* model = _streamSelection[streamType].second;

// TODO		addStreams(model, parameters.getInputMediaFile().getStreams( streamType ) );

		selectStream(model, streamMap[streamType], combo);
	}

}

void
VideoParametersDialog::save(Transcode::Parameters& parameters)
{
	std::cout << "Grabbing user input!" << std::endl;

	// Get encoder used
	Format::Encoding encoding = boost::any_cast<Format::Encoding>( _outputFormatModel->data(_outputFormatModel->index(_outputFormat->currentIndex(), 0), Wt::UserRole));
// TODO	parameters.setOutputFormat( Format::get(encoding) );

	// Get stream selected, if any
	BOOST_FOREACH(Stream::Type streamType, streamTypes)
	{
		Wt::WComboBox* combo = _streamSelection[streamType].first;
		Wt::WStringListModel* model = _streamSelection[streamType].second;

		if (combo->currentIndex() >= 0)
		{
			Stream::Id streamId = boost::any_cast<Stream::Id>( model->data(model->index(combo->currentIndex(), 0), Wt::UserRole));

			parameters.selectInputStream(streamType, streamId);
		}

	}
}

} // namespace UserInterface

