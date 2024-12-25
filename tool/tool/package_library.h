#pragma once

#include <filesystem>
#include <futile/futile.h>
#include <ulib/env.h>
#include <ulib/json.h>
#include <ulib/string.h>

namespace vcwin
{
    namespace fs = std::filesystem;

    class PackageLibrary
    {
    public:
        PackageLibrary()
        {
            if (auto userprofile = ulib::getenv(u8"USERPROFILE"))
                mPackageLibPath = *userprofile + u8"\\vcwin_packages.json";
            else
                throw ulib::RuntimeError{"Failed to determine USERPROFILE env variable"};

            if (!fs::exists(mPackageLibPath))
                futile::open(mPackageLibPath, "w").write(MakeDefaultPackageLibrary().dump());

            mPackageLibrary = ulib::json::parse(futile::open(mPackageLibPath, "r").read());
        }

        ulib::json &GetLocal()
        {
            return mPackageLibrary;
        }

        std::optional<ulib::string> FindPackage(ulib::string_view name, ulib::string_view version)
        {
            if (auto pckg = mPackageLibrary.search(name))
                if (auto link = pckg->search(version))
                    return link->get<ulib::string>();
            
            return std::nullopt;
        }

    private:
        ulib::json MakeDefaultPackageLibrary()
        {
            ulib::json r;

            auto &wdk = r["wdk"];

            wdk["10.1.26100.2454"] = "https://download.microsoft.com/download/a/0/4/"
                                     "a04a6ea0-d70d-496f-9949-a73e283be017/KIT_BUNDLE_WDK_MEDIACREATION/wdksetup.exe";
            wdk["10.1.26100.2161"] = "https://download.microsoft.com/download/b/3/c/"
                                     "b3cf3da3-9ca3-4bf7-b50f-3db0251a3211/KIT_BUNDLE_WDK_MEDIACREATION/wdksetup.exe";
            wdk["10.1.26100.1882"] = "https://download.microsoft.com/download/d/e/c/"
                                     "dec39407-4add-4651-9afe-88c4dd64325f/KIT_BUNDLE_WDK_MEDIACREATION/wdksetup.exe";
            wdk["10.1.26100.1591"] = "https://download.microsoft.com/download/8/6/c/"
                                     "86c40d63-a128-41d0-ac1d-ba489cbab9cd/KIT_BUNDLE_WDK_MEDIACREATION/wdksetup.exe";
            wdk["10.1.26100.1"] = "https://download.microsoft.com/download/2/5/f/25f22c34-1cc4-404c-9f92-2ff26cc4ac91/"
                                  "KIT_BUNDLE_WDK_MEDIACREATION/wdksetup.exe";
            wdk["10.1.22621.2428"] =
                "https://download.microsoft.com/download/7/b/f/7bfc8dbe-00cb-47de-b856-70e696ef4f46/wdk/wdksetup.exe";
            wdk["10.1.22621.382"] =
                "https://download.microsoft.com/download/c/6/4/c64e8e90-4143-4e86-93d7-0209b0b30626/wdk/wdksetup.exe";
            wdk["10.1.22000.1"] =
                "https://download.microsoft.com/download/7/d/6/7d602355-8ae9-414c-ae36-109ece2aade6/wdk/wdksetup.exe";
            wdk["10.1.20348.1"] =
                "https://download.microsoft.com/download/e/2/1/e2100c80-3148-4cee-8d67-610792a9ca97/wdk/wdksetup.exe";
            wdk["10.1.18362.1"] =
                "https://download.microsoft.com/download/2/9/3/29376990-B744-43C5-AE5C-99405068D58B/WDK/wdksetup.exe";
            wdk["10.1.17763.1"] =
                "https://download.microsoft.com/download/1/4/0/140EBDB7-F631-4191-9DC0-31C8ECB8A11F/wdk/wdksetup.exe";
            wdk["10.1.14393.0"] =
                "https://download.microsoft.com/download/8/1/6/816FE939-15C7-4185-9767-42ED05524A95/wdk/wdksetup.exe";

            auto &dxsdk = r["dxsdk"];
            dxsdk["9.29.1962.0"] =
                "https://download.microsoft.com/download/A/E/7/AE743F1F-632B-4809-87A9-AA1BB3458E31/DXSDK_Jun10.exe";

            auto &sdk = r["sdk"];
            sdk["10.0.18362.0"] = "Microsoft.VisualStudio.Component.Windows10SDK.18362";
            sdk["10.0.19041.0"] = "Microsoft.VisualStudio.Component.Windows10SDK.19041";
            sdk["10.0.20348.0"] = "Microsoft.VisualStudio.Component.Windows10SDK.20348";
            sdk["10.0.22000.0"] = "Microsoft.VisualStudio.Component.Windows11SDK.22000";
            sdk["10.0.22621.0"] = "Microsoft.VisualStudio.Component.Windows11SDK.22621";
            sdk["10.0.26100.0"] = "Microsoft.VisualStudio.Component.Windows11SDK.26100";

            return r;
        }

        fs::path mPackageLibPath;
        ulib::json mPackageLibrary;
    };
} // namespace vcwin