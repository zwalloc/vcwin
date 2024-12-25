#define _CRT_SECURE_NO_WARNINGS
// #define CPPHTTPLIB_OPENSSL_SUPPORT
// #include <httplib.h>
#include "download_file.h"

#include <filesystem>
#include <iostream>

#include <3rdparty/WinReg.hpp>
#include <Windows.h>
// #include <nlohmann/json.hpp>

#include <futile/futile.h>
#include <ulib/env.h>
#include <ulib/format.h>
#include <ulib/runtimeerror.h>
#include <ulib/strutility.h>
#include <ulib/yaml.h>

#include "dxsdk.h"
#include "installers.h"
#include "vctools.h"
#include "winsdk.h"

#include "package_library.h"
#include "vsinstaller.h"

namespace fs = std::filesystem;

namespace tool
{
    namespace detail
    {
        struct option_walk_state
        {
            size_t idx;
        };

        ulib::list<ulib::string_view> parse_any_arg_option(const ulib::list<ulib::string_view> &args,
                                                           ulib::string_view option, option_walk_state *ows = nullptr)
        {
            ulib::list<ulib::string_view> result;
            auto beginIdx = ulib::npos;

            if (ows)
                beginIdx = args.find(option, ows->idx);
            else
                beginIdx = args.find(option);

            if (beginIdx == ulib::npos)
                return result;

            size_t i = beginIdx + 1;
            for (size_t i = beginIdx + 1; i != args.size(); i++)
            {
                if (args[i].starts_with("-"))
                    break;

                result.push_back(args[i]);
            }

            if (ows)
                ows->idx = i;

            return result;
        }
    } // namespace detail

    enum class FormatType
    {
        Yaml = 0,
        Json = 1,
    };

    class ToolState
    {
    public:
        void print(const ulib::json &value)
        {
            if (mFormat == FormatType::Json)
            {
                fmt::print("{}\n", value.dump());
            }
            else
            {
                fmt::print("{}\n", ulib::yaml::parse(value.dump()).dump());
            }
        }

        void print_error(ulib::string_view error)
        {
            ulib::json value;
            value["error"] = error;

            print(value);
        }

        void print_help()
        {
            ulib::json help;

            auto &commands = help["commands"];
            commands.push_back() = "state";
            commands.push_back() = "install <package name> <package version>";
            commands.push_back() = "uninstall/remove <package name> <package version> [--show-string] [--full]";
            commands.push_back() = "search [<package name>] [<package version>]";
            commands.push_back() = "list <package name>";
            commands.push_back() = "get <package name>";

            auto &flags = help["flags"];
            flags["--format"] = "yaml/json";

            print(help);
        }

        int ExecuteGet()
        {
            if (mArgs.size() < 2)
            {
                print_error("Expected 1 args");
                return 1;
            }

            ulib::string productName = mArgs[1];
            if (productName == "wdk")
            {
                vcwin::WindowsSDK wsdk;
                if (auto productVersion = wsdk.GetWDKProductVersion10())
                {
                    fmt::print("WDKProductVersion10: {}\n", *productVersion);
                }
            }

            if (productName == "sdk")
            {
                vcwin::WindowsSDK wsdk;
                if (auto w10sdk = wsdk.GetWindows10SdkInfo())
                {
                    fmt::print("Name: {}\nVersion: {}\nDirectory: {}\n", w10sdk->name, w10sdk->version,
                               ulib::u8(w10sdk->directory.generic_u8string()));
                }
            }

            return 0;
        }

        int ExecuteState()
        {
            vcwin::VCTools vcTools;
            vcwin::WindowsSDK winsdk;
            vcwin::DirectXSdk dxsdk;

            ulib::json value;
            value["vctools"] = vcTools.ToJson();
            value["winsdk"] = winsdk.ToJson();
            value["dxsdk"] = dxsdk.ToJson();

            print(value);

            return 0;
        }

