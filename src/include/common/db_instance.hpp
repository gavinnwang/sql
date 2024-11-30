#pragma once

#include "binder/statement/create_statement.hpp"
#include "buffer/buffer_pool_manager.hpp"
#include "catalog/catalog_manager.hpp"
#include "concurrency/transaction.hpp"

#include <memory>
#include <string>
namespace db {

class DB {
public:
	explicit DB(const std::string &db_file_name);
	~DB();

	void HandleCreateStatement(Transaction &txn, const CreateStatement &stmt);

private:
	void SetUpInternalSystemCatalogTable();

	std::unique_ptr<BufferPoolManager> bpm_;
	std::unique_ptr<CatalogManager> catalog_manager_;
	/** Lock for CatalogManager */
	std::shared_mutex catalog_manager_lock_;
};

}; // namespace db
