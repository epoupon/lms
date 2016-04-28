/*
 * Copyright (C) 2015 Emeric Poupon
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

#include "Types.hpp"
#include "SearchFilter.hpp"
#include "SqlQuery.hpp"

namespace Database
{

Release::Release(const std::string& name, const std::string& MBID)
: _name(std::string(name, 0 , _maxNameLength)),
_MBID(MBID)
{

}

std::vector<Release::pointer>
Release::getByName(Wt::Dbo::Session& session, const std::string& name)
{
	Wt::Dbo::collection<Release::pointer> res = session.find<Release>().where("name = ?").bind( std::string(name, 0, _maxNameLength) );
	return std::vector<Release::pointer>(res.begin(), res.end());
}

Release::pointer
Release::getByMBID(Wt::Dbo::Session& session, const std::string& mbid)
{
	return session.find<Release>().where("mbid = ?").bind(mbid);
}

Release::pointer
Release::getById(Wt::Dbo::Session& session, Release::id_type id)
{
	return session.find<Release>().where("id = ?").bind(id);
}

Release::pointer
Release::create(Wt::Dbo::Session& session, const std::string& name, const std::string& MBID)
{
	return session.add(new Release(name, MBID));
}

bool
Release::isNone() const
{
	return _name == "<None>";
}

Release::pointer
Release::getNone(Wt::Dbo::Session& session)
{
	std::vector<pointer> res = getByName(session, "<None>");
	if (res.empty())
		return create(session, "<None>");

	return res.front();
}

std::vector<Release::pointer>
Release::getAll(Wt::Dbo::Session& session, int offset, int size)
{
	Wt::Dbo::collection<pointer> res = session.find<Release>().offset(offset).limit(size);
	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<Release::pointer>
Release::getAllOrphans(Wt::Dbo::Session& session)
{
	Wt::Dbo::collection<Release::pointer> res = session.query< Wt::Dbo::ptr<Release> >("select r from release r LEFT OUTER JOIN Track t ON r.id = t.release_id WHERE t.id IS NULL");

	return std::vector<pointer>(res.begin(), res.end());
}

Wt::Dbo::Query<Release::pointer>
Release::getQuery(Wt::Dbo::Session& session, SearchFilter filter)
{
	SqlQuery sqlQuery = generatePartialQuery(filter);

	Wt::Dbo::Query<pointer> query
		= session.query<pointer>("SELECT r FROM release r INNER JOIN artist a ON a.id = t.artist_id INNER JOIN track t ON t.release_id = r.id INNER JOIN cluster c ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id " + sqlQuery.where().get()).groupBy("r.id").orderBy("r.name");

	for (const std::string& bindArg : sqlQuery.where().getBindArgs())
		query.bind(bindArg);

	return query;
}

Wt::Dbo::Query<Release::UIQueryResult>
Release::getUIQuery(Wt::Dbo::Session& session, SearchFilter filter)
{
	SqlQuery sqlQuery = generatePartialQuery(filter);

	// TODO DATE of RELEASE
	Wt::Dbo::Query<UIQueryResult> query
		= session.query<UIQueryResult>("SELECT r.id, r.name, t.date, COUNT(DISTINCT t.id) FROM release r INNER JOIN track t ON t.release_id = r.id INNER JOIN artist a ON a.id = t.artist_id INNER JOIN cluster c ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id " + sqlQuery.where().get()).groupBy("r.id").orderBy("r.name");

	for (const std::string& bindArg : sqlQuery.where().getBindArgs())
		query.bind(bindArg);

	return query;
}

void
Release::updateUIQueryModel(Wt::Dbo::Session& session, Wt::Dbo::QueryModel<UIQueryResult>& model, SearchFilter filter, const std::vector<Wt::WString>& columnNames)
{
	Wt::Dbo::Query<UIQueryResult> query = getUIQuery(session, filter);

	model.setQuery(query, columnNames.empty() ? true : false);

	// TODO do something better
	if (columnNames.size() == 3)
	{
		model.addColumn( "r.name", columnNames[0]);
		model.addColumn( "t.date", columnNames[1]);
		model.addColumn( "COUNT(DISTINCT t.id)", columnNames[2]);
	}
}

std::vector<Release::pointer>
Release::getByFilter(Wt::Dbo::Session& session, SearchFilter filter, int offset, int size)
{
	Wt::Dbo::collection<pointer> res = getQuery(session, filter).limit(size).offset(offset);

	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<Release::pointer>
Release::getByFilter(Wt::Dbo::Session& session, SearchFilter filter, int offset, int size, bool& moreResults)
{
	auto res = getByFilter(session, filter, offset, size + 1);

	if (size != -1 && res.size() == static_cast<std::size_t>(size) + 1)
	{
		moreResults = true;
		res.pop_back();
	}
	else
		moreResults = false;

	return res;
}

int
Release::getReleaseYear(bool original) const
{
	assert(session());

	// TODO something better
	auto tracks = Track::getByFilter(*session(), SearchFilter::ById(SearchFilter::Field::Release, this->id()), -1, 1);

	if (tracks.empty())
		return 0;

	boost::gregorian::date date;

	if (original)
		date = tracks.front()->getOriginalDate().date();
	else
		date = tracks.front()->getDate().date();

	if (date.is_special())
		return 0;

	return date.year();
}

} // namespace Database
