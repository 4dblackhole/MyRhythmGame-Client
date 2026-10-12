#include "App/OptionSettings.h"
#include "FingerDrumAssets.h"

#include <filesystem>
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
        namespace assets = finger_drum::assets;
        Check(assets::ExecutableDirectory() == expectedExecutable,
              "Executable location must be independent of the working directory.");
        Check(assets::UserAssetRoot() == expectedAssets,
              "Development/deployment selection must follow executable-relative paths.");
        Check(assets::UserSongsRoot() == expectedAssets / L"Songs", "Songs must use the selected asset root.");
        Check(mrg_client::OptionSettings::NextToExecutable() == expectedExecutable / L"Option.ini",
              "Option.ini must remain beside the executable in both environments.");
        Check(assets::ResolveDefaultSkinAsset(L"path-test.dat") == expectedAssets / L"Skins/Default Skin/path-test.dat",
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
