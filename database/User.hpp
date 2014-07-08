#ifndef DATABASE_USER_HPP
#define DATABASE_USER_HPP


#include <Wt/Auth/Dbo/AuthInfo>

namespace Database {

class User;
typedef Wt::Auth::Dbo::AuthInfo<User> AuthInfo;

class User {

	public:

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _isAdmin, "admin");
			}

	private:

		bool	_isAdmin;

};

} // namespace Databas'

#endif
