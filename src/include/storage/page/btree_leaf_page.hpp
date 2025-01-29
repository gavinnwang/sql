#pragma once

#include "common/exception.hpp"
#include "common/logger.hpp"
#include "common/typedef.hpp"
#include "index/index.hpp"
#include "storage/page/btree_page.hpp"

#include <algorithm>
#include <optional>
namespace db {
static constexpr int LEAF_PAGE_HEADER_SIZE = 40;

using LeafNode = std::pair<IndexKeyType, IndexValueType>;
// static constexpr int LEAF_MAX_NODE_SIZE = 30;
static constexpr int LEAF_MAX_NODE_SIZE = (PAGE_SIZE - LEAF_PAGE_HEADER_SIZE) / (sizeof(LeafNode));

class BtreeLeafPage : public BtreePage {
	static_assert(sizeof(LeafNode) == 20);
	static_assert(sizeof(IndexKeyType) == 8);
	static_assert(sizeof(IndexValueType) == 12);

public:
	// delete all 5
	BtreeLeafPage() = delete;
	BtreeLeafPage(const BtreeLeafPage &other) = delete;
	BtreeLeafPage &operator=(const BtreeLeafPage &other) = delete;
	BtreeLeafPage(BtreeLeafPage &&other) = delete;
	BtreeLeafPage &operator=(BtreeLeafPage &&other) = delete;
	~BtreeLeafPage() = delete;

	void Init(page_id_t page_id, page_id_t parent_id) {
		SetPageType(IndexPageType::LEAF_PAGE);
		SetSize(0);
		SetPageId(page_id);
		SetParentPageId(parent_id);
		SetNextPageId(INVALID_PAGE_ID);
		SetMaxSize(LEAF_MAX_NODE_SIZE);
		LOG_TRACE("Setting size to 0 and max size to %d", static_cast<int>(GetMaxSize()));
		assert(GetMaxSize() > 0);
	}

	[[nodiscard]] page_id_t GetNextPageId() const {
		return next_page_id_;
	}
	void SetNextPageId(page_id_t next_page_id) {
		next_page_id_ = next_page_id;
	}
	[[nodiscard]] IndexKeyType KeyAt(int index) const {
		return node_array_[index].first;
	}
	void Insert(const IndexKeyType &key, const IndexValueType &value, const Comparator &comparator) {
		assert(GetMaxSize() > 0);
		if (GetSize() == GetMaxSize()) {
			throw RuntimeException("Leaf node is full, shouldve split bruh");
		}

		auto key_idx = FindKeyIndex(key, comparator);

		// if size is 0 then no way already exist
		if (GetSize() && comparator(node_array_[key_idx].first, key) == 0) {
			LOG_TRACE("Key %s already exists%s", IndexKeyTypeToString(key).c_str(), value.ToString().c_str());
			// todo(gavinnwang): update the value of the key?
			return;
		}
		LOG_TRACE("Inserting key %s at index %d with val: %s", IndexKeyTypeToString(key).c_str(),
		          static_cast<int>(key_idx), value.ToString().c_str());

		if (key_idx == GetSize()) {
			*(node_array_ + key_idx) = {key, value};
		} else {
			// shift everything at and after the key idx back one to make space
			std::move_backward(node_array_ + key_idx, node_array_ + GetSize(), node_array_ + GetSize() + 1);
			*(node_array_ + key_idx) = {key, value};
		}
		IncreaseSize(1);
	}
	[[nodiscard]] idx_t FindKeyIndex(const IndexKeyType &key, const Comparator &comparator) const {
		assert(GetMaxSize() > 0);
		auto target_idx =
		    std::lower_bound(node_array_, node_array_ + GetSize(), key,
		                     [&comparator](const auto &pair, auto k) { return comparator(pair.first, k) < 0; });
		return std::distance(node_array_, target_idx);
	}
	[[nodiscard]] std::optional<IndexValueType> Lookup(const IndexKeyType &key, const Comparator &comparator) const {

		idx_t target_index = FindKeyIndex(key, comparator);

		auto leaf_key = node_array_[target_index].first;
		LOG_TRACE("comparing to leaf key %s at index %d", IndexKeyTypeToString(leaf_key).c_str(),
		          static_cast<int>(target_index));

		if (target_index < GetSize() && leaf_key == key) {
			LOG_TRACE("Key %s index is %d", IndexKeyTypeToString(key).c_str(), static_cast<int>(target_index));
			return node_array_[target_index].second;
		}
		LOG_TRACE("Key with looked up index %d not found", static_cast<int>(target_index));

		return std::nullopt;
	}
	void MoveHalfTo(BtreeLeafPage &recipient) {
		assert(GetMaxSize() > 0);
		idx_t start_split_indx = GetMinSize();
		SetSize(start_split_indx);
		recipient.CopyNFrom(node_array_ + start_split_indx, GetMaxSize() - start_split_indx);
	}
	void CopyNFrom(LeafNode *items, idx_t size) {
		assert(GetMaxSize() > 0);
		std::copy(items, items + size, node_array_ + GetSize());
		IncreaseSize(size);
	}

	[[nodiscard]] IndexValueType ValueAt(int index) const {
		return node_array_[index].second;
	}

	[[nodiscard]] std::string ToString() const {

		std::string result = "\n(";
		bool first = true;
		for (idx_t i = 0; i < GetSize(); i++) {
			if (!first) {
				result += ", ";
			}
			first = false;
			result += "(" + IndexKeyTypeToString(KeyAt(i));
			result += ":";
			result += ValueAt(i).ToString() + ")";
			if (i % 5 == 4) {
				result += "\n";
			}
		}
		result += ")\n";
		return result;
	}

private:
	page_id_t next_page_id_ {INVALID_PAGE_ID};
	LeafNode node_array_[];
};

static_assert(sizeof(BtreeLeafPage) == LEAF_PAGE_HEADER_SIZE);

} // namespace db
