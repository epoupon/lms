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

#pragma once

#include <cstdint>
#include <cassert>
#include <functional>
#include <Wt/Dbo/ptr.h>

namespace Database
{
	class IdType
	{
		public:
			using ValueType = Wt::Dbo::dbo_default_traits::IdType;

			IdType() = default;
			IdType(ValueType id) : _id {id} { assert(isValid()); }

			bool isValid() const { return _id != Wt::Dbo::dbo_default_traits::invalidId(); }
			std::string toString() const { assert(isValid()); return std::to_string(_id); }

			ValueType getValue() const { return _id; }

			bool operator==(IdType other) const { return other._id == _id; }
			bool operator!=(IdType other) const { return !(*this == other); }
			bool operator<(IdType other) const { return other._id < _id; }

		private:
			Wt::Dbo::dbo_default_traits::IdType _id {Wt::Dbo::dbo_default_traits::invalidId()};
	};

	struct Range
	{
		std::size_t offset {};
		std::size_t limit {};
	};

	enum class TrackArtistLinkType
	{
		Artist,	// regular artist
		Arranger,
		Composer,
		Conductor,
		Lyricist,
		Mixer,
		Performer,
		Producer,
		ReleaseArtist,
		Remixer,
		Writer,
	};

	// User selectable audio file formats
	// Do not change values
	enum class AudioFormat
	{
		MP3		= 1,
		OGG_OPUS	= 2,
		OGG_VORBIS	= 3,
		WEBM_VORBIS	= 4,
		MATROSKA_OPUS	= 5,
	};
	using Bitrate = std::uint32_t;

	// Do not change enum values!
	enum class Scrobbler
	{
		Internal		= 0,
		ListenBrainz	= 1,
	};

	// Do not change enum values!
	enum class UserType
	{
		REGULAR	= 0,
		ADMIN	= 1,
		DEMO	= 2,
	};

	template <typename T>
	class ObjectPtr
	{
		public:
			ObjectPtr() = default;
			ObjectPtr(Wt::Dbo::ptr<T> obj) : _obj {obj} {}

			const T* operator->() const { return _obj.get(); }
			operator bool() const { return _obj.get(); }
			bool operator!() const { return !_obj.get(); }
			bool operator==(const ObjectPtr& other) const { return other._obj == _obj; }
			bool operator!=(const ObjectPtr& other) const { return other._obj != _obj; }

			auto modify() { return _obj.modify(); }
			void remove() { _obj.remove(); }

		private:
			template <typename, typename> friend class Object;
			Wt::Dbo::ptr<T> _obj;
	};

	template <typename T, typename ObjectIdType>
	class Object : public Wt::Dbo::Dbo<T>
	{
		static_assert(std::is_base_of_v<Database::IdType, ObjectIdType>);
		static_assert(!std::is_same_v<Database::IdType, ObjectIdType>);

		public:
			using pointer = ObjectPtr<T>;
			using IdType = ObjectIdType;

			IdType getId() const { return Wt::Dbo::Dbo<T>::self()->Wt::Dbo::template Dbo<T>::id(); }

			// catch some misuses
			typename Wt::Dbo::dbo_traits<T>::IdType id() const = delete;

		protected:
			// Can get raw dbo ptr only from Objects
			template <typename SomeObject>
			static
			Wt::Dbo::ptr<SomeObject> getDboPtr(ObjectPtr<SomeObject> ptr) { return ptr._obj; }
	};
}

// TODO factorize hash with std::enable_if
#define LMS_DECLARE_IDTYPE(name) \
namespace Database { \
	class name : public IdType \
	{ \
		public: \
			using IdType::IdType; \
	};\
} \
namespace std \
{ \
	template<> \
	class hash<Database::name> \
	{ \
		public: \
			size_t operator()(Database::name id) const \
			{ \
				return std::hash<Database::name::ValueType>()(id.getValue()); \
			} \
	}; \
} // ns std

LMS_DECLARE_IDTYPE(ArtistId)
LMS_DECLARE_IDTYPE(AuthTokenId)
LMS_DECLARE_IDTYPE(ClusterId)
LMS_DECLARE_IDTYPE(ClusterTypeId)
LMS_DECLARE_IDTYPE(ReleaseId)
LMS_DECLARE_IDTYPE(ScanSettingsId)
LMS_DECLARE_IDTYPE(TrackArtistLinkId)
LMS_DECLARE_IDTYPE(TrackBookmarkId)
LMS_DECLARE_IDTYPE(TrackFeaturesId)
LMS_DECLARE_IDTYPE(TrackId)
LMS_DECLARE_IDTYPE(TrackListId)
LMS_DECLARE_IDTYPE(TrackListEntryId)
LMS_DECLARE_IDTYPE(UserId)


