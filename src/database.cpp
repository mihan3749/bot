#include "bot/database.h"
#include <stdexcept>

DB1::DB1(const std::string& file_name)
{
	DB1::instance = this;
	read(file_name);
}

DB1::~DB1()
{
	users.disable();
	chats.disable();
	clients.disable();
	doctors.disable();
	appointments.disable();
	specialties.disable();
	clinics.disable();
	work_shedule.disable();
}

rapidjson::Value DB1::serialize(
	rapidjson::MemoryPoolAllocator<>& alloc) const
{
	rapidjson::Value obj(rapidjson::kObjectType);
	obj.AddMember("users", users.serialize(alloc), alloc);
	obj.AddMember("chats", chats.serialize(alloc), alloc);
	obj.AddMember("clients", clients.serialize(alloc), alloc);
	obj.AddMember("doctors", doctors.serialize(alloc), alloc);
	obj.AddMember("appointments", appointments.serialize(alloc), alloc);
	obj.AddMember("specialties", specialties.serialize(alloc), alloc);
	obj.AddMember("clinics", clinics.serialize(alloc), alloc);
	obj.AddMember("work_shedule", work_shedule.serialize(alloc), alloc);
	return obj;
}

void DB1::deserialize(const rapidjson::Value& obj)
{
	users.deserialize(obj["users"]);
	chats.deserialize(obj["chats"]);
	clients.deserialize(obj["clients"]);
	doctors.deserialize(obj["doctors"]);
	appointments.deserialize(obj["appointments"]);
	specialties.deserialize(obj["specialties"]);
	clinics.deserialize(obj["clinics"]);
	work_shedule.deserialize(obj["work_shedule"]);

	resolve_relations();
}

DB1& DB1::get_instance()
{
	if (instance == nullptr)
		throw std::runtime_error("DB1::get_instance(): no instance");
	return *instance;
}

void DB1::resolve_relations()
{
	users.resolve_relations();
	chats.resolve_relations();
	clients.resolve_relations();
	doctors.resolve_relations();
	appointments.resolve_relations();
	specialties.resolve_relations();
	clinics.resolve_relations();
	work_shedule.resolve_relations();
}

DB1* DB1::instance = nullptr;