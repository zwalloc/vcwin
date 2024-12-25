#pragma once

#include "installers.h"
#include <3rdparty/WinReg.hpp>
#include <filesystem>
#include <ulib/env.h>
#include <ulib/format.h>
#include <ulib/json.h>
#include <ulib/string.h>

namespace vcwin
{
    namespace fs = std::filesystem;

    namespace detail
    {
        struct Windows10SdkInfo
        {
            fs::path directory;

            // Without QFE (Quick Fix Engineering)
            ulib::string version;

            ulib::string name;
        };

        inline Windows10SdkInfo get_windows10_sdk_info()
        {
            winreg::RegKey winsdk_node;

            winsdk_node.Create(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Microsoft\\Microsoft SDKs\\Windows\\v10.0",
                               KEY_READ | KEY_WOW64_32KEY);

            std::filesystem::path winsdk_directory =
                ulib::sstr(ulib::u8(winsdk_node.GetStringValue(L"InstallationFolder")));
            auto winsdk_version = ulib::sstr(ulib::u8(winsdk_node.GetStringValue(L"ProductVersion")));
            auto winsdk_name = ulib::sstr(ulib::u8(winsdk_node.GetStringValue(L"ProductName")));

            winsdk_node.Close();

            Windows10SdkInfo info;
            info.directory = winsdk_directory;
            info.name = winsdk_name;
            info.version = winsdk_version;

            return info;
        }

    } // namespace detail

    struct wversion
    {
        ulib::string source; // 10.0.22621.2428

        uint32 win;   // 10
        uint32 mark;  // 0 - windows, 1 - software
        uint32 build; // 22621 os build number
        uint32 qfe;   // 2428 QFE (Quick Fix Engineering)

        static wversion parse(ulib::string_view ver)
        {
            wversion wv;
            wv.source = ver;

            auto spl = ulib::list<ulib::string>{ver.split(".")};
            if (spl.size() >= 3)
            {
                wv.win = std::stoul(spl[0]);
                wv.mark = std::stoul(spl[1]);
                wv.build = std::stoul(spl[2]);

                if (spl.size() >= 4)
                    wv.qfe = std::stoul(spl[3]);
                else
                    wv.qfe = 0;
            }

            return wv;
        }

        static ulib::string to_windows_build(const wversion &wver)
        {
            return ulib::format("{}.{}.{}.{}", wver.win, 0, wver.build, 0);
        }

        static ulib::string to_windows_build(ulib::string_view ver)
        {
            return to_windows_build(parse(ver));
        }

        static uint32 build_number(ulib::string_view ver)
        {
            return parse(ver).build;
        }
    };

    struct WDKUninstallComponent
    {
        ulib::string name;
        ulib::string version;
    };

    struct WindowsSDKItem
    {
        WindowsSDKItem()
        {
            inInstalledRoots = false;
            hasWDKInOptions = false;
        }

        ulib::string windowsBuildVersion;

        ulib::list<WindowsComponent> wdkUninstallComponents;
        ulib::list<WindowsComponent> wdkInstallerComponents;
        ulib::list<WindowsComponent> sdkUninstallComponents;
        ulib::list<WindowsComponent> sdkInstallerComponents;
        ulib::list<std::pair<ulib::string, bool>> options;

        std::optional<WindowsComponent> FindWdkUninstaller() const
        {
            for (auto &wuc : wdkUninstallComponents)
            {
                if (wuc.SystemComponent == false)
                {
                    return wuc;
                }
            }

            return std::nullopt;
        }

        bool inInstalledRoots;
        bool hasWDKInOptions;

        inline const std::pair<ulib::string, bool> *FindOption(ulib::string_view option) const
        {
            for (auto &opt : options)
            {
                if (opt.first == option)
                    return &opt;
            }

            return nullptr;
        }
    };

