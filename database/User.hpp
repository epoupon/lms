#ifndef DATABASE_USER_HPP
#define DATABASE_USER_HPP


#include <Wt/Auth/Dbo/AuthInfo>

namespace Database {

class User;
typedef Wt::Auth::Dbo::AuthInfo<User> AuthInfo;

class User {

	public:

		typedef Wt::Dbo::ptr<User>	pointer;

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
