#ifndef _STORAGE_H
#define _STORAGE_H

#include "bot/tools.h"
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <rapidjson/document.h>

template<typename T>
class Table;

class Model: public Enumerated, public Serializable
{
public:
	rapidjson::Value serialize(
		rapidjson::MemoryPoolAllocator<>& alloc) const override;

	void deserialize(const rapidjson::Value& obj) override;

	void resolve_relations() {};

	bool operator==(const Model& m) const;

	bool operator!=(const Model& m) const;

protected:
	Model();

	Model(const rapidjson::Value& json);
};

enum class Relation {OneToOne, OneToMany, BackToMany, ManyToMany};
enum class OnDelete {Cascade, SetNull, Restrict, NoAction};

template<typename From, typename To>
class ForeignKey: public Serializable
{
public:
	// зря копирует ForeignKey, но так getter проще определить
	using getter_f = ForeignKey<To, From>(*)(const To*);

	ForeignKey()
	:data{std::make_shared<ForeignKeyCore>()}
	{}

	ForeignKey(Relation rel, id_t from_id,
		const std::unordered_set<id_t>& to_ids={},
		OnDelete on_del=OnDelete::NoAction, getter_f getter=nullptr)
	:data{std::make_shared<ForeignKeyCore>(
		rel, from_id, to_ids,on_del, getter)}
	{
		data->validate();
	}

	rapidjson::Value serialize(
		rapidjson::MemoryPoolAllocator<>& alloc) const override
	{
		rapidjson::Value obj(rapidjson::kObjectType);
		if (data == nullptr)
			throw std::runtime_error("Иди нахуй");

		rapidjson::Value arr(rapidjson::kArrayType);
		for (id_t id : data->to_ids)
			arr.PushBack(id, alloc);

		add_prop(obj, alloc, "from", data->from_id);
		add_prop(obj, alloc, "to", arr);
		add_prop(obj, alloc, "rel", (int)data->rel);
		add_prop(obj, alloc, "del", (int)data->on_del);

		return obj;
	}

	void deserialize(const rapidjson::Value& obj) override
	{
		data->to_ids.clear();
		const rapidjson::Value& arr = obj["to"];
		for (size_t i = 0; i < arr.Size(); ++i)
			data->to_ids.insert(arr[i].GetUint());

		data->from_id = obj["from"].GetUint();
		data->rel = (Relation)(obj["rel"].GetInt());
		data->on_del = (OnDelete)(obj["del"].GetInt());

		data->validate();
	}

	std::shared_ptr<To> get()
	{
		requires_one_relation();
		return Table<To>::get_instance().get(*data->to_ids.begin());
	}

	std::shared_ptr<const To> get() const
	{
		requires_one_relation();
		return Table<To>::get_instance().get(*data->to_ids.begin());
	}

	inline size_t size() const
	{
		return data->to_ids.size();
	}

	inline id_t id() const
	{
		requires_one_relation();
		return *(data->to_ids.begin());
	}

	inline bool is_null() const
	{
		return id() == 0;
	}

	void delete_this()
	{
		data.reset();
	}

	void set_null()
	{
		set_id(0);
	}

	void resolve()
	{
		requires_other_to_have_many_relations();
		for (id_t id : data->to_ids)
			data->call_getter(id).insert(data->from_id);
	}

	void set_id(id_t to_id, bool must_resolve=false)
	{
		requires_one_relation();
		data->to_ids.clear();
		data->to_ids.insert(to_id);
		if (must_resolve)
			resolve();
	}

	void set_getter(getter_f new_getter)
	{
		data->getter = new_getter;
	}

	void erase(id_t to_id)
	{
		requires_many_relations();
		data->to_ids.erase(to_id);
	}

	void insert(id_t to_id)
	{
		requires_many_relations();
		data->to_ids.insert(to_id);
	}

	bool has(id_t to_id) const
	{
		return data->to_ids.find(to_id) != data->to_ids.end();
	}