        int ExecuteList()
        {
            if (mArgs.size() < 2)
            {
                print_error("Expected 2 args");
                return 1;
            }

            auto productName = mArgs[1];
            if (productName == "wdk")
            {
                vcwin::WindowsSDK winsdk;

                for (auto &sdk : winsdk.GetSDKs())
                {
                    bool hasWdkComponents = sdk.wdkUninstallComponents.size() > 0;
                    auto uninstaller = sdk.FindWdkUninstaller();

                    if (hasWdkComponents)
                    {
                        fmt::print(" - {}. Components: {}. Uninstaller: {}\n",
                                   sdk.wdkUninstallComponents.front().DisplayVersion, sdk.wdkUninstallComponents.size(),
                                   uninstaller ? uninstaller->DisplayName : "null");
                    }
                    else
                    {
                        if (sdk.hasWDKInOptions)
                        {
                            fmt::print(" - SDK for Windows {} have not wdk components, but have wdk in options\n",
                                       sdk.windowsBuildVersion);
                        }
                    }
                }
            }
            else if (productName == "sdk")
            {
                vcwin::WindowsSDK winsdk;

                for (auto &sdk : winsdk.GetSDKs())
                {
                    bool hasSdkComponents = sdk.sdkUninstallComponents.size() > 0;
                    if (hasSdkComponents)
                    {
                        fmt::print(" - {}. Components: {}\n", sdk.sdkUninstallComponents.front().DisplayVersion,
                                   sdk.sdkUninstallComponents.size());
                    }
                    else
                    {
                        fmt::print(" - SDK for Windows {} have not components\n", sdk.windowsBuildVersion);
                    }
                }
            }

            return 0;
        }

        int ExecuteSearch()
        {
            vcwin::PackageLibrary lib;
            auto localPackages = lib.GetLocal();

            if (mArgs.size() > 1)
            {
                if (auto pckg = localPackages.search(mArgs[1]))
                {
                    if (mArgs.size() > 2)
                    {
                        if (auto link = pckg->search(mArgs[2]))
                        {
                            print(*link);
                        }
                        else
                        {
                            print_error("Package not found");
                            return 1;
                        }
                    }
                    else
                    {
                        print(*pckg);
                    }
                }
                else
                {
                    print_error("Package not found");
                    return 1;
                }
            }
            else
            {
                print(localPackages);
            }

            return 0;
        }

        int ExecuteInstall()
        {
            if (mArgs.size() < 3)
            {
                print_error("Expected 3 args");
                return 1;
            }

            auto packageName = mArgs[1];
            auto version = mArgs[2];

            if (packageName == "wdk" || packageName == "dxsdk")
            {
                vcwin::PackageLibrary lib;
                if (auto link = lib.FindPackage(packageName, version))
                {
                    ulib::u8string path = ulib::format(u8"{}_{}.exe", packageName, version);
                    vcwin::download_file_with_progress(*link, path);

                    auto anim = barkeep::Animation({.message = ulib::format("Installing: {} ", path),
                                                    .style = barkeep::AnimationStyle::Moon,
                                                    .interval = 0.1});
                    int result = 0;

                    if (packageName == "wdk")
                        result = ulib::process{ulib::format(
                                                   u8"{} /features + /quiet /norestart /log wdksetup_latest.log", path)}
                                     .wait();
                    else if (packageName == "dxsdk")
                        result = ulib::process{ulib::format(u8"{} /F /S /O /U", path)}.wait();
                    else
                        throw ulib::RuntimeError{"vcwin doesn't know how to install this"};

                    anim->done();

                    fmt::print("Installer exited with code: {}\n", result);
                    return result;
                }
            }
            else if (packageName == "sdk")
            {
                vcwin::PackageLibrary lib;
                if (auto component = lib.FindPackage(packageName, version))
                {
                    vcwin::VSInstaller installer;

                    auto anim = barkeep::Animation({.message = ulib::format("Installing: {} ", *component),
                                                    .style = barkeep::AnimationStyle::Moon,
                                                    .interval = 0.1});

                    int code = installer.Install(*component);
                    anim->done();

                    fmt::print("Installer exited with code: {}\n", code);

                    return code;
                }
            }

            return print_error("Package not found"), 1;
        }

