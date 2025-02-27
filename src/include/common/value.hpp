#pragma once

#include "common/arithmetic_type.hpp"
#include "common/config.hpp"
#include "common/exception.hpp"
#include "common/logger.hpp"
#include "common/type.hpp"
#include "index/index_typdef.hpp"
#include "storage/serializer/deserializer.hpp"
#include "storage/serializer/serializer.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <utility>
#include <variant>

namespace db {
template <typename T>
bool ValueIsCorrectType(TypeId type) {
	switch (type) {
	case TypeId::BOOLEAN:
		return typeid(T) == typeid(uint8_t);
	case TypeId::INTEGER:
		return typeid(T) == typeid(int32_t);
	case TypeId::TIMESTAMP:
		return typeid(T) == typeid(uint64_t);
	case TypeId::VARCHAR:
		return typeid(T) == typeid(std::string);
	default:
		return false;
	}
}

class Value {
public:
	// create an value with type_id
	explicit Value(TypeId type) : type_id_(type) {
	}

	template <typename T>
	Value(TypeId type, T &&value) : type_id_(type), value_(std::forward<T>(value)) {
		if (!ValueIsCorrectType<T>(type)) {
			LOG_ERROR("Value isn't assigned to the correct type id, expected {}, got {}", Type::TypeIdToString(type),
			          typeid(T).name());
			throw RuntimeException("Value isn't assigned to the correct type id");
		}
	};

	// Get the length of the variable length data
	[[nodiscard]] inline uint32_t GetStorageSize() const {
		return Type::TypeSize(type_id_, GetVarlenStorageSize());
	}

	[[nodiscard]] inline TypeId GetTypeId() const {
		return type_id_;
	}

	[[nodiscard]] inline bool IsNull() const {
		return is_null_;
	}

	void SerializeTo(data_ptr_t storage) const;
	[[nodiscard]] std::string ToString() const;

	void Serialize(Serializer &serializer) const;
	static Value Deserialize(Deserializer &deserializer);

	[[nodiscard]] static Value DeserializeFromWithTypeInfo(const_data_ptr_t storage) {
		auto type_id = *reinterpret_cast<const TypeId *>(storage);
		return DeserializeFrom(storage + sizeof(TypeId), type_id);
	}

	void SerializeToWithTypeInfo(data_ptr_t storage) const {
		*reinterpret_cast<TypeId *>(storage) = type_id_;
		SerializeTo(storage + sizeof(TypeId));
	}

	[[nodiscard]] static Value DeserializeFrom(const_data_ptr_t storage, const TypeId type_id) {
		switch (type_id) {
		case TypeId::BOOLEAN: {
			int8_t val = *reinterpret_cast<const int8_t *>(storage);
			return {type_id, val};
		}
		case TypeId::INTEGER: {
			int32_t val = *reinterpret_cast<const int32_t *>(storage);
			return {type_id, val};
		}
		case TypeId::TIMESTAMP: {
			uint64_t val = *reinterpret_cast<const uint32_t *>(storage);
			return {type_id, val};
		}
		case TypeId::VARCHAR: {
			uint32_t var_len = *reinterpret_cast<const uint32_t *>(storage);
			assert(var_len < PAGE_SIZE && "Invalid varchar length");
			return {type_id, std::string(reinterpret_cast<const char *>(storage) + sizeof(uint32_t), var_len)};
		}
		case TypeId::INVALID: {
			throw RuntimeException("Invalid type");
		}
		}
		std::unreachable();
	}

	[[nodiscard]] IndexKeyType ConvertToIndexKeyType() const {
		IndexKeyType ret = {0};
		switch (type_id_) {
		case TypeId::BOOLEAN: {
			memcpy(&ret, &value_, sizeof(int8_t));
			return ret;
		}
		case TypeId::INTEGER: {
			memcpy(&ret, &value_, sizeof(int32_t));
			return ret;
		}
		case TypeId::TIMESTAMP: {
			memcpy(&ret, &value_, sizeof(uint32_t));
			return ret;
		}
		case TypeId::VARCHAR: {
			memcpy(&ret, &value_, sizeof(IndexKeyType) - 1);
			size_t var_len = std::get<std::string>(value_).size();
			ret[std::min(var_len, sizeof(IndexKeyType) - 1)] = '\0';
			return ret;
		}
		case TypeId::INVALID: {
			throw RuntimeException("Invalid type");
		}
		}
		std::unreachable();
	}

	template <typename T>
	[[nodiscard]] T GetAs() const {
		return std::get<T>(value_);
	}

#define HANDLE_ARITHMETIC_CASE(type, cpp_type, op)                                                                     \
	case TypeId::type:                                                                                                 \
		value_ = std::get<cpp_type>(value_) op other.GetAs<cpp_type>();                                                \
		return

#define HANDLE_UNREACHABLE_CASE(type)                                                                                  \
	case TypeId::type:                                                                                                 \
		throw RuntimeException(#type " arithmetic not supported");                                                     \
		return

	void ComputeArithmetic(const Value &other, ArithmeticType expression_type) {
		switch (expression_type) {
		case ArithmeticType::Plus:
			switch (type_id_) {
				HANDLE_UNREACHABLE_CASE(BOOLEAN);
				HANDLE_ARITHMETIC_CASE(INTEGER, int32_t, +);
				HANDLE_ARITHMETIC_CASE(TIMESTAMP, uint64_t, +);
				HANDLE_UNREACHABLE_CASE(VARCHAR);
				HANDLE_UNREACHABLE_CASE(INVALID);
			}
			break;
		case ArithmeticType::Minus:
			switch (type_id_) {
				HANDLE_UNREACHABLE_CASE(BOOLEAN);
				HANDLE_ARITHMETIC_CASE(INTEGER, int32_t, -);
				HANDLE_ARITHMETIC_CASE(TIMESTAMP, uint64_t, -);
				HANDLE_UNREACHABLE_CASE(VARCHAR);
				HANDLE_UNREACHABLE_CASE(INVALID);
			}
			break;
		case ArithmeticType::Multiply:
			switch (type_id_) {
				HANDLE_UNREACHABLE_CASE(BOOLEAN);
				HANDLE_ARITHMETIC_CASE(INTEGER, int32_t, *);
				HANDLE_UNREACHABLE_CASE(TIMESTAMP);
				HANDLE_UNREACHABLE_CASE(VARCHAR);
				HANDLE_UNREACHABLE_CASE(INVALID);
			}
			break;
		}
		std::unreachable();
	}

private:
	[[nodiscard]] uint32_t GetVarlenStorageSize() const {
		if (type_id_ != TypeId::VARCHAR) {
			return 0;
		}
		return std::get<std::string>(value_).size();
	}
	TypeId type_id_;

	// Define the variant to hold all possible types
	using Val = std::variant<int8_t, int32_t, uint64_t, std::string>;

	Val value_;
	bool is_null_ {};
};
} // namespace db
