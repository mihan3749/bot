#include "bot/handlers.h"
#include "bot/chat.h"
#include "bot/logic.h"
#include "bot/tools.h"
#include "bot/models.h"
#include <string>
#include <tgbot/tools/StringTools.h>
#include <tgbot/types/InputFile.h>

const TextManager& tm()
{
	return TextManager::get_instance();
}

bool hdl_start(const std::shared_ptr<const Chat>& chat,
	TgBot::Bot& bot, TgBot::Message::Ptr msg, TgBot::CallbackQuery::Ptr query)
{
	get_tmp_appo(chat->id()) = {};
	return generic_handler(tm(), "start", chat, bot, msg, query);
}

bool hdl_cmd_menu(const std::shared_ptr<const Chat>& chat,
	TgBot::Bot& bot, TgBot::Message::Ptr msg, TgBot::CallbackQuery::Ptr query)
{
	if (chat->last_msg_id) {
		try {
			delete_message(bot, chat->chat_id, chat->last_msg_id);
		} catch (...) {}
	}

	chat->last_msg_id = chat->last_query_msg_date = 0;
	get_tmp_appo(chat->id()) = {};
	remove_reply_keyboard(bot, chat->chat_id);

	set_chat_state(chat->id(), MainState::MainMenu, SubState::Ask);
	return false;
}

bool hdl_main_menu(const std::shared_ptr<const Chat>& chat,
	TgBot::Bot& bot, TgBot::Message::Ptr msg, TgBot::CallbackQuery::Ptr query)
{
	get_tmp_appo(chat->id()) = {};

	if (chat->user->client.is_null())
		return generic_handler(tm(), "menu", chat, bot, msg, query);

	switch (chat->ss) {
	case SubState::Ask: {
		auto kb_markup = tm().int_vec("menu", "kb_markup");
		auto kb_text = tm().vec("menu", "kb_text");
		auto kb_data = tm().vec("menu", "kb_data");

		kb_markup.push_back(1);
		kb_text.push_back("Мои записи");
		kb_data.push_back("appos");

		auto kb = keyboard(KeyboardType::Inline,
			kb_markup, kb_text, kb_data);
		auto text = tm()("menu", "prompt");

		send_message(tm(), "menu", chat, bot, text, nullptr, kb);
		set_chat_state(chat->id(), chat->ms, SubState::ProcAnsw);
		return true;
	}
	case SubState::ProcAnsw: {
		if (query->data == "appos") {
			set_chat_state(chat->id(),
				MainState::ListAppointments, SubState::Ask);
			return false;
		}

		return generic_handler(tm(), "menu", chat, bot, msg, query);		
	}
	default: return true;
	}
}

bool hdl_PA_select_service(const std::shared_ptr<const Chat>& chat,
	TgBot::Bot& bot, TgBot::Message::Ptr msg, TgBot::CallbackQuery::Ptr query)
{
	return generic_handler(tm(), "select_spec", chat, bot, msg, query,
	[&](){
		if (msg->text == tm()("select_spec", "return_btn_text")) {
			set_chat_state(chat->id(), MainState::MakeAppointment);
		} else {
			auto spec = get_speciality(msg->text);
			if (spec == nullptr) {
				set_chat_state(chat->id(), chat->ms, SubState::Invalid);
			} else {
				get_tmp_appo(chat->id()).spec = spec->id();
				set_chat_state(chat->id(),
					MainState::PASelectClinic, SubState::Ask);
			}
		}
		remove_reply_keyboard(bot, chat->chat_id);
		chat->last_msg_id = 0;
		return false;
	});
}

bool hdl_PA_select_clinic(const std::shared_ptr<const Chat>& chat,
	TgBot::Bot& bot, TgBot::Message::Ptr msg, TgBot::CallbackQuery::Ptr query)
{
	return generic_handler(tm(), "select_clinic", chat, bot, msg, query,
	[&](){
		if (query->data == "ret") {}
		else if (query->data == "any")
			get_tmp_appo(chat->id()).clinic = 0;
		else	
			get_tmp_appo(chat->id()).clinic = std::stoi(query->data);

		return false;
	});
}

