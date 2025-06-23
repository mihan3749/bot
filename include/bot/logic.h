#ifndef _LOGIC_H
#define _LOGIC_H

#include "bot/handlers.h"
#include "bot/models.h"

void log_user(id_t id);

void create_user_and_chat(int64_t tg_id, const std::string& user_name,
	const std::string& name, int64_t chat_id);

std::shared_ptr<const TelegramUser> get_user(int64_t tg_id);

void set_chat_state(id_t chat, MainState gs, SubState ss=SubState::Base);

std::shared_ptr<const Speciality> get_speciality(const std::string& title);

std::shared_ptr<const Speciality> get_speciality(id_t id);

std::shared_ptr<const Clinic> get_clinic(const std::string& address);

std::shared_ptr<const Clinic> get_clinic(id_t id);

Chat::Tmp& get_tmp_appo(id_t chat);

std::shared_ptr<const Doctor> get_doctor(id_t id);

std::vector<std::shared_ptr<const Doctor>> get_doctors(id_t spec=0, id_t clinic=0);

std::vector<time_t> all_available_in_day(id_t doctor,
	id_t speciality, time_t day);

time_t nearest_available_in_day(id_t doctor, id_t speciality, time_t day);

time_t nearest_available(id_t doctor, id_t speciality, time_t from=0, time_t to=0);

void create_client(const std::string& full_name, const std::string& email,
	const std::string& phone_num, id_t user);

void make_appointment(id_t client, id_t doctor, id_t speciality,
	time_t time, id_t clinic);

void cancel_appointment(id_t appointment);

std::vector<std::shared_ptr<const Appointment>> get_client_appointments(
	id_t client);

bool appointment_exist(id_t doctor, id_t speciality, time_t time);

std::vector<std::shared_ptr<const Speciality>> get_all_specialities();




void request_db_save();

#endif