    class WindowsSDK
    {
    public:
        WindowsSDK()
        {
            // for (size_t i = 0; i != 20; i++)
            {
                try
                {
                    mWindows10SdkInfoSource = "HKLM:SOFTWARE\\WOW6432Node\\Microsoft\\Microsoft SDKs\\Windows\\v10.0";
                    mWindows10SdkInfo = detail::get_windows10_sdk_info();
                    mWindowsSDKDirectory = mWindows10SdkInfo->directory;
                    mWindowsSDKVersion = mWindows10SdkInfo->version;
                    mWindowsSDKName = mWindows10SdkInfo->name;
                }
                catch (...)
                {
                }

                try
                {
                    ReadInstalledRoots();
                }
                catch (...)
                {
                }

                try
                {
                    auto filter = [](WindowsComponent &component) {
                        if (component.DisplayName.contains("Windows") && component.DisplayName.contains("SDK"))
                        {
                            try
                            {
                                auto ver = wversion::parse(component.DisplayVersion);
                                return ver.mark == 1 && ver.build != 0 && ver.win > 6 && ver.win < 12;
                            }
                            catch (...)
                            {
                            }
                        }

                        return false;
                    };

                    ulib::list<WindowsComponent> uninstall_components =
                        list_uninstall_components().filter([](WindowsComponent &component) {
                            return (component.DisplayName.contains("Windows") && component.DisplayName.contains("SDK"));
                        });

                    for (auto &package : uninstall_components.group_by(
                             [](const WindowsComponent &component) { return component.DisplayVersion; }))
                    {
                        MakeSDKItem(wversion::to_windows_build(package.first)).sdkUninstallComponents = package.second;
                    }

                    ulib::list<WindowsComponent> installer_components =
                        list_installer_components().filter([](WindowsComponent &component) {
                            return (component.DisplayName.contains("Windows") && component.DisplayName.contains("SDK"));
                        });

                    for (auto &package : installer_components.group_by(
                             [](const WindowsComponent &component) { return component.DisplayVersion; }))
                    {
                        MakeSDKItem(wversion::to_windows_build(package.first)).sdkInstallerComponents = package.second;
                    }
                }
                catch (...)
                {
                }

                try
                {

                    ulib::list<WindowsComponent> uninstall_components =
                        list_uninstall_components().filter([](WindowsComponent &component) {
                            return component.DisplayName.contains("Windows Driver Kit") ||
                                   component.DisplayName.contains("Windows Driver Framework");
                        });

                    for (auto &package : uninstall_components.group_by(
                             [](const WindowsComponent &component) { return component.DisplayVersion; }))
                    {
                        MakeSDKItem(wversion::to_windows_build(package.first)).wdkUninstallComponents = package.second;
                    }

                    ulib::list<WindowsComponent> installer_components =
                        list_installer_components().filter([](WindowsComponent &component) {
                            return component.DisplayName.contains("Windows Driver Kit") ||
                                   component.DisplayName.contains("Windows Driver Framework");
                        });

                    for (auto &package : installer_components.group_by(
                             [](const WindowsComponent &component) { return component.DisplayVersion; }))
                    {
                        MakeSDKItem(wversion::to_windows_build(package.first)).wdkInstallerComponents = package.second;
                    }
                }
                catch (...)
                {
                }

                ReadKMDFDir();
                ReadWDKProductVersion();
            }
        }

        std::optional<detail::Windows10SdkInfo> GetWindows10SdkInfo()
        {
            return mWindows10SdkInfo;
        }

        ulib::string GetWindows10SdkInfoSource()
        {
            return mWindows10SdkInfoSource;
        }

        const ulib::list<WindowsSDKItem> &GetSDKs() const
        {
            return mSDKs;
        }

        std::optional<ulib::string> GetWDKProductVersion10()
        {
            return mWDKProductVersion10;
        }

        ulib::string GetWDKProductVersion10Source()
        {
            return mWDKProductVersion10Source;
        }

        const ulib::list<ulib::string> &GetKMDFVersions()
        {
            return mKMDFVersions;
        }

        ulib::string GetKMDFVersionsSource()
        {
            return mKMDFVersionsSource;
        }

        ulib::json ToJson()
        {
            ulib::json val;

            val["v10_source"] = mWindows10SdkInfoSource;

            auto &v10 = val["v10"];
            v10["InstallationFolder"] = mWindowsSDKDirectory;
            v10["ProductVersion"] = mWindowsSDKVersion;
            v10["ProductName"] = mWindowsSDKName;

            val["WdkProductVersion10"] = mWDKProductVersion10;
            val["WdkProductVersion10_source"] = mWDKProductVersion10Source;

            auto &jSDKs = val["SDKs"];

            for (auto &sdk : mSDKs)
            {
                auto &jsdk = jSDKs.push_back();
                jsdk["windows_version"] = sdk.windowsBuildVersion;
                jsdk["has_wdk_in_options"] = sdk.hasWDKInOptions;
                jsdk["in_InstalledRoots"] = sdk.inInstalledRoots;

                auto &jwuc = jsdk["wdk_uninstall_components"];
                for (auto &comp : sdk.wdkUninstallComponents)
                {
                    auto &jcomp = jwuc[comp.DisplayName];
                    jcomp["DisplayVersion"] = comp.DisplayVersion;
                    jcomp["SystemComponent"] = comp.SystemComponent;
                    jcomp["UninstallString"] = comp.UninstallString;
                }

                auto &jwic = jsdk["wdk_installer_components"];
                for (auto &comp : sdk.wdkInstallerComponents)
                {
                    auto &jcomp = jwic[comp.DisplayName];
                    jcomp["DisplayVersion"] = comp.DisplayVersion;
                    jcomp["SystemComponent"] = comp.SystemComponent;
                    jcomp["UninstallString"] = comp.UninstallString;
                }

                auto &jsuc = jsdk["sdk_uninstall_components"];
                for (auto &comp : sdk.sdkUninstallComponents)
                {
                    auto &jcomp = jsuc[comp.DisplayName];
                    jcomp["DisplayVersion"] = comp.DisplayVersion;
                    jcomp["SystemComponent"] = comp.SystemComponent;
                    jcomp["UninstallString"] = comp.UninstallString;
                }

                auto &jsic = jsdk["sdk_installer_components"];
                for (auto &comp : sdk.sdkInstallerComponents)
                {
                    auto &jcomp = jsic[comp.DisplayName];
                    jcomp["DisplayVersion"] = comp.DisplayVersion;
                    jcomp["SystemComponent"] = comp.SystemComponent;
                    jcomp["UninstallString"] = comp.UninstallString;
                }

                auto &joptions = jsdk["options"];
                for (auto &opt : sdk.options)
                {
                    joptions[opt.first] = opt.second;
                }
            }

            val["kmdf_source"] = mKMDFVersionsSource;

            auto &jkmdf = val["kmdf"];
            for (auto &kmdf : mKMDFVersions)
            {
                jkmdf.push_back() = kmdf;
            }

            return val;
        }

