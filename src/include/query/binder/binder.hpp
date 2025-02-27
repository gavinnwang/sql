#pragma once

#include "query/binder/statement/bound_statement.hpp"
#include "query/binder/statement/create_statement.hpp"
#include "query/binder/statement/insert_statement.hpp"
#include "query/binder/statement/select_statement.hpp"
#include "query/binder/table_ref/bound_expression_list.hpp"
#include "meta/catalog.hpp"
#include "meta/column.hpp"
#include "sql/CreateStatement.h"
#include "sql/InsertStatement.h"
#include "sql/SQLStatement.h"

#include <memory>

namespace db {
class Binder {
public:
	explicit Binder(const Catalog &catalog) : catalog_(catalog) {
	}
	std::unique_ptr<BoundStatement> Bind(const hsql::SQLStatement *stmt);
	std::unique_ptr<CreateStatement> BindCreate(const hsql::CreateStatement *stmt);
	std::unique_ptr<SelectStatement> BindSelect(const hsql::SelectStatement *stmt);
	std::unique_ptr<InsertStatement> BindInsert(const hsql::InsertStatement *stmt);
	std::unique_ptr<BoundTableRef> BindFrom(const hsql::TableRef *table_ref);
	std::unique_ptr<BoundExpressionListRef> BindValuesList(const std::vector<hsql::Expr *> &list);
	std::vector<std::unique_ptr<BoundExpression>> BindExpressionList(const std::vector<hsql::Expr *> &list);
	std::unique_ptr<BoundExpression> BindExpression(const hsql::Expr *expr);
	Column BindColumnDefinition(const hsql::ColumnDefinition *col_def) const;
	std::unique_ptr<BoundBaseTableRef> BindBaseTableRef(const std::string &table_name);

private:
	const Catalog &catalog_;
	std::unique_ptr<BoundTableRef> *scope_ {nullptr};
};
} // namespace db
