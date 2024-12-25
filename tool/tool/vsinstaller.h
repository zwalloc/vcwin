#pragma once

#include <filesystem>
#include <ulib/env.h>
#include <ulib/format.h>
#include <ulib/process.h>

namespace vcwin
{
    namespace fs = std::filesystem;

    class VSInstaller
    {
    public:
        VSInstaller()
        {
            // Find the globally available vswhere.exe.
            if (auto pf = ulib::getenv(u8"ProgramFiles(x86)"))
            {
                mPath = fs::path{*pf} / "Microsoft Visual Studio/Installer/vs_installer.exe";
                if (!fs::exists(mPath))
                {
                    throw ulib::RuntimeError{"Failed to determine vsinstaller path"};
                }
            }
            else
            {
                throw ulib::RuntimeError{"Failed to determine vsinstaller path"};
            }
        }

        fs::path GetPath() const
        {
            return mPath;
        }

        ulib::string MakeComponentsList(const ulib::list<ulib::string> &components, ulib::string_view command)
        {
            ulib::string adds = "";
            for (auto &comp : components)
                adds.append(ulib::format("{} {} ", command, comp));

            if (adds.empty())
                return adds;
            adds.pop_back();

            return adds;
        }

        int Install(const ulib::list<ulib::string> &components)
        {
            /*

            "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vs_installer.exe" install
            --productId Microsoft.VisualStudio.Product.Community
            --add Microsoft.Component.MSBuild
            --add Microsoft.VisualStudio.Component.VC.CoreIde
            --add Microsoft.VisualStudio.Component.VC.Redist.14.Latest
            --add Microsoft.VisualStudio.ComponentGroup.NativeDesktop.Core
            --add Microsoft.VisualStudio.Component.CppBuildInsights
            --add Microsoft.VisualStudio.Component.Debugger.JustInTime
            --add Microsoft.VisualStudio.Component.Graphics.Tools
            --add Microsoft.VisualStudio.Component.IntelliCode
            --add Microsoft.VisualStudio.Component.NuGet
            --add Microsoft.VisualStudio.Component.Roslyn.LanguageServices
            --add Microsoft.VisualStudio.Component.VC.ASAN
            --add Microsoft.VisualStudio.Component.VC.ATL
            --add Microsoft.VisualStudio.Component.WindowsAppSdkSupport.Cpp
            --add Microsoft.VisualStudio.Component.VC.DiagnosticTools
            --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64
            --add Microsoft.VisualStudio.Component.Windows11SDK.22621
            --add Microsoft.VisualStudio.Component.Windows11Sdk.WindowsPerformanceToolkit
            --add Microsoft.VisualStudio.Component.VC.Modules.x86.x64
            --add Microsoft.VisualStudio.Component.Windows10SDK.20348
            --add Microsoft.VisualStudio.Component.Windows10SDK.19041
            --add Microsoft.VisualStudio.ComponentGroup.VC.Tools.142.x86.x64
            --add Microsoft.VisualStudio.Component.Windows10SDK
            --add Component.Microsoft.Windows.DriverKit
            --channelId "VisualStudio.17.Release" --norestart --passive


            */

            /*

"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vs_installer.exe" install --productId
Microsoft.VisualStudio.Product.Community  --add Microsoft.VisualStudio.Component.Windows10SDK.20348 --channelId
"VisualStudio.17.Release" --norestart --passive


*/

            ulib::string adds = MakeComponentsList(components, "--add");
            if (adds.empty())
                return 0;

            return ulib::process{ulib::format(u8"{} install --productId Microsoft.VisualStudio.Product.Community {} "
                                              u8"--channelId \"VisualStudio.17.Release\" --norestart --passive",
                                              ulib::u8(mPath.generic_u8string()), adds)}
                .wait();
        }

        int Install(ulib::string_view component)
        {
            ulib::list<ulib::string> components{component};
            return Install(components);
        }

        int Uninstall(const ulib::list<ulib::string> &components)
        {
            ulib::string rems = MakeComponentsList(components, "--remove");
            if (rems.empty())
                return 0;

            return ulib::process{ulib::format(u8"{} modify --productId Microsoft.VisualStudio.Product.Community {} "
                                              u8"--channelId \"VisualStudio.17.Release\" --norestart --passive",
                                              ulib::u8(mPath.generic_u8string()), rems)}
                .wait();
        }

        int Uninstall(ulib::string_view component)
        {
            ulib::list<ulib::string> components{component};
            return Uninstall(components);
        }

    private:
        fs::path mPath;
    };
} // namespace vcwin