#include "bot/chat.h"
#include "bot/logic.h"
#include "bot/models.h"
#include "bot/tools.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <tgbot/tools/StringTools.h>
#include <tgbot/types/CallbackQuery.h>
#include <tgbot/types/GenericReply.h>
#include <tgbot/types/Message.h>
#include <tgbot/types/ReplyKeyboardMarkup.h>
#include <vector>

static void handler_wrapper(Bot& bot, TgBot::Bot& tg_bot,
	TgBot::Message::Ptr msg=nullptr, TgBot::CallbackQuery::Ptr query=nullptr)
{
	int64_t tg_id = msg == nullptr ? query->from->id : msg->from->id;
	auto user = get_user(tg_id);
	if (user == nullptr) {
		std::string name = msg->from->firstName;
		if (msg->from->lastName != "")
			name += " " + msg->from->lastName;
		create_user_and_chat(tg_id, msg->from->username,
			name, msg->chat->id);
		user = get_user(tg_id);
		log_user(user->id());
	}

	if (msg != nullptr) {
		if (msg->text == "/menu") {
			set_chat_state(user->chat->id(), MainState::CmdMainMenu);
		}
	}

	bot.handle(const_cast<MainState&>(user->chat->ms), user->chat.get(), tg_bot, msg, query);

	if (query) {
		user->chat->last_query_msg_date = query->message->editDate ?
			query->message->editDate :
			query->message->date;
	} else {
		user->chat->last_query_msg_date = 0;
	}
}

Bot::Bot(const std::string& token)
:bot{TgBot::Bot(token)}, finished{false}
{	
	TgBot::Bot& bot_ref {bot};
	bot.getEvents().onCallbackQuery([&bot_ref, this](auto query) {
		handler_wrapper(*this, bot_ref, nullptr, query);
		try {
			bot.getApi().answerCallbackQuery(query->id);	
		} catch (const std::exception& e) {
			std::cerr << "answerCallbackQuery: " << e.what() << "\n";
		}
	});

	bot.getEvents().onAnyMessage([&bot_ref, this](auto msg) {
		handler_wrapper(*this, bot_ref, msg, nullptr);
	});	
}

void Bot::clear_queue()
{
	auto updates = bot.getApi().getUpdates(-1);
	if (!updates.empty()) {
		int64_t lastUpdateId = updates.back()->updateId;
		bot.getApi().getUpdates(lastUpdateId + 1);
	}
}

void Bot::infinit_polling()
{
	clear_queue();
	TgBot::TgLongPoll longPoll(bot, 100, 5);
	while (!finished) {
		try {
			longPoll.start();
		} catch (TgBot::TgException& e) {
			std::cerr << "TgBot error: " << e.what() << "\n";
			clear_queue();
		}
		request_db_save();
	}
}

void Bot::finish()
{
	finished = true;
}

Message::Message(const std::string& text,
	TgBot::InputFile::Ptr image,
	TgBot::GenericReply::Ptr keyboard,
	int64_t edit_msg_id)
:text{text}, image{image}, keyboard{keyboard}, msg_id(edit_msg_id)
{}

int32_t Message::send(TgBot::Bot& bot, int64_t chat)
{
	if (msg_id)
		return edit(bot, chat);

	TgBot::Message::Ptr msg;
	if (image != nullptr) {
		msg = bot.getApi().sendPhoto(chat, image, text, 0, keyboard, "html");
	} else {
		auto preview = std::make_shared<TgBot::LinkPreviewOptions>();
		preview->isDisabled = true;
		msg = bot.getApi().sendMessage(chat, text, preview, 0, keyboard, "html");
	}

	return msg->messageId;
}

int32_t Message::edit(TgBot::Bot& bot, int64_t chat)
{
	if (image != nullptr) {
		TgBot::InputMediaPhoto::Ptr media(new TgBot::InputMediaPhoto);
		media->media = "attach://data/img/" + image->fileName;
		bot.getApi().editMessageMedia(media, chat, msg_id);
	}

	if (std::dynamic_pointer_cast<TgBot::ReplyKeyboardMarkup>(keyboard)) {
		delete_message(bot, chat, msg_id);
		msg_id = 0;
		return send(bot, chat);
	}

	auto preview = std::make_shared<TgBot::LinkPreviewOptions>();
	preview->isDisabled = true;
	auto msg = bot.getApi().editMessageText(text, chat, msg_id,
		"", "html", preview,
		std::dynamic_pointer_cast<TgBot::InlineKeyboardMarkup>(keyboard));
	return msg->messageId;
}

void validate_keybord_input(
	KeyboardType type, const std::vector<int>& markup,
	const std::vector<std::string>& text,
	const std::vector<std::string>& callback_data)
{
	unsigned total = 0;
	for (unsigned row = 0; row < markup.size(); ++row) {
		if (markup[row] < 1)
			throw std::runtime_error("Keyboard::validate(): row size < 1");

		total += markup[row];
	}

	if (total != text.size())
		throw std::runtime_error(
			"Keyboard::validate(): total != text.size()");

	if (type == KeyboardType::Inline && total != callback_data.size())
		throw std::runtime_error(
			"Keyboard::validate(): total != callback_data.size()");
}

