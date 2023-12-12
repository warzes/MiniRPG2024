#include "Core.h"
//-----------------------------------------------------------------------------
void logMessage(const std::string& text)
{
	puts(text.c_str());
}
//-----------------------------------------------------------------------------
void Print(const std::string& text)
{
	logMessage(text);
}
//-----------------------------------------------------------------------------
void Warning(const std::string& text)
{
	logMessage("WARNING:" + text);
}
//-----------------------------------------------------------------------------
void Error(const std::string& text)
{
	logMessage("ERROR:" + text);
}
//-----------------------------------------------------------------------------
void Fatal(const std::string& text)
{
	extern bool IsEngineClose;
	IsEngineClose = true;
	logMessage("FATAL:" + text);
}
//-----------------------------------------------------------------------------