	inline auto begin() const
	{
		requires_many_relations();
		return data->to_ids.begin();
	}

	inline auto end() const
	{
		requires_many_relations();
		return data->to_ids.end();
	}

	To* operator->()
	{
		requires_non_null();
		return Table<To>::get_instance().get(id()).get();
	}

	const To* operator->() const
	{
		requires_non_null();
		return Table<To>::get_instance().get(id()).get();
	}

private:
	void requires_one_relation() const
	{
		if (data->rel != Relation::OneToOne &&
			data->rel != Relation::OneToMany)
			throw std::runtime_error("ForeignKey: requires_one_relation");
	}

	void requires_many_relations() const
	{
		if (data->rel != Relation::BackToMany &&
			data->rel != Relation::ManyToMany)
			throw std::runtime_error("ForeignKey: requires_many_relations");
	}

	void requires_other_to_have_many_relations() const
	{
		if (data->rel == Relation::OneToOne ||
			data->rel == Relation::BackToMany)
			throw std::runtime_error(
				"ForeignKey: requires_other_to_have_many_relations");
	}

	void requires_non_null() const
	{
		if (data == nullptr)
			throw std::runtime_error("ForeignKey: requires_non_null");

		requires_one_relation();

		if (id() == 0)
			throw std::runtime_error("ForeignKey: requires_non_null");
	}

	struct ForeignKeyCore
	{
		ForeignKeyCore() {}

		ForeignKeyCore(Relation rel, id_t from_id,
			const std::unordered_set<id_t>& to_ids,
			OnDelete on_del, getter_f getter)
		:rel{rel}, from_id{from_id}, to_ids{to_ids},
		on_del{on_del}, getter{getter}
		{}

		// может не работать
		void validate() const
		{
			assert(from_id != 0);

			// assert((rel != Relation::OneToOne && rel != Relation::OneToMany) ||
			// 	((rel == Relation::OneToOne || rel == Relation::OneToMany) &&
			// 	to_ids.size() == 1));

			assert(
				!((rel == Relation::OneToOne || rel == Relation::OneToMany) &&
				to_ids.size() != 1)
			);

			assert(
				!((rel == Relation::OneToMany || rel == Relation::ManyToMany) &&
				(on_del == OnDelete::Cascade || on_del == OnDelete::SetNull) &&
				getter == nullptr)
			);
		}

		ForeignKey<To, From> call_getter(id_t id)
		{
			return (
				getter(Table<To>::get_instance().get(id).get()));
		}

		void requires_getter() const
		{
			if (getter == nullptr)
				throw std::runtime_error("ForeignKeyCore: requires_getter");
		}

		void requires_empty() const
		{
			if (!to_ids.empty())
				throw std::runtime_error("ForeignKeyCore: requires_empty");
		}

		~ForeignKeyCore()
		{
			if (from_id == 0 || Table<To>::get_instance().is_disabled())
				return;

			switch (on_del) {
			case OnDelete::Cascade:
				if (rel == Relation::OneToMany || rel == Relation::ManyToMany) {
					requires_getter();
					for (id_t id : to_ids)
						call_getter(id).erase(from_id);
				}

				for (id_t id : to_ids) {
					if (id == 0)
						continue;
					Table<To>::get_instance().del(id);
				}
				break;
			case OnDelete::SetNull:
				requires_getter();
				switch (rel) {
				case Relation::OneToOne:
				case Relation::BackToMany:
					for (id_t id : to_ids) {
						if (id == 0)
							continue; // можно и return
						call_getter(id).set_null();
					}
					break;
				default:
					for (id_t id : to_ids) {
						if (id == 0)
							continue;
						call_getter(id).erase(from_id);
					}
				}
				break;
			case OnDelete::Restrict:
				requires_empty();
				break;
			case OnDelete::NoAction:
				break;
			}
		}

