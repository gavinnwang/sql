#pragma once

#include "common/config.hpp"
#include "common/logger.hpp"
#include "common/typedef.hpp"
#include "index/index.hpp"
#include "storage/buffer/buffer_pool.hpp"
#include "storage/page/btree_page.hpp"

#include <algorithm>
#include <array>

namespace db {
static constexpr int INTERNAL_PAGE_HEADER_SIZE = 32;
using InternalNode = std::pair<IndexKeyType, InternalValueType>;
// static constexpr int INTERNAL_MAX_NODE_SIZE = 10;
static constexpr int INTERNAL_MAX_NODE_SIZE = (PAGE_SIZE - INTERNAL_PAGE_HEADER_SIZE) / sizeof(InternalNode);

class BtreeInternalPage : public BtreePage {

public:
	BtreeInternalPage() = delete;
	BtreeInternalPage(const BtreeInternalPage &other) = delete;
	BtreeInternalPage &operator=(const BtreeInternalPage &other) = delete;
	BtreeInternalPage(BtreeInternalPage &&other) = delete;
	BtreeInternalPage &operator=(BtreeInternalPage &&other) = delete;
	~BtreeInternalPage() = delete;

	void Init(page_id_t page_id, page_id_t parent_id) {
		SetPageType(IndexPageType::INTERNAL_PAGE);
		SetPageId(page_id);
		SetSize(0);
		SetMaxSize(INTERNAL_MAX_NODE_SIZE);
		SetParentPageId(parent_id);
		LOG_TRACE("Setting internal page size to 0 and max size to %d", static_cast<int>(INTERNAL_MAX_NODE_SIZE));
	}

	[[nodiscard]] IndexKeyType KeyAt(idx_t index) const {
		return node_array_[index].first;
	}

	void SetKeyAt(idx_t index, const IndexKeyType &key) {
		node_array_[index].first = key;
	}

	[[nodiscard]] idx_t ValueIndex(const InternalValueType &value) const {
		const auto *it = std::find_if(node_array_, node_array_ + GetSize(),
		                              [&value](const auto &pair) { return pair.second == value; });
		return std::distance(node_array_, it);
	}

	[[nodiscard]] InternalValueType ValueAt(idx_t index) const {
		return node_array_[index].second;
	}

	void SetValueAt(int index, const InternalValueType &value) {
		node_array_[index].second = value;
	}

	[[nodiscard]] InternalValueType Lookup(const IndexKeyType &key, const Comparator &comparator) const {

		LOG_TRACE("Internal page id %d with size %d, parent id %d, max size %d content %s",
		          static_cast<int>(GetPageId()), static_cast<int>(GetSize()), static_cast<int>(GetParentPageId()),
		          static_cast<int>(GetMaxSize()), ToString().c_str());
		LOG_TRACE("Lookup key %s and page is %s", IndexKeyTypeToString(key).c_str(), ToString().c_str());
		// ignore the first key
		// lwoer bound returns first it where comp(*it, key) is false
		const auto *target =
		    std::lower_bound(node_array_ + 1, node_array_ + GetSize(), key,
		                     [&comparator](const auto &pair, auto key) { return comparator(pair.first, key) < 0; });
		// if target is last then lookup key is larger than all keys
		if (target == node_array_ + GetSize()) {
			return ValueAt(GetSize() - 1);
			// if target is the same as the search key go to that value due our internal
			// node convention
		}
		if (comparator(key, target->first) == 0) {
			return target->second;
		}
		return std::prev(target)->second;
	}

	void PopulateNewRoot(const InternalValueType &old_value, const IndexKeyType &new_key,
	                     const InternalValueType &new_value) {
		LOG_TRACE("Populating new root with key %s and value %d", IndexKeyTypeToString(new_key).c_str(), new_value);
		SetValueAt(0, old_value);
		SetKeyAt(1, new_key);
		SetValueAt(1, new_value);
		SetSize(2);
		LOG_TRACE("Page: %s", ToString().c_str());
	}

	[[nodiscard]] std::string ToString() const {
		std::string result = "(";
		bool first = true;
		for (idx_t i = 0; i < GetSize(); i++) {
			if (!first) {
				result += ", ";
			}
			first = false;
			result += IndexKeyTypeToString(KeyAt(i));
			result += ":";
			result += std::to_string(ValueAt(i));
		}
		result += ")";
		return result;
	}

	idx_t InsertNodeAfter(const InternalValueType &old_value, const IndexKeyType &new_key,
	                      const InternalValueType &new_value) {

		// assert old value is in the node
		assert(ValueIndex(old_value) < GetSize());
		auto new_value_idx = ValueIndex(old_value) + 1;
		std::move_backward(node_array_ + new_value_idx, node_array_ + GetSize(), node_array_ + GetSize() + 1);
		// LOG_TRACE("Inserting key %s with value %d after key %s with value %d", IndexKeyTypeToString(new_key).c_str(),
		//          new_value, IndexKeyTypeToString(KeyAt(new_value_idx - 1)).c_str(), old_value);
		node_array_[new_value_idx].first = new_key;
		node_array_[new_value_idx].second = new_value;
		IncreaseSize(1);

		return GetSize();
	}

	void MoveHalfTo(BtreeInternalPage &recipient, BufferPool &bpm, table_oid_t table_oid) {
		idx_t start_split_indx = GetMinSize();
		idx_t original_size = GetSize();
		SetSize(start_split_indx);
		recipient.CopyNFrom(node_array_ + start_split_indx, original_size - start_split_indx, bpm, table_oid);
	}

	void CopyNFrom(InternalNode *items, idx_t size, BufferPool &bpm, table_oid_t table_oid) {
		std::copy(items, items + size, node_array_ + GetSize());

		// because the recipient got the child originally referred by the owner the recipient have to be set the parent
		// of those child leaf nodes
		for (idx_t i = 0; i < size; i++) {
			auto &node_wpg = bpm.FetchPageBasic({table_oid, ValueAt(i + GetSize())}).AsMut<BtreePage>();
			node_wpg.SetParentPageId(GetPageId());
		}

		IncreaseSize(size);
	}

private:
	static_assert(std::is_trivially_copyable_v<IndexKeyType>);
	static_assert(std::is_trivially_copyable_v<InternalValueType>);

	InternalNode node_array_[];
};
static_assert(sizeof(BtreeInternalPage) == INTERNAL_PAGE_HEADER_SIZE);
} // namespace db