        int ExecuteUninstall()
        {
            if (mArgs.size() < 3)
            {
                print_error("Expected 3 args");
                return 1;
            }

            auto packageName = mArgs[1];
            auto version = mArgs[2];

            size_t uninstalled = 0;

            if (packageName == "wdk")
            {
                vcwin::WindowsSDK wsdk;
                if (auto sdk = wsdk.FindSDKByWDKVersion(version))
                {
                    if (auto uninstaller = sdk->FindWdkUninstaller())
                    {
                        if (uninstaller->DisplayVersion == version)
                        {
                            if (mArgs.contains("--show-string"))
                            {
                                fmt::print("{}\n", uninstaller->UninstallString);
                                return 0;
                            }

                            ulib::u8string exec = ulib::u8(uninstaller->UninstallString + " /q");
                            fmt::print(" -> {}\n", exec);

                            int code = ulib::process{exec}.wait();
                            fmt::print("Uninstaller exited with code: {}\n", code);

                            if (code != 0)
                                return code;

                            uninstalled++;
                        }
                    }
                }

                if (mArgs.contains("--full"))
                {
                    wsdk = vcwin::WindowsSDK{};
                    if (auto sdk = wsdk.FindSDKByWDKVersion(version))
                    {
                        for (auto &component : sdk->wdkUninstallComponents)
                        {
                            ulib::u8string exec = ulib::format(u8"msiexec /qn /x {}", component.guid);
                            fmt::print(" -> {}\n", exec);

                            int code = ulib::process{exec}.wait();
                            fmt::print("Uninstaller exited with code: {}\n", code);

                            if (code == 0)
                                uninstalled++;
                        }
                    }
                }
            }
            else if (packageName == "sdk")
            {
                vcwin::PackageLibrary lib;
                if (auto component = lib.FindPackage(packageName, version))
                {
                    vcwin::VSInstaller installer;

                    auto anim = barkeep::Animation({.message = ulib::format("Uninstalling: {} ", *component),
                                                    .style = barkeep::AnimationStyle::Moon,
                                                    .interval = 0.1});

                    int code = installer.Uninstall(*component);
                    anim->done();

                    if (code == 0)
                        uninstalled++;

                    fmt::print("Installer exited with code: {}\n", code);

                    return code;
                }
            }

            if (uninstalled)
            {
                fmt::print("Uninstalled {} entities\n", uninstalled);
            }
            else
            {

                fmt::print("Nothing to uninstall\n");
            }

            return 1;
        }

        int Execute(int argc, char *const *argv)
        {
            mFormat = FormatType::Yaml;

            mPathToThis = argv[0];
            mArgs = ulib::list<ulib::string_view>{argv + 1, argv + argc};

            if (detail::parse_any_arg_option(mArgs, "--format").contains("json"))
                mFormat = FormatType::Json;

            if (mArgs.size() >= 1)
            {
                if (mArgs[0] == "help" || mArgs[0] == "--help" || mArgs[0] == "-h" || mArgs[0] == "/?")
                    return print_help(), 0;

                if (mArgs[0] == "state")
                    return ExecuteState();

                if (mArgs[0] == "install")
                    return ExecuteInstall();

                if (mArgs[0] == "uninstall" || mArgs[0] == "remove")
                    return ExecuteUninstall();

                if (mArgs[0] == "search")
                    return ExecuteSearch();

                if (mArgs[0] == "list")
                    return ExecuteList();

                if (mArgs[0] == "get")
                    return ExecuteGet();
            }

            return print_help(), 0;
        }

    private:
        FormatType mFormat;
        ulib::list<ulib::string_view> mArgs;
        fs::path mPathToThis;
    };

    // void perform_state(const ulib::list<ulib::string_view> &args)
    // {
    // }

    // void perform(const ulib::list<ulib::string_view> &args)
    // {
    //     perform_state(args);
    //     return;

    //     if (args.size() == 0)
    //     {
    //         print_help();
    //         return;
    //     }

    //     if (args.size() == 1 && (args[0] == "--help" || args[0] == "-h"))
    //     {
    //         print_help();
    //         return;
    //     }

    //     if (args[0] == "state")
    //     {
    //         perform_state(args);
    //         return;
    //     }
    // }

} // namespace tool

