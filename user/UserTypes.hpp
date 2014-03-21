#ifndef USER_HPP__
#define USER_HPP__

#include <Wt/Dbo/Dbo.hpp>


// Save user preferences for each view
class UserViewPreferences
{
	public:



	private:

		std::string agent;



};


class UserPreferences
{
	public:





	private:

		// Prefered format?

		std::size_t maxAudioBitrate;
		std::size_t maxVideoBitrate;
};

class UserRights
{
	public:



	private:

		std::size_t 	_maxAudioBitrate;
		std::size_t 	_maxVideoBitrate;
		bool		_canDownload;
};

class User
{
	public:


	private:


		Wt::Dbo::Ptr< UserRights >	_rights;
		Wt::Dbo::Ptr< UserPreferences >	_preferences;
};


#endif

