#include "bot/logic.h"
#include "bot/database.h"
#include "bot/models.h"
#include "bot/tools.h"
#include <ctime>
#include <memory>
#include <stdexcept>
#include <tgbot/types/User.h>

static inline DB1& db()
{
	return DB1::get_instance();
}

void log_user(id_t id)
{
	time_t now = time(0);
	const auto& user = db().users.get(id);
	log(time_to_hh_mm_ss(now), user->name,
		user->user_name, user->tg_id);
}

void create_user_and_chat(int64_t tg_id, const std::string& user_name,
	const std::string& name, int64_t chat_id)
{
	auto user = get_user(tg_id);
	if (user != nullptr)
		throw std::runtime_error(
			"create_user() execution shouldn't reach this point");

	auto new_user = Ptr<TelegramUser>(
		tg_id, user_name, name, 0, 0);
	auto chat = Ptr<Chat>(new_user->id(), chat_id);
	new_user->chat.set_id(chat->id(), false);
}

std::shared_ptr<const TelegramUser> get_user(int64_t tg_id)
{
	return db().users.find([tg_id](const std::shared_ptr<TelegramUser>& u)
		-> bool { 
		return u->tg_id == tg_id;
	});
}

void set_chat_state(id_t chat, MainState ms, SubState ss)
{
	auto c = db().chats.get(chat);
	c->ms = ms;
	c->ss = ss;
}

std::shared_ptr<const Speciality> get_speciality(const std::string& title)
{
	return db().specialties.find(
		[&title](auto s){return s->title == title;});
}

std::shared_ptr<const Speciality> get_speciality(id_t id)
{
	return db().specialties.get(id);
}

std::shared_ptr<const Clinic> get_clinic(const std::string& address)
{
	return db().clinics.find(
		[&address](auto c){return c->address == address;});
}

std::shared_ptr<const Clinic> get_clinic(id_t id)
{
	return db().clinics.get(id);
}

Chat::Tmp& get_tmp_appo(id_t chat)
{
	return db().chats.get(chat)->tmp;
}

std::shared_ptr<const Doctor> get_doctor(id_t id)
{
	return db().doctors.get(id);
}

std::vector<std::shared_ptr<const Doctor>> get_doctors(id_t spec, id_t clinic)
{
	auto docs = db().doctors.filter([spec, clinic](auto d){
		if (spec) {
			if (clinic)
				return d->specialities.has(spec) && d->clinic.id() == clinic;
			return d->specialities.has(spec);
		} else {
			if (clinic)
				return d->clinic.id() == clinic;
			return true;
		}
	});
	
	std::vector<std::shared_ptr<const Doctor>> res(docs.size());
	for (size_t i = 0; i < docs.size(); ++i)
		res[i] = docs[i];
	return res;
}

std::vector<time_t> all_available_in_day(id_t doctor,
	id_t speciality, time_t day)
{
	struct tm t = *std::localtime(&day);
	if (t.tm_hour || t.tm_min || t.tm_sec)
		throw std::runtime_error("all_available_in_day: zrada");

	auto doc = db().doctors.get(doctor);
	auto spec = db().specialties.get(speciality);
	auto ws = doc->work_sch.get();
	
	if (ws->ws.find(day) == ws->ws.end())
		return {};

	const auto& shift = ws->ws[day].work_time;
	std::vector<time_t> all;

	for (const auto& p : shift) {
		for (time_t curr = p.from; curr < p.to; curr += spec->appointment_duration)
			all.push_back(day + curr);
	}

	size_t res_len = all.size();
	for (id_t appo_id : doc->appointments) {
		auto appo = db().appointments.get(appo_id);
		for (auto& t : all) {
			Period p;
			p.from = t;
			p.to = p.from + spec->appointment_duration - 1;

			if (appo->time.overlap(p)) {
				t = 0;
				--res_len;
			}
		}
	}

	std::vector<time_t> res(res_len);
	size_t i = 0;
	for (size_t j = 0; j < all.size(); ++j) {
		if (all[j])
			res[i++] = all[j];
	}

	return res;
}

time_t nearest_available_in_day(id_t doctor, id_t speciality, time_t day)
{
	auto vec = all_available_in_day(doctor, speciality, day);
	if (vec.size())
		return vec[0];
	return 0;
}

time_t nearest_available(id_t doctor, id_t speciality, time_t from, time_t to)
{
	struct tm from_tm;
	if (from == 0)
		from = time(0);
	from_tm = *std::localtime(&from);
	from_tm.tm_hour = from_tm.tm_min = from_tm.tm_sec = 0;

	struct tm to_tm;
	if (to == 0)
		to = time(0) + 30*24*3600;
	to_tm = *std::localtime(&from);
	to_tm.tm_hour = to_tm.tm_min = to_tm.tm_sec = 0;

	struct tm curr_tm = from_tm;
	for (;;) {
		time_t day = std::mktime(&curr_tm);
		time_t res = nearest_available_in_day(doctor, speciality, day);
		if (res)
			return res;
		++curr_tm.tm_mday;
		if (day > to)
			return 0;
	}
}

void create_client(const std::string& full_name, const std::string& email,
	const std::string& phone_num, id_t user)
{
	auto client = Ptr<Client>(full_name, email, phone_num, user, "");
	db().users.get(user)->client.set_id(client->id());
}

void make_appointment(id_t client, id_t doctor, id_t speciality,
	time_t time, id_t clinic)
{
	auto spec = get_speciality(speciality);
	Period p;
	p.from = time;
	p.to = time + spec->appointment_duration - 1;
	if (clinic == 0)
		clinic = db().doctors.get(doctor)->clinic->id();
	auto appo = Ptr<Appointment>(client, doctor, speciality, p, clinic);
	appo->resolve_relations();

	log(time_to_hh_mm_ss(std::time(0)), "add",
		db().clients.get(client)->user->user_name, spec->title);
}

void cancel_appointment(id_t appointment)
{
	const auto& appo = db().appointments.get(appointment);
	log(time_to_hh_mm_ss(time(0)), "del",
		appo->client->user->user_name, appo->speciality->title);

	db().appointments.del(appointment);
}

std::vector<std::shared_ptr<const Appointment>> get_client_appointments(
	id_t client)
{
	auto c = db().clients.get(client);
	std::vector<std::shared_ptr<const Appointment>> res(c->appointments.size());
	size_t i = 0;
	for (id_t appo_id : c->appointments)
		res[i++] = db().appointments.get(appo_id);
	return res;
}

bool appointment_exist(id_t doctor, id_t speciality, time_t time)
{
	Period p;
	p.from = time;
	p.to = p.from + db().specialties.get(speciality)->appointment_duration - 1;
	for (id_t a : get_doctor(doctor)->appointments) {
		const auto& appo = db().appointments.get(a);
		if (appo->time.overlap(p))
			return true;
	}
	return false;
}

std::vector<std::shared_ptr<const Speciality>> get_all_specialities()
{
	std::vector<std::shared_ptr<const Speciality>> res;
	for (auto ptr : db().specialties.all())
		res.push_back(ptr);
	return res;
}



// временно
void request_db_save()
{
	static time_t prev_save = 0;
	static const time_t savings_rate = 50;
	
	if (time(0) - prev_save < savings_rate)
		return;

	db().write("/home/ya/cpp/bots/bot/data/db.json");
	prev_save = time(0);
	log(time_to_hh_mm_ss(time(0)), "DB SAVED");
}