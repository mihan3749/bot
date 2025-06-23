#include "bot/tools.h"
#include <ctime>
#include <iostream>
#include <pcre.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include <fstream>
#include <stdexcept>
#include <string>

rapidjson::Value Period::serialize(
	rapidjson::MemoryPoolAllocator<>& alloc) const
{
	rapidjson::Value arr(rapidjson::kArrayType);
	arr.PushBack(from, alloc);
	arr.PushBack(to, alloc);
	return arr;
}

void Period::deserialize(const rapidjson::Value& arr)
{
	from = arr[0].GetInt64();
	to = arr[1].GetInt64();
}

bool Period::overlap(const Period& p) const
{
	return !(to < p.from || p.to < from);
}

rapidjson::Value WorkShift::serialize(
	rapidjson::MemoryPoolAllocator<>& alloc) const
{
	return serialize_vec(work_time, alloc);
}

void WorkShift::deserialize(const rapidjson::Value& obj)
{
	work_time = deserialize_vec<Period>(obj);
}

Enumerated::Enumerated()
:_id{++curr_id}
{}

id_t Enumerated::id() const
{
	return _id;
}

bool Enumerated::is_null_id() const
{
	return _id == 0;
}

void Enumerated::set_id(id_t id)
{
	if (id > curr_id)
		curr_id = id;
	_id = id;
}

void Enumerated::set_null_id()
{
	_id = 0;
}

id_t Enumerated::curr_id = 0;

TextManager::TextManager(const std::string& file_name,
    Language language)
:doc{read_json(file_name)}, lang{language}
{
	instance = this;
}

const rapidjson::Document& TextManager::raw() const
{
	return doc;
}

TextManager& TextManager::get_instance()
{
	if (instance == nullptr)
		throw std::runtime_error("TextManager::get_instance(): no instance");
	return *instance;
}

const char* TextManager::lang_code_to_str() const
{
    switch (lang) {
    case TextManager::Language::en:
        return "en";
    case TextManager::Language::ru:
        return "ru";
    }
}

TextManager* TextManager::instance = nullptr;

rapidjson::Document read_json(const std::string& file_path)
{
	FILE* fp = std::fopen(file_path.c_str(), "r");
    if (!fp)
		throw std::runtime_error("open_json(): can't open file");

    char readBuffer[65536];
    rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));

    rapidjson::Document document;
    document.ParseStream(is);
	std::fclose(fp);

    if (document.HasParseError())
		throw std::runtime_error("open_json(): can't parse file");

	return document;
}

void write_json(const std::string& file_path, const rapidjson::Document& doc)
{
	rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    std::ofstream ofs(file_path);
    if (!ofs.is_open()) {
        throw std::runtime_error("write_json(): can't open output file");
    }

    ofs << buffer.GetString();
    ofs.close();
}

void print_json(const rapidjson::Value& obj)
{
    rapidjson::Document document;
	document.SetObject();
	auto& allocator = document.GetAllocator();
    add_prop(document, allocator, "obj", obj);
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);
    std::cout << buffer.GetString() << "\n";
}

void add_str(rapidjson::Value& obj, rapidjson::MemoryPoolAllocator<>& alloc,
	const std::string& key, const std::string& val)
{
	obj.AddMember(
		rapidjson::Value(key.c_str(), alloc),
		rapidjson::Value(val.c_str(), alloc),
		alloc);
}

std::string time_to_dd_month_hh_mm(const TextManager& tm, time_t t)
{
	struct tm date = *std::localtime(&t);
	return std::to_string(date.tm_mday) + " " +
		tm.vec("month_genitive_case")[date.tm_mon] + ", " +
		time_to_hh_mm(t);
}

std::string time_to_hh_mm(time_t t)
{
	struct tm date = *std::localtime(&t);
	std::string res = std::to_string(date.tm_hour) + ":";
	res += (date.tm_min < 10 ? "0" : "") + std::to_string(date.tm_min);
	return res;
}

std::string time_to_mm_dd(time_t t)
{
	struct tm date = *std::localtime(&t);
	std::string res = "";
	res += (date.tm_mon + 1 < 10 ? "0" : "") + std::to_string(date.tm_mon + 1);
	res += ".";
	res += (date.tm_mday < 10 ? "0" : "") + std::to_string(date.tm_mday);
	return res;
}

bool regex_match(const std::string& pattern, const std::string& subject) {
    const char *error;
    int erroffset;

    pcre *re = pcre_compile(pattern.c_str(), PCRE_UTF8, &error, &erroffset, nullptr);
    if (!re) {
        std::cerr << "PCRE compilation failed at offset "
			<< erroffset << ": " << error << "\n";
        return false;
    }

    int ovector[30];
    int rc = pcre_exec(re, nullptr, subject.c_str(),
		(int)subject.length(), 0, 0, ovector, 30);
    pcre_free(re);

    if (rc < 0)
        return false;

    int start = ovector[0];
    int end = ovector[1];
    return (start == 0) && (end == (int)subject.length());
}

std::string format_appointment(const TextManager& tm, const std::string& spec,
	const std::string& doc, const std::string& addr, time_t time,
	const std::string& padding)
{
	return padding + tm("common", "spec") + ": " + spec + "\n" +
		padding + tm("common", "doc") + ": " + doc + "\n" +
		padding + tm("common", "place") + ": " + addr + "\n" +
		padding + tm("common", "time") + ": " +
			time_to_dd_month_hh_mm(tm, time);
}

std::string time_to_hh_mm_ss(time_t t)
{
	struct tm x = *std::localtime(&t);
	std::string res;
	res += (x.tm_hour < 10 ? "0" : "") + std::to_string(x.tm_hour) + ":";
	res += (x.tm_min < 10 ? "0" : "") + std::to_string(x.tm_min) + ":";
	res += (x.tm_sec < 10 ? "0" : "") + std::to_string(x.tm_sec);
	return res;
}

void log() {
#ifdef DEBUG
    std::cout << "\n";
#endif
}