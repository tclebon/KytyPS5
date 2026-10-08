#include "common/abi.h"
#include "common/emulatorConfig.h"
#include "libs/errno.h"
#include "loader/symbolDatabase.h"

#include <array>
#include <cstdio>
#include <cstring>

namespace Libs {
void InitUserService_1(Loader::SymbolDatabase* symbols);
}

namespace {
int failures = 0;

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "UserServiceTests:%d: %s\n", __LINE__, #condition);               \
			++failures;                                                                            \
		}                                                                                          \
	} while (false)

using GetUserName = int(KYTY_SYSV_ABI*)(int, char*, size_t);
} // namespace

int main() {
	Config::Initialize();
	Config::ConfigOptions  options;
	Loader::SymbolDatabase symbols;
	Libs::InitUserService_1(&symbols);
	const auto* record = symbols.FindByNid("1xxcMiGu2fo", Loader::SymbolType::Func);
	CHECK(record != nullptr);
	if (record == nullptr) return 1;
	const auto get_name = reinterpret_cast<GetUserName>(record->vaddr);

	for (const char* name: {"tc", "venerabile", "1234567890123456"}) {
		options.user_name = name;
		Config::Load(options);
		const auto           length = options.user_name.size();
		std::array<char, 20> buffer;
		for (size_t size = 0; size <= length; ++size) {
			buffer.fill('#');
			CHECK(get_name(options.user_id, buffer.data(), size) ==
			      Libs::UserService::USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
			for (char byte: buffer)
				CHECK(byte == '#');
		}
		for (size_t size: {length + 1, buffer.size()}) {
			buffer.fill('#');
			CHECK(get_name(options.user_id, buffer.data(), size) == OK);
			CHECK(std::strcmp(buffer.data(), name) == 0);
			CHECK(buffer[length + 1] == '#');
		}
		CHECK(get_name(options.user_id, nullptr, length + 1) ==
		      Libs::UserService::USER_SERVICE_ERROR_INVALID_ARGUMENT);
		buffer.fill('#');
		CHECK(get_name(options.user_id + 1, buffer.data(), buffer.size()) ==
		      Libs::UserService::USER_SERVICE_ERROR_NOT_LOGGED_IN);
		CHECK(buffer[0] == '#');
	}
	Config::Shutdown();
	return failures == 0 ? 0 : 1;
}
