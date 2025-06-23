#ifndef _DATABASE_H
#define _DATABASE_H

#include "bot/storage.h"
#include "bot/models.h"
#include <rapidjson/document.h>

class DB1: public Database
{
public:
	DB1(const std::string& file_name);

	~DB1();

	rapidjson::Value serialize(
		rapidjson::MemoryPoolAllocator<>& alloc) const override;

	void deserialize(const rapidjson::Value& obj) override;

	Table<TelegramUser> users;
	Table<Chat> chats;
	Table<Client> clients;
	Table<Doctor> doctors;
	Table<Appointment> appointments;
	Table<Speciality> specialties;
	Table<Clinic> clinics;
	Table<WorkSchedule> work_shedule;

	static DB1& get_instance();

private:
	void resolve_relations() override;

	static DB1* instance;
};

#endif