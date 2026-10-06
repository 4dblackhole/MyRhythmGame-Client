#include "FingerDrumAssets.h"
#include "Parsing/Submodules/ParserSupport.h"

#include <cstdio>
#include <cstdlib>
#include <new>
#include <sal.h>
#include <stdexcept>

namespace
{
// Only this single-threaded test executable replaces allocation. No asset pack is linked.
int allocationBudget = -1;
int deniedAllocations = 0;

class FailAllocationsAfter final
{
  public:
    explicit FailAllocationsAfter(int allowed) noexcept
    {
        deniedAllocations = 0;
        allocationBudget = allowed;
    }
    ~FailAllocationsAfter() { allocationBudget = -1; }
    FailAllocationsAfter(const FailAllocationsAfter &) = delete;
    FailAllocationsAfter &operator=(const FailAllocationsAfter &) = delete;
};

void Require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

void TestNumericAllocationFailure()
{
    const std::string number = "120." + std::string(64, '0');
    double value{};
    bool accepted{};
    {
        FailAllocationsAfter failure{0};
        accepted = finger_drum::chart::parsing::ParseDouble(number, value);
    }
    Require(!accepted && deniedAllocations != 0,
            "Numeric allocation failure must return false from the noexcept parser.");
}

void TestAssetErrorAllocationFailure()
{
    std::string error;
    Require(!finger_drum::assets::InitializeBuiltInAssets(error) && error.size() > 15,
            "The resource-free test must report missing assets before accessing the user's cache.");
    error.clear();
    error.shrink_to_fit();
    bool initialized{};
    {
        // Deny C++ allocations while the missing-resource error is reported.
        FailAllocationsAfter failure{0};
        initialized = finger_drum::assets::InitializeBuiltInAssets(error);
    }
    Require(!initialized && deniedAllocations != 0 && error.empty(),
            "An allocation failure while storing an asset error must return false with empty text.");
    Require(!finger_drum::assets::BuiltInAssetsInitialized(), "Failed initialization must not publish an asset root.");
}
} // namespace

_Ret_notnull_ _Post_writable_byte_size_(size) void *operator new(std::size_t size)
{
    if (allocationBudget == 0)
    {
        ++deniedAllocations;
        throw std::bad_alloc{};
    }
    if (allocationBudget > 0) --allocationBudget;
    if (void *memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc{};
}
_Ret_notnull_ _Post_writable_byte_size_(size) void *operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void *memory) noexcept { std::free(memory); }
void operator delete[](void *memory) noexcept { std::free(memory); }
void operator delete(void *memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void *memory, std::size_t) noexcept { std::free(memory); }

int main()
{
    try
    {
        TestNumericAllocationFailure();
        TestAssetErrorAllocationFailure();
        std::puts("Failure boundaries passed: numeric parsing and asset error reporting survive allocation failure.");
        return EXIT_SUCCESS;
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "Failure boundary test: %s\n", error.what());
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::fputs("Failure boundary test: unknown exception\n", stderr);
        return EXIT_FAILURE;
    }
}