		Relation rel;
		id_t from_id;
		std::unordered_set<id_t> to_ids;
		OnDelete on_del;
		getter_f getter;
	};

	std::shared_ptr<ForeignKeyCore> data; // наверно можно и без shared_ptr
};

class Database: public Serializable
{
public:
	virtual ~Database() = default;

	rapidjson::Value serialize(
		rapidjson::MemoryPoolAllocator<>& alloc) const =0;

	void deserialize(const rapidjson::Value& obj) =0;

	void read(const std::string& file_name);

	void write(const std::string& file_name) const;

private:
	virtual void resolve_relations() =0;
};

template<typename T, typename ...Args>
std::shared_ptr<T> Ptr(Args ...args)
{
	auto res = std::make_shared<T>(args...);
	Table<T>::get_instance().commit(res);
	return res;
};

template<typename T>
class Table:
	private std::unordered_map<id_t, std::shared_ptr<T>>,
	public Serializable
{
public:
	Table()
	:std::unordered_map<id_t, std::shared_ptr<T>>::unordered_map()
	{
		Table<T>::instance = this;
	}

	Table(const rapidjson::Value& table)
	:Table()
	{
		deserialize(table);
	}

	void commit(std::shared_ptr<T> x)
	{
		this->operator[](x->id()) = x;
	}

	std::shared_ptr<T> get(id_t id)
	{
		return this->at(id);
	}

	const std::shared_ptr<const T>& get(id_t id) const
	{
		return this->at(id);
	}

	void del(id_t id)
	{
		this->erase(id);
	}

	size_t size() const
	{
		return std::unordered_map<id_t, std::shared_ptr<T>>::size();
	}

	std::vector<std::shared_ptr<T>> all() const
	{
		std::vector<std::shared_ptr<T>> res (this->size());
		size_t n {};
		for (const auto& kv : *this)
			res[n++] = kv.second;
		return res;
	}

	void for_each(void (*func)(const std::shared_ptr<T>& x))
	{
		for (auto i = this->begin(); i != this->end(); ++i)
			func(i->second);
	}

	std::vector<std::shared_ptr<T>> filter(
		std::function<bool(const std::shared_ptr<T>& x)> pred)
	{
		std::vector<std::shared_ptr<T>> res {};
		for (const auto& kv : *this) {
			if (pred(kv.second))
				res.push_back(kv.second);
		}
		return res;
	}

	std::shared_ptr<T> find(
		std::function<bool(const std::shared_ptr<T>& x)> pred)
	{
		for (const auto& kv : *this) {
			if (pred(kv.second))
				return kv.second;
		}
		return nullptr;
	}

	rapidjson::Value serialize(
		rapidjson::MemoryPoolAllocator<>& alloc) const override
	{
		rapidjson::Value obj(rapidjson::kObjectType);
		for (const auto& kv : *this)
			add_prop(obj, alloc, std::to_string(kv.first),
				kv.second->serialize(alloc));
		return obj;
	}

	void deserialize(const rapidjson::Value& obj) override
	{
		for (auto itr = obj.MemberBegin(); itr != obj.MemberEnd(); ++itr) {
			id_t key = std::stoul(itr->name.GetString());
			const rapidjson::Value& value = itr->value;
			this->insert({key, Ptr<T, const rapidjson::Value&>(value)});
		}
	}

	void resolve_relations()
	{
		for (auto& kv : *this)
			kv.second->resolve_relations();
	}

	inline bool is_disabled() const
	{
		return _is_disabled;
	}

	inline void disable()
	{
		_is_disabled = true;
	}

	static Table<T>& get_instance()
	{
		if (instance == nullptr)
			throw std::runtime_error(std::string(
			"Table<") + typeid(T).name() + ">::get_instance(): no instance");
		return *instance;
	}

private:
	static Table<T>* instance;
	static bool _is_disabled;
};

template<typename T>
Table<T>* Table<T>::instance = nullptr;

template<typename T>
bool Table<T>::_is_disabled = false;

#endif