bool hdl_PA_select_dates(const std::shared_ptr<const Chat>& chat,
	TgBot::Bot& bot, TgBot::Message::Ptr msg, TgBot::CallbackQuery::Ptr query)
{
	auto& appo = get_tmp_appo(chat->id());
	switch (chat->ss) {
	case SubState::Ask: {
		if (appo.year == 0) {
			time_t now = time(0);
			struct tm* local_time = std::localtime(&now);
			int year = local_time->tm_year;
			int month = local_time->tm_mon;	
			appo.year = year;
			appo.month = month;
		}

		auto kb = make_calendar_keyboard(tm(), appo.year, appo.month);
		std::string prompt = tm()("select_dates", "prompt");
		prompt += appo.from ?
			tm()("select_dates", "text_to") : tm()("select_dates", "text_from");
		send_message(tm(), "select_dates", chat, bot,
			prompt, nullptr, kb);
		set_chat_state(chat->id(), chat->ms, SubState::ProcAnsw);
		return true;
	}
	case SubState::ProcAnsw:
		switch (str_hash(query->data)) {
		case str_hash("nan"):
			appo.date_err = Chat::Tmp::DateErr::Nan;
			set_chat_state(chat->id(), chat->ms, SubState::Invalid);
			return false;
		case str_hash("prev"): {
			--appo.month;
			if (appo.month == -1) {
				--appo.year;
				appo.month = 11;
			}
			set_chat_state(chat->id(), chat->ms, SubState::Ask);
			return false;
		}
		case str_hash("next"): {
			++appo.month;
			if (appo.month == 12) {
				++appo.year;
				appo.month = 0;
			}
			set_chat_state(chat->id(), chat->ms, SubState::Ask);
			return false;
		}
		case str_hash("ret"): {
			appo.from = appo.to = appo.year = appo.month = 0;
			set_chat_state(chat->id(), MainState::PASelectClinic, SubState::Ask);
			return false;
		}
		default: {
			struct tm date {};
			date.tm_year = appo.year;
			date.tm_mon = appo.month;
			date.tm_mday = std::stoi(query->data);
			time_t t = std::mktime(&date);
			time_t now = time(0);
			if (appo.from) {
				appo.to = t;
				if (appo.to < appo.from) {
					time_t tmp = appo.to;
					appo.to = appo.from;
					appo.from = tmp;

					if (appo.from < now) {
						appo.date_err = Chat::Tmp::DateErr::FromLessThanNow;
						set_chat_state(chat->id(), chat->ms, SubState::Invalid);
						return false;
					}
				}

				set_chat_state(chat->id(), MainState::PASelectDoctor, SubState::Ask);
			} else {
				appo.from = t;
				if (appo.from < now) {
					appo.date_err = Chat::Tmp::DateErr::FromLessThanNow;
					set_chat_state(chat->id(), chat->ms, SubState::Invalid);
				} else {
					set_chat_state(chat->id(), chat->ms, SubState::Ask);
				}
			}
			return false;
		}
		}
	case SubState::Invalid: {

		switch (appo.date_err) {
		case Chat::Tmp::DateErr::FromLessThanNow:
			send_message(tm(), "select_dates", chat, bot,
				tm()("select_dates", "date_err_from"));
			break;
		case Chat::Tmp::DateErr::Nan:
			send_message(tm(), "select_dates", chat, bot,
				tm()("select_dates", "date_err_nan"));
			break;
		}
		appo.from = appo.to = appo.year = appo.month = 0;
		set_chat_state(chat->id(), chat->ms, SubState::Ask);
		return false;
	}
	default: return true;
	}
}

