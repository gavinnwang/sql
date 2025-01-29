
#include "storage/table/table_heap.hpp"

#include "common/logger.hpp"
#include "common/page_id.hpp"
#include "storage/buffer/buffer_pool.hpp"
#include "storage/page/table_page.hpp"
#include "storage/table/table_iterator.hpp"
#include "storage/table/table_meta.hpp"

#include <utility>
namespace db {

TableHeap::TableHeap(BufferPool &bpm, TableMeta &table_meta) : bpm_(bpm), table_meta_(table_meta) {
	if (table_meta_.GetLastTableHeapDataPageId() == INVALID_PAGE_ID) {
		PageId new_page_id {table_meta_.table_oid_};
		auto guard = bpm.NewPageGuarded(*this, new_page_id);
		assert(new_page_id.page_number_ != INVALID_PAGE_ID && new_page_id.page_number_ >= 0 &&
		       "table heap create page failed");

		auto &first_page = guard.AsMut<TablePage>();
		first_page.Init();
	}
	assert(table_meta_.GetLastTableHeapDataPageId() >= 0);
};

std::optional<RID> TableHeap::InsertTuple(const TupleMeta &meta, const Tuple &tuple) {
	std::unique_lock<std::mutex> guard(latch_);
	PageId new_page_id {table_meta_.table_oid_, table_meta_.GetLastTableHeapDataPageId()};
	auto page_guard = bpm_.FetchPageWrite(new_page_id);
	while (true) {
		auto &page = page_guard.AsMut<TablePage>();
		if (page.GetNextTupleOffset(tuple) != std::nullopt) {
			break;
		}
		// tuple page is empty and still cannot insert
		assert(page.GetNumTuples() != 0 && "tuple is too large");
		// allocate a new page for the tuple because the current page is full
		// page_id_t next_page_id = INVALID_PAGE_ID;
		PageId next_page_id {table_meta_.table_oid_};
		auto npg = bpm_.NewPageGuarded(*this, next_page_id);
		assert(next_page_id.page_number_ != INVALID_PAGE_ID && "cannot allocate page");
		// construct the linked list
		page.SetNextPageId(next_page_id.page_number_);
		// initialize the next page
		auto &next_page = npg.AsMut<TablePage>();
		next_page.Init();
		// drop the current page
		page_guard.Drop();
		// fetch the next page
		auto next_page_guard = npg.UpgradeWrite();
		page_guard = std::move(next_page_guard);
	}
	auto last_page_id = table_meta_.GetLastTableHeapDataPageId();

	auto &page = page_guard.AsMut<TablePage>();
	auto slot_id = *page.InsertTuple(meta, tuple);
	guard.unlock();
	page_guard.Drop();

	table_meta_.IncreaseTupleCount();

	const auto rid = RID({table_meta_.table_oid_, last_page_id}, slot_id);
	LOG_TRACE("Inserted tuple with rid {}", rid.ToString());
	return rid;
};

void TableHeap::UpdateTupleMeta(const TupleMeta &meta, RID rid) {
	auto page_guard = bpm_.FetchPageWrite(rid.GetPageId());
	auto &page = page_guard.AsMut<TablePage>();
	page.UpdateTupleMeta(meta, rid);
};

std::optional<std::pair<TupleMeta, Tuple>> TableHeap::GetTuple(RID rid) const {
	auto page_guard = bpm_.FetchPageRead(rid.GetPageId());
	const auto &page = page_guard.As<TablePage>();
	auto ret = page.GetTuple(rid);
	if (!ret.has_value()) {
		return std::nullopt;
	}
	auto [meta, tuple] = *ret;
	tuple.rid_ = rid;
	return std::make_pair(meta, std::move(tuple));
};

TupleMeta TableHeap::GetTupleMeta(RID rid) {
	auto page_guard = bpm_.FetchPageRead(rid.GetPageId());
	const auto &page = page_guard.As<TablePage>();
	return page.GetTupleMeta(rid);
};

TableIterator TableHeap::MakeIterator() {
	std::unique_lock<std::mutex> guard(latch_);
	auto table_oid = table_meta_.table_oid_;
	auto last_page_id = table_meta_.GetLastTableHeapDataPageId();
	guard.unlock();

	auto page_guard = bpm_.FetchPageRead({table_oid, last_page_id});

	const auto &page = page_guard.As<TablePage>();
	auto num_tuples = page.GetNumTuples();
	page_guard.Drop();
	// iterate from rid 0, 0 to last_page_id and num_tuples
	return TableIterator {*this, {{table_oid, 1}, 0}, {{table_oid, last_page_id}, num_tuples}};
}

} // namespace db
