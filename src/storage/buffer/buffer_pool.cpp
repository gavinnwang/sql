
#include "storage/buffer/buffer_pool.hpp"

#include "common/config.hpp"
#include "common/logger.hpp"
#include "storage/buffer/random_replacer.h"
#include "storage/page/page_guard.hpp"
#include "storage/page_allocator.hpp"

#include <cassert>
#include <memory>
#include <mutex>
#include <vector>
namespace db {
BufferPool::BufferPool(frame_id_t pool_size, DiskManager &disk_manager)
    : pool_size_(pool_size), replacer_(std::make_unique<RandomBogoReplacer>()), disk_manager_(disk_manager),
      pages_(pool_size) {
	for (frame_id_t i = 0; i < pool_size_; ++i) {
		free_list_.emplace_back(i);
	}
}

bool BufferPool::AllocateFrame(frame_id_t &frame_id) {
	if (free_list_.empty()) {
		// gotta evict a random frame because
		if (!replacer_->Evict(frame_id)) {
			replacer_->Print();
			return false;
		}
		assert(pages_[frame_id].pin_count_ == 0);
		assert(pages_[frame_id].page_id_.page_number_ >= 0);
		if (pages_[frame_id].is_dirty_) {
			auto &evict_page = pages_[frame_id];
			disk_manager_.WritePage(evict_page.GetPageId(), evict_page.GetData());
		}
		pages_[frame_id].ResetMemory();
		return true;
	}
	frame_id = free_list_.front();
	free_list_.pop_front();
	return true;
}

Page &BufferPool::NewPage(PageAllocator &page_allocator, PageId &page_id) {
	std::lock_guard<std::mutex> lock(latch_);
	frame_id_t frame_id = -1;
	if (!AllocateFrame(frame_id)) {
		throw std::runtime_error("Failed to allocate frame");
		// return nullptr;
	}
	// assert that frame id is not in the free list
	assert(std::find(free_list_.begin(), free_list_.end(), frame_id) == free_list_.end() &&
	       "frame id should not be in the free list");
	// assert that frame id is valid
	assert(frame_id != -1 && "frame id has to be assigned a valid value here");

	replacer_->Pin(frame_id);

	// get rid fo the stale page table record
	auto page_id_to_replace = pages_[frame_id].page_id_;
	page_table_.erase(page_id_to_replace);

	// assign passed in page_id to the new page
	// page_id = AllocatePage(page_id.table_id_);
	page_id = page_allocator.AllocatePage();
	page_table_[page_id] = frame_id;
	assert(page_table_.at(page_id) == frame_id && "page table should have the new page id");
	assert(page_table_.contains(page_id_to_replace) == false && "page table should not have the old page id");
	// reset the memory and metadata for the new page
	Page &page = pages_[frame_id];
	page.page_id_ = page_id;
	page.pin_count_ = 1;
	page.is_dirty_ = false;
	page.ResetMemory();

	return page;
}

Page &BufferPool::FetchPage(PageId page_id) {
	assert(page_id.page_number_ != INVALID_PAGE_ID && "page number should be valid");
	std::lock_guard<std::mutex> lock(latch_);
	if (page_table_.find(page_id) != page_table_.end()) {
		frame_id_t frame_id = page_table_[page_id];
		Page &page = pages_[frame_id];
		page.pin_count_++;
		replacer_->Pin(frame_id);
		return page;
	}

	frame_id_t frame_id = -1;
	if (!AllocateFrame(frame_id)) {
		throw std::runtime_error("Failed to allocate frame");
		// return nullptr;
	}
	assert(frame_id != -1 && "frame id has to be assigned a valid value here");

	page_table_.erase(pages_[frame_id].page_id_);
	page_table_.insert({page_id, frame_id});

	Page &page = pages_[frame_id];
	page.page_id_ = page_id;
	page.pin_count_++;
	page.is_dirty_ = false;
	disk_manager_.ReadPage(page_id, page.GetData());

	return page;
}

bool BufferPool::UnpinPage(PageId page_id, bool is_dirty) {
	assert(page_id.page_number_ != INVALID_PAGE_ID);
	std::lock_guard<std::mutex> lock(latch_);
	if (page_table_.find(page_id) == page_table_.end()) {
		LOG_ERROR("Page %s not found in page table", page_id.ToString().c_str());
		assert(false);
		return false;
	}
	frame_id_t frame_id = page_table_[page_id];
	Page &page = pages_[frame_id];
	if (is_dirty) {
		page.is_dirty_ = true;
	}
	if (page.pin_count_ == 0) {
		return false;
	}
	page.pin_count_--;
	assert(page.pin_count_ >= 0);
	if (page.pin_count_ == 0) {
		replacer_->Unpin(frame_id);
	}
	return true;
}

bool BufferPool::FlushPage(PageId page_id) {
	std::lock_guard<std::mutex> lock(latch_);
	if (page_table_.find(page_id) == page_table_.end()) {
		return false;
	}
	frame_id_t frame_id = page_table_[page_id];
	Page &page = pages_[frame_id];
	disk_manager_.WritePage(page_id, page.GetData());
	page.is_dirty_ = false;
	return true;
}

void BufferPool::FlushAllPages() {
	for (frame_id_t i = 0; i < pool_size_; i++) {
		FlushPage(pages_[i].page_id_);
	}
}

bool BufferPool::DeletePage(PageId page_id) {
	std::lock_guard<std::mutex> lock(latch_);
	if (page_table_.find(page_id) == page_table_.end()) {
		return true;
	}
	frame_id_t frame_id = page_table_[page_id];
	Page &page = pages_[frame_id];
	if (page.pin_count_ > 0) {
		return false;
	}
	page_table_.erase(page_id);
	free_list_.push_back(frame_id);

	pages_[frame_id].ResetMemory();
	page.page_id_.page_number_ = INVALID_PAGE_ID;
	page.pin_count_ = 0;
	page.is_dirty_ = false;
	return true;
}

BasicPageGuard BufferPool::FetchPageBasic(PageId page_id) {
	auto &page = FetchPage(page_id);
	return {*this, page};
}

ReadPageGuard BufferPool::FetchPageRead(PageId page_id) {
	auto &page = FetchPage(page_id);
	page.RLatch();
	return {*this, page};
}

WritePageGuard BufferPool::FetchPageWrite(PageId page_id) {
	auto &page = FetchPage(page_id);
	page.WLatch();
	return {*this, page};
}

BasicPageGuard BufferPool::NewPageGuarded(PageAllocator &page_allocator, PageId &page_id) {
	auto &page = NewPage(page_allocator, page_id);
	return {*this, page};
}
} // namespace db
