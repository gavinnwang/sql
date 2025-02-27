#pragma once
#include "query/binder/statement/statement_type.hpp"

#include <string>

namespace db {

class BoundStatement {
public:
	explicit BoundStatement(StatementType type) : type_(type) {};
	virtual ~BoundStatement() = default;

	/** The statement type. */
	const StatementType type_;

	[[nodiscard]] virtual std::string ToString() const = 0;
};
} // namespace db
