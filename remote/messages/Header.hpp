#ifndef REMOTE_HEADER_HPP
#define REMOTE_HEADER_HPP

namespace Remote
{


class Header
{
	public:

		static const std::size_t size = 8;

		Header() : _size(0) {}

		void setSize(std::size_t size)	{ _size = size; }
		std::size_t getSize(void) const	{return _size;}


		bool from_istream(std::istream &is)
		{
			std::array<unsigned char, size>	buffer;

			bool res = is.read(reinterpret_cast<char*>(buffer.data()), buffer.size());

			if (res && is.gcount() == buffer.size())
				return from_buffer(buffer);
			else
				return false;
		}

		bool from_buffer(const std::array<unsigned char, size>& buffer)
		{
			if (decode32(&buffer[0]) != _magic) {
				std::cerr << "Header: bad magic" << std::endl;
				return false;
			}
			else
			{
				_size = decode32(&buffer[4]);

				if (_size > _maxSize)
					std::cerr << "Header: msg too big (" << _size << ")!" << std::endl;

				return _size <= _maxSize;
			}
		}

/*		void to_ostream(std::ostream& os)
		{


		}*/

		void to_buffer(std::array<unsigned char, size>& buffer) const
		{
			encode32(_magic, &buffer[0]);
			encode32(_size, &buffer[4]);
		}

	private:
		static uint32_t	decode32(const unsigned char* data)
		{
			return (static_cast<uint32_t>(data[0]) << 24)
				+ (static_cast<uint32_t>(data[1]) << 16)
				+ (static_cast<uint32_t>(data[2]) << 8)
				+ (static_cast<uint32_t>(data[3]));
		}

		static void		encode32(uint32_t value, unsigned char* data)
		{
			data[0] = (value >> 24) & 0xFF;
			data[1] = (value >> 16) & 0xFF;
			data[2] = (value >> 8) & 0xFF;
			data[3] = (value) & 0xFF;
		}

		static const uint32_t _magic = 0xbeef;
		static const uint32_t _maxSize = 65536;

		uint32_t	_size;
};



} // namespace Remote


#endif

