#ifndef _TOOLS_H
#define _TOOLS_H

#include <cstdint>
#include <ctime>
#include <functional>
#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
#include <iostream>

class Serializable
{
public:
	virtual ~Serializable() = default;

	virtual rapidjson::Value serialize(
		rapidjson::MemoryPoolAllocator<>& alloc) const =0;

	virtual void deserialize(const rapidjson::Value& obj) =0;
};

class Period: public Serializable
{
public:
	rapidjson::Value serialize(
		rapidjson::MemoryPoolAllocator<>& alloc) const override;
	
	void deserialize(const rapidjson::Value& obj)  override;

	bool overlap(const Period& p) const;

	time_t from;
	time_t to;
};

class WorkShift: public Serializable
{
public:
	~WorkShift() = default;
	
	rapidjson::Value serialize(
		rapidjson::MemoryPoolAllocator<>& alloc) const override;
	
	void deserialize(const rapidjson::Value& obj) override;

	std::vector<Period> work_time;
};

class Enumerated
{
public:
	Enumerated();

	id_t id() const;

	bool is_null_id() const;

protected:
	void set_id(id_t id);

	void set_null_id();

private:
	id_t _id;
	static id_t curr_id;
};

template<typename States, typename ...Args>
class StateMachine 
{
public:
	using Handler = std::function<bool(Args... args)>;

	void handle(States& state, const Args... args)
	{
		bool finished = false;
		while (!(finished = handlers[state](args...)));
	}

	void add_hdl(States state, Handler handler)
	{
		handlers[state] = handler;
	}

private:
	std::unordered_map<States, Handler> handlers;
};

class TextManager
{
public:
	enum class Language {ru, en};

	TextManager(const std::string& file_name,
		Language language=Language::ru);

	const rapidjson::Document& raw() const;

	template<typename ...Args>
	const rapidjson::Value& get(Args ...args) const
	{
		return get_nested_value(doc[lang_code_to_str()], args...);
	}

	template<typename ...Args>
	bool has(Args ...args) const
	{
		try {
			get_nested_value(doc[lang_code_to_str()], args...);
		} catch(...) {
			return false;
		}
		return true;
	}

	template<typename ...Args>
	std::string str(Args ...args) const
	{
		auto& res = get_nested_value(doc[lang_code_to_str()], args...);
		if (!res.IsString())
			throw std::runtime_error(
				"TextManager: received value is not a string");
		return res.GetString();
	}

	template<typename ...Args>
	std::string operator()(Args ...args) const
	{
		return str(args...);
	}

	template<typename ...Args>
	int num(Args ...args) const
	{
		auto& res = get_nested_value(doc[lang_code_to_str()], args...);
		if (!res.IsInt())
			throw std::runtime_error(
				"TextManager: received value is not an int");
		return res.GetInt();
	}

	template<typename ...Args>
	std::vector<std::string> str_vec(Args ...args) const
	{
		auto& res = get_nested_value(doc[lang_code_to_str()], args...);
		if (!res.IsArray())
			throw std::runtime_error(
				"TextManager: received value is not an array");
		
		if (res.Size() == 0)
			return {};

		if (!res[0].IsString())
			throw std::runtime_error(
				"TextManager: received array is not an string array");

		std::vector<std::string> vec(res.Size());
		for (size_t i {}; i < res.Size(); ++i)
			vec[i] = res[i].GetString();

		return vec;
	}

	template<typename ...Args>
	std::vector<std::string> vec(Args ...args) const
	{
		return str_vec(args...);
	}

	template<typename ...Args>
	std::vector<int> int_vec(Args ...args) const
	{
		auto& res = get_nested_value(doc[lang_code_to_str()], args...);
		if (!res.IsArray())
			throw std::runtime_error(
				"TextManager: received value is not an array");
		
		if (res.Size() == 0)
			return {};

		if (!res[0].IsInt())
			throw std::runtime_error(
				"TextManager: received array is not an int array");

		std::vector<int> vec(res.Size());
		for (size_t i {}; i < res.Size(); ++i)
			vec[i] = res[i].GetInt();

		return vec;
	}

	static TextManager& get_instance();

