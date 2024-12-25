#pragma once

#include <ulib/string.h>
#include <ulib/env.h>
#include <3rdparty/WinReg.hpp>
#include <filesystem>

#include <ulib/json.h>

namespace vcwin
{
    namespace fs = std::filesystem;

    class DirectXSdk
    {
    public:
        DirectXSdk()
        {
            mDXSDK_DIR = ulib::getenv(u8"DXSDK_DIR");

            mDXSDK_DIR_Source = "env:DXSDK_DIR";
            mVersionSource = "HKLM:SOFTWARE\\Microsoft\\DirectX\\SDKVersion";
            mPathSource = "Check Directories";

            {
                winreg::RegKey node;

                try
                {
                    node.Create(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\DirectX", KEY_READ | KEY_WOW64_32KEY);
                    mVersion = ulib::u8(node.GetStringValue(L"SDKVersion"));
                }
                catch (...)
                {
                }

                node.Close();
            }

            if (mDXSDK_DIR)
            {
                mPath = mDXSDK_DIR;
                mPathSource = mDXSDK_DIR_Source;
            }

            ulib::list<fs::path> sdk_paths = {
                "C:\\Program Files (x86)\\Microsoft DirectX SDK (June 2010)\\",
                "C:\\Program Files\\Microsoft DirectX SDK (June 2010)\\",
            };

            for (auto &path : sdk_paths)
            {
                if (fs::exists(path))
                {
                    if (mDXSDK_DIR)
                    {
                        auto first = ulib::u8string{path.u8string()}.replace(u8"/", u8"\\");
                        auto second = ulib::u8string{*mDXSDK_DIR}.replace(u8"/", u8"\\");

                        auto f = ulib::u8string_view{first}.rstrip(u8"\\");
                        auto s = ulib::u8string_view{second}.rstrip(u8"\\");

                        if (f != s)
                        {
                            mDXSDK_Mismatches.push_back({s, f});
                        }
                    }
                    else
                    {
                        mPath = path;
                    }
                }
            }
        }

        std::optional<ulib::u8string> GetVersion()
        {
            return mVersion;
        }
        ulib::string GetVersionSource()
        {
            return mVersionSource;
        }

        std::optional<fs::path> GetPath()
        {
            return mPath;
        }

        ulib::string GetPathSource()
        {
            return mPathSource;
        }

        std::optional<ulib::u8string> GetDXSDK_DIR()
        {
            return mDXSDK_DIR;
        }

        ulib::string GetDXSDK_DIR_Source()
        {
            return mDXSDK_DIR_Source;
        }

        ulib::json ToJson()
        {
            ulib::json val = ulib::json::object();

            val["version"] = mVersion;
            val["version_source"] = mVersionSource;
            val["path"] = mPath;
            val["path_source"] = mPathSource;
            val["DXSDK_DIR"] = mDXSDK_DIR;

            val["DXSDK_Mismatches"] = ulib::json::array();
            for (auto& mm : mDXSDK_Mismatches)
            {
                auto& newMm = val["DXSDK_Mismatches"].push_back();
                newMm.push_back() = mm.first;
                newMm.push_back() = mm.second;
            }

            return val;
        }

    private:
        std::optional<ulib::u8string> mVersion;
        ulib::string mVersionSource;

        std::optional<fs::path> mPath;
        ulib::string mPathSource;

        std::optional<ulib::u8string> mDXSDK_DIR;
        ulib::string mDXSDK_DIR_Source;

        ulib::list<std::pair<ulib::u8string, ulib::u8string>> mDXSDK_Mismatches;
    };
}