#include "bot/models.h"
#include "bot/storage.h"
#include "bot/storage.h"
#include "bot/tools.h"
#include <sys/types.h>
#include <unordered_map>


TelegramUser::TelegramUser(int64_t tg_id, const std::string& user_name,
	const std::string& name, id_t chat, id_t client)
:Model(), tg_id{tg_id}, user_name{user_name}, name{name},
chat{Relation::OneToOne, id(), {chat}, OnDelete::Cascade},
client{Relation::OneToOne, id(), {client}, OnDelete::Cascade}
{}

TelegramUser::TelegramUser(const rapidjson::Value& obj)
{
	deserialize(obj);
}

rapidjson::Value TelegramUser::serialize(
	rapidjson::MemoryPoolAllocator<>& alloc) const
{
	auto obj = Model::serialize(alloc);
	add_prop(obj, alloc, "tg_id", tg_id);
	add_str(obj, alloc, "uname", user_name);
	add_str(obj, alloc, "name", name);
	add_prop(obj, alloc, "chat", chat.serialize(alloc));
	add_prop(obj, alloc, "client", client.serialize(alloc));
	return obj;
}

void TelegramUser::deserialize(const rapidjson::Value& obj)
{
	Model::deserialize(obj);
	tg_id = obj["tg_id"].GetInt64();
	user_name = obj["uname"].GetString();
	name = obj["name"].GetString();
	chat.deserialize(obj["chat"]);
	client.deserialize(obj["client"]);
}

Chat::Chat(id_t user, int64_t chat_id)
:user{Relation::OneToOne, id(), {user}, OnDelete::SetNull,
	[](auto u){return u->chat;}},
ms{MainState::Start}, ss{SubState::Base}, chat_id{chat_id}, last_msg_id{},
last_query_msg_date{}, tmp{}
{}

Chat::Chat(const rapidjson::Value& json)
:tmp{}
{
	deserialize(json);
}

rapidjson::Value Chat::serialize(
	rapidjson::MemoryPoolAllocator<>& alloc) const
{
	auto obj = Model::serialize(alloc);
	add_prop(obj, alloc, "user", user.serialize(alloc));
	add_prop(obj, alloc, "gs", (int)ms);
	add_prop(obj, alloc, "ss", (int)ss);
	add_prop(obj, alloc, "chat_id", chat_id);
	add_prop(obj, alloc, "last_msg", last_msg_id);
	return obj;
}

void Chat::deserialize(const rapidjson::Value& obj)
{
	Model::deserialize(obj);
	user.set_getter([](auto u){return u->chat;});
	user.deserialize(obj["user"]);
	ms = (MainState)(obj["gs"].GetInt());
	ss = (SubState)(obj["ss"].GetInt());
	chat_id = obj["chat_id"].GetInt64();
	last_msg_id = obj["last_msg"].GetInt();
}

Person::Person(
	const std::string& full_name,
	const std::string& phone_number,
	const std::string& email)
:Model(), full_name{full_name}, phone_number{phone_number}, email{email}
{}

Person::Person(const rapidjson::Value& obj)
{
	deserialize(obj);
}

rapidjson::Value Person::serialize(rapidjson::MemoryPoolAllocator<>& alloc) const
{
	auto obj = Model::serialize(alloc);
	add_str(obj, alloc, "full_name", full_name);
	add_str(obj, alloc, "phone_number", phone_number);
	add_str(obj, alloc, "email", email);
	return obj;
}

void Person::deserialize(const rapidjson::Value& obj)
{
	Model::deserialize(obj);
	full_name = obj["full_name"].GetString();
	phone_number = obj["phone_number"].GetString();
	email = obj["email"].GetString();
}

Client::Client(
	const std::string& full_name,
	const std::string& phone_number,
	const std::string& email,
	id_t user,
	const std::string& insurance_number)
:Person(full_name, phone_number, email),
user{Relation::OneToOne, id(), {user}, OnDelete::SetNull,
	[](auto u){return u->client;}},
insurance_number{insurance_number},
appointments{Relation::BackToMany, id(), {}, OnDelete::Cascade}
{}

