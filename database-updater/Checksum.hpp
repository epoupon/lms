#include <vector>
#include <boost/filesystem.hpp>


void computeCrc(const boost::filesystem::path& p, std::vector<unsigned char>& checksum);