        const WindowsSDKItem *FindSDKByWDKVersion(ulib::string_view wdkVersion) const
        {
            for (auto &sdk : GetSDKs())
            {
                if (sdk.wdkUninstallComponents.size() > 0)
                {
                    if (sdk.wdkUninstallComponents.front().DisplayVersion == wdkVersion)
                        return &sdk;
                }
            }

            return nullptr;
        }

    private:
        void ReadKMDFDir()
        {
            mKMDFVersionsSource = "mWindows10SdkInfo->directory / \"Include\\wdf\\kmdf\"";

            if (mWindows10SdkInfo)
            {
                fs::path kmdfIncludeDir = (mWindows10SdkInfo->directory) / "Include\\wdf\\kmdf";
                mKMDFVersionsSource = ulib::str(kmdfIncludeDir.u8string());

                // fmt::print("Source: {}\n", ulib::str(kmdfIncludeDir.u8string()));

                if (fs::exists(kmdfIncludeDir))
                {
                    for (const fs::directory_entry &dir_entry : fs::directory_iterator{kmdfIncludeDir})
                    {
                        mKMDFVersions.push_back(ulib::u8(dir_entry.path().filename().u8string()));
                    }
                }
            }
            else
            {
                fmt::print("Error: Windows 10 SDK directory not found\n");
            }
        }

        void ReadInstalledRoots()
        {
            try
            {
                winreg::RegKey winsdk_node;
                winsdk_node.Create(HKEY_LOCAL_MACHINE,
                                   L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows Kits\\Installed Roots",
                                   KEY_READ | KEY_WOW64_32KEY | KEY_ENUMERATE_SUB_KEYS);

                auto subkeys = winsdk_node.EnumSubKeys();
                for (auto &sk : subkeys)
                {
                    auto &sdkItem = MakeSDKItem(ulib::str(ulib::u8(sk)));
                    sdkItem.inInstalledRoots = true;

                    try
                    {
                        winreg::RegKey node;
                        node.Create(HKEY_LOCAL_MACHINE,
                                    ulib::format(L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows Kits\\Installed "
                                                 L"Roots\\{}\\Installed Options",
                                                 ulib::wstr(sk)),
                                    KEY_READ | KEY_WOW64_32KEY);

                        for (auto &val : node.EnumValues())
                        {
                            if (val.first == L"OptionId.WindowsDriverKitComplete")
                                sdkItem.hasWDKInOptions = true;

                            sdkItem.options.push_back({ulib::u8(val.first), val.second});
                        }

                        node.Close();
                    }
                    catch (const std::exception &ex)
                    {
                    }
                }
            }
            catch (const std::exception &ex)
            {
                fmt::print("Installed Roots: Error: {}\n", ex.what());
            }
        }

        WindowsSDKItem &MakeSDKItem(ulib::string_view windowsBuildVersion)
        {
            for (auto &item : mSDKs)
            {
                if (item.windowsBuildVersion == windowsBuildVersion)
                    return item;
            }

            WindowsSDKItem item;
            mSDKs.push_back(item);

            auto &rv = mSDKs.back();
            rv.windowsBuildVersion = windowsBuildVersion;

            return rv;
        }

        void ReadWDKProductVersion()
        {
            mWDKProductVersion10Source =
                "HKLM:SOFTWARE\\WOW6432Node\\Microsoft\\Windows Kits\\WDK\\WDKProductVersion10";

            try
            {
                winreg::RegKey winsdk_node;
                winsdk_node.Create(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows Kits\\WDK",
                                   KEY_READ | KEY_WOW64_32KEY);

                ulib::string productVersion = ulib::u8(winsdk_node.GetStringValue(L"WDKProductVersion10"));
                mWDKProductVersion10 = productVersion;
            }
            catch (const std::exception &ex)
            {
                // fmt::print("WDK: Error: {}\n", ex.what());
            }
        }

        std::optional<detail::Windows10SdkInfo> mWindows10SdkInfo;
        ulib::string mWindows10SdkInfoSource;

        std::optional<fs::path> mWindowsSDKDirectory;
        std::optional<ulib::string> mWindowsSDKVersion;
        std::optional<ulib::string> mWindowsSDKName;

        ulib::list<WindowsSDKItem> mSDKs;

        std::optional<ulib::string> mWDKProductVersion10;
        ulib::string mWDKProductVersion10Source;

        ulib::list<ulib::string> mKMDFVersions;
        ulib::string mKMDFVersionsSource;
    };

} // namespace vcwin