#include "storage/serializer/binary_deserializer.hpp"
#include "storage/serializer/binary_serializer.hpp"
#include "storage/serializer/deserializer.hpp"
#include "storage/serializer/memory_stream.hpp"

#include "gtest/gtest.h"
#include <vector>
namespace db {

struct Bar {
	uint32_t b;
  std::vector<std::string> vec;
	void Serialize(Serializer &serializer) const {
		serializer.WriteProperty<uint32_t>(1, "b", b);
    serializer.WritePropertyWithDefault(2, "vec", vec, std::vector<std::string>());
	}

	static std::unique_ptr<Bar> Deserialize(Deserializer &deserializer) {
		auto result = std::make_unique<Bar>();
		deserializer.ReadProperty<uint32_t>(1, "b", result->b);
    deserializer.ReadPropertyWithDefault(2, "vec", result->vec, std::vector<std::string>());
		return result;
	}
};

struct Foo {
	int32_t a;
	std::unique_ptr<Bar> bar;
	int32_t c;

	void Serialize(Serializer &serializer) const {
		serializer.WriteProperty<int32_t>(1, "a", a);
		serializer.WritePropertyWithDefault<std::unique_ptr<Bar>>(2, "bar", bar, std::unique_ptr<Bar>());
		serializer.WriteProperty<int32_t>(3, "c", c);
	}

	static std::unique_ptr<Foo> Deserialize(Deserializer &deserializer) {
		auto result = std::make_unique<Foo>();
		deserializer.ReadProperty<int32_t>(1, "a", result->a);
		deserializer.ReadPropertyWithDefault<std::unique_ptr<Bar>>(2, "bar", result->bar, std::unique_ptr<Bar>());
		deserializer.ReadProperty<int32_t>(3, "c", result->c);
		return result;
	}
};

TEST(StorageTest, SerializerTest) {
	Foo foo_in;
	foo_in.a = 42;
	foo_in.bar = std::make_unique<Bar>();
	foo_in.bar->b = 43;
  std::vector<std::string> vec_str = {"a", "b", "c", "d", "e"};
  foo_in.bar->vec = vec_str;
	foo_in.c = 44;
	MemoryStream stream;

	BinarySerializer::Serialize(foo_in, stream, false);
	auto pos1 = stream.GetPosition();
	// stream.Print();
	stream.Rewind();

	auto foo_out_ptr = BinaryDeserializer::Deserialize<Foo>(stream);
	auto &foo_out = *foo_out_ptr.get();
	EXPECT_EQ(foo_in.a, foo_out.a);
	EXPECT_EQ(foo_in.bar->b, foo_out.bar->b);
	EXPECT_EQ(foo_in.c, foo_out.c);
  for (size_t i = 0; i < vec_str.size(); i++) {
    EXPECT_EQ(vec_str[i], foo_out.bar->vec[i]);
  }

	foo_in.bar = nullptr;

	stream.Rewind();

	BinarySerializer::Serialize(foo_in, stream, false);
	auto pos2 = stream.GetPosition();
	stream.Rewind();

	foo_out_ptr = BinaryDeserializer::Deserialize<Foo>(stream);
	auto &foo_out2 = *foo_out_ptr.get();
	EXPECT_EQ(foo_in.a, foo_out2.a);
	EXPECT_TRUE(foo_in.bar == nullptr && foo_out2.bar == nullptr);
	EXPECT_EQ(foo_in.c, foo_out2.c);

	// should not write the default value
	EXPECT_TRUE(pos1 > pos2);
}
} // namespace db
