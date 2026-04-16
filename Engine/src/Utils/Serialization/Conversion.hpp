#pragma once

#include <cstdint>

namespace Utils::Serialization::Conversion
{
	void ToNetwork(uint16_t in, uint16_t &out);
	void ToNetwork(uint32_t in, uint32_t &out);
	void ToNetwork(uint64_t in, uint64_t &out);
	void ToNetwork(float in, uint32_t &out);
	void ToNetwork(double in, uint64_t &out);

	void ToLocal(uint16_t in, uint16_t &out);
	void ToLocal(uint32_t in, uint32_t &out);
	void ToLocal(uint64_t in, uint64_t &out);
	void ToLocal(uint32_t in, float &out);
	void ToLocal(uint64_t in, double &out);
}