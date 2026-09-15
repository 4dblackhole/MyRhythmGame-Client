#include "FingerDrumAssets.h"

#include <Windows.h>
#include <ShlObj.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <mutex>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Shell32.lib")

namespace finger_drum::assets
{
    namespace
    {
        constexpr std::string_view PackMagic = "FDRPAK01";
        constexpr std::uint32_t PackVersion = 1;
        constexpr std::size_t MaximumEntryCount = 16'384;
        constexpr std::size_t MaximumPathBytes = 32'768;
        constexpr std::wstring_view DefaultSkinRelative =
            L"assets\\skins\\Default Skin";
        constexpr std::wstring_view SongsRelative = L"assets\\songs";

        struct PackEntry
        {
            std::filesystem::path relativePath;
            std::uint64_t offset{};
            std::uint64_t size{};
        };

        std::mutex assetMutex;
        std::filesystem::path installedRoot;

        class ScopedMutex final
        {
        public:
            explicit ScopedMutex(const wchar_t* name)
            {
                handle_ = CreateMutexW(nullptr, FALSE, name);
                if (handle_ == nullptr)
                {
                    throw std::runtime_error(
                        "Unable to create the built-in asset installation lock.");
                }
                const DWORD wait = WaitForSingleObject(handle_, INFINITE);
                if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED)
                {
                    CloseHandle(handle_);
                    handle_ = nullptr;
                    throw std::runtime_error(
                        "Unable to acquire the built-in asset installation lock.");
                }
                ownsMutex_ = true;
            }

            ~ScopedMutex()
            {
                if (ownsMutex_)
                {
                    ReleaseMutex(handle_);
                }
                if (handle_ != nullptr)
                {
                    CloseHandle(handle_);
                }
            }

            ScopedMutex(const ScopedMutex&) = delete;
            ScopedMutex& operator=(const ScopedMutex&) = delete;

        private:
            HANDLE handle_{};
            bool ownsMutex_{};
        };

