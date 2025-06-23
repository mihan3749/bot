#ifndef _CHAT_CONTEXT_H
#define _CHAT_CONTEXT_H

#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include "bot/models.h"
#include "bot/tools.h"
#include <tgbot/tgbot.h>

class EventHandler: public StateMachine<
	MainState, const std::shared_ptr<const Chat>&, TgBot::Bot&,
		TgBot::Message::Ptr, TgBot::CallbackQuery::Ptr>
{
public:
	EventHandler();

	void handle(MainState& state, const std::shared_ptr<const Chat>& chat,
		TgBot::Bot& bot, TgBot::Message::Ptr msg=nullptr,
		TgBot::CallbackQuery::Ptr query=nullptr);

private:
	bool is_input_valid(const MainState& state,
		const std::shared_ptr<const Chat>& chat,
		TgBot::Message::Ptr msg=nullptr,
		TgBot::CallbackQuery::Ptr query=nullptr);
};

#endif