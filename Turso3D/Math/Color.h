#pragma once

#include <Turso3D/Math/Math.h>
#include <Turso3D/Math/Vector4.h>

namespace Turso3D
{
	// RGBA color.
	class Color
	{
	public:
		// Construct undefined.
		Color()
		{
		}

		// Copy-construct.
		Color(const Color& color) :
			r(color.r),
			g(color.g),
			b(color.b),
			a(color.a)
		{
		}

		// Construct from another color and modify the alpha.
		Color(const Color& color, float a) :
			r(color.r),
			g(color.g),
			b(color.b),
			a(a)
		{
		}

		// Construct from RGB values and set alpha fully opaque.
		Color(float r, float g, float b) :
			r(r),
			g(g),
			b(b),
			a(1.0f)
		{
		}

		// Construct from RGBA values.
		Color(float r, float g, float b, float a) :
			r(r),
			g(g),
			b(b),
			a(a)
		{
		}

		// Add-assign a color.
		Color& operator += (const Color& rhs)
		{
			r += rhs.r;
			g += rhs.g;
			b += rhs.b;
			a += rhs.a;
			return *this;
		}

		// Test for equality with another color without epsilon.
		bool operator == (const Color& rhs) const
		{
			return r == rhs.r && g == rhs.g && b == rhs.b && a == rhs.a;
		}

		// Test for inequality with another color without epsilon.
		bool operator != (const Color& rhs) const
		{
			return !(*this == rhs);
		}

		// Multiply with a scalar.
		Color operator * (float rhs) const
		{
			return Color {r * rhs, g * rhs, b * rhs, a * rhs};
		}

		// Add a color.
		Color operator + (const Color& rhs) const
		{
			return Color {r + rhs.r, g + rhs.g, b + rhs.b, a + rhs.a};
		}

		// Substract a color.
		Color operator - (const Color& rhs) const
		{
			return Color {r - rhs.r, g - rhs.g, b - rhs.b, a - rhs.a};
		}

		// Return float data.
		const float* Data() const
		{
			return &r;
		}

		// Return color packed to a 32-bit integer, with R component in the lowest 8 bits.
		// Components are clamped to [0, 1] range.
		unsigned ToUInt() const
		{
			unsigned r_ = Clamp(((int)(r * 255.0f)), 0, 255);
			unsigned g_ = Clamp(((int)(g * 255.0f)), 0, 255);
			unsigned b_ = Clamp(((int)(b * 255.0f)), 0, 255);
			unsigned a_ = Clamp(((int)(a * 255.0f)), 0, 255);
			return (a_ << 24) | (b_ << 16) | (g_ << 8) | r_;
		}

		// Return RGB as a three-dimensional vector.
		Vector3 ToVector3() const
		{
			return Vector3(r, g, b);
		}

		// Return RGBA as a four-dimensional vector.
		Vector4 ToVector4() const
		{
			return Vector4(r, g, b, a);
		}

		// Return sum of RGB components.
		float SumRGB() const
		{
			return r + g + b;
		}

		// Return average value of the RGB channels.
		float Average() const
		{
			return (r + g + b) / 3.0f;
		}

		// Return linear interpolation of this color with another color.
		Color Lerp(const Color& rhs, float t) const
		{
			float invT = 1.0f - t;
			return Color(
				r * invT + rhs.r * t,
				g * invT + rhs.g * t,
				b * invT + rhs.b * t,
				a * invT + rhs.a * t
			);
		}

		// Return color with absolute components.
		Color Abs() const
		{
			return Color(std::abs(r), std::abs(g), std::abs(b), std::abs(a));
		}

		// Test for equality with another color with epsilon.
		bool Equals(const Color& rhs, float epsilon = M_EPSILON) const
		{
			return EpsilonEquals(r, rhs.r, epsilon) && EpsilonEquals(g, rhs.g, epsilon) && EpsilonEquals(b, rhs.b, epsilon) && EpsilonEquals(a, rhs.a, epsilon);
		}

		// Opaque white color.
		static Color WHITE();
		// Opaque gray color.
		static Color GRAY();
		// Opaque black color.
		static Color BLACK();
		// Opaque red color.
		static Color RED();
		// Opaque green color.
		static Color GREEN();
		// Opaque blue color.
		static Color BLUE();
		// Opaque cyan color.
		static Color CYAN();
		// Opaque magenta color.
		static Color MAGENTA();
		// Opaque yellow color.
		static Color YELLOW();

	public:
		// Red value.
		float r;
		// Green value.
		float g;
		// Blue value.
		float b;
		// Alpha value.
		float a;
	};

	// ==========================================================================================
	// Multiply Color with a scalar.
	inline Color operator * (float lhs, const Color& rhs)
	{
		return rhs * lhs;
	}

	inline Color Color::WHITE() { return Color {1.0f, 1.0f, 1.0f}; }
	inline Color Color::GRAY() { return Color {0.5f, 0.5f, 0.5f}; }
	inline Color Color::BLACK() { return Color {0.0f, 0.0f, 0.0f}; }
	inline Color Color::RED() { return Color {1.0f, 0.0f, 0.0f}; }
	inline Color Color::GREEN() { return Color {0.0f, 1.0f, 0.0f}; }
	inline Color Color::BLUE() { return Color {0.0f, 0.0f, 1.0f}; }
	inline Color Color::CYAN() { return Color {0.0f, 1.0f, 1.0f}; }
	inline Color Color::MAGENTA() { return Color {1.0f, 0.0f, 1.0f}; }
	inline Color Color::YELLOW() { return Color {1.0f, 1.0f, 0.0f}; }
}
