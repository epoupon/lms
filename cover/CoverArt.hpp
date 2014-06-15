#ifndef COVER_ART_HPP
#define COVER_ART_HPP

#include <vector>
#include <string>


namespace CoverArt
{

class CoverArt
{
	public:
		typedef std::vector<unsigned char>	data_type;

		CoverArt();
		CoverArt(const std::string& mime, const data_type& data) : _mimeType(mime), _data(data) {}

		const std::string&	getMimeType() const { return _mimeType; }
		const data_type&	getData() const { return _data; }

		void	setMimeType(const std::string& mimeType) { _mimeType = mimeType;}
		void	setData(const data_type& data) { _data = data; }

		bool	scale(std::size_t size);

	private:

		std::string	_mimeType;
		data_type	_data;
};


} // namespace CoverArt

#endif

