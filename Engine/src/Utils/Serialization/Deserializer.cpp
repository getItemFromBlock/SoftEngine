#include "Deserializer.hpp"

#include "Debug/Log.h"

using namespace Utils::Serialization;

bool Deserializer::Read(uint8_t &in)
{
	if (cPos >= bufferSize) return false;
	in = buffer[cPos++];
	return true;
}

bool Deserializer::Read(int8_t &in)
{
	return Read(reinterpret_cast<uint8_t &>(in));
}

bool Deserializer::Read(uint16_t &in)
{
	uint16_t tmp = 0;
	if (cPos + sizeof(tmp) > bufferSize) return false;
	for (size_t i = 0; i < sizeof(tmp); i++)
	{
		tmp |= (uint64_t)buffer[cPos++] << (i * 8);
	}
	Conversion::ToLocal(tmp, in);
	return true;
}

bool Deserializer::Read(int16_t &in)
{
	return Read(reinterpret_cast<uint16_t &>(in));
}

bool Deserializer::Read(uint32_t &in)
{
	uint32_t tmp = 0;
	if (cPos + sizeof(tmp) > bufferSize) return false;
	for (size_t i = 0; i < sizeof(tmp); i++)
	{
		tmp |= (uint64_t)buffer[cPos++] << (i * 8);
	}
	Conversion::ToLocal(tmp, in);
	return true;
}

bool Deserializer::Read(int32_t &in)
{
	return Read(reinterpret_cast<uint32_t &>(in));
}

bool Deserializer::Read(uint64_t &in)
{
	uint64_t tmp = 0;
	if (cPos + sizeof(tmp) > bufferSize) return false;
	for (size_t i = 0; i < sizeof(tmp); i++)
	{
		tmp |= (uint64_t)buffer[cPos++] << (i * 8);
	}
	Conversion::ToLocal(tmp, in);
	return true;
}

bool Deserializer::Read(int64_t &in)
{
	return Read(reinterpret_cast<uint64_t &>(in));
}

bool Deserializer::Read(float &in)
{
	uint32_t tmp = 0;
	if (cPos + sizeof(tmp) > bufferSize) return false;
	for (size_t i = 0; i < sizeof(tmp); i++)
	{
		tmp |= (uint64_t)buffer[cPos++] << (i * 8);
	}
	Conversion::ToLocal(tmp, in);
	return true;
}

bool Deserializer::Read(double &in)
{
	uint64_t tmp = 0;
	if (cPos + sizeof(tmp) > bufferSize) return false;
	for (size_t i = 0; i < sizeof(tmp); i++)
	{
		tmp |= (uint64_t)buffer[cPos++] << (i * 8);
	}
	Conversion::ToLocal(tmp, in);
	return true;
}

bool Deserializer::Read(bool &in)
{
	return Read(reinterpret_cast<uint8_t &>(in));
}

bool Deserializer::Read(uint8_t *dataIn, uint64_t dataSize)
{
	if (cPos + dataSize > bufferSize) return false;
	std::copy(buffer + cPos, buffer + (cPos + dataSize), dataIn);
	cPos += dataSize;
	return true;
}

bool Deserializer::Read(std::string &in)
{
	uint64_t size;
	if (!Read(size)) return false;
	ASSERT(size + cPos <= bufferSize);
	in.resize(size);
	return Read(reinterpret_cast<uint8_t *>(in.data()), size);
}

bool Deserializer::Read(Vec2f &in)
{
	return Read(in.x) && Read(in.y);
}

bool Deserializer::Read(Vec3f &in)
{
	return Read(in.x) && Read(in.y) && Read(in.z);
}

bool Deserializer::Read(Vec4f &in)
{
	return Read(in.x) && Read(in.y) && Read(in.z) && Read(in.w);
}

bool Deserializer::Read(Vec2i &in)
{
	return Read(in.x) && Read(in.y);
}

bool Deserializer::Read(Vec3i &in)
{
	return Read(in.x) && Read(in.y) && Read(in.z);
}

bool Deserializer::Read(Quat &in)
{
	return Read(in.x) && Read(in.y) && Read(in.z) && Read(in.w);
}

bool Deserializer::Read(AABB &in)
{
	return Read(in.Min) && Read(in.Max);
}