bool hdl_PA_select_doctor(const std::shared_ptr<const Chat>& chat,
	TgBot::Bot& bot, TgBot::Message::Ptr msg, TgBot::CallbackQuery::Ptr query)
{
	auto& appo = get_tmp_appo(chat->id());
	switch (chat->ss) {
	case SubState::Ask: {
		auto docs = get_doctors(appo.spec, appo.clinic);
		std::sort(docs.begin(), docs.end(), 
		[&](const auto& d1, const auto& d2) {
			return nearest_available(d1->id(), appo.spec, appo.from, appo.to)
				< nearest_available(d2->id(), appo.spec, appo.from, appo.to);
		});

		std::vector<int> kb_markup;
		std::vector<std::string> kb_text;
		std::vector<std::string> kb_data;

		if (docs.size() == 0) {
			kb_markup = tm().int_vec("select_doc", "kb_markup0");
			kb_text = tm().vec("select_doc", "kb_text0");
			kb_data = tm().vec("select_doc", "kb_data0");
		} else if (docs.size() == 1) {
			kb_markup = tm().int_vec("select_doc", "kb_markup1");
			kb_text = tm().vec("select_doc", "kb_text1");
			kb_data = tm().vec("select_doc", "kb_data1");
		} else {
			kb_markup = tm().int_vec("select_doc", "kb_markup");
			kb_text = tm().vec("select_doc", "kb_text");
			kb_data = tm().vec("select_doc", "kb_data");

			if (appo.doc_n + 1 == docs.size()) {
				kb_text[2] = u8"\u200B";
				kb_data[2] = "nan";
			} else if (appo.doc_n == 0) {
				kb_text[1] = u8"\u200B";
				kb_data[1] = "nan";
			}
		}

		std::string text;
		if (docs.size()) {
			text = "(" + std::to_string(appo.doc_n + 1) +
				"/" + std::to_string(docs.size()) + ")\n\n" +
			docs[appo.doc_n]->full_name + "\n" +
			docs[appo.doc_n]->clinic->address + "\n" +
			tm()("select_doc", "text_nearest_on") +
			time_to_dd_month_hh_mm(tm(),
				nearest_available(docs[appo.doc_n]->id(),
					appo.spec, appo.from, appo.to));

			appo.doc = docs[appo.doc_n]->id();
		} else {
			text = tm()("select_doc", "text_no_available");
		}

		auto kb = keyboard(KeyboardType::Inline, kb_markup, kb_text, kb_data);
		send_message(tm(), "select_doc", chat, bot,
			text, nullptr, kb);
		set_chat_state(chat->id(), chat->ms, SubState::ProcAnsw);
		return true;
	}
	case SubState::ProcAnsw:
		switch (str_hash(query->data)) {
		case str_hash("ret"):
			appo.from = appo.to = appo.year = appo.month = 0;
			set_chat_state(chat->id(),
				MainState::PASelectDates, SubState::Ask);
			return false;
		case str_hash("prev"):
			--appo.doc_n;
			set_chat_state(chat->id(), chat->ms, SubState::Ask);
			return false;
		case str_hash("next"):
			appo.doc_n++;
			set_chat_state(chat->id(), chat->ms, SubState::Ask);
			return false;
		case str_hash("nan"):
			set_chat_state(chat->id(), chat->ms, SubState::Invalid);
			return false;
		case str_hash("make_appo"): {
			set_chat_state(chat->id(),
				MainState::PASelectTime, SubState::Ask);
			return false;
		}
		}
	case SubState::Invalid:
		send_message(tm(), "select_doc", chat, bot,
			tm()("select_doc", "err"));
		set_chat_state(chat->id(), chat->ms, SubState::Ask);
		return false;
	default: return true;
	}
}

bool hdl_PA_nearest_time(const std::shared_ptr<const Chat>& chat,
	TgBot::Bot& bot, TgBot::Message::Ptr msg, TgBot::CallbackQuery::Ptr query)
{
	auto& appo = get_tmp_appo(chat->id());
	appo.res = nearest_available(appo.doc, appo.spec, appo.from, appo.to);
	set_chat_state(chat->id(), MainState::PASetPersInfo);
	return false;
}

