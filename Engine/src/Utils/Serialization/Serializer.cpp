#include "Serializer.hpp"

using namespace Utils::Serialization;

void Serializer::Write(uint8_t in)
{
	buffer.push_back(in);
}

void Serializer::Write(int8_t in)
{
	buffer.push_back(static_cast<uint8_t>(in));
}

void Serializer::Write(uint16_t in)
{
	uint16_t tmp;
	Conversion::ToNetwork(in, tmp);
	for (size_t i = 0; i < sizeof(tmp); i++)
	{
		buffer.push_back((tmp >> (i*8) & 0xff));
	}
}

void Serializer::Write(int16_t in)
{
	Write(static_cast<uint16_t>(in));
}

void Serializer::Write(uint32_t in)
{
	uint32_t tmp;
	Conversion::ToNetwork(in, tmp);
	for (size_t i = 0; i < sizeof(tmp); i++)
	{
		buffer.push_back((tmp >> (i * 8) & 0xff));
	}
}

void Serializer::Write(int32_t in)
{
	Write(static_cast<uint32_t>(in));
}

void Serializer::Write(uint64_t in)
{
	uint64_t tmp;
	Conversion::ToNetwork(in, tmp);
	for (size_t i = 0; i < sizeof(tmp); i++)
	{
		buffer.push_back((tmp >> (i * 8) & 0xff));
	}
}

void Serializer::Write(int64_t in)
{
	Write(static_cast<uint64_t>(in));
}

void Serializer::Write(float in)
{
	uint32_t tmp;
	Conversion::ToNetwork(in, tmp);
	for (size_t i = 0; i < sizeof(tmp); i++)
	{
		buffer.push_back((tmp >> (i * 8) & 0xff));
	}
}

void Serializer::Write(double in)
{
	uint64_t tmp;
	Conversion::ToNetwork(in, tmp);
	for (size_t i = 0; i < sizeof(tmp); i++)
	{
		buffer.push_back((tmp >> (i * 8) & 0xff));
	}
}

void Serializer::Write(bool in)
{
	Write(static_cast<uint8_t>(in));
}

void Serializer::Write(const uint8_t* dataIn, uint64_t dataSize)
{
	buffer.resize(buffer.size() + dataSize);
	std::copy(dataIn, dataIn + dataSize, buffer.data() + (buffer.size() - dataSize));
}

void Serializer::Write(const std::string& in, bool writeSize)
{
	if (writeSize) Write(in.size());
	Write(reinterpret_cast<const uint8_t*>(in.data()), in.size());
}

void Serializer::Write(const Vec2f &in)
{
	Write(in.x);
	Write(in.y);
}

void Serializer::Write(const Vec3f &in)
{
	Write(in.x);
	Write(in.y);
	Write(in.z);
}

void Serializer::Write(const Vec4f &in)
{
	Write(in.x);
	Write(in.y);
	Write(in.z);
	Write(in.w);
}

void Serializer::Write(const Vec2i &in)
{
	Write(in.x);
	Write(in.y);
}

void Serializer::Write(const Vec3i &in)
{
	Write(in.x);
	Write(in.y);
	Write(in.z);
}

void Serializer::Write(const Quat &in)
{
	Write(in.x);
	Write(in.y);
	Write(in.z);
	Write(in.w);
}

void Serializer::Write(const AABB &in)
{
	Write(in.Min);
	Write(in.Max);
}