        [[nodiscard]] std::filesystem::path ExecutableDirectory()
        {
            std::wstring buffer(512, L'\0');
            while (true)
            {
                const DWORD length = GetModuleFileNameW(
                    nullptr,
                    buffer.data(),
                    static_cast<DWORD>(buffer.size()));
                if (length == 0)
                {
                    throw std::runtime_error(
                        "Unable to determine the executable path.");
                }
                if (static_cast<std::size_t>(length) < buffer.size() - 1)
                {
                    buffer.resize(length);
                    return std::filesystem::path(buffer).parent_path();
                }
                if (buffer.size() >= 32'768)
                {
                    throw std::runtime_error("The executable path is too long.");
                }
                buffer.resize(buffer.size() * 2);
            }
        }

        [[nodiscard]] std::filesystem::path LocalApplicationDataDirectory()
        {
            PWSTR value = nullptr;
            const HRESULT result = SHGetKnownFolderPath(
                FOLDERID_LocalAppData,
                KF_FLAG_CREATE,
                nullptr,
                &value);
            if (FAILED(result) || value == nullptr)
            {
                if (value != nullptr)
                {
                    CoTaskMemFree(value);
                }
                throw std::runtime_error(
                    "Unable to locate the local application-data directory.");
            }
            std::filesystem::path path(value);
            CoTaskMemFree(value);
            return path;
        }

        [[nodiscard]] std::span<const std::byte> EmbeddedPack()
        {
            HMODULE module = GetModuleHandleW(nullptr);
            HRSRC resource = FindResourceW(
                module,
                MAKEINTRESOURCEW(BuiltInAssetResourceId),
                RT_RCDATA);
            if (resource == nullptr)
            {
                throw std::runtime_error(
                    "The built-in asset RCDATA resource is missing.");
            }
            HGLOBAL loaded = LoadResource(module, resource);
            const DWORD size = SizeofResource(module, resource);
            const void* data = loaded != nullptr ? LockResource(loaded) : nullptr;
            if (data == nullptr || size == 0)
            {
                throw std::runtime_error(
                    "The built-in asset RCDATA resource is empty.");
            }
            return {
                static_cast<const std::byte*>(data),
                static_cast<std::size_t>(size)};
        }

        template <typename Integer>
        [[nodiscard]] Integer ReadUnsigned(
            const std::span<const std::byte> bytes,
            std::size_t& cursor)
        {
            static_assert(std::is_unsigned_v<Integer>);
            if (cursor > bytes.size() ||
                bytes.size() - cursor < sizeof(Integer))
            {
                throw std::runtime_error(
                    "The built-in asset pack header is truncated.");
            }
            Integer value{};
            for (std::size_t index = 0; index < sizeof(Integer); ++index)
            {
                value |= static_cast<Integer>(
                    std::to_integer<unsigned char>(bytes[cursor + index]))
                    << (index * 8);
            }
            cursor += sizeof(Integer);
            return value;
        }

        [[nodiscard]] std::wstring Utf8ToWide(const std::string_view value)
        {
            if (value.empty())
            {
                return {};
            }
            const int length = MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                value.data(),
                static_cast<int>(value.size()),
                nullptr,
                0);
            if (length <= 0)
            {
                throw std::runtime_error(
                    "The built-in asset pack contains an invalid UTF-8 path.");
            }
            std::wstring result(static_cast<std::size_t>(length), L'\0');
            MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                value.data(),
                static_cast<int>(value.size()),
                result.data(),
                length);
            return result;
        }

        void ValidateRelativePath(const std::filesystem::path& path)
        {
            if (path.empty() || path.is_absolute() || path.has_root_path())
            {
                throw std::runtime_error(
                    "A built-in asset path must be relative.");
            }
            for (const std::filesystem::path& component : path)
            {
                if (component.empty() || component == L"." ||
                    component == L"..")
                {
                    throw std::runtime_error(
                        "A built-in asset path escapes its asset root.");
                }
            }
        }

        [[nodiscard]] std::vector<PackEntry> ParsePack(
            const std::span<const std::byte> bytes)
        {
            if (bytes.size() < PackMagic.size() ||
                !std::equal(
                    PackMagic.begin(),
                    PackMagic.end(),
                    bytes.begin(),
                    [](const char expected, const std::byte actual)
                    {
                        return static_cast<unsigned char>(expected) ==
                            std::to_integer<unsigned char>(actual);
                    }))
            {
                throw std::runtime_error(
                    "The built-in asset pack has an invalid signature.");
            }
            std::size_t cursor = PackMagic.size();
            if (ReadUnsigned<std::uint32_t>(bytes, cursor) != PackVersion)
            {
                throw std::runtime_error(
                    "The built-in asset pack version is unsupported.");
            }
            const std::uint32_t count =
                ReadUnsigned<std::uint32_t>(bytes, cursor);
            if (count == 0 || count > MaximumEntryCount)
            {
                throw std::runtime_error(
                    "The built-in asset pack entry count is invalid.");
            }

            std::vector<PackEntry> entries;
            entries.reserve(count);
            for (std::uint32_t index = 0; index < count; ++index)
            {
                const std::uint32_t pathSize =
                    ReadUnsigned<std::uint32_t>(bytes, cursor);
                if (pathSize == 0 || pathSize > MaximumPathBytes ||
                    cursor > bytes.size() || bytes.size() - cursor < pathSize)
                {
                    throw std::runtime_error(
                        "The built-in asset pack path is invalid.");
                }
                const std::string_view utf8Path(
                    reinterpret_cast<const char*>(bytes.data() + cursor),
                    pathSize);
                cursor += pathSize;
                PackEntry entry;
                entry.relativePath = std::filesystem::path(
                    Utf8ToWide(utf8Path));
                ValidateRelativePath(entry.relativePath);
                entry.offset = ReadUnsigned<std::uint64_t>(bytes, cursor);
                entry.size = ReadUnsigned<std::uint64_t>(bytes, cursor);
                if (entry.offset > bytes.size() ||
                    entry.size > bytes.size() - entry.offset)
                {
                    throw std::runtime_error(
                        "The built-in asset pack data range is invalid.");
                }
                entries.push_back(std::move(entry));
            }
            return entries;
        }

        [[nodiscard]] std::string PackHash(
            const std::span<const std::byte> bytes)
        {
            std::uint64_t hash = 14695981039346656037ULL;
            for (const std::byte value : bytes)
            {
                hash ^= std::to_integer<unsigned char>(value);
                hash *= 1099511628211ULL;
            }
            std::ostringstream stream;
            stream << std::hex << std::setfill('0') << std::setw(16) << hash;
            return stream.str();
        }

        [[nodiscard]] bool InstallationIsComplete(
            const std::filesystem::path& root,
            const std::string_view hash,
            const std::vector<PackEntry>& entries)
        {
            std::ifstream marker(root / ".complete", std::ios::binary);
            std::string markerHash;
            if (!marker || !std::getline(marker, markerHash) ||
                markerHash != hash)
            {
                return false;
            }
            for (const PackEntry& entry : entries)
            {
                const std::filesystem::path target = root / entry.relativePath;
                std::error_code error;
                if (!std::filesystem::is_regular_file(target, error) || error ||
                    std::filesystem::file_size(target, error) != entry.size ||
                    error)
                {
                    return false;
                }
            }
            return true;
        }

        void WriteEntry(
            const std::filesystem::path& target,
            const std::span<const std::byte> bytes,
            const PackEntry& entry)
        {
            std::filesystem::create_directories(target.parent_path());
            std::ofstream stream(target, std::ios::binary | std::ios::trunc);
            if (!stream)
            {
                throw std::runtime_error(
                    "Unable to create a built-in asset cache file.");
            }
            if (entry.size >
                static_cast<std::uint64_t>(
                    std::numeric_limits<std::streamsize>::max()))
            {
                throw std::runtime_error(
                    "A built-in asset is too large to extract.");
            }
            stream.write(
                reinterpret_cast<const char*>(bytes.data() + entry.offset),
                static_cast<std::streamsize>(entry.size));
            if (!stream)
            {
                throw std::runtime_error(
                    "Unable to write a built-in asset cache file.");
            }
        }

        [[nodiscard]] std::filesystem::path InstallPack(
            const std::span<const std::byte> bytes,
            const std::vector<PackEntry>& entries,
            const std::string& hash)
        {
            const std::filesystem::path cache =
                LocalApplicationDataDirectory() /
                L"FingerDrum" / L"BuiltInAssets";
            const std::filesystem::path destination = cache / hash;

            const ScopedMutex installationMutex(
                L"Local\\FingerDrum.BuiltInAssets.Install");

            if (InstallationIsComplete(destination, hash, entries))
            {
                return destination;
            }

            const std::filesystem::path staging = cache /
                (hash + ".tmp." + std::to_string(GetCurrentProcessId()));
            try
            {
                std::filesystem::create_directories(cache);
                std::filesystem::remove_all(staging);
                std::filesystem::create_directories(staging);
                for (const PackEntry& entry : entries)
                {
                    WriteEntry(staging / entry.relativePath, bytes, entry);
                }
                {
                    std::ofstream marker(
                        staging / ".complete",
                        std::ios::binary | std::ios::trunc);
                    marker << hash << '\n';
                    if (!marker)
                    {
                        throw std::runtime_error(
                            "Unable to finalize the built-in asset cache.");
                    }
                }
                std::filesystem::remove_all(destination);
                std::filesystem::rename(staging, destination);
            }
            catch (...)
            {
                std::error_code ignored;
                std::filesystem::remove_all(staging, ignored);
                throw;
            }
            return destination;
        }

        [[nodiscard]] std::filesystem::path RequireBuiltInRoot()
        {
            std::scoped_lock lock(assetMutex);
            if (installedRoot.empty())
            {
                throw std::logic_error(
                    "Built-in assets have not been initialized.");
            }
            return installedRoot;
        }

        [[nodiscard]] std::filesystem::path ResolveBuiltInFile(
            const std::filesystem::path& relativePath)
        {
            ValidateRelativePath(relativePath);
            const std::filesystem::path path =
                RequireBuiltInRoot() / relativePath;
            std::error_code error;
            if (!std::filesystem::is_regular_file(path, error) || error)
            {
                throw std::runtime_error(
                    "A required built-in asset is missing from the cache.");
            }
            return path;
        }
    }

    bool InitializeBuiltInAssets(std::string& errorMessage) noexcept
    {
        try
        {
            std::scoped_lock lock(assetMutex);
            if (!installedRoot.empty())
            {
                errorMessage.clear();
                return true;
            }
            const std::span<const std::byte> bytes = EmbeddedPack();
            const std::vector<PackEntry> entries = ParsePack(bytes);
            const std::string hash = PackHash(bytes);
            installedRoot = InstallPack(bytes, entries, hash);
            errorMessage.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            errorMessage = exception.what();
            return false;
        }
    }

    bool BuiltInAssetsInitialized() noexcept
    {
        std::scoped_lock lock(assetMutex);
        return !installedRoot.empty();
    }

    std::filesystem::path BuiltInAssetRoot()
    {
        return RequireBuiltInRoot();
    }

    std::filesystem::path BuiltInSongsRoot()
    {
        return RequireBuiltInRoot() / std::filesystem::path(SongsRelative);
    }

    std::filesystem::path UserSongsRoot()
    {
        return ExecutableDirectory() / std::filesystem::path(SongsRelative);
    }

    std::filesystem::path ResolveBuiltInAsset(
        const std::filesystem::path& relativePath)
    {
        return ResolveBuiltInFile(relativePath);
    }

    std::filesystem::path ResolveSkinAsset(
        const std::filesystem::path& preferredSkinRoot,
        const std::filesystem::path& relativePath)
    {
        ValidateRelativePath(relativePath);
        if (!preferredSkinRoot.empty())
        {
            const std::filesystem::path preferred =
                preferredSkinRoot / relativePath;
            std::error_code error;
            if (std::filesystem::is_regular_file(preferred, error) && !error)
            {
                return preferred;
            }
        }
        return ResolveBuiltInFile(
            std::filesystem::path(DefaultSkinRelative) / relativePath);
    }

    std::filesystem::path ResolveDefaultSkinAsset(
        const std::filesystem::path& relativePath)
    {
        const std::filesystem::path looseDefaultSkin =
            ExecutableDirectory() /
            std::filesystem::path(DefaultSkinRelative);
        return ResolveSkinAsset(looseDefaultSkin, relativePath);
    }
}
