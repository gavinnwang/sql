#pragma once
#include "common/config.hpp"
#include "common/logger.hpp"
#include "common/typedef.hpp"
#include "meta/schema.hpp"
#include "storage/page_allocator.hpp"
#include "storage/serializer/serializer.hpp"

#include <cstdint>
#include <memory>
#include <mutex>

namespace db {

struct TableMeta {
	explicit TableMeta() = default;

	TableMeta(Schema schema, std::string name, table_oid_t table_oid)
	    : schema_ {std::move(schema)}, name_ {std::move(name)}, table_oid_ {table_oid} {
	}

	void Serialize(Serializer &serializer) const {
		serializer.WriteProperty(100, "table_name", name_);
		serializer.WriteProperty(101, "table_oid", table_oid_);
		serializer.WriteProperty(102, "table_schema", schema_);
		serializer.WriteProperty(103, "last_table_data_page_id", last_table_data_page_id_);
		serializer.WriteProperty(104, "last_table_heap_data_page_id", last_table_heap_data_page_id_);
		serializer.WriteProperty(105, "tuple_count", tuple_count_);
	}

	[[nodiscard]] static std::unique_ptr<TableMeta> Deserialize(Deserializer &deserializer) {
		auto meta = std::make_unique<TableMeta>();
		deserializer.ReadProperty(100, "table_name", meta->name_);
		deserializer.ReadProperty(101, "table_oid", meta->table_oid_);
		deserializer.ReadProperty(102, "table_schema", meta->schema_);
		deserializer.ReadProperty(103, "last_table_data_page_id", meta->last_table_data_page_id_);
		deserializer.ReadProperty(104, "last_table_heap_data_page_id", meta->last_table_heap_data_page_id_);
		deserializer.ReadProperty(105, "tuple_count", meta->tuple_count_);
		return meta;
	}

	[[nodiscard]] page_id_t GetLastTableDataPageId() const {
		return last_table_data_page_id_;
	}

	void SetLastTableDataPageId(page_id_t last_table_data_page_id) {
		last_table_data_page_id_ = last_table_data_page_id;
	}

	void SetLastTableHeapDataPageId(page_id_t last_table_heap_data_page_id) {
		last_table_heap_data_page_id_ = last_table_heap_data_page_id;
	}

	[[nodiscard]] page_id_t GetLastTableHeapDataPageId() const {
		return last_table_heap_data_page_id_;
	}

	// effectively bump the end of the table data file
	page_id_t IncrementTableDataPageId() {
		assert(table_oid_ != INVALID_TABLE_OID);
		if (last_table_data_page_id_ == INVALID_PAGE_ID) {
			// start from 0
			return last_table_data_page_id_ = START_PAGE_ID;
		}
		return ++last_table_data_page_id_;
	}

	void IncreaseTupleCount() {
		tuple_count_++;
	}

	std::string ToString() const {
		return fmt::format("TableMeta(name: {}, table_oid: {}, schema: {}, last_table_data_page_id: {}, "
		                   "last_table_heap_data_page_id: {}, tuple_count: {})",
		                   name_, table_oid_, schema_.ToString(), last_table_data_page_id_,
		                   last_table_heap_data_page_id_, tuple_count_);
	}
	Schema schema_;
	std::string name_;
	table_oid_t table_oid_ {INVALID_TABLE_OID};

	static constexpr page_id_t START_PAGE_ID = 0;
	// the last page id of the table data file
	// effectively the end of the linked list
	// thus invariant: last_table_data_page_id_ >= last_table_heap_data_page_id_ && last_index_data_page_id_ <=
	// last_index_heap_data_page_id_
	// this also include index data
	page_id_t last_table_data_page_id_ {INVALID_PAGE_ID};
	// this only include table heap (tuple) data
	// the last page id of the table heap file
	// effectively the end of the table heap
	page_id_t last_table_heap_data_page_id_ {INVALID_PAGE_ID};

	uint64_t tuple_count_ {0};
	std::mutex latch_;
};
} // namespace db

template <>
struct fmt::formatter<db::TableMeta> : formatter<std::string_view> {
	auto format(db::TableMeta x, format_context &ctx) const {
		return formatter<string_view>::format(x.ToString(), ctx);
	}
};