TgBot::InlineKeyboardMarkup::Ptr inline_keyboard(
	const std::vector<int>& markup,
	const std::vector<std::string>& text,
	const std::vector<std::string>& callback_data)
{
	std::vector<std::vector<TgBot::InlineKeyboardButton::Ptr>> buttons (
		markup.size());

	unsigned total = 0;
	for (unsigned row = 0; row < markup.size(); ++row) {
		buttons[row].resize(markup[row]);
		for (unsigned col = 0; col < markup[row]; ++col) {
			auto btn = std::make_shared<TgBot::InlineKeyboardButton>();
			btn->text = text[total];
			btn->callbackData = callback_data[total];
			buttons[row][col] = btn;
			++total;
		}
	}

	auto res = std::make_shared<TgBot::InlineKeyboardMarkup>();
	res->inlineKeyboard = buttons;
	return res;
}

TgBot::ReplyKeyboardMarkup::Ptr reply_keyboard(
	const std::vector<int>& markup,
	const std::vector<std::string>& text)
{
	std::vector<std::vector<TgBot::KeyboardButton::Ptr>> buttons (
		markup.size());

	unsigned total = 0;
	for (unsigned row = 0; row < markup.size(); ++row) {
		buttons[row].resize(markup[row]);
		for (unsigned col = 0; col < markup[row]; ++col) {
			auto btn = std::make_shared<TgBot::KeyboardButton>();
			btn->text = text[total];
			buttons[row][col] = btn;
			++total;
		}
	}

	auto res = std::make_shared<TgBot::ReplyKeyboardMarkup>();
	res->keyboard = buttons;
	res->resizeKeyboard = true;
	return res;
}

TgBot::GenericReply::Ptr keyboard(
	KeyboardType type, const std::vector<int>& markup,
	const std::vector<std::string>& text,
	const std::vector<std::string>& callback_data)
{
	validate_keybord_input(type, markup, text, callback_data);

	switch (type) {
	case KeyboardType::Inline:
		return inline_keyboard(markup, text, callback_data);
	case KeyboardType::Reply:
		return reply_keyboard(markup, text);
	}
}

std::string deduce_image_mime_type(const std::string& file_name)
{
	if (StringTools::endsWith(file_name, ".jpg"))
		return "image/jpeg";
	else if (StringTools::endsWith(file_name, ".png"))
		return "image/png";
	else
		return "";
}

TgBot::InputFile::Ptr image(
	const std::string& file_name, const std::string& mime_type)
{
	if (mime_type == "")
		return TgBot::InputFile::fromFile(
			file_name, deduce_image_mime_type(file_name));

	return TgBot::InputFile::fromFile(file_name, mime_type);
}

void remove_inline_keyboard(TgBot::Bot& bot, TgBot::CallbackQuery::Ptr query) {
	bot.getApi().editMessageReplyMarkup(
		query->message->chat->id,
		query->message->messageId,
		"",
		nullptr
	);
}

void remove_reply_keyboard(TgBot::Bot& bot, int64_t chat_id)
{
	TgBot::ReplyKeyboardRemove::Ptr keyboard(new TgBot::ReplyKeyboardRemove);
	keyboard->removeKeyboard = true;

	TgBot::Message::Ptr msg = bot.getApi().sendMessage(
		chat_id,
		"...",
		0,
		0,
		keyboard
	);
	delete_message(bot, chat_id, msg->messageId);
}

void delete_message(TgBot::Bot& bot, int64_t chat_id, int32_t msg_id)
{
	bot.getApi().deleteMessage(chat_id, msg_id);
}

void set_next_state(const rapidjson::Value& next_state, id_t chat)
{
	if (!next_state.IsArray())
		throw std::runtime_error("set_next_state: argument must be json array");

	if (next_state.Size() == 1) {
		set_chat_state(chat,
			(MainState)next_state[0].GetInt(), SubState::Base);
	} else if (next_state.Size() == 2) {
		set_chat_state(chat,
			(MainState)next_state[0].GetInt(),
			(SubState)next_state[1].GetInt());
	} else {
		throw std::runtime_error(
			"set_next_state: invalid next state array length");
	}
}

// говнокодище хуже некуда
void send_message(const TextManager& tm,
	const char* config,
	const std::shared_ptr<const Chat>& chat,
	TgBot::Bot& bot,
	const std::string& text,
	TgBot::InputFile::Ptr image,
	TgBot::GenericReply::Ptr keyboard)
{
	switch (chat->ss) {
	case SubState::Ask:
		if (tm.has(config, "del_prev")) {
			int32_t prev_id = chat->last_msg_id;
			chat->last_msg_id = Message(text, image, keyboard, chat->last_msg_id)\
				.send(bot, chat->chat_id);
			if (prev_id && chat->last_msg_id != prev_id)
				chat->last_msg_id = 0;
			break;
		}
	case SubState::Base:
	case SubState::Invalid:
		if (chat->last_msg_id)
			delete_message(bot, chat->chat_id, chat->last_msg_id);
		chat->last_msg_id = 0;
	default:
		Message(text, image, keyboard).send(bot, chat->chat_id);
	}
}