// добавить проверку, чтою не раньше сегодня
bool hdl_PA_input_time(const std::shared_ptr<const Chat>& chat,
	TgBot::Bot& bot, TgBot::Message::Ptr msg, TgBot::CallbackQuery::Ptr query)
{
	auto& appo = get_tmp_appo(chat->id());
	switch (chat->ss) {
	case SubState::Ask: {
		if (appo.day == 0)
			appo.day = appo.from;
		
		auto times = all_available_in_day(appo.doc, appo.spec, appo.day);
		size_t len = times.size();
		int rows = (len + 3) / 4;
		std::vector<int> kb_markup(rows + 2, 4);
		kb_markup[rows] = 2;
		kb_markup[rows + 1] = 1;

		std::vector<std::string> kb_text(rows * 4 + 3);
		std::vector<std::string> kb_data(rows * 4 + 3);
		for (size_t i = 0; i < len; ++i) {
			kb_text[i] = time_to_hh_mm(times[i]);
			kb_data[i] = std::to_string(times[i]);
		}

		int right_offset = 4 - len % 4;
		if (right_offset == 4)
			right_offset = 0;
		for (int i = 0; i < right_offset; ++i) {
			kb_text[len + i] = u8"\u200B";
			kb_data[len + i] = "nan";		
		}

		if (appo.day - 24*3600 < time(0)) {
			kb_text[len + right_offset] = u8"\u200B";
			kb_data[len + right_offset] = "nan";
		} else {
			kb_text[len + right_offset] = "<- " + time_to_mm_dd(appo.day - 24*3600);
			kb_data[len + right_offset] = "prev";
		}

		kb_text[len + right_offset + 1] = time_to_mm_dd(appo.day + 24*3600) + " ->";
		kb_data[len + right_offset + 1] = "next";

		kb_text[len + right_offset + 2] = tm()("input_time", "text_ret");
		kb_data[len + right_offset + 2] = "ret";

		std::string text = tm()("input_time", "prompt");
		auto kb = keyboard(KeyboardType::Inline, kb_markup, kb_text, kb_data);
		send_message(tm(), "input_time", chat, bot, text, nullptr, kb);
		set_chat_state(chat->id(), chat->ms, SubState::ProcAnsw);
		return true;
	}
	case SubState::ProcAnsw: {
		switch (str_hash(query->data)) {
		case str_hash("ret"):
			appo.day = 0;
			set_chat_state(chat->id(), MainState::PASelectTime, SubState::Ask);
			return false;
		case str_hash("next"):
			appo.day += 24*3600;
			set_chat_state(chat->id(), chat->ms, SubState::Ask);
			return false;
		case str_hash("prev"):
			appo.day -= 24*3600;
			set_chat_state(chat->id(), chat->ms, SubState::Ask);
			return false;
		case str_hash("nan"):
			set_chat_state(chat->id(), chat->ms, SubState::Invalid);
			return false;
		}
		appo.res = std::stoll(query->data);
		set_chat_state(chat->id(), MainState::PASetPersInfo);
		return false;
	}
	case SubState::Invalid:
		send_message(tm(), "input_time", chat, bot, tm()("input_time", "err"));
		set_chat_state(chat->id(), chat->ms, SubState::Ask);
		return false;
	default: return true;
	}
}

