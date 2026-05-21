#pragma once

#include <vector>
#include <string>
#include "Conversion.hpp"
#include "Physic/AABB.h"
#include "galaxymath/Maths.h"

namespace Utils::Serialization
{
	class Deserializer
	{
	public:
		Deserializer(const uint8_t *data, uint64_t dataSize) : buffer(data), bufferSize(dataSize) {}
		Deserializer(const std::vector<uint8_t> &data) : Deserializer(data.data(), data.size()) {}

		~Deserializer() = default;

		const uint64_t CursorPos() const { return cPos; }
		const uint64_t BufferSize() const { return bufferSize; }

		bool Read(uint8_t &in);
		bool Read(int8_t &in);
		bool Read(uint16_t &in);
		bool Read(int16_t &in);
		bool Read(uint32_t &in);
		bool Read(int32_t &in);
		bool Read(uint64_t &in);
		bool Read(int64_t &in);
		bool Read(float &in);
		bool Read(double &in);
		bool Read(bool &in);
		bool Read(Vec2f &in);
		bool Read(Vec3f &in);
		bool Read(Vec4f &in);
		bool Read(Vec2i &in);
		bool Read(Vec3i &in);
		bool Read(Quat &in);
		bool Read(AABB &in);
		bool Read(uint8_t *dataIn, uint64_t dataSize);
		bool Read(std::string &in, uint64_t strSize);
		bool Read(std::string &in);
	private:
		const uint8_t *buffer;
		const uint64_t bufferSize;
		uint64_t cPos = 0;
	};

}