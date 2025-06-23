#include "bot/storage.h"
#include "bot/tools.h"
#include <rapidjson/rapidjson.h>

Model::Model()
:Enumerated()
{}

Model::Model(const rapidjson::Value& json)
{
	deserialize(json);
}

rapidjson::Value Model::serialize(
	rapidjson::MemoryPoolAllocator<>& alloc) const
{
	rapidjson::Value obj(rapidjson::kObjectType);
	obj.AddMember("id", id(), alloc);
	return obj;
}

void Model::deserialize(const rapidjson::Value& obj)
{
	set_id(obj["id"].GetInt64());
}

bool Model::operator==(const Model& m) const
{
	return m.id() == id();
}

bool Model::operator!=(const Model& m) const
{
	return m.id() != id();
}

void Database::read(const std::string& file_name)
{
	auto document = read_json(file_name);
	deserialize(document["db"]);
}

void Database::write(const std::string& file_name) const
{
	rapidjson::Document document;
	document.SetObject();
	auto& allocator = document.GetAllocator();
	add_prop(document, allocator, "db", serialize(allocator));
	write_json(file_name, document);
}