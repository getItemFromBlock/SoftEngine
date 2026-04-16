#pragma once

#include <vector>
#include <string>
#include "Conversion.hpp"
#include "Physic/AABB.h"
#include "galaxymath/Maths.h"

namespace Utils::Serialization
{
	class Serializer
	{
	public:
		Serializer() = default;

		~Serializer() = default;

		const uint8_t *GetBuffer() const { return buffer.data(); }
		const uint64_t GetBufferSize() const { return buffer.size(); }

		void Write(uint8_t in);
		void Write(int8_t in);
		void Write(uint16_t in);
		void Write(int16_t in);
		void Write(uint32_t in);
		void Write(int32_t in);
		void Write(uint64_t in);
		void Write(int64_t in);
		void Write(float in);
		void Write(double in);
		void Write(bool in);
		void Write(const Vec2f &in);
		void Write(const Vec3f &in);
		void Write(const Vec4f &in);
		void Write(const Vec2i &in);
		void Write(const Vec3i &in);
		void Write(const Quat &in);
		void Write(const AABB &in);
		void Write(const uint8_t *dataIn, uint64_t dataSize);
		void Write(const std::string &in, bool writeSize = true);
	private:
		std::vector<uint8_t> buffer;
	};

}