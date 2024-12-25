#pragma once

#include <3rdparty/WinReg.hpp>
#include <filesystem>
#include <futile/futile.h>
// #include <nlohmann/json.hpp>
#include <ulib/env.h>
#include <ulib/json.h>
#include <ulib/process.h>
#include <ulib/string.h>

namespace vcwin
{
    namespace fs = std::filesystem;

    namespace detail
    {
        inline fs::path find_vswhere()
        {
            fs::path vswhere_exe = "vswhere.exe";

            if (!std::filesystem::exists(vswhere_exe))
            {
                // Find the globally available vswhere.exe.
                std::filesystem::path program_files_x86 = std::getenv("ProgramFiles(x86)");
                vswhere_exe = program_files_x86 / "Microsoft Visual Studio/Installer/vswhere.exe";
            }

            if (!std::filesystem::exists(vswhere_exe))
            {
                // If it STILL hasn't been found, exit with an error
                throw ulib::RuntimeError("Failed to find 'vswhere.exe'! You probably don't have Visual Studio. ");
            }

            return vswhere_exe;
        }

        inline fs::path find_vs_installation_path(const fs::path &vswhere_path)
        {
            ulib::process vswhere{vswhere_path,
                                  {"-latest", "-nocolor", "-utf8", "-format", "json"},
                                  ulib::process::die_with_parent | ulib::process::pipe_stdout |
                                      ulib::process::pipe_stderr};

            if (vswhere.wait() != 0)
            {
                auto vswhere_error_output = ulib::sstr(vswhere.err().read_all());
                throw ulib::RuntimeError{"vswhere failed: " + vswhere_error_output};
            }

            auto vswhere_stuff_u = vswhere.out().read_all();
            auto vswhere_stuff_text = ulib::u8(vswhere_stuff_u);

            auto vswhere_stuff = ulib::json::parse(vswhere_stuff_text);
            
            if (vswhere_stuff.size() == 0)
                throw ulib::RuntimeError{"No Visual Studio installations were found on this computer"};

            const auto &vs_info = vswhere_stuff[0];
            std::filesystem::path vs_path = vs_info["installationPath"].get<std::string>();

            return vs_path;
        }

        inline ulib::string get_vc_tools_version(const fs::path &vs_path)
        {
            constexpr auto vctoolsversion_path = "VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt";
            auto vc_tools_version = futile::open(vs_path / vctoolsversion_path).lines<std::string>().front();

            return vc_tools_version;
        }
    } // namespace detail

    class VCTools
    {
    public:
        VCTools()
        {
            try
            {
                mVswherePath = detail::find_vswhere();

                if (mVswherePath)
                    mVSPath = detail::find_vs_installation_path(*mVswherePath);

                if (mVSPath)
                    mVCToolsDefaultVersion = detail::get_vc_tools_version(*mVSPath);
            }
            catch (...)
            {
            }
        }

        std::optional<fs::path> GetVSPath()
        {
            return mVSPath;
        };

        std::optional<fs::path> GetVswherePath()
        {
            return mVswherePath;
        };

        std::optional<ulib::string> GetVCToolsDefaultVersion()
        {
            return mVCToolsDefaultVersion;
        };

        ulib::json ToJson()
        {
            ulib::json val = ulib::json::object();

            val["vs_path"] = mVSPath;
            val["vswhere_path"] = mVSPath;
            val["vc_tools_default_version"] = mVSPath;

            return val;
        }

    private:
        std::optional<fs::path> mVSPath;
        std::optional<fs::path> mVswherePath;
        std::optional<ulib::string> mVCToolsDefaultVersion;
    };
} // namespace vcwin