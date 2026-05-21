#pragma once
#include <cmath>

struct f_vector
{
	float m_x; // 0x00(0x04)
	float m_y; // 0x04(0x04)
	float m_z; // 0x08(0x04)

	inline f_vector()
		: m_x(0.f), m_y(0.f), m_z(0.f)
	{
	}

	inline f_vector(decltype(m_x) value)
		: m_x(value), m_y(value), m_z(value)
	{
	}

	inline f_vector(decltype(m_x) x, decltype(m_y) y, decltype(m_z) z)
		: m_x(x), m_y(y), m_z(z)
	{
	}

	bool is_zero() const
	{
		return (m_x == 0.f && m_y == 0.f && m_z == 0.f);
	}
	
	// Squared length (for fast comparison without sqrt)
	inline float length_sqr() const
	{
		return (m_x * m_x) + (m_y * m_y) + (m_z * m_z);
	}
	
	// 3D distance to another point
	inline float dist_to(const f_vector& other) const
	{
		float dx = m_x - other.m_x;
		float dy = m_y - other.m_y;
		float dz = m_z - other.m_z;
		return std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
	}
	
	// Arithmetic operators for LERP and other calculations
	inline f_vector operator-(const f_vector& other) const
	{
		return f_vector(m_x - other.m_x, m_y - other.m_y, m_z - other.m_z);
	}
	
	inline f_vector operator+(const f_vector& other) const
	{
		return f_vector(m_x + other.m_x, m_y + other.m_y, m_z + other.m_z);
	}
	
	inline f_vector operator*(float scalar) const
	{
		return f_vector(m_x * scalar, m_y * scalar, m_z * scalar);
	}
};
static_assert(sizeof(f_vector) == 0x0c, "wrong size on f_vector");