bool hdl_PA_set_pers_info(const std::shared_ptr<const Chat>& chat,
	TgBot::Bot& bot, TgBot::Message::Ptr msg, TgBot::CallbackQuery::Ptr query)
{
	auto& appo = get_tmp_appo(chat->id());
	switch (chat->ss) {
	case SubState::Base:
		if (!chat->user->client.is_null()) {
			set_chat_state(chat->id(),
				MainState::PAConfirm, SubState::Ask);
		} else {
			send_message(tm(), "pers_info", chat,
				bot, tm()("pers_info", "prompt"));
			chat->last_msg_id = 0;
			set_chat_state(chat->id(), chat->ms, SubState::Ask);
		}
		return false;
	case SubState::Ask: {
		std::string text;
		switch (appo.pers) {
		case Chat::Tmp::PersInfo::Name:
			text = tm()("pers_info", "prompt_name");
			break;
		case Chat::Tmp::PersInfo::Email:
			text = tm()("pers_info", "prompt_email");
			break;
		case Chat::Tmp::PersInfo::Phone:
			text = tm()("pers_info", "prompt_phone");
			break;
		}

		send_message(tm(), "pers_info", chat, bot, text);
		chat->last_msg_id = 0;
		set_chat_state(chat->id(), chat->ms, SubState::ProcAnsw);
		return true;
	}
	case SubState::ProcAnsw:
		switch (appo.pers) {
		case Chat::Tmp::PersInfo::Name:
			if (!regex_match(tm()("regexp", "full_name"), msg->text)) {
				set_chat_state(chat->id(), chat->ms, SubState::Invalid);
			} else {
				appo.full_name = msg->text;
				appo.pers = Chat::Tmp::PersInfo::Email;
				set_chat_state(chat->id(), chat->ms, SubState::Ask);
			}
			break;
		case Chat::Tmp::PersInfo::Email:
			if (!regex_match(tm()("regexp", "email"), msg->text)) {
				set_chat_state(chat->id(), chat->ms, SubState::Invalid);
			} else {
				appo.email = msg->text;
				appo.pers = Chat::Tmp::PersInfo::Phone;
				set_chat_state(chat->id(), chat->ms, SubState::Ask);
			}
			break;
		case Chat::Tmp::PersInfo::Phone:
			if (!regex_match(tm()("regexp", "phone"), msg->text)) {
				set_chat_state(chat->id(), chat->ms, SubState::Invalid);
			} else {
				appo.phone_num = msg->text;
				set_chat_state(chat->id(), MainState::PAConfirm, SubState::Ask);
				create_client(appo.full_name, appo.email,
					appo.phone_num, chat->user.id());
			}
			break;
		}
		return false;
	case SubState::Invalid: {
		std::string text;
		switch (appo.pers) {
		case Chat::Tmp::PersInfo::Name:
			text = tm()("pers_info", "err_name");
			break;
		case Chat::Tmp::PersInfo::Email:
			text = tm()("pers_info", "err_email");
			break;
		case Chat::Tmp::PersInfo::Phone:
			text = tm()("pers_info", "err_phone");
			break;
		}

		send_message(tm(), "pers_info", chat, bot, text);
		set_chat_state(chat->id(), chat->ms, SubState::Ask);
		return false;
	}
	default: return true;
	}
}

bool hdl_PA_confirm(const std::shared_ptr<const Chat>& chat,
	TgBot::Bot& bot, TgBot::Message::Ptr msg, TgBot::CallbackQuery::Ptr query)
{
	switch (chat->ss) {
	case SubState::Ask: {
		auto& appo = get_tmp_appo(chat->id());
		std::string addr = appo.clinic ?
			get_clinic(appo.clinic)->address :
			get_doctor(appo.doc)->clinic->address;
		std::string text = tm()("common", "confirm_appo") + ".\n" +
			format_appointment(tm(),
				get_speciality(appo.spec)->title,
				get_doctor(appo.doc)->full_name,
				addr, appo.res, "    ");
		auto kb = keyboard(KeyboardType::Inline,
			tm().int_vec("confirm", "kb_markup"),
			tm().vec("confirm", "kb_text"),
			tm().vec("confirm", "kb_data"));
		send_message(tm(), "confirm", chat, bot, text, nullptr, kb);
		set_chat_state(chat->id(), chat->ms, SubState::ProcAnsw);
		return true;
	}
	case SubState::ProcAnsw:
		if (query->data == "ret")
			set_chat_state(chat->id(), MainState::PASelectTime, SubState::Ask);
		else {
			remove_inline_keyboard(bot, query);
			chat->last_msg_id = 0;
		 	set_chat_state(chat->id(), MainState::PAMake);
		}
		return false;
	default: return true;
	}
}

