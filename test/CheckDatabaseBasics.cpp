#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>

#include "database/DatabaseHandler.hpp"

#include "database/FileTypes.hpp"

int main(void)
{

	try {

		boost::filesystem::remove("test2.db");

		// Set up the long living database session
		DatabaseHandler database("test2.db");

		Wt::Dbo::Transaction transaction(database.getSession());

		std::cout << "Creating objects..." << std::endl;

		assert( Path::getRoots(database.getSession()).empty() );

		Path::pointer parentPath = database.getSession().add(new Path("/PARENT") );

		assert( Path::getRoots(database.getSession()).size() == 1);

		for (std::size_t i = 0; i < 5; ++i)
		{
			std::ostringstream oss; oss << "/PARENT/toto" << i << ".mp4";
			Path::pointer childPath = database.getSession().add(new Path( oss.str()) );

			{
				std::ostringstream oss; oss << "/PARENT/toto" << i << "_dir";
				Path::pointer childDirPath = database.getSession().add(new Path( oss.str()) );
				for (std::size_t j = 0; j < 6; ++j) {

					std::ostringstream oss2; oss2 << "/PARENT/toto" << i << "_dir/" << j << ".mp4";
					Path::pointer childPath2 = database.getSession().add(new Path( oss2.str()) );

					childDirPath.modify()->addChild( childPath2 );
				}

				childDirPath.modify()->addChild( childDirPath );
			}

			parentPath.modify()->addChild( childPath );

			assert( childPath->getParent());
		}

		{
			std::vector<Path::pointer> roots = Path::getRoots(database.getSession());

			std::cout << "There are now " << roots.size() << " roots!" << std::endl;
			BOOST_FOREACH(Path::pointer root, roots)
			{
				std::cout << "ROOT Path = " << root->getPath() << std::endl;
			}
		}

		assert( !parentPath->getParent() );

		std::cout << "PRINT objects..." << std::endl;

		std::vector< Path::pointer > pathes = parentPath->getChilds();
		BOOST_FOREACH(Path::pointer path, pathes)
		{
			std::cout << "Found a child: " << path->getPath()  << std::endl;
			std::cout << "Parent's = " << path->getParent()->getPath() << std::endl;
		}



		// make some empty root dirs

		transaction.commit();

	}
	catch(std::exception& e)
	{
		std::cerr << "Caught exception " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
