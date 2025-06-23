#ifndef _MODELS_H
#define _MODELS_H

#include "bot/storage.h"
#include <ctime>
#include <unordered_map>
#include <vector>

class Client;
class Appointment;
class Speciality;
class WorkSchedule;
class Clinic;
class Chat;

class TelegramUser: public Model
{
public:
	TelegramUser() {};

	TelegramUser(int64_t tg_id, const std::string& user_name,
		const std::string& name, id_t chat, id_t client);

	TelegramUser(const rapidjson::Value& json);

	rapidjson::Value serialize(
		rapidjson::MemoryPoolAllocator<>& alloc) const override;

	void deserialize(const rapidjson::Value& obj) override;

	void resolve_relations() {}

	int64_t tg_id;
	std::string user_name;
	std::string name;
	ForeignKey<TelegramUser, Chat> chat;
	ForeignKey<TelegramUser, Client> client;
};

enum class MainState {
	Start,
		MainMenu,
			MakeAppointment,
				PaidAppointment,
					PASelectService,
						PASelectClinic,
							PASelectDates,
								PASelectDoctor,
									PASelectTime,
										PANearestTime,
										PAInputTime,
											PASetPersInfo,
												PAConfirm,
													PAMake,
														PACantMake,
				StateInsuranceApp,
				PrivateInsuranceApp,
			Doctors,
			Services,
			Clinics,
			ForClient,
			Sales,
			Contacts,
			ListAppointments,
				ConfirmCancel,
			
		CmdMainMenu};

enum class SubState {Base, Ask, ProcAnsw, Invalid};

class Chat: public Model
{
public:
	Chat(id_t user, int64_t chat_id);

	Chat(const rapidjson::Value& json);

	rapidjson::Value serialize(
		rapidjson::MemoryPoolAllocator<>& alloc) const override;

	void deserialize(const rapidjson::Value& obj) override;

	void resolve_relations() {}

	ForeignKey<Chat, TelegramUser> user;
	MainState ms;
	SubState ss;
	int64_t chat_id;
	mutable int32_t last_msg_id;
	mutable uint32_t last_query_msg_date;

	struct Tmp { // making appointment
		id_t spec;
		id_t clinic;
		int year;
		int month;
		time_t from ;
		time_t to;
		enum class DateErr {FromLessThanNow, Nan} date_err;
		id_t doc;
		int doc_n;
		time_t day;
		time_t res;

		enum class PersInfo {Name, Email, Phone} pers;
		std::string full_name;
		std::string email;
		std::string phone_num;

		int display_appo;
		id_t canc_id;

		int docs_page;
		bool docs_has_next_page;
	} tmp;
};

class Person: public Model
{
public:
	Person() = default;

	Person(
		const std::string& full_name,
		const std::string& phone_number,
		const std::string& email);

	Person(const rapidjson::Value& json);

	rapidjson::Value serialize(
		rapidjson::MemoryPoolAllocator<>& alloc) const override;

	void deserialize(const rapidjson::Value& obj) override;

	void resolve_relations();

	std::string full_name;
	std::string phone_number;
	std::string email;
};

class Client: public Person
{
public:
	Client(
		const std::string& full_name,
		const std::string& phone_number,
		const std::string& email,
		id_t user,
		const std::string& insurance_number);

	Client(const rapidjson::Value& json);

	rapidjson::Value serialize(
		rapidjson::MemoryPoolAllocator<>& alloc) const override;

	void deserialize(const rapidjson::Value& obj) override;

	void resolve_relations() {}

	ForeignKey<Client, TelegramUser> user;
	std::string insurance_number;
	ForeignKey<Client, Appointment> appointments;
};

class Doctor: public Person
{
public:
	// ненужный конструктор
	Doctor(
		const std::string& full_name,
		const std::string& phone_number,
		const std::string& email,
		const std::string& photo_file,
		const std::string& description,
		const std::vector<id_t>& specs,
		id_t work_sch,
		id_t clinic);

	Doctor(const rapidjson::Value& json);

	rapidjson::Value serialize(
		rapidjson::MemoryPoolAllocator<>& alloc) const override;

	void deserialize(const rapidjson::Value& obj) override;

	void resolve_relations();

	std::string photo_file;
	std::string description;
	ForeignKey<Doctor, Appointment> appointments;
	ForeignKey<Doctor, Speciality> specialities; // many to many !!
	ForeignKey<Doctor, WorkSchedule> work_sch;
	ForeignKey<Doctor, Clinic> clinic;
};

class Appointment: public Model
{
public:
	Appointment(id_t client, id_t doctor, id_t speciality,
		Period time, id_t clinic);

	Appointment(const rapidjson::Value& json);

	rapidjson::Value serialize(
		rapidjson::MemoryPoolAllocator<>& alloc) const override;

	void deserialize(const rapidjson::Value& obj) override;

	void resolve_relations();

	ForeignKey<Appointment, Client> client;
	ForeignKey<Appointment, Doctor> doctor;
	ForeignKey<Appointment, Speciality> speciality;
	Period time;
	ForeignKey<Appointment, Clinic> clinic;
};

class Speciality: public Model
{
public:
	// ненужный конструктор
	Speciality(const std::string& title, time_t appointment_duration);

	Speciality(const rapidjson::Value& json);

	rapidjson::Value serialize(
		rapidjson::MemoryPoolAllocator<>& alloc) const override;

	void deserialize(const rapidjson::Value& obj) override;
	
	void resolve_relations() {}

	std::string title;
	time_t appointment_duration;
	ForeignKey<Speciality, Doctor> doctors;
	ForeignKey<Speciality, Appointment> appointments;
};

class Clinic: public Model
{
public:
	// ненужный конструктор
	Clinic(const std::string& address);

	Clinic(const rapidjson::Value& json);

	rapidjson::Value serialize(
		rapidjson::MemoryPoolAllocator<>& alloc) const override;

	void deserialize(const rapidjson::Value& obj) override;

	void resolve_relations() {}

	std::string address;
	ForeignKey<Clinic, Appointment> appointments;
	ForeignKey<Clinic, Doctor> doctors;
};

class WorkSchedule: public Model
{
public:
	// ненужный конструктор
	WorkSchedule(id_t doctor,
		const std::unordered_map<time_t, WorkShift>& ws);

	WorkSchedule(const rapidjson::Value& json);

	rapidjson::Value serialize(
		rapidjson::MemoryPoolAllocator<>& alloc) const override;

	void deserialize(const rapidjson::Value& obj) override;

	void resolve_relations() {};

	ForeignKey<WorkSchedule, Doctor> doctors;
	std::unordered_map<time_t, WorkShift> ws;
};



#endif