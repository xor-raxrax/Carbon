#pragma once

import <iostream>;

typedef void* HANDLE;

class Console
{
public:
	enum class Color
	{
		Grey,
		Blue,
		Green,
		Cyan,
		Red,
		Pink,
		Yellow,
		White,
	};

	Console(const std::string& title);
	~Console();
	void setDec() const;
	void setHex() const;
	void setColor(Color color);
	Color getColor() const;
private:
	FILE* file = nullptr;
	HANDLE handle = nullptr;
	Color currentColor = Color::White;
};