Client::Client(const rapidjson::Value& obj)
{
	deserialize(obj);
}

rapidjson::Value Client::serialize(
	rapidjson::MemoryPoolAllocator<>& alloc) const
{
	auto obj = Person::serialize(alloc);
	add_prop(obj, alloc, "user", user.serialize(alloc));
	add_str(obj, alloc, "ins", insurance_number);
	return obj;
}

void Client::deserialize(const rapidjson::Value& obj)
{
	Person::deserialize(obj);
	user.set_getter([](auto u){return u->client;});
	user.deserialize(obj["user"]);
	insurance_number = obj["ins"].GetString();
	appointments = ForeignKey<Client, Appointment>(
		Relation::BackToMany, id(), {}, OnDelete::Cascade);
}

Doctor::Doctor(
	const std::string& full_name,
	const std::string& phone_number,
	const std::string& email,
	const std::string& photo_file,
	const std::string& description,
	const std::vector<id_t>& specs,
	id_t work_sch,
	id_t clinic)
:Person(full_name, phone_number, email),
photo_file{photo_file},
description{description},
appointments{Relation::BackToMany, id(), {}, OnDelete::Cascade},
specialities{Relation::ManyToMany, id(), {}, OnDelete::SetNull,
	[](auto s){return s->doctors;}},
work_sch{Relation::OneToMany, id(), {work_sch}, OnDelete::SetNull,
	[](auto ws){return ws->doctors;}},
clinic{Relation::OneToMany, id(), {clinic}, OnDelete::SetNull,
	[](auto c){return c->doctors;}}
{
	for (id_t id : specs)
		specialities.insert(id);
}

Doctor::Doctor(const rapidjson::Value& obj)
{
	deserialize(obj);
}

rapidjson::Value Doctor::serialize(
	rapidjson::MemoryPoolAllocator<>& alloc) const
{
	auto obj = Person::serialize(alloc);
	add_str(obj, alloc, "photo", photo_file);
	add_str(obj, alloc, "desc", description);
	add_prop(obj, alloc, "spec", specialities.serialize(alloc));
	add_prop(obj, alloc, "ws", work_sch.serialize(alloc));
	add_prop(obj, alloc, "clinic", clinic.serialize(alloc));
	return obj;
}

void Doctor::deserialize(const rapidjson::Value& obj)
{
	Person::deserialize(obj);
	photo_file = obj["photo"].GetString();
	description = obj["desc"].GetString();
	appointments = ForeignKey<Doctor, Appointment>(
		Relation::BackToMany, id(), {}, OnDelete::Cascade);
	specialities.set_getter([](auto s){return s->doctors;});
	specialities.deserialize(obj["spec"]);
	work_sch.set_getter([](auto ws){return ws->doctors;});
	work_sch.deserialize(obj["ws"]);
	clinic.set_getter([](auto c){return c->doctors;});
	clinic.deserialize(obj["clinic"]);
}

void Doctor::resolve_relations()
{
	specialities.resolve();
	work_sch.resolve();
	clinic.resolve();
}

Appointment::Appointment(id_t client, id_t doctor,
	id_t speciality, Period time, id_t clinic)
:client{Relation::OneToMany, id(), {client}, OnDelete::SetNull,
	[](auto c){return c->appointments;}},
doctor{Relation::OneToMany, id(), {doctor}, OnDelete::SetNull,
	[](auto d){return d->appointments;}},
speciality{Relation::OneToMany, id(), {speciality}, OnDelete::SetNull,
	[](auto s){return s->appointments;}},
time{time},
clinic{Relation::OneToMany, id(), {clinic}, OnDelete::SetNull,
	[](auto c){return c->appointments;}}
{}

Appointment::Appointment(const rapidjson::Value& obj)
{
	deserialize(obj);
}

rapidjson::Value Appointment::serialize(
	rapidjson::MemoryPoolAllocator<>& alloc) const
{
	auto obj = Model::serialize(alloc);
	add_prop(obj, alloc, "client", client.serialize(alloc));
	add_prop(obj, alloc, "doctor", doctor.serialize(alloc));
	add_prop(obj, alloc, "spec", speciality.serialize(alloc));
	add_prop(obj, alloc, "time", time.serialize(alloc));
	add_prop(obj, alloc, "clinic", clinic.serialize(alloc));
	return obj;
}