int main(int argc, char **argv)
{
    try
    {
        // auto anim = barkeep::Animation({.message = "Install", .style = barkeep::AnimationStyle::Moon, .interval =
        // 0.1}); auto anim2 = barkeep::Animation({.message = "Downloading...", .style =
        // barkeep::AnimationStyle::Clock});

        tool::ToolState state;
        return state.Execute(argc, argv);

        // vcwin::download_file_with_progress(
        //     "https://download.microsoft.com/download/a/0/4/"
        //     "a04a6ea0-d70d-496f-9949-a73e283be017/KIT_BUNDLE_WDK_MEDIACREATION/wdksetup.exe",
        //     "C:/wdk.exe");
        // return 0;

        // std::filesystem::path path_to_this = argv[0];

        // ulib::list<ulib::string_view> args{argv + 1, argv + argc};
        // tool::perform(args);
    }
    catch (const std::exception &ex)
    {

        Sleep(3000);
        fmt::print("exception: {}\n", ex.what());
        return -1;
    }

    return 0;

    // try
    // {
    //     std::filesystem::path path_to_this = argv[0];
    //     path_to_this = path_to_this.parent_path();

    //     auto vswhere_exe = detail::find_vswhere();
    //     auto vs_path = detail::find_vs_installation_path(vswhere_exe);
    //     auto vc_tools_version = detail::get_vc_tools_version(vs_path);
    //     auto vc_tools_dir = vs_path / "VC/Tools/MSVC/" / vc_tools_version;

    //     detail::Windows10SdkInfo winsdk_info = detail::get_windows10_sdk_info();
    //     std::filesystem::path winsdk_directory = winsdk_info.directory;
    //     ulib::string winsdk_version = winsdk_info.version;
    //     ulib::string winsdk_name = winsdk_info.name;

    //     winsdk_version.append(".0"); // Add Zero QFE (Quick Fix Engineering) to compatibility with folder name

    //     ulib::list<std::filesystem::path> default_include_dirs;

    //     default_include_dirs.push_back(vc_tools_dir / "include");
    //     default_include_dirs.push_back(vc_tools_dir / "ATLMFC" / "include");
    //     default_include_dirs.push_back(vs_path / "VC/Auxiliary/VS/include");

    //     auto winsdk_include_root = winsdk_directory / "include" / winsdk_version;

    //     for (auto &entry : std::filesystem::directory_iterator{winsdk_include_root})
    //     {
    //         if (entry.is_directory())
    //             default_include_dirs.push_back(entry.path());
    //     }

    //     auto winsdk_bin_root = winsdk_directory / "bin" / winsdk_version;

    //     auto result = nlohmann::json::object();

    //     result["vs_install_dir"] = ulib::str(vs_path.generic_u8string());

    //     result["vc_tools_version"] = vc_tools_version;
    //     result["vc_tools_dir"] = ulib::str(vc_tools_dir.generic_u8string());

    //     result["winsdk_directory"] = ulib::str(winsdk_directory.generic_u8string());
    //     result["winsdk_version"] = winsdk_version;
    //     result["winsdk_bin"] = ulib::str(winsdk_bin_root.generic_u8string());

    //     ulib::list<ulib::u8string> include_dirs;

    //     for (auto &path : default_include_dirs)
    //     {
    //         if (std::filesystem::exists(path))
    //         {
    //             include_dirs.push_back(path.generic_u8string());
    //         }
    //     }

    //     result["vc_include_dirs"] = include_dirs;

    //     // Generate the necessary environment variables
    //     auto &env = result["environment"];

    //     env["VCToolsInstallDir"] = result["vc_tools_dir"];
    //     env["VCToolsVersion"] = result["vc_tools_version"];
    //     env["WindowsSdkDir"] = result["winsdk_directory"];
    //     env["WindowsSDKLibVersion"] = result["winsdk_version"];
    //     env["WindowsSDKVersion"] = result["winsdk_version"];
    //     env["WindowsSdkBinPath"] = ulib::str((winsdk_directory / "bin").generic_u8string());
    //     env["WindowsSdkVerBinPath"] = result["winsdk_bin"];
    //     env["VSINSTALLDIR"] = result["vs_install_dir"];
    //     env["INCLUDE"] = ulib::join(include_dirs, u8";");

    //     std::cout << result.dump(4) << std::endl;

    //     return 0;
    // }
    // catch (const std::exception &ex)
    // {
    //     fmt::print("exception: {}\n", ex.what());
    //     return -1;
    // }
    // catch (...)
    // {
    //     fmt::print("unknown exception\n");
    //     return -1;
    // }
}