	Language lang;

private:
	template<typename ...Args>
	const rapidjson::Value& get_nested_value(const rapidjson::Value& obj,
		const char* prop, Args ...args) const
	{
		if (!obj.IsObject())
			throw std::runtime_error("TextManager: can't extract the value");

		auto itr = obj.FindMember(prop);
		if (itr == obj.MemberEnd())
			throw std::runtime_error("TextManager: can't extract the value");

		if constexpr (sizeof...(args) == 0) {
			return itr->value;
		} else {
			return get_nested_value(itr->value, args...);
		}
	}

	const char* lang_code_to_str() const;
	
	const rapidjson::Document doc;
	static TextManager* instance;
};

template<typename T>
rapidjson::Value serialize_u_map(const std::unordered_map<int64_t, T>& map,
	rapidjson::MemoryPoolAllocator<>& alloc);

template<typename T>
std::unordered_map<int64_t, T> deserialize_u_map(const rapidjson::Value& obj);

rapidjson::Document read_json(const std::string& file_path);

void write_json(const std::string& file_path, const rapidjson::Document& doc);

void add_prop(rapidjson::Value& obj, rapidjson::MemoryPoolAllocator<>& alloc,
	const std::string& key, const std::string& val);

template<typename T>
void add_prop(rapidjson::Value& obj, rapidjson::MemoryPoolAllocator<>& alloc,
	const std::string& key, const T& val)
{
	auto value {rapidjson::Value(key.c_str(), alloc)};
	obj.AddMember(
		value,
		(T&)val,
		alloc);
}

// template<>
// inline void add_prop<const std::string&>(rapidjson::Value& obj, rapidjson::MemoryPoolAllocator<>& alloc,
// 	const std::string& key, const std::string& val)
// {
// 	obj.AddMember(
// 		rapidjson::Value(key.c_str(), alloc),
// 		rapidjson::Value(val.c_str(), alloc),
// 		alloc);
// } не работает блядина

void add_str(rapidjson::Value& obj, rapidjson::MemoryPoolAllocator<>& alloc,
	const std::string& key, const std::string& val);

template<typename T>
rapidjson::Value serialize_vec(const std::vector<T>& vec,
	rapidjson::MemoryPoolAllocator<>& alloc)
{
	rapidjson::Value arr(rapidjson::kArrayType);
	for (const auto& i : vec)
		arr.PushBack(i.serialize(alloc), alloc);
	return arr;
}

template<typename T>
std::vector<T> deserialize_vec(const rapidjson::Value& arr)
{
	assert(arr.IsArray());
	std::vector<T> res (arr.Size());
    for (rapidjson::SizeType i = 0; i < arr.Size(); ++i) {
		T x;
		x.deserialize(arr[i]);
		res[i] = x;
	}
	return res;
}

template<typename T>
rapidjson::Value serialize_u_map(const std::unordered_map<int64_t, T>& map,
	rapidjson::MemoryPoolAllocator<>& alloc)
{
	rapidjson::Value obj(rapidjson::kObjectType);
	for (const auto& kv : map)
		add_prop(obj, alloc, std::to_string(kv.first),
			kv.second.serialize(alloc));
	return obj;
}

template<typename T>
std::unordered_map<int64_t, T> deserialize_u_map(const rapidjson::Value& obj)
{
	std::unordered_map<int64_t, T> res {};
    for (auto itr = obj.MemberBegin(); itr != obj.MemberEnd(); ++itr) {
        int64_t key = std::stol(itr->name.GetString());
        const rapidjson::Value& value = itr->value;
		T x;
		x.deserialize(value);
		res[key] = x;
	}
	return res;
}

void print_json(const rapidjson::Value& obj);

constexpr uint32_t str_hash(std::string_view sv, uint32_t hash=2166136261u) {
    for (char c : sv) {
        hash ^= static_cast<uint32_t>(c);
        hash *= 16777619u;
    }
    return hash;
}

std::string time_to_dd_month_hh_mm(const TextManager& tm, time_t t);

std::string time_to_hh_mm(time_t t);

std::string time_to_mm_dd(time_t t);

bool regex_match(const std::string& pattern, const std::string& subject);

std::string format_appointment(const TextManager& tm, const std::string& spec,
	const std::string& doc, const std::string& addr, time_t time,
	const std::string& padding="");

std::string time_to_hh_mm_ss(time_t t);

void log();

template<typename First, typename... Rest>
void log(const First& first, const Rest&... rest) {
#ifdef DEBUG
    std::cout << first;
    if constexpr (sizeof...(rest) > 0) {
        std::cout << " ";
    }
    log(rest...);
#endif
}

#endif