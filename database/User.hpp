#ifndef DATABASE_USER_HPP
#define DATABASE_USER_HPP


#include <Wt/Auth/Dbo/AuthInfo>

namespace Database {

class User;
typedef Wt::Auth::Dbo::AuthInfo<User> AuthInfo;

class User {

	public:

		User();

		typedef Wt::Dbo::ptr<User>	pointer;

		// accessors
		static std::vector<pointer>	getAll(Wt::Dbo::Session& session);

		bool isAdmin() const {return _isAdmin;}

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _maxAudioBitrate, "max_audio_bitrate");
				Wt::Dbo::field(a, _maxVideoBitrate, "max_video_bitrate");
				Wt::Dbo::field(a, _isAdmin, "admin");
				Wt::Dbo::field(a, _audioBitrate, "audio_bitrate");
				Wt::Dbo::field(a, _videoBitrate, "video_bitrate");
			}

	private:

		static const std::size_t	maxAudioBitrate = 320000;
		static const std::size_t	maxVideoBitrate = 7500000;

		static const std::size_t	defaultAudioBitrate = 128000;
		static const std::size_t	defaultVideoBitrate = 1500000;

		// Admin defined settings
		int	 	_maxAudioBitrate;
		int		_maxVideoBitrate;
		bool		_isAdmin;

		// User defined settings
		int		_audioBitrate;
		int		_videoBitrate;

};

} // namespace Databas'

#endif