void Appointment::deserialize(const rapidjson::Value& obj)
{
	Model::deserialize(obj);
	client.set_getter([](auto x){return x->appointments;});
	client.deserialize(obj["client"]);
	doctor.set_getter([](auto x){return x->appointments;});
	doctor.deserialize(obj["doctor"]);
	speciality.set_getter([](auto x){return x->appointments;});
	speciality.deserialize(obj["spec"]);
	time.deserialize(obj["time"]);
	clinic.set_getter([](auto x){return x->appointments;});
	clinic.deserialize(obj["clinic"]);
}

void Appointment::resolve_relations()
{
	client.resolve();
	doctor.resolve();
	speciality.resolve();
	clinic.resolve();
}

Speciality::Speciality(const std::string& title, time_t appointment_duration)
:title{title},
appointment_duration{appointment_duration},
doctors{Relation::ManyToMany, id(), {}, OnDelete::SetNull,
	[](auto d){return d->specialities;}},
appointments{Relation::BackToMany, id(), {}, OnDelete::Restrict}
{}

Speciality::Speciality(const rapidjson::Value& obj)
{
	deserialize(obj);
}

rapidjson::Value Speciality::serialize(
	rapidjson::MemoryPoolAllocator<>& alloc) const
{
	auto obj = Model::serialize(alloc);
	add_str(obj, alloc, "title", title);
	add_prop(obj, alloc, "dur", appointment_duration);
	return obj;
}

void Speciality::deserialize(const rapidjson::Value& obj)
{
	Model::deserialize(obj);
	title = obj["title"].GetString();
	appointment_duration = obj["dur"].GetInt64();
	doctors = ForeignKey<Speciality, Doctor>(
		Relation::ManyToMany, id(), {}, OnDelete::SetNull,
		[](auto d){return d->specialities;});
	appointments = ForeignKey<Speciality, Appointment>(
		Relation::BackToMany, id(), {}, OnDelete::Restrict);
}

Clinic::Clinic(const std::string& address)
:address{address},
appointments{Relation::BackToMany, id(), {}, OnDelete::Restrict},
doctors{Relation::BackToMany, id(), {}, OnDelete::Restrict}
{}

Clinic::Clinic(const rapidjson::Value& obj)
{
	deserialize(obj);
}

rapidjson::Value Clinic::serialize(
	rapidjson::MemoryPoolAllocator<>& alloc) const
{
	auto obj = Model::serialize(alloc);
	add_str(obj, alloc, "addr", address);
	return obj;
}

void Clinic::deserialize(const rapidjson::Value& obj)
{
	Model::deserialize(obj);
	address = obj["addr"].GetString();
	doctors = ForeignKey<Clinic, Doctor>(
		Relation::BackToMany, id(), {}, OnDelete::Restrict);
	appointments = ForeignKey<Clinic, Appointment>(
		Relation::BackToMany, id(), {}, OnDelete::Restrict);
}

WorkSchedule::WorkSchedule(id_t doctor,
	const std::unordered_map<time_t, WorkShift>& ws)
:doctors{Relation::BackToMany, id(), {}, OnDelete::Restrict},
ws{ws}
{}

WorkSchedule::WorkSchedule(const rapidjson::Value& json)
{
	deserialize(json);
}

rapidjson::Value WorkSchedule::serialize(
	rapidjson::MemoryPoolAllocator<>& alloc) const
{
	auto obj = Model::serialize(alloc);
	add_prop(obj, alloc, "ws", serialize_u_map(ws, alloc));
	return obj;	
}

void WorkSchedule::deserialize(const rapidjson::Value& obj)
{
	Model::deserialize(obj);
	doctors = ForeignKey<WorkSchedule, Doctor>(
		Relation::BackToMany, id(), {}, OnDelete::Restrict);
	ws = deserialize_u_map<WorkShift>(obj["ws"]);
}