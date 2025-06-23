#ifndef _CHAT_BOT_APP_H
#define _CHAT_BOT_APP_H

#include "bot/chat.h"
#include "bot/database.h"
#include "bot/tools.h"
#include <string>

class ChatBotApp
{
public:
	ChatBotApp(const std::string& file_name);

	void start();

private:
	rapidjson::Document config;
	DB1 db;
	Bot bot;
	TextManager tm;
};

#endif