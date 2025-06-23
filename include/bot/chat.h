#ifndef _CHAT_H
#define _CHAT_H

#include "bot/handlers.h"
#include <tgbot/tgbot.h>
#include <tgbot/types/GenericReply.h>
#include <tgbot/types/InlineKeyboardButton.h>
#include <tgbot/types/InputFile.h>
#include <tgbot/types/Message.h>
#include <tgbot/types/ReplyKeyboardMarkup.h>

class Bot: public EventHandler
{
public:
	Bot(const std::string& token);

	void infinit_polling();

	void finish();

private:
	void clear_queue();

	TgBot::Bot bot;
	bool finished;
};

class Message
{
public:
	Message(const std::string& text="",
		TgBot::InputFile::Ptr image=nullptr,
		TgBot::GenericReply::Ptr keyboard=nullptr,
		int64_t edit_msg_id=0);

	int32_t send(TgBot::Bot& bot, int64_t chat);

private:
	int32_t edit(TgBot::Bot& bot, int64_t chat);

	std::string text;
	TgBot::InputFile::Ptr image;
	TgBot::GenericReply::Ptr keyboard;
	int64_t msg_id;
};

enum class KeyboardType {Inline, Reply};

TgBot::InputFile::Ptr image(
	const std::string& file_name, const std::string& mime_type="");

TgBot::GenericReply::Ptr keyboard(
	KeyboardType type, const std::vector<int>& markup,
	const std::vector<std::string>& text,
	const std::vector<std::string>& callback_data={});

void remove_inline_keyboard(TgBot::Bot& bot, TgBot::CallbackQuery::Ptr query);

void remove_reply_keyboard(TgBot::Bot& bot, int64_t chat_id);

void delete_message(TgBot::Bot& bot, int64_t chat_id, int32_t msg_id);

void set_next_state(const rapidjson::Value& next_state, id_t chat);

void send_message(const TextManager& tm,
	const char* config,
	const std::shared_ptr<const Chat>& chat,
	TgBot::Bot& bot,
	const std::string& text="",
	TgBot::InputFile::Ptr image=nullptr,
	TgBot::GenericReply::Ptr keyboard=nullptr);

bool generic_handler(const TextManager& tm, const char* config,
	const std::shared_ptr<const Chat>& chat,
	TgBot::Bot& bot, TgBot::Message::Ptr msg=nullptr,
	TgBot::CallbackQuery::Ptr query=nullptr,
	std::function<bool()> proc_answ=nullptr);

TgBot::InlineKeyboardMarkup::Ptr make_calendar_keyboard(const TextManager& tm,
	int year, int month);

#endif