bool hdl_PA_make(const std::shared_ptr<const Chat>& chat,
	TgBot::Bot& bot, TgBot::Message::Ptr msg, TgBot::CallbackQuery::Ptr query)
{
	auto& appo = get_tmp_appo(chat->id());
	if (appointment_exist(appo.doc, appo.spec, appo.res)) {
		appo.from = appo.to = appo.year = appo.month = 0;
		set_chat_state(chat->id(), MainState::PACantMake, SubState::Ask);
		return false;
	}

	make_appointment(chat->user->client.id(), appo.doc,
		appo.spec, appo.res, appo.clinic);

	send_message(tm(), "make_appo", chat, bot, tm()("make_appo", "text"));
	set_chat_state(chat->id(), MainState::MainMenu, SubState::Ask);
	chat->last_msg_id = 0;
	appo = {};
	return false;
}

bool hdl_list_appointments(const std::shared_ptr<const Chat>& chat,
	TgBot::Bot& bot, TgBot::Message::Ptr msg, TgBot::CallbackQuery::Ptr query)
{
	auto& tmp = get_tmp_appo(chat->id());
	auto appos = get_client_appointments(chat->user->client.id());
	if (appos.empty())
		return generic_handler(tm(), "list_appos", chat, bot, msg, query);

	switch (chat->ss) {
	case SubState::Ask: {
		tmp.canc_id = appos[tmp.display_appo]->id();
		int max_units_per_page = tm().get("list_appos", "max_per_page").GetInt();
		int from, to;
		if (appos.size() > max_units_per_page) {
			from = std::max(0, tmp.display_appo - max_units_per_page / 2);
			to = std::min((int)appos.size(),
				tmp.display_appo + max_units_per_page / 2 + 1);
		} else {
			from = 0;
			to = appos.size();
		}

		std::string text = tm()("list_appos", "text_you_have_n_appos") +
			std::to_string(appos.size()) + "\n\n";
		for (int i = from; i < to; ++i) {
			const auto& a = appos[i];
			text += std::to_string(i + 1) + ")\n";
			if (i == tmp.display_appo)
				text += "<u>";

			text += format_appointment(tm(), a->speciality->title,
				a->doctor->full_name, a->clinic->address,
					a->time.from, "    ");

			if (i == tmp.display_appo)
				text += "</u>";

			text += "\n\n";
		}

		std::vector<int> kb_markup;
		std::vector<std::string> kb_text;
		std::vector<std::string> kb_data;

		if (appos.size() > 1) {
			kb_markup = tm().int_vec("list_appos", "kb_markup2");
			kb_text = tm().vec("list_appos", "kb_text2");
			kb_data = tm().vec("list_appos", "kb_data2");

			if (tmp.display_appo == 0) {
				kb_text[1] = u8"\u200B";
				kb_data[1] = "nan";
			} else if (tmp.display_appo == appos.size() - 1) {
				kb_text[2] = u8"\u200B";
				kb_data[2] = "nan";
			}
		} else {
			kb_markup = tm().int_vec("list_appos", "kb_markup1");
			kb_text = tm().vec("list_appos", "kb_text1");
			kb_data = tm().vec("list_appos", "kb_data1");
		}

		auto kb = keyboard(KeyboardType::Inline, kb_markup, kb_text, kb_data);
		send_message(tm(), "list_appos", chat, bot,
			text, nullptr, kb);
		set_chat_state(chat->id(), chat->ms, SubState::ProcAnsw);
		return true;
	}
	case SubState::ProcAnsw: {
		switch (str_hash(query->data)) {
		case str_hash("ret"):
			tmp.canc_id = tmp.display_appo = 0;
			set_chat_state(chat->id(), MainState::MainMenu, SubState::Ask);
			return false;
		case str_hash("nan"):
			set_chat_state(chat->id(), chat->ms, SubState::Invalid);
			return false;
		case str_hash("prev"):
			--tmp.display_appo;
			set_chat_state(chat->id(), chat->ms, SubState::Ask);
			return false;
		case str_hash("next"):
			++tmp.display_appo;
			set_chat_state(chat->id(), chat->ms, SubState::Ask);
			return false;
		case str_hash("canc"):
			set_chat_state(chat->id(), MainState::ConfirmCancel, SubState::Ask);
			return false;
		}
	}
	case SubState::Invalid:
		send_message(tm(), "list_appos", chat, bot, tm()("list_appos", "err"));
		set_chat_state(chat->id(), chat->ms, SubState::Ask);
		return false;
	default: return true;
	}
}

