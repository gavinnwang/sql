#pragma once

#include "query/expressions/abstract_expression.hpp"
#include "query/plans/abstract_plan.hpp"
namespace db {

/** The SeqScanPlanNode represents a sequential table scan operation. */
class SeqScanPlanNode : public AbstractPlanNode {
public:
	SeqScanPlanNode(SchemaRef output, table_oid_t table_oid, std::string table_name,
	                AbstractExpressionRef filter_predicate)
	    : AbstractPlanNode(std::move(output)), table_oid_ {table_oid}, table_name_(std::move(table_name)),
	      filter_predicate_(std::move(filter_predicate)) {
	}

	[[nodiscard]] PlanType GetType() const override {
		return PlanType::SeqScan;
	}

	[[nodiscard]] table_oid_t GetTableOid() const {
		return table_oid_;
	}

	[[nodiscard]] std::string ToString() const override {
		if (filter_predicate_) {
			return fmt::format("SeqScan {{ table={}, filter={} }}", table_name_, filter_predicate_);
		}
		return fmt::format("SeqScan {{ table={} }}", table_name_);
	}

	// The table whose tuples should be scanned
	table_oid_t table_oid_;

	// The table name
	std::string table_name_;

	AbstractExpressionRef filter_predicate_;
};

} // namespace db
