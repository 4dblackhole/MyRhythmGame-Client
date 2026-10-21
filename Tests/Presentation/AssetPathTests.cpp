#include "App/OptionSettings.h"
#include "YOUMUAssets.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
    void Check(bool condition, const char *message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
} // namespace

int wmain(int argc, wchar_t *argv[])
{
    try
    {
        if (argc != 3)
            throw std::runtime_error("Expected asset root and executable directory arguments.");
        const std::filesystem::path expectedAssets(argv[1]);
        const std::filesystem::path expectedExecutable(argv[2]);

        Check(AssetExecutableDirectory() == expectedExecutable,
              "Executable location must be independent of the working directory.");
        Check(UserAssetRoot() == expectedAssets,
              "Development/deployment selection must follow executable-relative paths.");
        Check(UserSongsRoot() == expectedAssets / L"Songs", "Songs must use the selected asset root.");
        Check(OptionSettings::NextToExecutable() == expectedExecutable / L"Option.ini",
              "Option.ini must remain beside the executable in both environments.");
        {
            std::ofstream ini(expectedExecutable / L"Option.ini", std::ios::binary);
            ini << "[General]\nSongsPath=../customSongs\n";
        }
        OptionSettings options(OptionSettings::NextToExecutable());
        std::string error;
        Check(options.Load(error), error.c_str());
        Check(options.SongsRoot() == (expectedExecutable / L"../customSongs").lexically_normal(),
              "Configured relative Songs paths must stay executable-relative after moving the executable.");
        auto values = options.Values();
        values.songsPath.clear();
        Check(options.Save(values, error) && options.SongsRoot() == UserSongsRoot(),
              "Restoring the default Songs folder must reuse the development/deployment asset rule.");
        Check(ResolveDefaultSkinAsset(L"path-test.dat") == expectedAssets / L"Skins/Default Skin/path-test.dat",
              "External default skins must use the same root as Songs.");
        std::cout << "Asset paths and Option.ini location passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