bool hdl_confirm_appointment_cancellation(const std::shared_ptr<const Chat>& chat,
	TgBot::Bot& bot, TgBot::Message::Ptr msg, TgBot::CallbackQuery::Ptr query)
{
	return generic_handler(tm(), "confirm_cancel", chat, bot, msg, query,
	[&](){
		auto& tmp = get_tmp_appo(chat->id());
		if (query->data == "canc") {
			cancel_appointment(tmp.canc_id);
			tmp.canc_id = tmp.display_appo = 0;
		}

		set_chat_state(chat->id(),
			MainState::ListAppointments, SubState::Ask);
		return false;
	});
}

bool hdl_list_doctors(const std::shared_ptr<const Chat>& chat,
	TgBot::Bot& bot, TgBot::Message::Ptr msg, TgBot::CallbackQuery::Ptr query)
{
	auto& tmp = get_tmp_appo(chat->id());
	switch (chat->ss) {
	case SubState::Ask: {
		auto specs = get_all_specialities();
		std::sort(specs.begin(), specs.end(), [](const auto& s1, const auto& s2) {
			return s1->title < s2->title;
		});

		std::string text;
		int curr_page = 0;
		int text_limit = tm().num("doctors", "text_limit");
		tmp.docs_has_next_page = false;
		for (const auto& spec : specs) {
			std::string more_text = "<b>" + spec->title + "</b>\n";
			for (id_t doc : spec->doctors)
				more_text += "    " + get_doctor(doc)->full_name + "\n";
			more_text += "\n";

			if (text.size() + more_text.size() > text_limit) {
				if (curr_page == tmp.docs_page) {
					tmp.docs_has_next_page = true;
					break;
				}
				text = more_text;
				++curr_page;
			} else {
				text += more_text;
			}
		}

		auto kb_markup = tm().int_vec("doctors", "kb_markup");
		auto kb_text = tm().vec("doctors", "kb_text");
		auto kb_data = tm().vec("doctors", "kb_data");

		if (tmp.docs_page == 0) {
			kb_text[0] = u8"\u200B";
			kb_data[0] = "nan";
		} else if (!tmp.docs_has_next_page) {
			kb_text[1] = u8"\u200B";
			kb_data[1] = "nan";
		}

		auto kb = keyboard(KeyboardType::Inline,
			kb_markup, kb_text, kb_data);
		send_message(tm(), "doctors", chat, bot, text, nullptr, kb);
		set_chat_state(chat->id(), chat->ms, SubState::ProcAnsw);
		return true;
	}
	case SubState::ProcAnsw: {
		switch (str_hash(query->data)) {
		case str_hash("ret"):
			tmp.docs_page = 0;
			set_chat_state(chat->id(), MainState::MainMenu, SubState::Ask);
			return false;
		case str_hash("prev"):
			--tmp.docs_page;
			set_chat_state(chat->id(), chat->ms, SubState::Ask);
			return false;
		case str_hash("next"):
			++tmp.docs_page;
			set_chat_state(chat->id(), chat->ms, SubState::Ask);
			return false;
		case str_hash("nan"):
			set_chat_state(chat->id(), chat->ms, SubState::Invalid);
			return false;
		}
	}
	case SubState::Invalid:
		send_message(tm(), "doctors", chat, bot, tm()("doctors", "err"));
		set_chat_state(chat->id(), chat->ms, SubState::Ask);
		return false;
	default: return true;
	}
}