bool generic_handler(const TextManager& tm, const char* config,
	const std::shared_ptr<const Chat>& chat,
	TgBot::Bot& bot, TgBot::Message::Ptr msg,
	TgBot::CallbackQuery::Ptr query,
	std::function<bool()> proc_answ)
{
	if (!tm.has(config))
		throw std::runtime_error("generic_handler: invalid config");

	if (tm.has(config, "redir")) {
		set_next_state(tm.get(config, "redir"), chat->id());
		return false;
	}

	switch (chat->ss) {
	case SubState::Base: {
		TgBot::InputFile::Ptr img = nullptr;
		if (tm.has(config, "img"))
			img = image(tm(config, "img"));
		if (tm.has(config, "base"))
			send_message(tm, config, chat, bot, tm(config, "base"), img);
		if (tm.has(config, "base_next_state"))
			set_next_state(tm.get(config, "base_next_state"), chat->id());
		else
		 	set_chat_state(chat->id(), chat->ms, SubState::Ask);
		return false;
	}
	case SubState::Ask: {
		std::string text {tm(config, "prompt")};

		TgBot::InputFile::Ptr img = nullptr;
		if (tm.has(config, "img"))
			img = image(tm(config, "img"));

		TgBot::GenericReply::Ptr kb = nullptr;
		if (tm.has(config, "kb_markup")) {
			auto kb_markup = tm.int_vec(config, "kb_markup");
			auto kb_text = tm.vec(config, "kb_text");
			KeyboardType kb_type = tm.has(config, "kb_data") ?
				KeyboardType::Inline : KeyboardType::Reply;
			std::vector<std::string> kb_data = {};
			if (kb_type == KeyboardType::Inline)
				kb_data = tm.vec(config, "kb_data");

			kb = keyboard(kb_type, kb_markup, kb_text, kb_data);
		}

		send_message(tm, config, chat, bot, text, img, kb);
		set_chat_state(chat->id(), chat->ms, SubState::ProcAnsw);
		return true;
	}
	case SubState::ProcAnsw:
		if (tm.has(config, "next_states") || tm.has(config, "next_state")) {
			if (proc_answ != nullptr)
				proc_answ();

			if (tm.has(config, "next_state")) {
				set_next_state(tm.get(config, "next_state"), chat->id());
				return false;
			}

			const rapidjson::Value& next_states = tm.get(config, "next_states");
			const rapidjson::Value& callbacks = tm.get(config, "kb_data");

			for (size_t i = 0; i < callbacks.Size(); ++i) {
				if (query->data == callbacks[i].GetString()) {
					set_next_state(next_states[i], chat->id());
					return false;
				}
			}
		} else if (proc_answ != nullptr) {
			return proc_answ();
		} else {
			throw std::runtime_error(
				"generic_handler(): SubState::ProcAnsw has to be hanled");
		}
	case SubState::Invalid:
		send_message(tm, config, chat, bot, tm(config, "err"));
		set_chat_state(chat->id(), chat->ms, SubState::Ask);
		return false;
	}
}

TgBot::InlineKeyboardMarkup::Ptr make_calendar_keyboard(const TextManager& tm,
	int year, int month)
{
	static const int days_per_mon[12] = {31, 28, 31, 30,
		31, 30, 31, 31, 30, 31, 30, 31};

	struct tm date {};
	date.tm_year = year;
	date.tm_mon = month;
	date.tm_mday = 1;
	std::mktime(&date);
	int left_offset = (date.tm_wday + 6) % 7;	
	int days = days_per_mon[month];
	if (year % 4 == 0 && month == 1)
		++days;

	int rows = (left_offset + days + 6) / 7;
	std::vector<int> kb_markup (rows + 2);
	for (int i = 0; i < rows; ++i)
		kb_markup[i] = 7;
	kb_markup[rows] = 2;
	kb_markup[rows + 1] = 1;

	std::vector<std::string> kb_text (rows * 7 + 3);
	std::vector<std::string> kb_data (rows * 7 + 3);
	for (int i = 0; i < left_offset; ++i) {
		kb_text[i] = u8"\u200B";
		kb_data[i] = "nan";		
	}

	for (int i = 0; i < days; ++i)
		kb_text[left_offset + i] = kb_data[left_offset + i] =
			std::to_string(i + 1);

	int right_offset = 7 - (left_offset + days) % 7;
	if (right_offset == 7)
		right_offset = 0;
	for (int i = 0; i < right_offset; ++i) {
		kb_text[left_offset + days + i] = u8"\u200B";
		kb_data[left_offset + days + i] = "nan";		
	}

	auto months = tm.vec("months");

	int len = kb_text.size();
	kb_text[len - 1] = "Назад";
	kb_data[len - 1] = "ret";

	kb_text[len - 2] = months[(month + 1) % 12] + " ->";
	kb_data[len - 2] = "next";

	kb_text[len - 3] = "<- " + months[(month + 11) % 12];
	kb_data[len - 3] = "prev";

	return inline_keyboard(kb_markup, kb_text, kb_data);
}