using hdl_f = std::function<bool(const std::shared_ptr<const Chat>& chat,
	TgBot::Bot& bot, TgBot::Message::Ptr msg, TgBot::CallbackQuery::Ptr query)>;

hdl_f generic_reply(const char* config, std::function<bool()> proc_answ=nullptr)
{
	return [config, proc_answ](const std::shared_ptr<const Chat>& chat,
		TgBot::Bot& bot, TgBot::Message::Ptr msg,
		TgBot::CallbackQuery::Ptr query) -> bool
	{
		return generic_handler(tm(), config,
			chat, bot, msg, query, proc_answ);
	};
}

EventHandler::EventHandler()
{
	add_hdl(MainState::Start, hdl_start);
	add_hdl(MainState::CmdMainMenu, hdl_cmd_menu);
	add_hdl(MainState::MainMenu, hdl_main_menu);
	add_hdl(MainState::MakeAppointment, generic_reply("appo"));
	add_hdl(MainState::PaidAppointment, generic_reply("paid_appo"));
	add_hdl(MainState::PASelectService, hdl_PA_select_service);
	add_hdl(MainState::PASelectClinic, hdl_PA_select_clinic);
	add_hdl(MainState::PASelectDates, hdl_PA_select_dates);
	add_hdl(MainState::PASelectDoctor, hdl_PA_select_doctor);
	add_hdl(MainState::PASelectTime, generic_reply("select_time"));
	add_hdl(MainState::PANearestTime, hdl_PA_nearest_time);
	add_hdl(MainState::PAInputTime, hdl_PA_input_time);
	add_hdl(MainState::PASetPersInfo, hdl_PA_set_pers_info);
	add_hdl(MainState::PAConfirm, hdl_PA_confirm);
	add_hdl(MainState::PAMake, hdl_PA_make);
	add_hdl(MainState::PACantMake, generic_reply("cant_make"));

	add_hdl(MainState::StateInsuranceApp, generic_reply("state_ins"));
	add_hdl(MainState::PrivateInsuranceApp, generic_reply("priv_ins"));

	add_hdl(MainState::Doctors, hdl_list_doctors);
	add_hdl(MainState::Clinics, generic_reply("clinics"));
	add_hdl(MainState::Sales, generic_reply("sales"));
	add_hdl(MainState::Contacts, generic_reply("contacts"));

	add_hdl(MainState::ListAppointments, hdl_list_appointments);
	add_hdl(MainState::ConfirmCancel, hdl_confirm_appointment_cancellation);
}

void EventHandler::handle(MainState& state, const std::shared_ptr<const Chat>& chat,
	TgBot::Bot& bot, TgBot::Message::Ptr msg, TgBot::CallbackQuery::Ptr query)
{
	log(time_to_hh_mm_ss(time(0)), "handle",
		tm()("states", std::to_string((int)chat->ms).c_str()),
		chat->user->user_name);

	if (is_input_valid(state, chat, msg, query)) {
		StateMachine::handle(state, chat, bot, msg, query);
		log(time_to_hh_mm_ss(time(0)), "done",
			tm()("states", std::to_string((int)chat->ms).c_str()),
			chat->user->user_name, "\n");
	} else
		log("invalid input\n");
}

bool EventHandler::is_input_valid(const MainState& state,
	const std::shared_ptr<const Chat>& chat,
	TgBot::Message::Ptr msg,
	TgBot::CallbackQuery::Ptr query)
{
	std::string state_name = tm()("states", std::to_string((int)state).c_str());
	if (tm().has(state_name.c_str(), "msg_expected") ^ !!msg)
		return false;

	if (query && (chat->last_query_msg_date ==
			(query->message->editDate ? query->message->editDate : query->message->date)))
		return false;

	return true;
}